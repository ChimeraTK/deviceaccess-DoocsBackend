/*
 * DoocsBackendStringRegisterAccessor.h
 *
 *  Created on: May 3, 2016
 *      Author: Martin Hierholzer
 */

#ifndef MTCA4U_DOOCS_BACKEND_STRING_REGISTER_ACCESSOR_H
#define MTCA4U_DOOCS_BACKEND_STRING_REGISTER_ACCESSOR_H

#include <type_traits>

#include <mtca4u/NDRegisterAccessor.h>
#include <mtca4u/DeviceException.h>
#include <mtca4u/FixedPointConverter.h>

#include <eq_client.h>

#include "DoocsBackendRegisterAccessor.h"

namespace mtca4u {

  template<typename UserType>
  class DoocsBackendStringRegisterAccessor : public DoocsBackendRegisterAccessor<UserType> {

    public:

      virtual ~DoocsBackendStringRegisterAccessor();

    protected:

      DoocsBackendStringRegisterAccessor(const RegisterPath &path, size_t numberOfWords, size_t wordOffsetInRegister,
          AccessModeFlags flags);

      void postRead() override;

      void preWrite() override;

      friend class DoocsBackend;
  };

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendStringRegisterAccessor<UserType>::DoocsBackendStringRegisterAccessor(const RegisterPath &path,
      size_t numberOfWords, size_t wordOffsetInRegister, AccessModeFlags flags)
  : DoocsBackendRegisterAccessor<UserType>(path, numberOfWords, wordOffsetInRegister, flags, false)
  {
    try {
      // set buffer size (nElements will be the number of characters, so the buffer allocation in the base class would be incorrect)
      NDRegisterAccessor<UserType>::buffer_2D.resize(1);
      NDRegisterAccessor<UserType>::buffer_2D[0].resize(1);

      // check data type
      if( DoocsBackendRegisterAccessor<UserType>::dst.type() != DATA_TEXT &&
          DoocsBackendRegisterAccessor<UserType>::dst.type() != DATA_STRING &&
          DoocsBackendRegisterAccessor<UserType>::dst.type() != DATA_STRING16) {
        throw DeviceException("DOOCS data type not supported by DoocsBackendStringRegisterAccessor.",     // LCOV_EXCL_LINE (already prevented in the Backend)
            DeviceException::WRONG_PARAMETER);                                                            // LCOV_EXCL_LINE
      }

      // check UserType
      if(typeid(UserType) != typeid(std::string)) {
        throw DeviceException("Trying to access a string DOOCS property with a non-string user data type.",
            DeviceException::WRONG_PARAMETER);
      }
    }
    catch(...) {
      this->shutdown();
      throw;
    }
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendStringRegisterAccessor<UserType>::~DoocsBackendStringRegisterAccessor() {
    this->shutdown();
  }


  /**********************************************************************************************************************/

  template<>
  void DoocsBackendStringRegisterAccessor<std::string>::postRead() {
    // copy data into our buffer
    NDRegisterAccessor<std::string>::buffer_2D[0][0] = DoocsBackendRegisterAccessor<std::string>::dst.get_string();
  }

  /**********************************************************************************************************************/

  template<>
  void DoocsBackendStringRegisterAccessor<std::string>::preWrite() {
    // copy data into our buffer
    DoocsBackendRegisterAccessor<std::string>::src.set(NDRegisterAccessor<std::string>::buffer_2D[0][0].c_str());
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendStringRegisterAccessor<UserType>::postRead() {                                           // LCOV_EXCL_LINE (already prevented in constructor)
    throw DeviceException("Trying to access a string DOOCS property with a non-string user data type.",     // LCOV_EXCL_LINE
        DeviceException::WRONG_PARAMETER);                                                                  // LCOV_EXCL_LINE
  }                                                                                                         // LCOV_EXCL_LINE

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendStringRegisterAccessor<UserType>::preWrite() {                                           // LCOV_EXCL_LINE (already prevented in constructor)
    throw DeviceException("Trying to access a string DOOCS property with a non-string user data type.",     // LCOV_EXCL_LINE
        DeviceException::WRONG_PARAMETER);                                                                  // LCOV_EXCL_LINE
  }                                                                                                         // LCOV_EXCL_LINE

} /* namespace mtca4u */

#endif /* MTCA4U_DOOCS_BACKEND_STRING_REGISTER_ACCESSOR_H */
