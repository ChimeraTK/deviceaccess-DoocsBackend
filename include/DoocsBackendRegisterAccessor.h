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

      DoocsBackendRegisterAccessor(const RegisterPath &path, size_t numberOfWords, size_t wordOffsetInRegister,
          AccessModeFlags flags, bool allocateBuffers = true);

      virtual ~DoocsBackendRegisterAccessor();

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

      /// flag if the DOOCS data type is an array or not
      bool isArray;

      /// number of elements
      size_t nElements;

      /// element offset specified by the user
      size_t elementOffset;

      /// flag if the accessor should affect only a part of the property (in case of an array)
      bool isPartial;

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
  DoocsBackendRegisterAccessor<UserType>::DoocsBackendRegisterAccessor(const RegisterPath &path, size_t numberOfWords,
      size_t wordOffsetInRegister, AccessModeFlags flags, bool allocateBuffers)
  : _path(path), elementOffset(wordOffsetInRegister)
  {

    // check for unknown access mode flags
    flags.checkForUnknownFlags({AccessMode::raw});

    // set address
    ea.adr(std::string(path).substr(1).c_str());        // strip leading slash

    // try to read data, to check connectivity and to obtain size of the register
    read_internal();

    // obtain number of elements
    size_t actualLength = dst.array_length();
    if(actualLength == 0) {
      actualLength = 1;
      isArray = false;
    }
    else {
      isArray = true;
    }
    if(numberOfWords == 0) {
      nElements = actualLength;
    }
    else {
      nElements = numberOfWords;
    }
    if(nElements + elementOffset > actualLength) {
        throw DeviceException("Requested number of words exceeds the length of the DOOCS property!",
            DeviceException::WRONG_PARAMETER);
    }
    if(nElements == actualLength && elementOffset == 0) {
      isPartial = false;
    }
    else {
      isPartial = true;
    }

    // allocate buffers
    if(allocateBuffers) {
      NDRegisterAccessor<UserType>::buffer_2D.resize(1);
      NDRegisterAccessor<UserType>::buffer_2D[0].resize(nElements);
    }

    // set proper type information in the source EqData
    src.set_type(dst.type());
    if(allocateBuffers) {
      src.length(actualLength);
    }
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

} /* namespace mtca4u */

#endif /* MTCA4U_DOOCS_BACKEND_REGISTER_ACCESSOR_H */
