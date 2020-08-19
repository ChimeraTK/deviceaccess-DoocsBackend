#ifndef CHIMERATK_DOOCS_BACKEND_REGISTER_ACCESSOR_H
#define CHIMERATK_DOOCS_BACKEND_REGISTER_ACCESSOR_H
#include <mutex>
#include <queue>
#include <type_traits>

#include <ChimeraTK/AccessMode.h>
#include <ChimeraTK/Exception.h>
#include <ChimeraTK/FixedPointConverter.h>
#include <ChimeraTK/RegisterPath.h>
#include <ChimeraTK/NDRegisterAccessor.h>

#include <eq_client.h>
#include <eq_fct.h>
#include <eq_errors.h>

#include "DoocsBackend.h"
#include "EventIdMapper.h"
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
    bool isArray{false};

    /// number of elements
    size_t nElements{0};

    /// element offset specified by the user
    size_t elementOffset{0};

    /// flag if the accessor should affect only a part of the property (in case of an array)
    bool isPartial{false};

    /// flag if a ZeroMQ subscribtion is used for reading data (c.f. AccessMode::wait_for_new_data)
    bool useZMQ{false};

    /// future_queue used to notify the TransferFuture about completed transfers
    cppext::future_queue<EqData> notifications;

    /// Flag whether shutdown() has been called or not
    bool shutdownCalled{false};

    /// Pointer to the backend
    boost::shared_ptr<DoocsBackend> _backend;

   protected:
    /// first valid eventId
    //doocs::EventId _startEventId;
    doocs::EventId _lastEventId;
    VersionNumber _lastVersionNumber;
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
      shutdownCalled = true;
    }

    void doReadTransferSynchronously() override;

    bool doWriteTransfer(VersionNumber) override {
      write_internal();
      return false;
    }

    void doPreRead(TransferType) override {
      if(!_backend->isOpen()) throw ChimeraTK::logic_error("Read operation not allowed while device is closed.");
      initialise();
      if(!isReadable()) throw ChimeraTK::logic_error("Try to read from write-only register \"" + _path + "\".");
    }

    void doPreWrite(TransferType, VersionNumber) override {
      if(!_backend->isOpen()) throw ChimeraTK::logic_error("Write operation not allowed while device is closed.");
      initialise();
      if(!isWriteable()) throw ChimeraTK::logic_error("Try to write read-only register \"" + _path + "\".");
    }

    void doPostRead(TransferType, bool hasNewData) override {
      TransferElement::setDataValidity(DataValidity::ok);
      if(!hasNewData) return;
      // Note: the original idea was to extract the time stamp from the received data. This idea has been dropped since
      // the time stamp attached to the data seems to be unreliably, at least for the x2timer macro pulse number. If the
      // unreliable time stamp is attached to the trigger, all data will get this time stamp. This leads to error
      // messages of the DOOCS history archiver, which rejects data due to wrong time stamps. Hence we better generate
      // our own time stamp here.
      //if the eventid is already there create new version number but dont add it to the map. Add datfault flag.
      //TODO!! Ignore if eventId is the first entry, server startup.
      //if the eventid is not the first entry in the map print warning.
      if (dst.get_event_id() <= _lastEventId)  {
        TransferElement::_versionNumber = VersionNumber();
        TransferElement::setDataValidity(DataValidity::faulty);
        std::cout<<"warning, eventId smaller than the last vaild eventId"<<std::endl;
      }
      else {
        TransferElement::_versionNumber = EventIdMapper::getInstance().getVersionForEventId(dst.get_event_id());
        if (TransferElement::_versionNumber < _lastVersionNumber) {
          TransferElement::_versionNumber = VersionNumber();
        }
        _lastEventId = dst.get_event_id();
      }
      _lastVersionNumber = TransferElement::_versionNumber;
    }

    bool isReadOnly() const override { return isReadable() && not isWriteable(); }

    bool isReadable() const override { return _isReadable; }

    bool isWriteable() const override { return _isWriteable; }

    using TransferElement::_readQueue;

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

    void interrupt() override { this->interrupt_impl(this->notifications); }

   protected:
    DoocsBackendRegisterAccessor(boost::shared_ptr<DoocsBackend> backend, const std::string& path,
        const std::string& registerPathName, size_t numberOfWords, size_t wordOffsetInRegister, AccessModeFlags flags,
        bool allocateBuffers = true);

    /// internal write from EqData src
    void write_internal();

    /** Flag whether the initialisation has been performed already */
    bool isInitialised{false};

    /**
     *  Perform initialisation (i.e. connect to server etc.). 
     * 
     *  Note: must *only* throw ChimeraTK::logic_error. Just do not proceed with the initialisation if a runtime_error
     *  is to be thrown - this will then be done in the transfer.
     */
    void initialise();

    /**
     *  Perform accessor-type specific part of the initialisation. Called at the end of initialise().
     * 
     *  Note: must *only* throw ChimeraTK::logic_error. Just do not proceed with the initialisation if a runtime_error
     *  is to be thrown - this will then be done in the transfer.
     */
    virtual void initialiseImplementation() = 0;

    bool _allocateBuffers;
    bool _isReadable;
    bool _isWriteable;
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
      if(rc == eq_errors::ill_property || rc == eq_errors::ill_location ||
          rc == eq_errors::ill_address) { // no property by that name
        throw ChimeraTK::logic_error("Property does not exist: " + _path);
      }
      // runtime error will later be thrown by the transfer...
      return;
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
      throw ChimeraTK::logic_error("Requested number of words exceeds the length of the DOOCS property!");
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
  : NDRegisterAccessor<UserType>(path, flags), _allocateBuffers(allocateBuffers), _isReadable(true),
    _isWriteable(true) {
    try {
      _backend = backend;
      _path = path;
      elementOffset = wordOffsetInRegister;
      nElements = numberOfWords;
      useZMQ = false;

      // check for unknown access mode flags
      flags.checkForUnknownFlags({AccessMode::wait_for_new_data});

      // set address
      ea.adr(path);

      // use zero mq subscriptiopn?
      auto reg = backend->getRegisterCatalogue().getRegister(registerPathName);

      if(flags.has(AccessMode::wait_for_new_data)) {
        if(!reg->getSupportedAccessModes().has(AccessMode::wait_for_new_data)) {
          throw ChimeraTK::logic_error("invalid access mode for this register");
        }
        useZMQ = true;

        // Create notification queue.
        notifications = cppext::future_queue<EqData>(3);
        _readQueue = notifications.then<void>([this](EqData& data) { this->dst = data; }, std::launch::deferred);
      }

      if(not backend->isOpen()) {
        // we have to check the size agains the catalogue.
        size_t actualLength = reg->getNumberOfElements();
        if(nElements == 0) nElements = actualLength;

        if(nElements + elementOffset > actualLength) {
          throw ChimeraTK::logic_error("Requested number of words exceeds the length of the DOOCS property!");
        }

        // if the backend has not yet been openend, obtain size of the register from catalogue
        if(allocateBuffers) {
          NDRegisterAccessor<UserType>::buffer_2D.resize(1);
          NDRegisterAccessor<UserType>::buffer_2D[0].resize(actualLength);

          allocateBuffers = false;
        }
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
  void DoocsBackendRegisterAccessor<UserType>::doReadTransferSynchronously() {
    if(!isInitialised) {
      // if initialise() fails to contact the server, it cannot yet throw a runtime_error, so we have to do this here.
      _backend->informRuntimeError(_path);
      throw ChimeraTK::runtime_error(std::string("Cannot read from DOOCS property: ") + dst.get_string());
    }
    if(!_backend->isFunctional()) {
      throw ChimeraTK::runtime_error(std::string("Exception reported by another accessor."));
    }

    assert(!useZMQ);

    boost::this_thread::interruption_point();

    // read data
    EqData tmp;
    int rc = eq.get(&ea, &tmp, &dst);

    // check error
    if(rc) {
      _backend->informRuntimeError(_path);
      throw ChimeraTK::runtime_error(std::string("Cannot read from DOOCS property: ") + dst.get_string());
    }
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendRegisterAccessor<UserType>::write_internal() {
    if(!isInitialised) {
      // if initialise() fails to contact the server, it cannot yet throw a runtime_error, so we have to do this here.
      _backend->informRuntimeError(_path);
      throw ChimeraTK::runtime_error(std::string("Cannot read from DOOCS property: ") + dst.get_string());
    }
    if(!_backend->isFunctional()) {
      throw ChimeraTK::runtime_error(std::string("Exception reported by another accessor."));
    }

    // write data
    int rc = eq.set(&ea, &src, &dst);
    // check error
    if(rc || dst.error() != 0) {
      _backend->informRuntimeError(_path);
      if(dst.error() == eq_errors::read_only) {
        this->_isWriteable = false;
      }
      throw ChimeraTK::runtime_error(std::string("Cannot write to DOOCS property: ") + dst.get_string());
    }
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK

#endif /* CHIMERATK_DOOCS_BACKEND_REGISTER_ACCESSOR_H */
