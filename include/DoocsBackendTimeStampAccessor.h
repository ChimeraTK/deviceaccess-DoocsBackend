#pragma once

#include <type_traits>

#include <eq_client.h>

#include <ChimeraTK/SupportedUserTypes.h>

#include "DoocsBackendRegisterAccessor.h"

namespace ChimeraTK {

  template<typename UserType>
  class DoocsBackendTimeStampRegisterAccessor : public DoocsBackendRegisterAccessor<UserType> {
   public:
    ~DoocsBackendTimeStampRegisterAccessor() override;

   protected:
    DoocsBackendTimeStampRegisterAccessor(boost::shared_ptr<DoocsBackend> backend, const std::string& path,
        const std::string& registerPathName, AccessModeFlags flags);

    void doPostRead() override;

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
  DoocsBackendTimeStampRegisterAccessor<UserType>::DoocsBackendTimeStampRegisterAccessor(
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
  DoocsBackendTimeStampRegisterAccessor<UserType>::~DoocsBackendTimeStampRegisterAccessor() {
    this->shutdown();
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendTimeStampRegisterAccessor<UserType>::doPostRead() {
    EqData d = DoocsBackendRegisterAccessor<UserType>::dst;
    auto timeStamp = d.get_timestamp();

    UserType val = numericToUserType<UserType>(timeStamp.get_seconds_since_epoch());
    NDRegisterAccessor<UserType>::buffer_2D[0][0] = val;

    DoocsBackendRegisterAccessor<UserType>::doPostRead();
  }

} // namespace ChimeraTK
