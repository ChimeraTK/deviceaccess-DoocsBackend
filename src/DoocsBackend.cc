/*
 * DoocsBackend.cc
 *
 *  Created on: Apr 26, 2016
 *      Author: Martin Hierholzer
 */

#include <mtca4u/BackendFactory.h>

#include "DoocsBackend.h"
#include "DoocsBackendRegisterAccessor.h"

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
    if (parameters.size() < 2) {
      throw DeviceException("",DeviceException::WRONG_PARAMETER);
    }

    std::cout << parameters.front() << std::endl;

    // form server address
    RegisterPath serverAddress = parameters.front();
    parameters.pop_front();
    std::cout << parameters.front() << std::endl;
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
      const RegisterPath &registerPathName, size_t numberOfWords, size_t wordOffsetInRegister, bool enforceRawAccess) {
    auto *p = new DoocsBackendRegisterAccessor<UserType>(_serverAddress/registerPathName);
    return boost::shared_ptr< NDRegisterAccessor<UserType> > ( p );
  }

} /* namespace mtca4u */
