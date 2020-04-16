#pragma once

#include <type_traits>

#include <eq_client.h>

#include <ChimeraTK/SupportedUserTypes.h>

#include "DoocsBackendRegisterAccessor.h"

namespace ChimeraTK {

  template<typename UserType>
  class DoocsBackendEventIdRegisterAccessor : public DoocsBackendRegisterAccessor<UserType> {
   public:
    virtual ~DoocsBackendEventIdRegisterAccessor();

   protected:
    DoocsBackendEventIdRegisterAccessor(boost::shared_ptr<DoocsBackend> backend, const std::string& path,
        const std::string& registerPathName, AccessModeFlags flags);

    void doPostRead(TransferType type, bool hasNewData) override;

    bool isReadOnly() const override { return true; }

    bool isReadable() const override { return true; }

    bool isWriteable() const override { return false; }

    void initialiseImplementation() override {}

    bool doWriteTransfer(VersionNumber /*versionNumber = {}*/) override {
      throw ChimeraTK::logic_error(
          "Trying to write to read-only register \"" + DoocsBackendRegisterAccessor<UserType>::_path + "\"");

      return false;
    }

    friend class DoocsBackend;
  };

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendEventIdRegisterAccessor<UserType>::DoocsBackendEventIdRegisterAccessor(
      boost::shared_ptr<DoocsBackend> backend, const std::string& path, const std::string& registerPathName,
      AccessModeFlags flags)
  : DoocsBackendRegisterAccessor<UserType>(backend, path, registerPathName, 1, 0, flags) {
    try {
      // initialise fully only if backend is open
      if(backend->isOpen()) {
        this->initialise();
      }
    }
    catch(...) {
      this->shutdown();
      throw;
    }
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendEventIdRegisterAccessor<UserType>::~DoocsBackendEventIdRegisterAccessor() {
    this->shutdown();
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendEventIdRegisterAccessor<UserType>::doPostRead(TransferType type, bool hasNewData) {
    DoocsBackendRegisterAccessor<UserType>::doPostRead(type, hasNewData);
    if(!hasNewData) return;

    EqData d = DoocsBackendRegisterAccessor<UserType>::dst;
    auto id = d.get_event_id();

    UserType val = numericToUserType<UserType>(id.to_int());
    NDRegisterAccessor<UserType>::buffer_2D[0][0] = val;
  }

} // namespace ChimeraTK
