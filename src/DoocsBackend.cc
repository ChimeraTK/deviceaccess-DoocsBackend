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

#include "DoocsBackend.h"
#include "DoocsBackendFloatRegisterAccessor.h"
#include "DoocsBackendIIIIRegisterAccessor.h"
#include "DoocsBackendIFFFRegisterAccessor.h"
#include "DoocsBackendIntRegisterAccessor.h"
#include "DoocsBackendLongRegisterAccessor.h"
#include "DoocsBackendStringRegisterAccessor.h"

// this is required since we link against the DOOCS libEqServer.so
const char* object_name = "DoocsBackend";

extern "C" {
boost::shared_ptr<ChimeraTK::DeviceBackend> ChimeraTK_DeviceAccess_createBackend(
    std::string address, std::map<std::string, std::string> parameters) {
  return ChimeraTK::DoocsBackend::createInstance(address, parameters);
}

std::vector<std::string> ChimeraTK_DeviceAccess_sdmParameterNames{"facility", "device", "location"};

std::string ChimeraTK_DeviceAccess_version{CHIMERATK_DEVICEACCESS_VERSION};

std::string backend_name = "doocs";
}

/**
 *  RegisterInfo-derived class to be put into the RegisterCatalogue
 */
namespace {
  class DoocsBackendRegisterInfo : public ChimeraTK::RegisterInfo {
   public:
    ~DoocsBackendRegisterInfo() override {}

    ChimeraTK::RegisterPath getRegisterName() const override { return name; }

    unsigned int getNumberOfElements() const override { return length; }

    unsigned int getNumberOfChannels() const override { return 1; }

    unsigned int getNumberOfDimensions() const override { return length > 1 ? 1 : 0; }

    bool isReadable() const override { return true; }

    bool isWriteable() const override { return true; } /// @todo fixme: return true for read-only properties

    ChimeraTK::AccessModeFlags getSupportedAccessModes() const override { return accessModeFlags; }

    const ChimeraTK::RegisterInfo::DataDescriptor& getDataDescriptor() const override { return dataDescriptor; }

    ChimeraTK::RegisterPath name;
    unsigned int length;
    ChimeraTK::RegisterInfo::DataDescriptor dataDescriptor;
    ChimeraTK::AccessModeFlags accessModeFlags{};
  };
} // namespace

namespace ChimeraTK {

  /********************************************************************************************************************/

  DoocsBackend::BackendRegisterer DoocsBackend::backendRegisterer;

  DoocsBackend::BackendRegisterer::BackendRegisterer() {
    std::cout << "DoocsBackend::BackendRegisterer: registering backend type doocs" << std::endl;
    ChimeraTK::BackendFactory::getInstance().registerBackendType(
        "doocs", &DoocsBackend::createInstance, {"facility", "device", "location"});
  }

  /********************************************************************************************************************/

  bool DoocsBackend::dmsgStartCalled = false;
  std::mutex DoocsBackend::dmsgStartCalled_mutex;

  /********************************************************************************************************************/

  DoocsBackend::DoocsBackend(const std::string& serverAddress) : _serverAddress(serverAddress) {
    FILL_VIRTUAL_FUNCTION_TEMPLATE_VTABLE(getRegisterAccessor_impl);
  }

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

    // create and return the backend
    return boost::shared_ptr<DeviceBackend>(new DoocsBackend(address));
  }

  /********************************************************************************************************************/

  bool DoocsBackend::checkZmqAvailability(const std::string& fullLocationPath, const std::string& propertyName) {
    int rc;
    float f1;
    float f2;
    char* sp;
    time_t tm;
    EqAdr ea;
    EqData dat;
    EqData dst;
    EqCall eq;
    int portp;

    ea.adr(fullLocationPath + "/SPN");

    // get channel port number
    dat.set(1, 0.0f, 0.0f, time_t{0}, propertyName, 0);

    // call get () to see whether it is supported
    rc = eq.get(&ea, &dat, &dst);
    if(rc) {
      return false;
    }

    rc = dst.get_ustr(&portp, &f1, &f2, &tm, &sp, 0);
    if(rc && !portp && !static_cast<int>(f1 + f2)) rc = 0; // not supported

    if(!rc) {
      dst.get_ustr(&portp, &f1, &f2, &tm, &sp, 0);
      // get () not supported, call set ()
      rc = eq.set(&ea, &dat, &dst);
      if(rc) {
        return false;
      }
    }

    if(dst.type() == DATA_INT) {
      portp = dst.get_int();
    }
    else {
      dst.get_ustr(&portp, &f1, &f2, &tm, &sp, 0);
    }

    return portp != 0;
  }

  /********************************************************************************************************************/

  bool DoocsBackend::ignorePattern(std::string name, std::string pattern) const {
    return boost::algorithm::ends_with(name, pattern);
  }

  /********************************************************************************************************************/
  void DoocsBackend::fillCatalogue(std::string fixedComponents, long level) const {
    // obtain list of elements within the given partial address
    EqAdr ea;
    EqCall eq;
    EqData src, propList;
    ea.adr((fixedComponents + "/*").c_str());
    int rc = eq.names(&ea, &propList);
    if(rc) {
      // if the enumeration failes, maybe the server is not available (but exists
      // in ENS) -> just ignore this address
      return;
    }

    // iterate over list
    for(int i = 0; i < propList.array_length(); ++i) {
      // obtain the name of the element
      char c[255];
      propList.get_string_arg(i, c, 255);
      std::string name = c;
      name = name.substr(0, name.find_first_of(" ")); // ignore comment which is following the space

      // if we are not yet at the property-level, recursivly call the function
      // again to resolve the next hierarchy level
      if(level < 2) {
        fillCatalogue(fixedComponents + "/" + name, level + 1);
      }
      else {
        // this is a property: create RegisterInfo entry and set its name
        bool skipRegister = false;
        for(uint i = 0; i < SIZE_IGNORE_PATTERNS; i++) {
          std::string pattern = IGNORE_PATTERNS[i];
          if(ignorePattern(name, pattern)) {
            skipRegister = true;
            break;
          }
        }
        if(skipRegister) {
          continue;
        }
        boost::shared_ptr<DoocsBackendRegisterInfo> info(new DoocsBackendRegisterInfo());
        std::string fqn = fixedComponents + "/" + name;
        info->name = fqn.substr(std::string(_serverAddress).length());

        // read property once to determine its length and data type
        ///@todo Is there a more efficient way to do this?
        EqData dst;
        ea.adr(fqn.c_str()); // strip leading slash
        rc = eq.get(&ea, &src, &dst);
        if(rc) {
          // if the property is not accessible, ignore it. This happens frequently
          // e.g. for archiver-related properties
          continue;
        }

        if(DoocsBackend::checkZmqAvailability(fixedComponents, name))
          info->accessModeFlags.add(AccessMode::wait_for_new_data);

        info->length = dst.array_length();
        if(info->length == 0) info->length = 1;                    // DOOCS reports 0 if not an array
        if(dst.type() == DATA_TEXT || dst.type() == DATA_STRING || // in case of strings, DOOCS reports the
                                                                   // length of the string
            dst.type() == DATA_STRING16 || dst.type() == DATA_USTR) {
          info->length = 1;
          info->dataDescriptor =
              ChimeraTK::RegisterInfo::DataDescriptor(ChimeraTK::RegisterInfo::FundamentalType::string);
        }
        else if(dst.type() == DATA_INT || dst.type() == DATA_A_INT || dst.type() == DATA_A_SHORT ||
            dst.type() == DATA_A_LONG || dst.type() == DATA_A_BYTE || dst.type() == DATA_IIII) { // integral data types
          size_t digits;
          if(dst.type() == DATA_A_SHORT) { // 16 bit signed
            digits = 6;
          }
          else if(dst.type() == DATA_A_BYTE) { // 8 bit signed
            digits = 4;
          }
          else if(dst.type() == DATA_A_LONG) { // 64 bit signed
            digits = 20;
          }
          else { // 32 bit signed
            digits = 11;
          }
          if(dst.type() == DATA_IIII) info->length = 4;

          info->dataDescriptor = ChimeraTK::RegisterInfo::DataDescriptor(
              ChimeraTK::RegisterInfo::FundamentalType::numeric, true, true, digits);
        }
        else if(dst.type() == DATA_IFFF) {
          info->name = fqn.substr(std::string(_serverAddress).length()) + "/I";
          info->dataDescriptor = ChimeraTK::RegisterInfo::DataDescriptor(
              ChimeraTK::RegisterInfo::FundamentalType::numeric, true, true, 11); // 32 bit integer

          boost::shared_ptr<DoocsBackendRegisterInfo> infoF1(new DoocsBackendRegisterInfo(*info));
          infoF1->name = fqn.substr(std::string(_serverAddress).length()) + "/F1";
          infoF1->dataDescriptor = ChimeraTK::RegisterInfo::DataDescriptor(
              ChimeraTK::RegisterInfo::FundamentalType::numeric, false, true, 320, 300); // float

          boost::shared_ptr<DoocsBackendRegisterInfo> infoF2(new DoocsBackendRegisterInfo(*infoF1));
          infoF2->name = fqn.substr(std::string(_serverAddress).length()) + "/F2";

          boost::shared_ptr<DoocsBackendRegisterInfo> infoF3(new DoocsBackendRegisterInfo(*infoF1));
          infoF3->name = fqn.substr(std::string(_serverAddress).length()) + "/F3";

          _catalogue_mutable.addRegister(infoF1);
          _catalogue_mutable.addRegister(infoF2);
          _catalogue_mutable.addRegister(infoF3);
          // the info for the integer is added below
        }
        else { // floating point data types: always treat like double
          info->dataDescriptor = ChimeraTK::RegisterInfo::DataDescriptor(
              ChimeraTK::RegisterInfo::FundamentalType::numeric, false, true, 320, 300);
        }

        // add info to catalogue
        _catalogue_mutable.addRegister(info);
      }
    }
  }

  /********************************************************************************************************************/

  void DoocsBackend::open() {
    _opened = true;
    if(catalogueObtained) fillCatalogue();
  }

  /********************************************************************************************************************/

  void DoocsBackend::fillCatalogue() const {
    if(!catalogueFilled && _opened) {
      // Fill the catalogue:
      // first, count number of elements in address part given in DMAP file to
      // determine how many components we have to iterate over
      size_t nSlashes;
      if(!boost::starts_with(_serverAddress, "doocs://")) {
        nSlashes = std::count(_serverAddress.begin(), _serverAddress.end(), '/');
      }
      else {
        nSlashes = std::count(_serverAddress.begin(), _serverAddress.end(), '/') - 3;
      }
      // next, iteratively call the function to fill the catalogue
      fillCatalogue(_serverAddress, nSlashes);
      catalogueFilled = true;
    }
  }

  /********************************************************************************************************************/

  const RegisterCatalogue& DoocsBackend::getRegisterCatalogue() const {
    if(_opened) fillCatalogue();
    catalogueObtained = true;
    return _catalogue_mutable;
  }

  /********************************************************************************************************************/

  void DoocsBackend::close() { _opened = false; }

  /********************************************************************************************************************/

  template<typename UserType>
  boost::shared_ptr<NDRegisterAccessor<UserType>> DoocsBackend::getRegisterAccessor_impl(
      const RegisterPath& registerPathName, size_t numberOfWords, size_t wordOffsetInRegister, AccessModeFlags flags) {
    NDRegisterAccessor<UserType>* p;
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

    // read property once
    EqAdr ea;
    EqCall eq;
    EqData src, dst;
    ea.adr(std::string(path).c_str());
    int rc = eq.get(&ea, &src, &dst);
    if(rc) {
      throw ChimeraTK::runtime_error("Cannot open DOOCS property '" + path + "': " + dst.get_string());
    }

    // check type and create matching accessor
    bool extraLevelUsed = false;
    if(dst.type() == DATA_INT || dst.type() == DATA_A_INT || dst.type() == DATA_BOOL || dst.type() == DATA_A_BOOL ||
        dst.type() == DATA_A_SHORT) {
      p = new DoocsBackendIntRegisterAccessor<UserType>(path, numberOfWords, wordOffsetInRegister, flags);
    }
    else if(dst.type() == DATA_A_LONG) {
      p = new DoocsBackendLongRegisterAccessor<UserType>(path, numberOfWords, wordOffsetInRegister, flags);
    }
    else if(dst.type() == DATA_FLOAT || dst.type() == DATA_A_FLOAT || dst.type() == DATA_DOUBLE ||
        dst.type() == DATA_A_DOUBLE || dst.type() == DATA_SPECTRUM) {
      p = new DoocsBackendFloatRegisterAccessor<UserType>(path, numberOfWords, wordOffsetInRegister, flags);
    }
    else if(dst.type() == DATA_TEXT || dst.type() == DATA_STRING || dst.type() == DATA_STRING16) {
      p = new DoocsBackendStringRegisterAccessor<UserType>(path, numberOfWords, wordOffsetInRegister, flags);
    }
    else if(dst.type() == DATA_IIII) {
      p = new DoocsBackendIIIIRegisterAccessor<UserType>(path, numberOfWords, wordOffsetInRegister, flags);
    }
    else if(dst.type() == DATA_IFFF) {
      extraLevelUsed = true;
      p = new DoocsBackendIFFFRegisterAccessor<UserType>(path, field, numberOfWords, wordOffsetInRegister, flags);
    }
    else {
      throw ChimeraTK::logic_error("Unsupported DOOCS data type " + std::string(dst.type_string()) + " of property '" +
          _serverAddress + registerPathName + "'");
    }

    // if the field name has been specified but the data type does not use it, throw an exception
    if(hasExtraLevel && !extraLevelUsed) {
      throw ChimeraTK::logic_error("Specifiaction of field name is not supported for the DOOCS data type " +
          std::string(dst.type_string()) + ": " + _serverAddress + registerPathName);
    }

    return boost::shared_ptr<NDRegisterAccessor<UserType>>(p);
  }

} /* namespace ChimeraTK */
