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

namespace mtca4u {

  template<typename UserType>
  class DoocsBackendStringRegisterAccessor : public NDRegisterAccessor<UserType> {

    public:

      DoocsBackendStringRegisterAccessor(const RegisterPath &path);

      virtual ~DoocsBackendStringRegisterAccessor();

      virtual void read();

      virtual void write();

      virtual bool isSameRegister(const boost::shared_ptr<TransferElement const> &other) const {
        auto rhsCasted = boost::dynamic_pointer_cast< const DoocsBackendStringRegisterAccessor<UserType> >(other);
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
  DoocsBackendStringRegisterAccessor<UserType>::DoocsBackendStringRegisterAccessor(const RegisterPath &path)
  : _path(path)
  {

    // set address
    ea.adr(std::string(path).substr(1).c_str());        // strip leading slash

    // try to read data, to check connectivity and to obtain size of the register
    read_internal();

    // check data type
    if( dst.type() != DATA_TEXT && dst.type() != DATA_STRING && dst.type() != DATA_STRING16) {
      throw DeviceException("DOOCS data type not supported by DoocsBackendStringRegisterAccessor.",
          DeviceException::WRONG_PARAMETER);
    }

    // check UserType
    if(typeid(UserType) != typeid(std::string)) {
      throw DeviceException("Trying to access a string DOOCS property with a non-string user data type.",
          DeviceException::WRONG_PARAMETER);
    }

    // set buffer size
    if(dst.array_length() != 0) {
      throw DeviceException("Arrays of strings are not supported by DoocsBackendStringRegisterAccessor.",
          DeviceException::WRONG_PARAMETER);
    }
    NDRegisterAccessor<UserType>::buffer_2D.resize(1);
    NDRegisterAccessor<UserType>::buffer_2D[0].resize(1);
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
    NDRegisterAccessor<std::string>::buffer_2D[0][0] = dst.get_string();
  }

  /**********************************************************************************************************************/

  template<>
  void DoocsBackendStringRegisterAccessor<std::string>::write() {
    // copy data into our buffer
    src.set(NDRegisterAccessor<std::string>::buffer_2D[0][0].c_str());
    // write data
    write_internal();
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendStringRegisterAccessor<UserType>::write_internal() {
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
  void DoocsBackendStringRegisterAccessor<UserType>::read_internal() {
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
