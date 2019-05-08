/*
 * DoocsBackendLongRegisterAccessor.h
 *
 *  Created on: May 13, 2016
 *      Author: Martin Hierholzer
 */

#ifndef MTCA4U_DOOCS_BACKEND_LONG_REGISTER_ACCESSOR_H
#define MTCA4U_DOOCS_BACKEND_LONG_REGISTER_ACCESSOR_H

#include <type_traits>

#include <eq_client.h>

#include <ChimeraTK/SupportedUserTypes.h>

#include "DoocsBackendRegisterAccessor.h"

namespace ChimeraTK {

  template<typename UserType>
  class DoocsBackendLongRegisterAccessor : public DoocsBackendRegisterAccessor<UserType> {
   public:
    virtual ~DoocsBackendLongRegisterAccessor();

   protected:
    DoocsBackendLongRegisterAccessor(DoocsBackend* backend, const std::string& path,
        const std::string& registerPathName, size_t numberOfWords, size_t wordOffsetInRegister, AccessModeFlags flags);

    void doPostRead() override;

    void doPreWrite() override;

    void initialiseImplementation() override;

    friend class DoocsBackend;
  };

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendLongRegisterAccessor<UserType>::DoocsBackendLongRegisterAccessor(DoocsBackend* backend,
      const std::string& path, const std::string& registerPathName, size_t numberOfWords, size_t wordOffsetInRegister,
      AccessModeFlags flags)
  : DoocsBackendRegisterAccessor<UserType>(
        backend, path, registerPathName, numberOfWords, wordOffsetInRegister, flags) {
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
  void DoocsBackendLongRegisterAccessor<UserType>::initialiseImplementation() {
    // check data type
    if(DoocsBackendRegisterAccessor<UserType>::dst.type() != DATA_A_LONG) {
      throw ChimeraTK::logic_error("DOOCS data type not supported by "
                                   "DoocsBackendIntRegisterAccessor."); // LCOV_EXCL_LINE (already
                                                                        // prevented in the Backend)
    }
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendLongRegisterAccessor<UserType>::~DoocsBackendLongRegisterAccessor() {
    this->shutdown();
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendLongRegisterAccessor<UserType>::doPostRead() {
    // copy data into our buffer
    for(size_t i = 0; i < DoocsBackendRegisterAccessor<UserType>::nElements; i++) {
      int idx = i + DoocsBackendRegisterAccessor<UserType>::elementOffset;
      UserType val = numericToUserType<UserType>(DoocsBackendRegisterAccessor<UserType>::dst.get_long(idx));
      NDRegisterAccessor<UserType>::buffer_2D[0][i] = val;
    }
    DoocsBackendRegisterAccessor<UserType>::doPostRead();
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendLongRegisterAccessor<UserType>::doPreWrite() {
    // copy data into our buffer
    if(DoocsBackendRegisterAccessor<UserType>::isPartial) { // implement
                                                            // read-modify-write
      DoocsBackendRegisterAccessor<UserType>::doReadTransfer();
      for(int i = 0; i < DoocsBackendRegisterAccessor<UserType>::src.array_length(); i++) {
        DoocsBackendRegisterAccessor<UserType>::src.set(DoocsBackendRegisterAccessor<UserType>::dst.get_long(i), i);
      }
    }
    for(size_t i = 0; i < DoocsBackendRegisterAccessor<UserType>::nElements; i++) {
      int32_t raw = userTypeToNumeric<int32_t>(NDRegisterAccessor<UserType>::buffer_2D[0][i]);
      int idx = i + DoocsBackendRegisterAccessor<UserType>::elementOffset;
      DoocsBackendRegisterAccessor<UserType>::src.set(raw, idx);
    }
  }

} // namespace ChimeraTK

#endif /* MTCA4U_DOOCS_BACKEND_LONG_REGISTER_ACCESSOR_H */
