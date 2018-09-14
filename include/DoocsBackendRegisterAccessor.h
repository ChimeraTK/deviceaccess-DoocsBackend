/*
 * DoocsBackendRegisterAccessor.h
 *
 *  Created on: May 3, 2016
 *      Author: Martin Hierholzer
 */

#ifndef MTCA4U_DOOCS_BACKEND_REGISTER_ACCESSOR_H
#define MTCA4U_DOOCS_BACKEND_REGISTER_ACCESSOR_H

#include <mutex>
#include <type_traits>
#include <condition_variable>
#include <queue>

#include <ChimeraTK/SyncNDRegisterAccessor.h>
#include <ChimeraTK/RegisterPath.h>
#include <ChimeraTK/Exception.h>
#include <ChimeraTK/FixedPointConverter.h>
#include <ChimeraTK/AccessMode.h>

#include <eq_client.h>
#include <eq_fct.h>

namespace ChimeraTK {

  template<typename UserType>
  class DoocsBackendRegisterAccessor : public NDRegisterAccessor<UserType> {

    public:

      virtual ~DoocsBackendRegisterAccessor();

      /**
       * All implementations must call this function in their destructor. Also, implementations must call it in their
       * constructors before throwing an exception.
       */
      void shutdown() {
        if(readAsyncThread.joinable()) {
          readAsyncThread.interrupt();
          readAsyncThread.join();
        }
        shutdownCalled = true;
      }

      void doReadTransfer() override;

      bool doReadTransferNonBlocking() override;

      bool doReadTransferLatest() override;

      bool doWriteTransfer(ChimeraTK::VersionNumber /*versionNumber*/={}) override {
        write_internal();
        return false;
      }

      AccessModeFlags getAccessModeFlags() const override {
        if(useZMQ) return {AccessMode::wait_for_new_data};
        return {};
      }

      TransferFuture doReadTransferAsync();

    protected:

      DoocsBackendRegisterAccessor(const std::string &path, size_t numberOfWords, size_t wordOffsetInRegister,
          AccessModeFlags flags, bool allocateBuffers = true);

      bool mayReplaceOther(const boost::shared_ptr<TransferElement const> &other) const override {
        auto rhsCasted = boost::dynamic_pointer_cast< const DoocsBackendRegisterAccessor<UserType> >(other);
        if(!rhsCasted) return false;
        if(_path != rhsCasted->_path) return false;
        return true;
      }

      bool isReadOnly() const override {
        return false;
      }

      bool isReadable() const override {
        return true;
      }

      bool isWriteable() const override {
        return true;
      }

      /// register path
      std::string _path;

      /// DOOCS address structure
      EqAdr ea;

      /// DOOCS rpc call object
      EqCall eq;

      /// DOOCS data structures
      EqData src,dst;

      /// flag if the DOOCS data type is an array or not
      bool isArray;

      /// number of elements
      size_t nElements;

      /// element offset specified by the user
      size_t elementOffset;

      /// flag if the accessor should affect only a part of the property (in case of an array)
      bool isPartial;

      /// flag if a ZeroMQ subscribtion is used for reading data (c.f. AccessMode::wait_for_new_data)
      bool useZMQ;

      /// Thread which might be launched in readAsync() if ZQM is *not* used
      boost::thread readAsyncThread;

      /// future_queue used to notify the TransferFuture about completed transfers
      cppext::future_queue<EqData> notifications;

      /// Flag whether TransferFuture has been created
      bool futureCreated{false};

      /// Flag whether shutdown() has been called or not
      bool shutdownCalled{false};

      using TransferElement::activeFuture;

      /// internal write from EqData src
      void write_internal();

      /// callback function for ZeroMQ
      /// This is a static function so we can pass a plain pointer to the DOOCS client. The first argument will contain
      /// the pointer to the object (will be statically casted into DoocsBackendRegisterAccessor<UserType>*).
      static void zmq_callback(void *self, EqData *data, dmsg_info_t *info);

      std::vector< boost::shared_ptr<TransferElement> > getHardwareAccessingElements() override {
        return { boost::enable_shared_from_this<TransferElement>::shared_from_this() };
      }

      std::list<boost::shared_ptr<ChimeraTK::TransferElement> > getInternalElements() override {
        return {};
      }

      void replaceTransferElement(boost::shared_ptr<TransferElement> /*newElement*/) override {} // LCOV_EXCL_LINE

  };

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendRegisterAccessor<UserType>::DoocsBackendRegisterAccessor(const std::string &path, size_t numberOfWords,
      size_t wordOffsetInRegister, AccessModeFlags flags, bool allocateBuffers)
  : NDRegisterAccessor<UserType>(path),
    _path(path),
    elementOffset(wordOffsetInRegister),
    useZMQ(false)
  {
    try {
      // check for unknown access mode flags
      flags.checkForUnknownFlags({AccessMode::wait_for_new_data});

      // set address
      ea.adr(std::string(path).c_str());

      // try to read data, to check connectivity and to obtain size of the register
      doReadTransfer();

      // obtain number of elements
      size_t actualLength = dst.array_length();
      if(actualLength == 0 && dst.length() == 1) {
        actualLength = 1;
        isArray = false;
      }
      else {
        if (actualLength == 0)
            actualLength = dst.length();
        isArray = true;
      }
      if(numberOfWords == 0) {
        nElements = actualLength;
      }
      else {
        nElements = numberOfWords;
      }
      if(nElements + elementOffset > actualLength) {
          throw ChimeraTK::logic_error("Requested number of words exceeds the length of the DOOCS property!");
      }
      if(nElements == actualLength && elementOffset == 0) {
        isPartial = false;
      }
      else {
        isPartial = true;
      }

      // allocate buffers
      if(allocateBuffers) {
        NDRegisterAccessor<UserType>::buffer_2D.resize(1);
        NDRegisterAccessor<UserType>::buffer_2D[0].resize(nElements);
      }

      // set proper type information in the source EqData
      src.set_type(dst.type());
      if(allocateBuffers && dst.type() != DATA_IIII) {
        src.length(actualLength);
      }

      // use ZeroMQ with AccessMode::wait_for_new_data
      if(flags.has(AccessMode::wait_for_new_data)) {
        // set flag
        useZMQ = true;

        // create notification queue and TransferFuture
        notifications = cppext::future_queue<EqData>(3);
        activeFuture = TransferFuture(notifications.then<void>([](EqData&){}, std::launch::deferred), this);
        futureCreated = true;

        // subscribe to property
        int err = dmsg_attach(&ea, &dst, (void*)this, &zmq_callback);
        if(err) {
          throw ChimeraTK::runtime_error(std::string("Cannot subscribe to DOOCS property via ZeroMQ: ")+dst.get_string());
        }
        // run dmsg_start() once
        std::unique_lock<std::mutex> lck(DoocsBackend::dmsgStartCalled_mutex);
        if(!DoocsBackend::dmsgStartCalled) {
          dmsg_start();
          DoocsBackend::dmsgStartCalled = true;
        }
      }
    }
    catch(...) {
      this->shutdown();
      throw;
    }
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendRegisterAccessor<UserType>::~DoocsBackendRegisterAccessor() {
    assert(shutdownCalled);
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  bool DoocsBackendRegisterAccessor<UserType>::doReadTransferNonBlocking() {
    if(!useZMQ) {
      this->doReadTransfer();
      return true;
    }
    else {
      // loop to allow retries in case of timeout errors
      while(true) {
        // wait until new data has been received
        bool gotData = notifications.pop(dst);
        if(!gotData) return false;
        // check for an error
        if(dst.error() != 0) {
          // try obtaining data through RPC call instead to verify error
          int rc = eq.get(&ea, &src, &dst);
          // if again error received, throw exception
          if(rc) {
            throw ChimeraTK::runtime_error(std::string("Cannot read from DOOCS property: ")+dst.get_string());
          }
          // otherwise try again
          continue;
        }
        // we have received data, so return
        return true;
      }
    }
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  bool DoocsBackendRegisterAccessor<UserType>::doReadTransferLatest() {
    if(!useZMQ) {
      this->doReadTransfer();
      return true;
    }
    else {
      if(notifications.empty()) return false;
      while(notifications.pop(dst));            // remove all elements
      return true;
    }
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendRegisterAccessor<UserType>::doReadTransfer() {
    boost::this_thread::interruption_point();
    if(!useZMQ) {
      // read data
      int rc = eq.get(&ea, &src, &dst);
      // check error
      if(rc) {
        throw ChimeraTK::runtime_error(std::string("Cannot read from DOOCS property: ")+dst.get_string());
      }
    }
    else {
      // loop to allow retries in case of timeout errors
      while(true) {
        // wait until new data has been received
        notifications.pop_wait(dst);
        // check for an error
        if(dst.error() != 0) {
          // try obtaining data through RPC call instead to verify error
          int rc = eq.get(&ea, &src, &dst);
          // if again error received, throw exception
          if(rc) {
            throw ChimeraTK::runtime_error(std::string("Cannot read from DOOCS property: ")+dst.get_string());
          }
          // otherwise try again
          continue;
        }
        // terminate loop, since no retry needed
        break;
      }
    }
    // check for error code in dst
    if(dst.error() != 0) {
      throw ChimeraTK::runtime_error(std::string("Cannot read from DOOCS property: ")+dst.get_string());
    }
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendRegisterAccessor<UserType>::write_internal() {
    // write data
    int rc = eq.set(&ea, &src, &dst);
    // check error
    if(rc || dst.error() != 0) {
      throw ChimeraTK::runtime_error(std::string("Cannot write to DOOCS property: ")+dst.get_string());
    }
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendRegisterAccessor<UserType>::zmq_callback(void *self_, EqData *data, dmsg_info_t *) {

    // obtain pointer to accessor object
    DoocsBackendRegisterAccessor<UserType> *self = static_cast<DoocsBackendRegisterAccessor<UserType>*>(self_);

    // add (a copy of) EqData to queue
    self->notifications.push_overwrite(*data);

  }

  /**********************************************************************************************************************/

  template<typename UserType>
  TransferFuture DoocsBackendRegisterAccessor<UserType>::doReadTransferAsync() {
    // create future_queue if not already created and continue it to enusre postRead is called (in the user thread,
    // so we use the deferred launch policy)
    if(!futureCreated) {
      notifications = cppext::future_queue<EqData>(2);
      activeFuture = TransferFuture(notifications.then<void>([](EqData&){}), this);
      futureCreated = true;
    }

    if(!useZMQ) {
      // launch doReadTransfer in separate thread
      readAsyncThread = boost::thread(
        [this] {
          try {
            this->doReadTransfer();
          }
          catch(...) {
            this->notifications.push_exception(std::current_exception());
            throw;
          }
          this->notifications.push({});
        }
      );
    }

    // return the TransferFuture
    return activeFuture;
  }
} /* namespace mtca4u */

#endif /* MTCA4U_DOOCS_BACKEND_REGISTER_ACCESSOR_H */
