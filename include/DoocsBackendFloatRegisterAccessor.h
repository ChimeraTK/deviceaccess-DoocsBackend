/*
 * DoocsBackendFloatRegisterAccessor.h
 *
 *  Created on: May 3, 2016
 *      Author: Martin Hierholzer
 */

#ifndef MTCA4U_DOOCS_BACKEND_FLOAT_REGISTER_ACCESSOR_H
#define MTCA4U_DOOCS_BACKEND_FLOAT_REGISTER_ACCESSOR_H

#include <type_traits>

#include <mtca4u/NDRegisterAccessor.h>
#include <mtca4u/DeviceException.h>
#include <mtca4u/FixedPointConverter.h>

#include <eq_client.h>

#include "DoocsBackendRegisterAccessor.h"

namespace mtca4u {

  template<typename UserType>
  class DoocsBackendFloatRegisterAccessor : public DoocsBackendRegisterAccessor<UserType> {

    public:

      DoocsBackendFloatRegisterAccessor(const RegisterPath &path);

      virtual ~DoocsBackendFloatRegisterAccessor();

      virtual void read();

      virtual void write();

  };

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendFloatRegisterAccessor<UserType>::DoocsBackendFloatRegisterAccessor(const RegisterPath &path)
  : DoocsBackendRegisterAccessor<UserType>(path)
  {

    // check data type
    if( DoocsBackendRegisterAccessor<UserType>::dst.type() != DATA_FLOAT &&
        DoocsBackendRegisterAccessor<UserType>::dst.type() != DATA_A_FLOAT &&
        DoocsBackendRegisterAccessor<UserType>::dst.type() != DATA_DOUBLE &&
        DoocsBackendRegisterAccessor<UserType>::dst.type() != DATA_A_DOUBLE  ) {
      throw DeviceException("DOOCS data type not supported by DoocsBackendFloatRegisterAccessor.",
          DeviceException::WRONG_PARAMETER);
    }
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendFloatRegisterAccessor<UserType>::~DoocsBackendFloatRegisterAccessor() {

  }

  /**********************************************************************************************************************/

  template<>
  void DoocsBackendFloatRegisterAccessor<float>::read() {
    // read data
    read_internal();
    // copy data into our buffer
    if(nElements == 1) {
      NDRegisterAccessor<float>::buffer_2D[0][0] = dst.get_float();
    }
    else {
      memcpy(dst.get_float_array(), NDRegisterAccessor<float>::buffer_2D[0].data(), sizeof(float)*nElements);
    }
  }

  /**********************************************************************************************************************/

  template<>
  void DoocsBackendFloatRegisterAccessor<double>::read() {
    // read data
    read_internal();
    // copy data into our buffer
    if(nElements == 1) {
      NDRegisterAccessor<double>::buffer_2D[0][0] = dst.get_double();
    }
    else {
      memcpy(dst.get_float_array(), NDRegisterAccessor<double>::buffer_2D[0].data(), sizeof(double)*nElements);
    }
  }

  /**********************************************************************************************************************/

  template<>
  void DoocsBackendFloatRegisterAccessor<std::string>::read() {
    // read data
    read_internal();
    // copy data into our buffer
    if(nElements == 1) {
      NDRegisterAccessor<std::string>::buffer_2D[0][0] = dst.get_string();
    }
    else {
      throw(DeviceException("Reading an array of std::string is not supported by DOOCS.",
          DeviceException::NOT_IMPLEMENTED));
    }
  }

  /**********************************************************************************************************************/

  template<typename UserType>   // only integral types left!  FIXME better add type trait
  void DoocsBackendFloatRegisterAccessor<UserType>::read() {
    // read data
    DoocsBackendRegisterAccessor<UserType>::read_internal();
    // copy data into our buffer
    if(DoocsBackendRegisterAccessor<UserType>::nElements == 1) {
      NDRegisterAccessor<UserType>::buffer_2D[0][0] = std::round(DoocsBackendRegisterAccessor<UserType>::dst.get_double());
    }
    else {
      for(size_t i=0; i<DoocsBackendRegisterAccessor<UserType>::nElements; i++) {
        NDRegisterAccessor<UserType>::buffer_2D[0][i] = std::round(DoocsBackendRegisterAccessor<UserType>::dst.get_double(i));
      }
    }
  }

  /**********************************************************************************************************************/

  template<>
  void DoocsBackendFloatRegisterAccessor<std::string>::write() {
    // copy data into our buffer
    if(nElements == 1) {
      src.set(NDRegisterAccessor<std::string>::buffer_2D[0][0].c_str());
    }
    else {
      throw(DeviceException("Writing an array of std::string is not supported by DOOCS.",
          DeviceException::NOT_IMPLEMENTED));
    }
    // write data
    write_internal();
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendFloatRegisterAccessor<UserType>::write() {
    // copy data into our buffer
    if(DoocsBackendRegisterAccessor<UserType>::nElements == 1) {
      double val = NDRegisterAccessor<UserType>::buffer_2D[0][0];
      DoocsBackendRegisterAccessor<UserType>::src.set(val);
    }
    else {
      for(size_t i=0; i<DoocsBackendRegisterAccessor<UserType>::nElements; i++) {
        double val = NDRegisterAccessor<UserType>::buffer_2D[0][i];
        DoocsBackendRegisterAccessor<UserType>::src.set(val,(int)i);
      }
    }
    // write data
    DoocsBackendRegisterAccessor<UserType>::write_internal();
  }

} /* namespace mtca4u */

#endif /* MTCA4U_DOOCS_BACKEND_FLOAT_REGISTER_ACCESSOR_H */
