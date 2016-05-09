/*
 * DoocsBackendIntRegisterAccessor.h
 *
 *  Created on: May 3, 2016
 *      Author: Martin Hierholzer
 */

#ifndef MTCA4U_DOOCS_BACKEND_INT_REGISTER_ACCESSOR_H
#define MTCA4U_DOOCS_BACKEND_INT_REGISTER_ACCESSOR_H

#include <type_traits>

#include <mtca4u/NDRegisterAccessor.h>
#include <mtca4u/DeviceException.h>
#include <mtca4u/FixedPointConverter.h>

#include <eq_client.h>

#include "DoocsBackendRegisterAccessor.h"

namespace mtca4u {

  template<typename UserType>
  class DoocsBackendIntRegisterAccessor : public DoocsBackendRegisterAccessor<UserType> {

    public:

      DoocsBackendIntRegisterAccessor(const RegisterPath &path);

      virtual ~DoocsBackendIntRegisterAccessor();

      virtual void read();

      virtual void write();

    protected:

      /// fixed point converter for writing integers (used with default 32.0 signed settings, since DOOCS knows only "int")
      FixedPointConverter fixedPointConverter;

  };

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendIntRegisterAccessor<UserType>::DoocsBackendIntRegisterAccessor(const RegisterPath &path)
  : DoocsBackendRegisterAccessor<UserType>(path)
  {

    // check data type
    if( DoocsBackendRegisterAccessor<UserType>::dst.type() != DATA_INT &&
        DoocsBackendRegisterAccessor<UserType>::dst.type() != DATA_A_INT &&
        DoocsBackendRegisterAccessor<UserType>::dst.type() != DATA_BOOL &&
        DoocsBackendRegisterAccessor<UserType>::dst.type() != DATA_A_BOOL &&
        DoocsBackendRegisterAccessor<UserType>::dst.type() != DATA_A_SHORT &&
        DoocsBackendRegisterAccessor<UserType>::dst.type() != DATA_A_LONG  ) {
      throw DeviceException("DOOCS data type not supported by DoocsBackendIntRegisterAccessor.",
          DeviceException::WRONG_PARAMETER);
    }
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendIntRegisterAccessor<UserType>::~DoocsBackendIntRegisterAccessor() {

  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendIntRegisterAccessor<UserType>::read() {
    // read data
    DoocsBackendRegisterAccessor<UserType>::read_internal();
    // copy data into our buffer
    if(DoocsBackendRegisterAccessor<UserType>::nElements == 1) {
      NDRegisterAccessor<UserType>::buffer_2D[0][0] = fixedPointConverter.toCooked<UserType>(DoocsBackendRegisterAccessor<UserType>::dst.get_int());
    }
    else {
      for(size_t i=0; i<DoocsBackendRegisterAccessor<UserType>::nElements; i++) {
        NDRegisterAccessor<UserType>::buffer_2D[0][i] = fixedPointConverter.toCooked<UserType>(DoocsBackendRegisterAccessor<UserType>::dst.get_int(i));
      }
    }
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendIntRegisterAccessor<UserType>::write() {
    // copy data into our buffer
    if(DoocsBackendRegisterAccessor<UserType>::nElements == 1) {
      int32_t raw = fixedPointConverter.toRaw(NDRegisterAccessor<UserType>::buffer_2D[0][0]);
      DoocsBackendRegisterAccessor<UserType>::src.set(raw);
    }
    else {
      for(size_t i=0; i<DoocsBackendRegisterAccessor<UserType>::nElements; i++) {
        int32_t raw = fixedPointConverter.toRaw(NDRegisterAccessor<UserType>::buffer_2D[0][i]);
        DoocsBackendRegisterAccessor<UserType>::src.set(raw,(int)i);
      }
    }
    // write data
    DoocsBackendRegisterAccessor<UserType>::write_internal();
  }

} /* namespace mtca4u */

#endif /* MTCA4U_DOOCS_BACKEND_INT_REGISTER_ACCESSOR_H */
