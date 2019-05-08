/*
 * DoocsBackendIntRegisterAccessor.h
 *
 *  Created on: May 3, 2016
 *      Author: Martin Hierholzer
 */

#ifndef MTCA4U_DOOCS_BACKEND_INT_REGISTER_ACCESSOR_H
#define MTCA4U_DOOCS_BACKEND_INT_REGISTER_ACCESSOR_H

#include <type_traits>

#include <eq_client.h>

#include <ChimeraTK/SupportedUserTypes.h>

#include "DoocsBackendRegisterAccessor.h"

namespace ChimeraTK {

  template<typename UserType>
  class DoocsBackendIntRegisterAccessor : public DoocsBackendRegisterAccessor<UserType> {
   public:
    virtual ~DoocsBackendIntRegisterAccessor();

   protected:
    DoocsBackendIntRegisterAccessor(DoocsBackend* backend, const std::string& path, size_t numberOfWords,
        size_t wordOffsetInRegister, AccessModeFlags flags);

    void doPostRead() override;

    void doPreWrite() override;

    void initialiseImplementation() override;

    friend class DoocsBackend;
  };

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendIntRegisterAccessor<UserType>::DoocsBackendIntRegisterAccessor(DoocsBackend* backend,
      const std::string& path, size_t numberOfWords, size_t wordOffsetInRegister, AccessModeFlags flags)
  : DoocsBackendRegisterAccessor<UserType>(backend, path, numberOfWords, wordOffsetInRegister, flags) {
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
  void DoocsBackendIntRegisterAccessor<UserType>::initialiseImplementation() {
    // check data type
    if(DoocsBackendRegisterAccessor<UserType>::dst.type() != DATA_INT &&
        DoocsBackendRegisterAccessor<UserType>::dst.type() != DATA_BOOL &&
        DoocsBackendRegisterAccessor<UserType>::dst.type() != DATA_A_INT &&
        DoocsBackendRegisterAccessor<UserType>::dst.type() != DATA_IIII &&
        DoocsBackendRegisterAccessor<UserType>::dst.type() != DATA_A_BOOL &&
        DoocsBackendRegisterAccessor<UserType>::dst.type() != DATA_A_SHORT) {
      throw ChimeraTK::logic_error("DOOCS data type not supported by "
                                   "DoocsBackendIntRegisterAccessor."); // LCOV_EXCL_LINE (already
                                                                        // prevented in the Backend)
    }
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendIntRegisterAccessor<UserType>::~DoocsBackendIntRegisterAccessor() {
    this->shutdown();
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendIntRegisterAccessor<UserType>::doPostRead() {
    // copy data into our buffer
    if(!DoocsBackendRegisterAccessor<UserType>::isArray) {
      UserType val = numericToUserType<UserType>(DoocsBackendRegisterAccessor<UserType>::dst.get_int());
      NDRegisterAccessor<UserType>::buffer_2D[0][0] = val;
    }
    else {
      for(size_t i = 0; i < DoocsBackendRegisterAccessor<UserType>::nElements; i++) {
        int idx = i + DoocsBackendRegisterAccessor<UserType>::elementOffset;
        UserType val = numericToUserType<UserType>(DoocsBackendRegisterAccessor<UserType>::dst.get_int(idx));
        NDRegisterAccessor<UserType>::buffer_2D[0][i] = val;
      }
    }
    DoocsBackendRegisterAccessor<UserType>::doPostRead();
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendIntRegisterAccessor<UserType>::doPreWrite() {
    // copy data into our buffer
    if(!DoocsBackendRegisterAccessor<UserType>::isArray) {
      int32_t raw = userTypeToNumeric<int32_t>(NDRegisterAccessor<UserType>::buffer_2D[0][0]);
      DoocsBackendRegisterAccessor<UserType>::src.set(raw);
    }
    else {
      if(DoocsBackendRegisterAccessor<UserType>::isPartial) { // implement read-modify-write
        DoocsBackendRegisterAccessor<UserType>::doReadTransfer();
        for(int i = 0; i < DoocsBackendRegisterAccessor<UserType>::src.array_length(); i++) {
          DoocsBackendRegisterAccessor<UserType>::src.set(DoocsBackendRegisterAccessor<UserType>::dst.get_int(i), i);
        }
      }
      for(size_t i = 0; i < DoocsBackendRegisterAccessor<UserType>::nElements; i++) {
        int32_t raw = userTypeToNumeric<int32_t>(NDRegisterAccessor<UserType>::buffer_2D[0][i]);
        int idx = i + DoocsBackendRegisterAccessor<UserType>::elementOffset;
        DoocsBackendRegisterAccessor<UserType>::src.set(raw, idx);
      }
    }
  }

} // namespace ChimeraTK

#endif /* MTCA4U_DOOCS_BACKEND_INT_REGISTER_ACCESSOR_H */
