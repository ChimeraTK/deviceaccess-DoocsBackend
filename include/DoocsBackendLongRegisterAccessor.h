/*
 * DoocsBackendLongRegisterAccessor.h
 *
 *  Created on: May 13, 2016
 *      Author: Martin Hierholzer
 */

#ifndef MTCA4U_DOOCS_BACKEND_LONG_REGISTER_ACCESSOR_H
#define MTCA4U_DOOCS_BACKEND_LONG_REGISTER_ACCESSOR_H

#include <type_traits>

#include <mtca4u/NDRegisterAccessor.h>
#include <mtca4u/DeviceException.h>
#include <mtca4u/FixedPointConverter.h>

#include <eq_client.h>

#include "DoocsBackendRegisterAccessor.h"

namespace mtca4u {

  template<typename UserType>
  class DoocsBackendLongRegisterAccessor : public DoocsBackendRegisterAccessor<UserType> {

    public:

      virtual ~DoocsBackendLongRegisterAccessor();

    protected:

      DoocsBackendLongRegisterAccessor(const RegisterPath &path, size_t numberOfWords, size_t wordOffsetInRegister,
          AccessModeFlags flags);

      void postRead() override;

      void preWrite() override;

      /// fixed point converter for writing integers (used with default 32.0 signed settings, since DOOCS knows only "int")
      FixedPointConverter fixedPointConverter;

      friend class DoocsBackend;

  };

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendLongRegisterAccessor<UserType>::DoocsBackendLongRegisterAccessor(const RegisterPath &path,
      size_t numberOfWords, size_t wordOffsetInRegister, AccessModeFlags flags)
  : DoocsBackendRegisterAccessor<UserType>(path, numberOfWords, wordOffsetInRegister, flags),
    fixedPointConverter(path)
  {

    // check data type
    if( DoocsBackendRegisterAccessor<UserType>::dst.type() != DATA_A_LONG ) {
      throw DeviceException("DOOCS data type not supported by DoocsBackendIntRegisterAccessor.",    // LCOV_EXCL_LINE (already prevented in the Backend)
          DeviceException::WRONG_PARAMETER);                                                        // LCOV_EXCL_LINE
    }
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendLongRegisterAccessor<UserType>::~DoocsBackendLongRegisterAccessor() {

  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendLongRegisterAccessor<UserType>::postRead() {
    // copy data into our buffer
    for(size_t i=0; i<DoocsBackendRegisterAccessor<UserType>::nElements; i++) {
      int idx = i+DoocsBackendRegisterAccessor<UserType>::elementOffset;
      UserType val = fixedPointConverter.toCooked<UserType>(DoocsBackendRegisterAccessor<UserType>::dst.get_long(idx));
      NDRegisterAccessor<UserType>::buffer_2D[0][i] = val;
    }
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendLongRegisterAccessor<UserType>::preWrite() {
    // copy data into our buffer
    if(DoocsBackendRegisterAccessor<UserType>::isPartial) {   // implement read-modify-write
      DoocsBackendRegisterAccessor<UserType>::doReadTransfer();
      for(int i=0; i<DoocsBackendRegisterAccessor<UserType>::src.array_length(); i++) {
        DoocsBackendRegisterAccessor<UserType>::src.set(DoocsBackendRegisterAccessor<UserType>::dst.get_long(i),i);
      }
    }
    for(size_t i=0; i<DoocsBackendRegisterAccessor<UserType>::nElements; i++) {
      int32_t raw = fixedPointConverter.toRaw(NDRegisterAccessor<UserType>::buffer_2D[0][i]);
      int idx = i+DoocsBackendRegisterAccessor<UserType>::elementOffset;
      DoocsBackendRegisterAccessor<UserType>::src.set(raw,idx);
    }
  }

} /* namespace mtca4u */

#endif /* MTCA4U_DOOCS_BACKEND_LONG_REGISTER_ACCESSOR_H */
