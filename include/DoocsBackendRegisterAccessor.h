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

#include <mtca4u/NDRegisterAccessor.h>
#include <mtca4u/RegisterPath.h>
#include <mtca4u/DeviceException.h>
#include <mtca4u/FixedPointConverter.h>
#include <mtca4u/AccessMode.h>

#include <eq_client.h>
#include <eq_fct.h>

namespace mtca4u {

  template<typename UserType>
  class DoocsBackendRegisterAccessor : public NDRegisterAccessor<UserType> {

    public:

      virtual ~DoocsBackendRegisterAccessor();

      virtual unsigned int getNInputQueueElements() const;

    protected:

      DoocsBackendRegisterAccessor(const RegisterPath &path, size_t numberOfWords, size_t wordOffsetInRegister,
          AccessModeFlags flags, bool allocateBuffers = true);

      virtual bool isSameRegister(const boost::shared_ptr<TransferElement const> &other) const {
        auto rhsCasted = boost::dynamic_pointer_cast< const DoocsBackendRegisterAccessor<UserType> >(other);
        if(!rhsCasted) return false;
        if(_path != rhsCasted->_path) return false;
        return true;
      }

      virtual bool isReadOnly() const {
        return false;
      }

      /// register path
      RegisterPath _path;

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

      /// ring buffer of EqData buffers received via ZMQ
      std::queue<EqData> ZMQbuffer;

      /// condition_variable and mutex pair used to synchronise the ZMQqueue
      /// the mutex is mutable since we might need to obtain the lock also in const functions.
      std::condition_variable ZMQnotifier;
      mutable std::mutex ZMQmtx;

      /// timeout in milliseconds for initiating the ZeroMQ subscription
      static constexpr int ZMQsubscribeTimeout = 10000;

      /// internal read into EqData dst
      void read_internal();

      /// internal write from EqData src
      void write_internal();

      /// callback function for ZeroMQ
      /// This is a static function so we can pass a plain pointer to the DOOCS client. The first argument will contain
      /// the pointer to the object (will be statically casted into DoocsBackendRegisterAccessor<UserType>*).
      static void zmq_callback(void *self, EqData *data, dmsg_info_t *info);

      virtual std::vector< boost::shared_ptr<TransferElement> > getHardwareAccessingElements() {
        return { boost::enable_shared_from_this<TransferElement>::shared_from_this() };
      }

      virtual void replaceTransferElement(boost::shared_ptr<TransferElement> /*newElement*/) {} // LCOV_EXCL_LINE

  };

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendRegisterAccessor<UserType>::DoocsBackendRegisterAccessor(const RegisterPath &path, size_t numberOfWords,
      size_t wordOffsetInRegister, AccessModeFlags flags, bool allocateBuffers)
  : _path(path), elementOffset(wordOffsetInRegister), useZMQ(false)
  {

    // check for unknown access mode flags
    flags.checkForUnknownFlags({AccessMode::wait_for_new_data});

    // set address
    ea.adr(std::string(path).substr(1).c_str());        // strip leading slash

    // try to read data, to check connectivity and to obtain size of the register
    read_internal();

    // obtain number of elements
    size_t actualLength = dst.array_length();
    if(actualLength == 0) {
      actualLength = 1;
      isArray = false;
    }
    else {
      isArray = true;
    }
    if(numberOfWords == 0) {
      nElements = actualLength;
    }
    else {
      nElements = numberOfWords;
    }
    if(nElements + elementOffset > actualLength) {
        throw DeviceException("Requested number of words exceeds the length of the DOOCS property!",
            DeviceException::WRONG_PARAMETER);
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
    if(allocateBuffers) {
      src.length(actualLength);
    }

    // use ZeroMQ with AccessMode::wait_for_new_data
    if(flags.has(AccessMode::wait_for_new_data)) {
      // set flag
      useZMQ = true;
      // subscribe to property
      int err = dmsg_attach(&ea, &dst, (void*)this, &zmq_callback);
      if(err) {
        throw DeviceException(std::string("Cannot subscribe to DOOCS property via ZeroMQ: ")+dst.get_string(),
                  DeviceException::CANNOT_OPEN_DEVICEBACKEND);
      }
      // run dmsg_start() once
      std::unique_lock<std::mutex> lck(DoocsBackend::dmsgStartCalled_mutex);
      if(!DoocsBackend::dmsgStartCalled) {
        dmsg_start();
        DoocsBackend::dmsgStartCalled = true;
      }
      // wait until subscription is done
#ifndef DOOCS_COMPAT_18_10_5
      int timeoutCounter = 0;
      while(!dmsg_is_attached(&ea)) {
        usleep(1000);
        timeoutCounter++;
        if(timeoutCounter > ZMQsubscribeTimeout) {
          throw DeviceException("Cannot subscribe to DOOCS property via ZeroMQ: timeout reached while subscribing.",
                    DeviceException::CANNOT_OPEN_DEVICEBACKEND);
        }
      }
#else
      usleep(200000); // compatibility with DOOCS 18.10.5 and earlier: wait 200ms and hope the subscription is done.
#endif
    }
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendRegisterAccessor<UserType>::~DoocsBackendRegisterAccessor() {
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  unsigned int DoocsBackendRegisterAccessor<UserType>::getNInputQueueElements() const {
    if(!useZMQ) {
      return 1;
    }
    else {
      std::unique_lock<std::mutex> lck(ZMQmtx);
      return ZMQbuffer.size();
    }
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendRegisterAccessor<UserType>::read_internal() {
    if(!useZMQ) {
      // read data
      int rc = eq.get(&ea, &src, &dst);
      // check error
      if(rc) {
        throw DeviceException(std::string("Cannot read from DOOCS property: ")+dst.get_string(),
            DeviceException::CANNOT_OPEN_DEVICEBACKEND);
      }
    }
    else {
      std::unique_lock<std::mutex> lck(ZMQmtx);
      while(ZMQbuffer.empty()) ZMQnotifier.wait(lck);
      dst = ZMQbuffer.front();
      ZMQbuffer.pop();
    }
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendRegisterAccessor<UserType>::write_internal() {
    // write data
    int rc = eq.set(&ea, &src, &dst);
    // check error
    if(rc) {
      throw DeviceException(std::string("Cannot write to DOOCS property: ")+dst.get_string(),
          DeviceException::CANNOT_OPEN_DEVICEBACKEND);
    }
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendRegisterAccessor<UserType>::zmq_callback(void *self_, EqData *data, dmsg_info_t *info) {

    // obtain pointer to accessor object
    DoocsBackendRegisterAccessor<UserType> *self = static_cast<DoocsBackendRegisterAccessor<UserType>*>(self_);

    // create lock
    std::unique_lock<std::mutex> lck(self->ZMQmtx);

    // add (a copy of) EqData to queue
    self->ZMQbuffer.push(*data);

    // send notification to a potentially waiting read()
    self->ZMQnotifier.notify_one();
  }

} /* namespace mtca4u */

#endif /* MTCA4U_DOOCS_BACKEND_REGISTER_ACCESSOR_H */
