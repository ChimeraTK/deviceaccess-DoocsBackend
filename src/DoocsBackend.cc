/*
 * DoocsBackend.cc
 *
 *  Created on: Apr 26, 2016
 *      Author: Martin Hierholzer
 */

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <ChimeraTK/BackendFactory.h>
#include <ChimeraTK/DeviceAccessVersion.h>
#include <libxml++/libxml++.h>
#include <fstream>

#include "CatalogueCache.h"
#include "CatalogueFetcher.h"
#include "DoocsBackend.h"
#include "DoocsBackendFloatRegisterAccessor.h"
#include "DoocsBackendIIIIRegisterAccessor.h"
#include "DoocsBackendIFFFRegisterAccessor.h"
#include "DoocsBackendIntRegisterAccessor.h"
#include "DoocsBackendLongRegisterAccessor.h"
#include "DoocsBackendStringRegisterAccessor.h"
#include "DoocsBackendEventIdAccessor.h"
#include "RegisterInfo.h"
#include "StringUtility.h"

// this is required since we link against the DOOCS libEqServer.so
const char* object_name = "DoocsBackend";

namespace ctk = ChimeraTK;

extern "C" {
boost::shared_ptr<ChimeraTK::DeviceBackend> ChimeraTK_DeviceAccess_createBackend(
    std::string address, std::map<std::string, std::string> parameters) {
  return ChimeraTK::DoocsBackend::createInstance(address, parameters);
}

static std::vector<std::string> ChimeraTK_DeviceAccess_sdmParameterNames{"facility", "device", "location"};

static std::string ChimeraTK_DeviceAccess_version{CHIMERATK_DEVICEACCESS_VERSION};

static std::string backend_name = "doocs";
}

static std::unique_ptr<ctk::RegisterCatalogue> fetchCatalogue(
    std::string serverAddress, std::string cacheFile, std::future<void> cancelFlag);

/********************************************************************************************************************/

static std::unique_ptr<ctk::RegisterCatalogue> fetchCatalogue(
    std::string serverAddress, std::string cacheFile, std::future<void> cancelFlag) {
  auto result = CatalogueFetcher(serverAddress, std::move(cancelFlag)).fetch();
  auto catalogue = std::move(result.first);
  auto isCatalogueComplete = result.second;
  bool isCacheFileNameSpecified = not cacheFile.empty();

  if(isCatalogueComplete && isCacheFileNameSpecified) {
    Cache::saveCatalogue(*catalogue, cacheFile);
  }
  return catalogue;
}

namespace ChimeraTK {

  /********************************************************************************************************************/

  DoocsBackend::BackendRegisterer DoocsBackend::backendRegisterer;

  DoocsBackend::BackendRegisterer::BackendRegisterer() {
    std::cout << "DoocsBackend::BackendRegisterer: registering backend type doocs" << std::endl;
    ChimeraTK::BackendFactory::getInstance().registerBackendType(
        "doocs", &DoocsBackend::createInstance, {"facility", "device", "location"});
  }

  /********************************************************************************************************************/

  DoocsBackend::DoocsBackend(const std::string& serverAddress, const std::string& cacheFile)
  : _serverAddress(serverAddress), _cacheFile(cacheFile) {
    if(cacheFileExists() && isCachingEnabled()) {
      _catalogue_mutable = Cache::readCatalogue(_cacheFile);
    }
    _catalogueFuture =
        std::async(std::launch::async, fetchCatalogue, serverAddress, cacheFile, _cancelFlag.get_future());
    FILL_VIRTUAL_FUNCTION_TEMPLATE_VTABLE(getRegisterAccessor_impl);
  }

  /********************************************************************************************************************/

  DoocsBackend::~DoocsBackend() {
    if(_catalogueFuture.valid()) {
      try {
        _cancelFlag.set_value(); // cancel fill catalogue async task
        _catalogueFuture.get();
      }
      catch(...) {
        // prevent throwing in destructor (ub if it does);
      }
    }
  }

  /********************************************************************************************************************/

  bool DoocsBackend::cacheFileExists() {
    if(_cacheFile.empty()) {
      return false;
    }
    std::ifstream f(_cacheFile.c_str());
    return f.good();
  }

  /********************************************************************************************************************/

  bool DoocsBackend::isCachingEnabled() const { return !_cacheFile.empty(); }

  /********************************************************************************************************************/

  boost::shared_ptr<DeviceBackend> DoocsBackend::createInstance(
      std::string address, std::map<std::string, std::string> parameters) {
    // if address is empty, build it from parameters (for compatibility with SDM)
    if(address.empty()) {
      RegisterPath serverAddress;
      serverAddress /= parameters["facility"];
      serverAddress /= parameters["device"];
      serverAddress /= parameters["location"];
      address = std::string(serverAddress).substr(1);
    }
    std::string cacheFile{};
    try {
      cacheFile = parameters.at("cacheFile");
    }
    catch(std::out_of_range&) {
      // empty cacheFile string => no caching
    }
    // create and return the backend
    return boost::shared_ptr<DeviceBackend>(new DoocsBackend(address, cacheFile));
  }

  /********************************************************************************************************************/

  void DoocsBackend::open() { _opened = true; }

  /********************************************************************************************************************/

  const RegisterCatalogue& DoocsBackend::getRegisterCatalogue() const {
    if(_catalogue_mutable == nullptr) {
      _catalogue_mutable = _catalogueFuture.get();
    }
    return *_catalogue_mutable;
  }

  /********************************************************************************************************************/

  void DoocsBackend::close() { _opened = false; }

  bool DoocsBackend::isFunctional() const {
#warning This default implementation causes fluctuating error states in ApplicationCore. Write a proper implementsation (issue #22)
    return _opened;
  }

  /********************************************************************************************************************/

  template<typename UserType>
  boost::shared_ptr<NDRegisterAccessor<UserType>> DoocsBackend::getRegisterAccessor_impl(
      const RegisterPath& registerPathName, size_t numberOfWords, size_t wordOffsetInRegister, AccessModeFlags flags) {
    boost::shared_ptr<NDRegisterAccessor<UserType>> p;
    std::string path = _serverAddress + registerPathName;

    // check for additional hierarchy level, which indicates an access to a field of a complex property data type
    bool hasExtraLevel = false;
    if(!boost::starts_with(path, "doocs://") && !boost::starts_with(path, "epics://")) {
      size_t nSlashes = std::count(path.begin(), path.end(), '/');
      if(nSlashes == 4) {
        hasExtraLevel = true;
      }
      else if(nSlashes < 3 || nSlashes > 4) {
        throw ChimeraTK::logic_error(std::string("DOOCS address has an illegal format: ") + path);
      }
    }
    else if(boost::starts_with(path, "doocs://")) {
      size_t nSlashes = std::count(path.begin(), path.end(), '/');
      // we have 3 extra slashes compared to the standard syntax without "doocs:://"
      if(nSlashes == 4 + 3) {
        hasExtraLevel = true;
      }
      else if(nSlashes < 3 + 3 || nSlashes > 4 + 3) {
        throw ChimeraTK::logic_error(std::string("DOOCS address has an illegal format: ") + path);
      }
    }

    // split the path into property name and field name
    std::string field;
    if(hasExtraLevel) {
      field = path.substr(path.find_last_of('/') + 1);
      path = path.substr(0, path.find_last_of('/'));
    }

    // if backend is open, read property once to obtain type. otherwise use the (potentially cached) catalogue
    int doocsTypeId = DATA_NULL;
    if(isOpen()) {
      EqAdr ea;
      EqCall eq;
      EqData src, dst;
      ea.adr(path);
      int rc = eq.get(&ea, &src, &dst);
      if(rc) {
        throw ChimeraTK::runtime_error("Cannot open DOOCS property '" + path + "': " + dst.get_string());
      }
      doocsTypeId = dst.type();
    }
    else {
      auto reg =
          boost::dynamic_pointer_cast<DoocsBackendRegisterInfo>(getRegisterCatalogue().getRegister(registerPathName));
      doocsTypeId = reg->doocsTypeId;
    }

    // check type and create matching accessor
    bool extraLevelUsed = false;
    auto sharedThis = boost::static_pointer_cast<DoocsBackend>(shared_from_this());

    if(field == "eventId") {
      extraLevelUsed = true;
      p.reset(new DoocsBackendEventIdRegisterAccessor<UserType>(sharedThis, path, registerPathName, flags));
    }
    else if(doocsTypeId == DATA_INT || doocsTypeId == DATA_A_INT || doocsTypeId == DATA_BOOL ||
        doocsTypeId == DATA_A_BOOL || doocsTypeId == DATA_A_SHORT) {
      p.reset(new DoocsBackendIntRegisterAccessor<UserType>(
          sharedThis, path, registerPathName, numberOfWords, wordOffsetInRegister, flags));
    }
    else if(doocsTypeId == DATA_A_LONG) {
      p.reset(new DoocsBackendLongRegisterAccessor<UserType>(
          sharedThis, path, registerPathName, numberOfWords, wordOffsetInRegister, flags));
    }
    else if(doocsTypeId == DATA_FLOAT || doocsTypeId == DATA_A_FLOAT || doocsTypeId == DATA_DOUBLE ||
        doocsTypeId == DATA_A_DOUBLE || doocsTypeId == DATA_SPECTRUM) {
      p.reset(new DoocsBackendFloatRegisterAccessor<UserType>(
          sharedThis, path, registerPathName, numberOfWords, wordOffsetInRegister, flags));
    }
    else if(doocsTypeId == DATA_TEXT || doocsTypeId == DATA_STRING || doocsTypeId == DATA_STRING16) {
      p.reset(new DoocsBackendStringRegisterAccessor<UserType>(
          sharedThis, path, registerPathName, numberOfWords, wordOffsetInRegister, flags));
    }
    else if(doocsTypeId == DATA_IIII) {
      p.reset(new DoocsBackendIIIIRegisterAccessor<UserType>(
          sharedThis, path, registerPathName, numberOfWords, wordOffsetInRegister, flags));
    }
    else if(doocsTypeId == DATA_IFFF) {
      extraLevelUsed = true;
      p.reset(new DoocsBackendIFFFRegisterAccessor<UserType>(
          sharedThis, path, field, registerPathName, numberOfWords, wordOffsetInRegister, flags));
    }
    else {
      throw ChimeraTK::logic_error("Unsupported DOOCS data type " + std::string(EqData().type_string(doocsTypeId)) +
          " of property '" + _serverAddress + registerPathName + "'");
    }

    // if the field name has been specified but the data type does not use it, throw an exception
    if(hasExtraLevel && !extraLevelUsed) {
      throw ChimeraTK::logic_error("Specifiaction of field name is not supported for the DOOCS data type " +
          std::string(EqData().type_string(doocsTypeId)) + ": " + _serverAddress + registerPathName);
    }

    return p;
  }

} /* namespace ChimeraTK */
