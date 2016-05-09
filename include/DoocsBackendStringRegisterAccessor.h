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

      DoocsBackendStringRegisterAccessor(const RegisterPath &path);

      virtual ~DoocsBackendStringRegisterAccessor();

      virtual void read();

      virtual void write();
  };

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendStringRegisterAccessor<UserType>::DoocsBackendStringRegisterAccessor(const RegisterPath &path)
  : DoocsBackendRegisterAccessor<UserType>(path, false)
  {

    // set buffer size (nElements will be the number of characters, so the buffer allocation in the base class would be incorrect)
    NDRegisterAccessor<UserType>::buffer_2D.resize(1);
    NDRegisterAccessor<UserType>::buffer_2D[0].resize(1);

    // check data type
    if( DoocsBackendRegisterAccessor<UserType>::dst.type() != DATA_TEXT &&
        DoocsBackendRegisterAccessor<UserType>::dst.type() != DATA_STRING &&
        DoocsBackendRegisterAccessor<UserType>::dst.type() != DATA_STRING16) {
      throw DeviceException("DOOCS data type not supported by DoocsBackendStringRegisterAccessor.",
          DeviceException::WRONG_PARAMETER);
    }

    // check UserType
    if(typeid(UserType) != typeid(std::string)) {
      throw DeviceException("Trying to access a string DOOCS property with a non-string user data type.",
          DeviceException::WRONG_PARAMETER);
    }
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendStringRegisterAccessor<UserType>::~DoocsBackendStringRegisterAccessor() {
  }


  /**********************************************************************************************************************/

  template<>
  void DoocsBackendStringRegisterAccessor<std::string>::read() {
    // read data
    read_internal();
    // copy data into our buffer
    NDRegisterAccessor<std::string>::buffer_2D[0][0] = DoocsBackendRegisterAccessor<std::string>::dst.get_string();
  }

  /**********************************************************************************************************************/

  template<>
  void DoocsBackendStringRegisterAccessor<std::string>::write() {
    // copy data into our buffer
    DoocsBackendRegisterAccessor<std::string>::src.set(NDRegisterAccessor<std::string>::buffer_2D[0][0].c_str());
    // write data
    write_internal();
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendStringRegisterAccessor<UserType>::read() {
    throw DeviceException("Trying to access a string DOOCS property with a non-string user data type.",
        DeviceException::WRONG_PARAMETER);
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendStringRegisterAccessor<UserType>::write() {
    throw DeviceException("Trying to access a string DOOCS property with a non-string user data type.",
        DeviceException::WRONG_PARAMETER);
  }

} /* namespace mtca4u */

#endif /* MTCA4U_DOOCS_BACKEND_STRING_REGISTER_ACCESSOR_H */
