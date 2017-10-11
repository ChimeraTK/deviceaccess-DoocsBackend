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

      virtual ~DoocsBackendFloatRegisterAccessor();

    protected:

      DoocsBackendFloatRegisterAccessor(const RegisterPath &path, size_t numberOfWords, size_t wordOffsetInRegister,
          AccessModeFlags flags);

      void postRead() override;

      void preWrite() override;

      friend class DoocsBackend;

  };

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendFloatRegisterAccessor<UserType>::DoocsBackendFloatRegisterAccessor(const RegisterPath &path,
      size_t numberOfWords, size_t wordOffsetInRegister, AccessModeFlags flags)
  : DoocsBackendRegisterAccessor<UserType>(path, numberOfWords, wordOffsetInRegister, flags)
  {
    try {
      // check data type
      if( DoocsBackendRegisterAccessor<UserType>::dst.type() != DATA_FLOAT &&
          DoocsBackendRegisterAccessor<UserType>::dst.type() != DATA_A_FLOAT &&
          DoocsBackendRegisterAccessor<UserType>::dst.type() != DATA_DOUBLE &&
          DoocsBackendRegisterAccessor<UserType>::dst.type() != DATA_A_DOUBLE &&
          DoocsBackendRegisterAccessor<UserType>::dst.type() != DATA_SPECTRUM    ) {
        throw DeviceException("DOOCS data type not supported by DoocsBackendFloatRegisterAccessor.",  // LCOV_EXCL_LINE (already prevented in the Backend)
            DeviceException::WRONG_PARAMETER);                                                        // LCOV_EXCL_LINE
      }
    }
    catch(...) {
      this->shutdown();
      throw;
    }
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendFloatRegisterAccessor<UserType>::~DoocsBackendFloatRegisterAccessor() {
    this->shutdown();
  }

  /**********************************************************************************************************************/

  template<>
  void DoocsBackendFloatRegisterAccessor<float>::postRead() {
    // copy data into our buffer
    if(!isArray) {
      NDRegisterAccessor<float>::buffer_2D[0][0] = dst.get_float();
    }
    else {
      for(size_t i=0; i<DoocsBackendRegisterAccessor<float>::nElements; i++) {
        int idx = i+DoocsBackendRegisterAccessor<float>::elementOffset;
        NDRegisterAccessor<float>::buffer_2D[0][i] = dst.get_float(idx);
      }
    }
  }

  /**********************************************************************************************************************/

  template<>
  void DoocsBackendFloatRegisterAccessor<double>::postRead() {
    // copy data into our buffer
    if(!isArray) {
      NDRegisterAccessor<double>::buffer_2D[0][0] = dst.get_double();
    }
    else {
      for(size_t i=0; i<DoocsBackendRegisterAccessor<double>::nElements; i++) {
        int idx = i+DoocsBackendRegisterAccessor<double>::elementOffset;
        NDRegisterAccessor<double>::buffer_2D[0][i] = dst.get_double(idx);
      }
    }
  }

  /**********************************************************************************************************************/

  template<>
  void DoocsBackendFloatRegisterAccessor<std::string>::postRead() {
    // copy data into our buffer
    if(!isArray) {
      NDRegisterAccessor<std::string>::buffer_2D[0][0] = dst.get_string();
    }
    else {
      for(size_t i=0; i<DoocsBackendRegisterAccessor<std::string>::nElements; i++) {
        int idx = i+DoocsBackendRegisterAccessor<std::string>::elementOffset;
        NDRegisterAccessor<std::string>::buffer_2D[0][i] = std::to_string(
            DoocsBackendRegisterAccessor<std::string>::dst.get_double(idx));
      }
    }
  }

  /**********************************************************************************************************************/

  template<typename UserType>   // only integral types left!  FIXME better add type trait
  void DoocsBackendFloatRegisterAccessor<UserType>::postRead() {
    // copy data into our buffer
    if(!DoocsBackendRegisterAccessor<UserType>::isArray) {
      NDRegisterAccessor<UserType>::buffer_2D[0][0] = std::round(DoocsBackendRegisterAccessor<UserType>::dst.get_double());
    }
    else {
      for(size_t i=0; i<DoocsBackendRegisterAccessor<UserType>::nElements; i++) {
        int idx = i+DoocsBackendRegisterAccessor<UserType>::elementOffset;
        NDRegisterAccessor<UserType>::buffer_2D[0][i] = std::round(DoocsBackendRegisterAccessor<UserType>::dst.get_double(idx));
      }
    }
  }

  /**********************************************************************************************************************/

  template<>
  void DoocsBackendFloatRegisterAccessor<std::string>::preWrite() {
    // copy data into our buffer
    if(!isArray) {
      src.set(std::stod(NDRegisterAccessor<std::string>::buffer_2D[0][0].c_str()));
    }
    else {
      if(DoocsBackendRegisterAccessor<std::string>::isPartial) {   // implement read-modify-write
        DoocsBackendRegisterAccessor<std::string>::doReadTransfer();
        for(int i=0; i<DoocsBackendRegisterAccessor<std::string>::src.array_length(); i++) {
          DoocsBackendRegisterAccessor<std::string>::src.set(DoocsBackendRegisterAccessor<std::string>::dst.get_double(i),i);
        }
      }
      for(size_t i=0; i<DoocsBackendRegisterAccessor<std::string>::nElements; i++) {
        int idx = i+DoocsBackendRegisterAccessor<std::string>::elementOffset;
        src.set(std::stod(NDRegisterAccessor<std::string>::buffer_2D[0][i].c_str()), idx);
      }
    }
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendFloatRegisterAccessor<UserType>::preWrite() {
    // copy data into our buffer
    if(!DoocsBackendRegisterAccessor<UserType>::isArray) {
      double val = NDRegisterAccessor<UserType>::buffer_2D[0][0];
      DoocsBackendRegisterAccessor<UserType>::src.set(val);
    }
    else {
      if(DoocsBackendRegisterAccessor<UserType>::isPartial) {   // implement read-modify-write
        DoocsBackendRegisterAccessor<UserType>::doReadTransfer();
        for(int i=0; i<DoocsBackendRegisterAccessor<UserType>::src.array_length(); i++) {
          DoocsBackendRegisterAccessor<UserType>::src.set(DoocsBackendRegisterAccessor<UserType>::dst.get_double(i),i);
        }
      }
      for(size_t i=0; i<DoocsBackendRegisterAccessor<UserType>::nElements; i++) {
        int idx = i+DoocsBackendRegisterAccessor<UserType>::elementOffset;
        double val = NDRegisterAccessor<UserType>::buffer_2D[0][i];
        DoocsBackendRegisterAccessor<UserType>::src.set(val,idx);
      }
    }
  }

} /* namespace mtca4u */

#endif /* MTCA4U_DOOCS_BACKEND_FLOAT_REGISTER_ACCESSOR_H */
