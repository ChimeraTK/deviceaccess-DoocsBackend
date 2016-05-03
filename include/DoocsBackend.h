/*
 * DoocsBackend.h
 *
 *  Created on: Apr 26, 2016
 *      Author: Martin Hierholzer
 */

#ifndef MTCA4U_DOOCS_BACKEND_H
#define MTCA4U_DOOCS_BACKEND_H

#include <mtca4u/DeviceBackendImpl.h>

namespace mtca4u {

  /** Backend to access DOOCS control system servers.
   *
   *  The sdm URI should look like this:
   *  sdm://./doocs=FACILITY,DEVICE
   *
   *  FACILITY and DEVICE are the first two components of the DOOCS addresses targeted by this device. The full addess
   *  is completed by adding the location and property name from the register path names. Thus the register path names
   *  must be of the form "LOCATION/PROPERTY".
   */
  class DoocsBackend : public DeviceBackendImpl {

    public:

      DoocsBackend(const RegisterPath &serverAddress);

      virtual ~DoocsBackend(){}

      virtual void open();

      virtual void close();

      virtual std::string readDeviceInfo() {
        return std::string("DOOCS server address: ") + _serverAddress;
      }

      static boost::shared_ptr<DeviceBackend> createInstance(std::string host, std::string instance,
          std::list<std::string> parameters, std::string mapFileName);

    protected:

      template<typename UserType>
      boost::shared_ptr< NDRegisterAccessor<UserType> > getRegisterAccessor_impl(
          const RegisterPath &registerPathName, size_t numberOfWords, size_t wordOffsetInRegister, bool enforceRawAccess);
      DEFINE_VIRTUAL_FUNCTION_TEMPLATE_VTABLE_FILLER( DoocsBackend, getRegisterAccessor_impl, 4);

      /// DOOCS address component for the server (FACILITY/DEVICE)
      RegisterPath _serverAddress;

      /* Class to register the backend type with the factory. */
      class BackendRegisterer {
        public:
          BackendRegisterer();
      };
      static BackendRegisterer backendRegisterer;

  };

}

#endif /* MTCA4U_DOOCS_BACKEND_H */
