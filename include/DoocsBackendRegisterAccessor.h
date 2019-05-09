#ifndef CHIMERATK_DOOCS_BACKEND_REGISTER_ACCESSOR_H
#define CHIMERATK_DOOCS_BACKEND_REGISTER_ACCESSOR_H
#include <condition_variable>
#include <mutex>
#include <queue>
#include <type_traits>

#include <ChimeraTK/AccessMode.h>
#include <ChimeraTK/Exception.h>
#include <ChimeraTK/FixedPointConverter.h>
#include <ChimeraTK/RegisterPath.h>
#include <ChimeraTK/SyncNDRegisterAccessor.h>

#include <eq_client.h>
#include <eq_fct.h>

#include "DoocsBackend.h"
#include "ZMQSubscriptionManager.h"

namespace ChimeraTK {

  /** This is the untemplated base class which unifies all data members not depending on the UserType. */
  class DoocsBackendRegisterAccessorBase {
   public:
    /// register path
    std::string _path;

    /// DOOCS address structure
    EqAdr ea;

    /// DOOCS rpc call object
    EqCall eq;

    /// DOOCS data structures
    EqData src, dst;

    /// flag if the DOOCS data type is an array or not
    bool isArray;

    /// number of elements
    size_t nElements;

    /// element offset specified by the user
    size_t elementOffset;

    /// flag if the accessor should affect only a part of the property (in case of
    /// an array)
    bool isPartial;

    /// flag if a ZeroMQ subscribtion is used for reading data (c.f.
    /// AccessMode::wait_for_new_data)
    bool useZMQ;

    /// Thread which might be launched in readAsync() if ZQM is *not* used
    boost::thread readAsyncThread;

    /// future_queue used to notify the TransferFuture about completed transfers
    cppext::future_queue<EqData> notifications;

    /// Flag whether TransferFuture has been created
    bool futureCreated{false};

    /// Flag whether shutdown() has been called or not
    bool shutdownCalled{false};

    ChimeraTK::VersionNumber currentVersion;
  };

  /********************************************************************************************************************/

  template<typename UserType>
  class DoocsBackendRegisterAccessor : public DoocsBackendRegisterAccessorBase, public NDRegisterAccessor<UserType> {
   public:
    ~DoocsBackendRegisterAccessor() override;

    /**
     * All implementations must call this function in their destructor. Also,
     * implementations must call it in their constructors before throwing an
     * exception.
     */
    void shutdown() {
      if(useZMQ) {
        DoocsBackendNamespace::ZMQSubscriptionManager::getInstance().unsubscribe(_path, this);
      }
      if(readAsyncThread.joinable()) {
        readAsyncThread.interrupt();
        readAsyncThread.join();
      }
      shutdownCalled = true;
    }

    void doReadTransfer() override;

    bool doReadTransferNonBlocking() override;

    bool doReadTransferLatest() override;

    bool doWriteTransfer(ChimeraTK::VersionNumber versionNumber = {}) override {
      write_internal();
      currentVersion = versionNumber;
      return false;
    }

    void doPostRead() override { currentVersion = {}; }

    AccessModeFlags getAccessModeFlags() const override {
      if(useZMQ) return {AccessMode::wait_for_new_data};
      return {};
    }

    TransferFuture doReadTransferAsync() override;

    void interrupt() override {
      try {
        throw boost::thread_interrupted();
      }
      catch(boost::thread_interrupted&) {
        notifications.push_exception(std::current_exception());
      }
    }

    ChimeraTK::VersionNumber getVersionNumber() const override { return currentVersion; }

    bool isReadOnly() const override { return false; }

    bool isReadable() const override { return true; }

    bool isWriteable() const override { return true; }

    using TransferElement::activeFuture;

    bool mayReplaceOther(const boost::shared_ptr<TransferElement const>& other) const override {
      auto rhsCasted = boost::dynamic_pointer_cast<const DoocsBackendRegisterAccessor<UserType>>(other);
      if(!rhsCasted) return false;
      if(_path != rhsCasted->_path) return false;
      if(nElements != rhsCasted->nElements) return false;
      if(elementOffset != rhsCasted->elementOffset) return false;
      return true;
    }

    std::vector<boost::shared_ptr<TransferElement>> getHardwareAccessingElements() override {
      return {boost::enable_shared_from_this<TransferElement>::shared_from_this()};
    }

    std::list<boost::shared_ptr<ChimeraTK::TransferElement>> getInternalElements() override { return {}; }

    void replaceTransferElement(boost::shared_ptr<TransferElement> /*newElement*/) override {} // LCOV_EXCL_LINE

   protected:
    DoocsBackendRegisterAccessor(boost::shared_ptr<DoocsBackend> backend, const std::string& path,
        const std::string& registerPathName, size_t numberOfWords, size_t wordOffsetInRegister, AccessModeFlags flags,
        bool allocateBuffers = true);

    /// internal write from EqData src
    void write_internal();

    /** Flag whether the initialisation has been performed already */
    bool isInitialised{false};

    /** Perform initialisation (i.e. connect to server etc.) */
    void initialise();
    virtual void initialiseImplementation() = 0;

    bool _allocateBuffers;

    boost::shared_ptr<DoocsBackend> _backend;
  };

  /********************************************************************************************************************/
  /********************************************************************************************************************/
  /** Implementations below this point                                                                               **/
  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendRegisterAccessor<UserType>::initialise() {
    if(isInitialised) return;

    // try to read data, to check connectivity and to obtain size of the register
    EqData tmp;
    int rc = eq.get(&ea, &tmp, &dst);
    if(rc) {
      throw ChimeraTK::runtime_error(std::string("Cannot read from DOOCS property: ") + dst.get_string());
    }

    // obtain number of elements
    size_t actualLength = dst.array_length();
    if(actualLength == 0 && dst.length() == 1) {
      actualLength = 1;
      isArray = false;
    }
    else {
      if(actualLength == 0) actualLength = dst.length();
      isArray = true;
    }
    if(nElements == 0) {
      nElements = actualLength;
    }
    if(nElements + elementOffset > actualLength) {
      throw ChimeraTK::logic_error("Requested number of words exceeds the "
                                   "length of the DOOCS property!");
    }
    if(nElements == actualLength && elementOffset == 0) {
      isPartial = false;
    }
    else {
      isPartial = true;
    }

    // allocate buffers
    if(_allocateBuffers) {
      NDRegisterAccessor<UserType>::buffer_2D.resize(1);
      NDRegisterAccessor<UserType>::buffer_2D[0].resize(nElements);
    }

    // set proper type information in the source EqData
    src.set_type(dst.type());
    if(_allocateBuffers && dst.type() != DATA_IIII) {
      src.length(actualLength);
    }

    // use ZeroMQ with AccessMode::wait_for_new_data
    if(useZMQ) {
      // create notification queue and TransferFuture
      notifications = cppext::future_queue<EqData>(3);
      activeFuture = TransferFuture(notifications.then<void>([](EqData&) {}, std::launch::deferred), this);
      futureCreated = true;

      // subscribe via subscription manager
      DoocsBackendNamespace::ZMQSubscriptionManager::getInstance().subscribe(_path, this);
    }

    initialiseImplementation();
    isInitialised = true;
  }

  /********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendRegisterAccessor<UserType>::DoocsBackendRegisterAccessor(boost::shared_ptr<DoocsBackend> backend,
      const std::string& path, const std::string& registerPathName, size_t numberOfWords, size_t wordOffsetInRegister,
      AccessModeFlags flags, bool allocateBuffers)
  : NDRegisterAccessor<UserType>(path), _allocateBuffers(allocateBuffers), _backend(backend) {
    try {
      _path = path;
      elementOffset = wordOffsetInRegister;
      nElements = numberOfWords;
      useZMQ = false;

      // check for unknown access mode flags
      flags.checkForUnknownFlags({AccessMode::wait_for_new_data});

      // set address
      ea.adr(std::string(path).c_str());

      // use zero mq subscriptiopn?
      if(flags.has(AccessMode::wait_for_new_data)) {
        useZMQ = true;
      }

      // if the backend has not yet been openend, obtain size of the register from catalogue
      if(allocateBuffers && !backend->isOpen()) {
        auto reg = backend->getRegisterCatalogue().getRegister(registerPathName);
        NDRegisterAccessor<UserType>::buffer_2D.resize(1);
        NDRegisterAccessor<UserType>::buffer_2D[0].resize(reg->getNumberOfElements());
        allocateBuffers = false;
      }
    }
    catch(...) {
      this->shutdown();
      throw;
    }
  }

  /********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendRegisterAccessor<UserType>::~DoocsBackendRegisterAccessor() {
    assert(shutdownCalled);
  }

  /********************************************************************************************************************/

  template<typename UserType>
  bool DoocsBackendRegisterAccessor<UserType>::doReadTransferNonBlocking() {
    if(!_backend->isOpen()) throw ChimeraTK::logic_error("Attept to read from closed device.");
    initialise();
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
          EqData tmp;
          int rc = eq.get(&ea, &tmp, &dst);
          // if again error received, throw exception
          if(rc) {
            throw ChimeraTK::runtime_error(std::string("Cannot read from DOOCS property: ") + dst.get_string());
          }
          // otherwise try again
          continue;
        }
        // we have received data, so return
        return true;
      }
    }
  }

  /********************************************************************************************************************/

  template<typename UserType>
  bool DoocsBackendRegisterAccessor<UserType>::doReadTransferLatest() {
    if(!_backend->isOpen()) throw ChimeraTK::logic_error("Attept to read from closed device.");
    initialise();
    if(!useZMQ) {
      this->doReadTransfer();
      return true;
    }
    else {
      if(notifications.empty()) return false;
      while(notifications.pop(dst))
        ; // remove all elements
      return true;
    }
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendRegisterAccessor<UserType>::doReadTransfer() {
    if(!_backend->isOpen()) throw ChimeraTK::logic_error("Attept to read from closed device.");
    initialise();
    boost::this_thread::interruption_point();
    if(!useZMQ) {
      // read data
      EqData tmp;
      int rc = eq.get(&ea, &tmp, &dst);
      // check error
      if(rc) {
        throw ChimeraTK::runtime_error(std::string("Cannot read from DOOCS property: ") + dst.get_string());
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
          EqData tmp;
          int rc = eq.get(&ea, &tmp, &dst);
          // if again error received, throw exception
          if(rc) {
            throw ChimeraTK::runtime_error(std::string("Cannot read from DOOCS property: ") + dst.get_string());
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
      throw ChimeraTK::runtime_error(std::string("Cannot read from DOOCS property: ") + dst.get_string());
    }
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendRegisterAccessor<UserType>::write_internal() {
    if(!_backend->isOpen()) throw ChimeraTK::logic_error("Attept to write to closed device.");
    initialise();
    // write data
    int rc = eq.set(&ea, &src, &dst);
    // check error
    if(rc || dst.error() != 0) {
      throw ChimeraTK::runtime_error(std::string("Cannot write to DOOCS property: ") + dst.get_string());
    }
  }

  /********************************************************************************************************************/

  template<typename UserType>
  TransferFuture DoocsBackendRegisterAccessor<UserType>::doReadTransferAsync() {
    if(!_backend->isOpen()) throw ChimeraTK::logic_error("Attept to read from closed device.");
    initialise();
    // create future_queue if not already created and continue it to enusre
    // postRead is called (in the user thread, so we use the deferred launch
    // policy)
    if(!futureCreated) {
      notifications = cppext::future_queue<EqData>(2);
      activeFuture = TransferFuture(notifications.then<void>([](EqData&) {}), this);
      futureCreated = true;
    }

    if(!useZMQ) {
      // launch doReadTransfer in separate thread
      readAsyncThread = boost::thread([this] {
        try {
          this->doReadTransfer();
        }
        catch(...) {
          this->notifications.push_exception(std::current_exception());
          throw;
        }
        this->notifications.push({});
      });
    }

    // return the TransferFuture
    return activeFuture;
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK

#endif /* CHIMERATK_DOOCS_BACKEND_REGISTER_ACCESSOR_H */
