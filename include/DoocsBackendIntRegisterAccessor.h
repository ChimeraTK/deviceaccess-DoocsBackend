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

namespace mtca4u {

  template<typename UserType>
  class DoocsBackendIntRegisterAccessor : public NDRegisterAccessor<UserType> {

    public:

      DoocsBackendIntRegisterAccessor(const RegisterPath &path);

      virtual ~DoocsBackendIntRegisterAccessor();

      virtual void read();

      virtual void write();

      virtual bool isSameRegister(const boost::shared_ptr<TransferElement const> &other) const {
        auto rhsCasted = boost::dynamic_pointer_cast< const DoocsBackendIntRegisterAccessor<UserType> >(other);
        if(!rhsCasted) return false;
        if(_path != rhsCasted->_path) return false;
        return true;
      }

      virtual bool isReadOnly() const {
        return false;
      }

    protected:

      /// register path
      RegisterPath _path;

      /// DOOCS address structure
      EqAdr ea;

      /// DOOCS rpc call object
      EqCall eq;

      /// DOOCS data structures
      EqData src,dst;

      /// number of elements
      size_t nElements;

      /// fixed point converter for writing integers (used with default 32.0 signed settings, since DOOCS knows only "int")
      FixedPointConverter fixedPointConverter;

      /// internal read into EqData dst
      void read_internal();

      /// internal write from EqData src
      void write_internal();

      virtual std::vector< boost::shared_ptr<TransferElement> > getHardwareAccessingElements() {
        return { boost::enable_shared_from_this<TransferElement>::shared_from_this() };
      }

      virtual void replaceTransferElement(boost::shared_ptr<TransferElement> /*newElement*/) {} // LCOV_EXCL_LINE

  };

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendIntRegisterAccessor<UserType>::DoocsBackendIntRegisterAccessor(const RegisterPath &path)
  : _path(path)
  {

    // set address
    ea.adr(std::string(path).substr(1).c_str());        // strip leading slash

    // try to read data, to check connectivity and to obtain size of the register
    read_internal();

    // check data type
    if( dst.type() != DATA_INT && dst.type() != DATA_A_INT &&
        dst.type() != DATA_BOOL && dst.type() != DATA_A_BOOL &&
        dst.type() != DATA_A_SHORT && dst.type() != DATA_A_LONG  ) {
      throw DeviceException("DOOCS data type not supported by DoocsBackendIntRegisterAccessor.",
          DeviceException::WRONG_PARAMETER);
    }

    // set buffer size
    nElements = dst.array_length();
    if(nElements == 0) nElements = 1;
    NDRegisterAccessor<UserType>::buffer_2D.resize(1);
    NDRegisterAccessor<UserType>::buffer_2D[0].resize(nElements);
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendIntRegisterAccessor<UserType>::~DoocsBackendIntRegisterAccessor() {

  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendIntRegisterAccessor<UserType>::read_internal() {
    // read data
    int rc = eq.get(&ea, &src, &dst);
    // check error
    if(rc) {
      throw DeviceException(std::string("Cannot read from DOOCS property: ")+dst.get_string(),
          DeviceException::CANNOT_OPEN_DEVICEBACKEND);
    }
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendIntRegisterAccessor<UserType>::read() {
    // read data
    read_internal();
    // copy data into our buffer
    if(nElements == 1) {
      NDRegisterAccessor<UserType>::buffer_2D[0][0] = fixedPointConverter.toCooked<UserType>(dst.get_int());
    }
    else {
      for(size_t i=0; i<nElements; i++) {
        NDRegisterAccessor<UserType>::buffer_2D[0][i] = fixedPointConverter.toCooked<UserType>(dst.get_int(i));
      }
    }
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendIntRegisterAccessor<UserType>::write_internal() {
    // write data
    int rc = eq.set(&ea, &src, &dst);
    // check error
    if(rc) {
      throw DeviceException(std::string("Cannot write to DOOCS property: ")+dst.get_string(),
          DeviceException::CANNOT_OPEN_DEVICEBACKEND);
    }
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendIntRegisterAccessor<UserType>::write() {
    // copy data into our buffer
    if(nElements == 1) {
      int32_t raw = fixedPointConverter.toRaw(NDRegisterAccessor<UserType>::buffer_2D[0][0]);
      src.set(raw);
    }
    else {
      for(size_t i=0; i<nElements; i++) {
        int32_t raw = fixedPointConverter.toRaw(NDRegisterAccessor<UserType>::buffer_2D[0][i]);
        src.set(raw,(int)i);
      }
    }
    // write data
    write_internal();
  }

} /* namespace mtca4u */

#endif /* MTCA4U_DOOCS_BACKEND_INT_REGISTER_ACCESSOR_H */
