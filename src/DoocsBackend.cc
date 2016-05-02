/*
 * DoocsBackend.cc
 *
 *  Created on: Apr 26, 2016
 *      Author: Martin Hierholzer
 */

#include "DoocsBackend.h"

namespace mtca4u {

  DoocsBackend::DoocsBackend(const RegisterPath &serverAddress)
  : _serverAddress(_serverAddress)
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

    // form server address
    RegisterPath serverAddress = parameters.front();
    parameters.pop_front();
    serverAddress /= parameters.front();

    // create and return the backend
    return boost::shared_ptr<DeviceBackend>(new DoocsBackend(serverAddress));
  }


} /* namespace mtca4u */
