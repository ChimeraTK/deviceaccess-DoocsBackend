/*
 * DoocsBackendRegisterAccessor.h
 *
 *  Created on: May 3, 2016
 *      Author: Martin Hierholzer
 */

#ifndef MTCA4U_DOOCS_BACKEND_REGISTER_ACCESSOR_H
#define MTCA4U_DOOCS_BACKEND_REGISTER_ACCESSOR_H

#include <type_traits>

#include <mtca4u/NDRegisterAccessor.h>
#include <mtca4u/DeviceException.h>
#include <mtca4u/FixedPointConverter.h>

#include <eq_client.h>

namespace mtca4u {

  template<typename UserType>
  class DoocsBackendRegisterAccessor : public NDRegisterAccessor<UserType> {

    public:

      DoocsBackendRegisterAccessor(const RegisterPath &path);

      virtual ~DoocsBackendRegisterAccessor();

      virtual void read();

      virtual void write();

      virtual bool isSameRegister(const boost::shared_ptr<TransferElement const> &other) const {
        auto rhsCasted = boost::dynamic_pointer_cast< const DoocsBackendRegisterAccessor<UserType> >(other);
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

      /// data type (DOOCS type constant)
      int dataType;

      /// fixed point converter for writing integers (used with default 32.0 signed settings, since DOOCS knows only "int")
      FixedPointConverter fixedPointConverter;

      /// internal read into EqData dst
      void read_internal();

      /// internal write from EqData src
      void write_internal();

      /// type-tagged version of write(), to realise different implementations for integral and floating-point write()
      void write_tagged(std::true_type);
      void write_tagged(std::false_type);

      /// callback function used for the monitor
      static void callback(EqData *ed, void *p);

      virtual std::vector< boost::shared_ptr<TransferElement> > getHardwareAccessingElements() {
        return { boost::enable_shared_from_this<TransferElement>::shared_from_this() };
      }

      virtual void replaceTransferElement(boost::shared_ptr<TransferElement> /*newElement*/) {} // LCOV_EXCL_LINE

  };

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendRegisterAccessor<UserType>::DoocsBackendRegisterAccessor(const RegisterPath &path)
  : _path(path)
  {

    // set address
    ea.adr(std::string(path).substr(1).c_str());        // strip leading slash

    // try to read data, to check connectivity and to obtain size of the register
    read_internal();

    // set buffer size
    nElements = dst.array_length();
    if(nElements == 0) nElements = 1;
    NDRegisterAccessor<UserType>::buffer_2D.resize(1);
    NDRegisterAccessor<UserType>::buffer_2D[0].resize(nElements);
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendRegisterAccessor<UserType>::~DoocsBackendRegisterAccessor() {

  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendRegisterAccessor<UserType>::read_internal() {
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
  void DoocsBackendRegisterAccessor<float>::read() {
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
  void DoocsBackendRegisterAccessor<double>::read() {
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
  void DoocsBackendRegisterAccessor<std::string>::read() {
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
  void DoocsBackendRegisterAccessor<UserType>::read() {
    // read data
    read_internal();
    // copy data into our buffer
    if(nElements == 1) {
      NDRegisterAccessor<UserType>::buffer_2D[0][0] = dst.get_int();
    }
    else {
      for(size_t i=0; i<nElements; i++) {
        NDRegisterAccessor<UserType>::buffer_2D[0][i] = dst.get_int(i);
      }
    }
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendRegisterAccessor<UserType>::write_internal() {
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
  void DoocsBackendRegisterAccessor<std::string>::write() {
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
  void DoocsBackendRegisterAccessor<UserType>::write_tagged(std::true_type) {   // integral UserType
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

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendRegisterAccessor<UserType>::write_tagged(std::false_type) {   // floating-point UserType
    // copy data into our buffer
    if(nElements == 1) {
      src.set(NDRegisterAccessor<UserType>::buffer_2D[0][0]);
    }
    else {
      for(size_t i=0; i<nElements; i++) {
        src.set(NDRegisterAccessor<UserType>::buffer_2D[0][i],(int)i);
      }
    }
    // write data
    write_internal();
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendRegisterAccessor<UserType>::write() {
    // dispatch to tagged version
    write_tagged(std::is_integral<UserType>());
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendRegisterAccessor<UserType>::callback(EqData *ed, void *p) {
    DoocsBackendRegisterAccessor *self = static_cast<DoocsBackendRegisterAccessor*>(p);

  }

} /* namespace mtca4u */

#endif /* MTCA4U_DOOCS_BACKEND_REGISTER_ACCESSOR_H */
