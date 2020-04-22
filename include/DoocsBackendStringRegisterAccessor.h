/*
 * DoocsBackendStringRegisterAccessor.h
 *
 *  Created on: May 3, 2016
 *      Author: Martin Hierholzer
 */

#ifndef MTCA4U_DOOCS_BACKEND_STRING_REGISTER_ACCESSOR_H
#define MTCA4U_DOOCS_BACKEND_STRING_REGISTER_ACCESSOR_H

#include <type_traits>

#include <eq_client.h>

#include "DoocsBackendRegisterAccessor.h"

namespace ChimeraTK {

  template<typename UserType>
  class DoocsBackendStringRegisterAccessor : public DoocsBackendRegisterAccessor<UserType> {
   public:
    virtual ~DoocsBackendStringRegisterAccessor();

   protected:
    DoocsBackendStringRegisterAccessor(boost::shared_ptr<DoocsBackend> backend, const std::string& path,
        const std::string& registerPathName, size_t numberOfWords, size_t wordOffsetInRegister, AccessModeFlags flags);

    void doPostRead(TransferType type, bool hasNewData) override;

    void doPreWrite(TransferType type, VersionNumber version) override;

    void initialiseImplementation() override;

    friend class DoocsBackend;
  };

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendStringRegisterAccessor<UserType>::DoocsBackendStringRegisterAccessor(
      boost::shared_ptr<DoocsBackend> backend, const std::string& path, const std::string& registerPathName,
      size_t numberOfWords, size_t wordOffsetInRegister, AccessModeFlags flags)
  : DoocsBackendRegisterAccessor<UserType>(
        backend, path, registerPathName, numberOfWords, wordOffsetInRegister, flags, false) {
    try {
      // set buffer size (nElements will be the number of characters, so the
      // buffer allocation in the base class would be incorrect)
      NDRegisterAccessor<UserType>::buffer_2D.resize(1);
      NDRegisterAccessor<UserType>::buffer_2D[0].resize(1);

      // check UserType
      if(typeid(UserType) != typeid(std::string)) {
        throw ChimeraTK::logic_error("Trying to access a string DOOCS property "
                                     "with a non-string user data type.");
      }

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
  void DoocsBackendStringRegisterAccessor<UserType>::initialiseImplementation() {
    // check data type
    if(DoocsBackendRegisterAccessor<UserType>::dst.type() != DATA_TEXT &&
        DoocsBackendRegisterAccessor<UserType>::dst.type() != DATA_STRING &&
        DoocsBackendRegisterAccessor<UserType>::dst.type() != DATA_STRING16) {
      throw ChimeraTK::logic_error("DOOCS data type not supported by "
                                   "DoocsBackendStringRegisterAccessor."); // LCOV_EXCL_LINE (already
                                                                           // prevented in the Backend)
    }
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendStringRegisterAccessor<UserType>::~DoocsBackendStringRegisterAccessor() {
    this->shutdown();
  }

  /**********************************************************************************************************************/

  template<>
  void DoocsBackendStringRegisterAccessor<std::string>::doPostRead(TransferType type, bool hasNewData) {
    DoocsBackendRegisterAccessor<std::string>::doPostRead(type, hasNewData);
    if(!hasNewData) return;

    // copy data into our buffer
    NDRegisterAccessor<std::string>::buffer_2D[0][0] = DoocsBackendRegisterAccessor<std::string>::dst.get_string();
  }

  /**********************************************************************************************************************/

  template<>
  void DoocsBackendStringRegisterAccessor<std::string>::doPreWrite(TransferType type, VersionNumber version) {
    DoocsBackendRegisterAccessor<std::string>::doPreWrite(type, version);

    // copy data into our buffer
    DoocsBackendRegisterAccessor<std::string>::src.set(NDRegisterAccessor<std::string>::buffer_2D[0][0].c_str());
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendStringRegisterAccessor<UserType>::doPostRead(
      TransferType, bool) { // LCOV_EXCL_LINE (already prevented in constructor)
    throw ChimeraTK::logic_error("Trying to access a string DOOCS property with a non-string user data "
                                 "type."); // LCOV_EXCL_LINE
  }                                        // LCOV_EXCL_LINE

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendStringRegisterAccessor<UserType>::doPreWrite(
      TransferType, VersionNumber) { // LCOV_EXCL_LINE (already prevented in constructor)
    throw ChimeraTK::logic_error("Trying to access a string DOOCS property with a non-string user data "
                                 "type."); // LCOV_EXCL_LINE
  }                                        // LCOV_EXCL_LINE

} // namespace ChimeraTK

#endif /* MTCA4U_DOOCS_BACKEND_STRING_REGISTER_ACCESSOR_H */
