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

namespace mtca4u {

  template<typename UserType>
  class DoocsBackendFloatRegisterAccessor : public NDRegisterAccessor<UserType> {

    public:

      DoocsBackendFloatRegisterAccessor(const RegisterPath &path);

      virtual ~DoocsBackendFloatRegisterAccessor();

      virtual void read();

      virtual void write();

      virtual bool isSameRegister(const boost::shared_ptr<TransferElement const> &other) const {
        auto rhsCasted = boost::dynamic_pointer_cast< const DoocsBackendFloatRegisterAccessor<UserType> >(other);
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
  DoocsBackendFloatRegisterAccessor<UserType>::DoocsBackendFloatRegisterAccessor(const RegisterPath &path)
  : _path(path)
  {

    // set address
    ea.adr(std::string(path).substr(1).c_str());        // strip leading slash

    // try to read data, to check connectivity and to obtain size of the register
    read_internal();

    // check data type
    if( dst.type() != DATA_FLOAT && dst.type() != DATA_A_FLOAT &&
        dst.type() != DATA_DOUBLE && dst.type() != DATA_A_DOUBLE  ) {
      throw DeviceException("DOOCS data type not supported by DoocsBackendFloatRegisterAccessor.",
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
  DoocsBackendFloatRegisterAccessor<UserType>::~DoocsBackendFloatRegisterAccessor() {

  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendFloatRegisterAccessor<UserType>::read_internal() {
    // read data
    int rc = eq.get(&ea, &src, &dst);
    // check error
    if(rc) {
      throw DeviceException(std::string("Cannot read from DOOCS property: ")+dst.get_string(),
          DeviceException::CANNOT_OPEN_DEVICEBACKEND);
    }
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
    read_internal();
    // copy data into our buffer
    if(nElements == 1) {
      NDRegisterAccessor<UserType>::buffer_2D[0][0] = std::round(dst.get_double());
    }
    else {
      for(size_t i=0; i<nElements; i++) {
        NDRegisterAccessor<UserType>::buffer_2D[0][i] = std::round(dst.get_double(i));
      }
    }
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendFloatRegisterAccessor<UserType>::write_internal() {
    // write data
    int rc = eq.set(&ea, &src, &dst);
    // check error
    if(rc) {
      throw DeviceException(std::string("Cannot write to DOOCS property: ")+dst.get_string(),
          DeviceException::CANNOT_OPEN_DEVICEBACKEND);
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
      for(size_t i=0; i<nElements; i++) {
        throw(DeviceException("Writing an array of std::string is not supported by DOOCS.",
            DeviceException::NOT_IMPLEMENTED));
      }
    }
    // write data
    write_internal();
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendFloatRegisterAccessor<UserType>::write() {
    // copy data into our buffer
    if(nElements == 1) {
      double val = NDRegisterAccessor<UserType>::buffer_2D[0][0];
      src.set(val);
    }
    else {
      for(size_t i=0; i<nElements; i++) {
        double val = NDRegisterAccessor<UserType>::buffer_2D[0][i];
        src.set(val,(int)i);
      }
    }
    // write data
    write_internal();
  }

} /* namespace mtca4u */

#endif /* MTCA4U_DOOCS_BACKEND_FLOAT_REGISTER_ACCESSOR_H */
