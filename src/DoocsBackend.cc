/*
 * DoocsBackend.cc
 *
 *  Created on: Apr 26, 2016
 *      Author: Martin Hierholzer
 */

#include <mtca4u/BackendFactory.h>

#include "DoocsBackend.h"
#include "DoocsBackendIntRegisterAccessor.h"
#include "DoocsBackendLongRegisterAccessor.h"
#include "DoocsBackendFloatRegisterAccessor.h"
#include "DoocsBackendStringRegisterAccessor.h"

namespace mtca4u {

  /********************************************************************************************************************/

  DoocsBackend::BackendRegisterer DoocsBackend::backendRegisterer;

  DoocsBackend::BackendRegisterer::BackendRegisterer() {
    mtca4u::BackendFactory::getInstance().registerBackendType("doocs","",&DoocsBackend::createInstance);
  }

  /********************************************************************************************************************/

  DoocsBackend::DoocsBackend(const RegisterPath &serverAddress)
  : _serverAddress(serverAddress)
  {
    FILL_VIRTUAL_FUNCTION_TEMPLATE_VTABLE(getRegisterAccessor_impl);
  }

  /********************************************************************************************************************/

  boost::shared_ptr<DeviceBackend> DoocsBackend::createInstance(std::string /*host*/,
      std::string /*instance*/, std::list<std::string> parameters, std::string /*mapFileName*/) {

    // check presense of required parameters
    if(parameters.size() < 2) {
      throw DeviceException("DoocsBackend: The SDM URI is missing a parameter: FACILITY and DEVICE must be specified.",
          DeviceException::WRONG_PARAMETER);
    }

    // form server address
    RegisterPath serverAddress = parameters.front();
    parameters.pop_front();
    serverAddress /= parameters.front();

    // create and return the backend
    return boost::shared_ptr<DeviceBackend>(new DoocsBackend(serverAddress));
  }

  /********************************************************************************************************************/

  void DoocsBackend::open() {
  }

  /********************************************************************************************************************/

  void DoocsBackend::close() {
  }

  /********************************************************************************************************************/

  template<typename UserType>
  boost::shared_ptr< NDRegisterAccessor<UserType> > DoocsBackend::getRegisterAccessor_impl(
      const RegisterPath &registerPathName, size_t numberOfWords, size_t wordOffsetInRegister, AccessModeFlags flags) {

    NDRegisterAccessor<UserType> *p;
    RegisterPath path = _serverAddress/registerPathName;

    // read property once
    EqAdr ea;
    EqCall eq;
    EqData src,dst;
    ea.adr(std::string(path).substr(1).c_str());        // strip leading slash
    int rc = eq.get(&ea, &src, &dst);
    if(rc) {
      throw DeviceException(std::string("Cannot open DOOCS property: ")+dst.get_string(),
          DeviceException::CANNOT_OPEN_DEVICEBACKEND);
    }

    // check type and create matching accessor
    if( dst.type() == DATA_INT || dst.type() == DATA_A_INT ||
        dst.type() == DATA_BOOL || dst.type() == DATA_A_BOOL ||
        dst.type() == DATA_A_SHORT ) {
      p = new DoocsBackendIntRegisterAccessor<UserType>(path, numberOfWords, wordOffsetInRegister, flags);
    }
    else if( dst.type() == DATA_A_LONG ) {
      p = new DoocsBackendLongRegisterAccessor<UserType>(path, numberOfWords, wordOffsetInRegister, flags);
    }
    else if( dst.type() == DATA_FLOAT || dst.type() == DATA_A_FLOAT ||
        dst.type() == DATA_DOUBLE || dst.type() == DATA_A_DOUBLE  ) {
      p = new DoocsBackendFloatRegisterAccessor<UserType>(path, numberOfWords, wordOffsetInRegister, flags);
    }
    else if( dst.type() == DATA_TEXT || dst.type() == DATA_STRING || dst.type() == DATA_STRING16) {
      p = new DoocsBackendStringRegisterAccessor<UserType>(path, numberOfWords, wordOffsetInRegister, flags);
    }
    else {
      throw DeviceException("Unsupported DOOCS data type: "+std::string(dst.type_string()),
          DeviceException::WRONG_PARAMETER);
    }


    return boost::shared_ptr< NDRegisterAccessor<UserType> > ( p );
  }

} /* namespace mtca4u */
