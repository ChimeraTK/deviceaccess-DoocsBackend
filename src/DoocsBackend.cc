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

#include "DoocsBackend.h"
#include "DoocsBackendFloatRegisterAccessor.h"
#include "DoocsBackendIIIIRegisterAccessor.h"
#include "DoocsBackendIFFFRegisterAccessor.h"
#include "DoocsBackendIntRegisterAccessor.h"
#include "DoocsBackendLongRegisterAccessor.h"
#include "DoocsBackendStringRegisterAccessor.h"

// this is required since we link against the DOOCS libEqServer.so
const char* object_name = "DoocsBackend";

namespace ctk = ChimeraTK;

extern "C" {
boost::shared_ptr<ChimeraTK::DeviceBackend> ChimeraTK_DeviceAccess_createBackend(
    std::string address, std::map<std::string, std::string> parameters) {
  return ChimeraTK::DoocsBackend::createInstance(address, parameters);
}

std::vector<std::string> ChimeraTK_DeviceAccess_sdmParameterNames{"facility", "device", "location"};

std::string ChimeraTK_DeviceAccess_version{CHIMERATK_DEVICEACCESS_VERSION};

std::string backend_name = "doocs";
}

static std::unique_ptr<ctk::RegisterCatalogue> fetchCatalogue(std::string serverAddress, std::string cacheFile, std::future<void> cancelFlag);

namespace Cache {
  static std::unique_ptr<ctk::RegisterCatalogue> readCatalogue(const std::string& xmlfile);
  static void saveCatalogue(ctk::RegisterCatalogue& c, const std::string& xmlfile);
} // namespace Cache

/**
 *  RegisterInfo-derived class to be put into the RegisterCatalogue
 */
namespace {

  class DoocsBackendRegisterInfo : public ChimeraTK::RegisterInfo {
   public:
    DoocsBackendRegisterInfo() = default;

    DoocsBackendRegisterInfo(const std::string& n, unsigned int len, ctk::RegisterInfo::DataDescriptor& descriptor,
        ctk::AccessModeFlags& flags, int type)
    : name(n), length(len), dataDescriptor(descriptor), accessModeFlags(flags), doocsTypeId(type) {}

    DoocsBackendRegisterInfo(const DoocsBackendRegisterInfo& other) = default;

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
    int doocsTypeId;
  };

} // namespace

class CatalogueFetcher {
 public:
  CatalogueFetcher(const std::string& serverAddress, std::future<void> cancelIndicator)
  : serverAddress_(serverAddress), cancelFlag_(std::move(cancelIndicator)) {}

  std::unique_ptr<ctk::RegisterCatalogue> fetch();
  static std::vector<boost::shared_ptr<DoocsBackendRegisterInfo>>getRegInfo(const std::string &name, unsigned int length, int doocsType);
  static std::tuple<bool, std::string> endsWith(std::string const &s, const std::vector<std::string>& patterns);

 private:
  std::string serverAddress_;
  std::future<void> cancelFlag_;
  std::unique_ptr<ctk::RegisterCatalogue> catalogue_{};

  void fillCatalogue(std::string fixedComponents, long level) const;
  static long slashes(const std::string& s);
  bool isCancelled() const { return (cancelFlag_.wait_for(std::chrono::microseconds(0)) == std::future_status::ready); }
  bool checkZmqAvailability(const std::string& fullLocationPath, const std::string& propertyName) const;
};

/********************************************************************************************************************/

static std::unique_ptr<ctk::RegisterCatalogue> fetchCatalogue(std::string serverAddress, std::string cacheFile, std::future<void> cancelFlag) {
  auto catalogue =  CatalogueFetcher(serverAddress, std::move(cancelFlag)).fetch();

  // catalogue == nullptr when fetch is canceled.
  if (catalogue != nullptr && cacheFile.empty() == false ){
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
    if (cacheFileExists() && isCachingEnabled()) {
      _catalogue_mutable = Cache::readCatalogue(_cacheFile);
    }
    _catalogueFuture = std::async(std::launch::async, fetchCatalogue, serverAddress, cacheFile, _cancelFlag.get_future());
    FILL_VIRTUAL_FUNCTION_TEMPLATE_VTABLE(getRegisterAccessor_impl);
  }

  /********************************************************************************************************************/

  DoocsBackend::~DoocsBackend() {
    if (_catalogueFuture.valid()) {
      try {
          _cancelFlag.set_value(); // cancel fill catalogue async task
          _catalogueFuture.get();
      } catch (...) {
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

  const RegisterCatalogue &DoocsBackend::getRegisterCatalogue() const {
    if (_catalogue_mutable == nullptr) {
      _catalogue_mutable = _catalogueFuture.get();
    }
    return *_catalogue_mutable;
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

    // if backend is open, read property once to obtain type. otherwise use the (potentially cached) catalogue
    int doocsTypeId = DATA_NULL;
    if(isOpen()) {
      EqAdr ea;
      EqCall eq;
      EqData src, dst;
      ea.adr(std::string(path).c_str());
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
    if(doocsTypeId == DATA_INT || doocsTypeId == DATA_A_INT || doocsTypeId == DATA_BOOL || doocsTypeId == DATA_A_BOOL ||
        doocsTypeId == DATA_A_SHORT) {
      p = new DoocsBackendIntRegisterAccessor<UserType>(
          sharedThis, path, registerPathName, numberOfWords, wordOffsetInRegister, flags);
    }
    else if(doocsTypeId == DATA_A_LONG) {
      p = new DoocsBackendLongRegisterAccessor<UserType>(
          sharedThis, path, registerPathName, numberOfWords, wordOffsetInRegister, flags);
    }
    else if(doocsTypeId == DATA_FLOAT || doocsTypeId == DATA_A_FLOAT || doocsTypeId == DATA_DOUBLE ||
        doocsTypeId == DATA_A_DOUBLE || doocsTypeId == DATA_SPECTRUM) {
      p = new DoocsBackendFloatRegisterAccessor<UserType>(
          sharedThis, path, registerPathName, numberOfWords, wordOffsetInRegister, flags);
    }
    else if(doocsTypeId == DATA_TEXT || doocsTypeId == DATA_STRING || doocsTypeId == DATA_STRING16) {
      p = new DoocsBackendStringRegisterAccessor<UserType>(
          sharedThis, path, registerPathName, numberOfWords, wordOffsetInRegister, flags);
    }
    else if(doocsTypeId == DATA_IIII) {
      p = new DoocsBackendIIIIRegisterAccessor<UserType>(
          sharedThis, path, registerPathName, numberOfWords, wordOffsetInRegister, flags);
    }
    else if(doocsTypeId == DATA_IFFF) {
      extraLevelUsed = true;
      p = new DoocsBackendIFFFRegisterAccessor<UserType>(
          sharedThis, path, field, registerPathName, numberOfWords, wordOffsetInRegister, flags);
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

    return boost::shared_ptr<NDRegisterAccessor<UserType>>(p);
  }

} /* namespace ChimeraTK */

/********************************************************************************************************************/

std::unique_ptr<ctk::RegisterCatalogue> CatalogueFetcher::fetch() {
  catalogue_ = std::make_unique<ctk::RegisterCatalogue>();

  auto nSlashes = slashes(serverAddress_);
  fillCatalogue(serverAddress_, nSlashes);

  return isCancelled() ? std::make_unique<ctk::RegisterCatalogue>() : std::move(catalogue_);
}

/********************************************************************************************************************/

long CatalogueFetcher::slashes(const std::string& s) {
  long nSlashes;
  if(!boost::starts_with(s, "doocs://")) {
    nSlashes = std::count(s.begin(), s.end(), '/');
  }
  else {
    nSlashes = std::count(s.begin(), s.end(), '/') - 3;
  }
  return nSlashes;
}

/********************************************************************************************************************/

std::tuple<bool, std::string> CatalogueFetcher::endsWith(std::string const &s,
                                const std::vector<std::string> &patterns) {
  for (auto &p : patterns) {
    if (boost::algorithm::ends_with(s, p)) {
      return std::tuple<bool, std::string>{true, p};
    }
  }
  return std::tuple<bool, std::string>{false, ""};
}

/********************************************************************************************************************/

void CatalogueFetcher::fillCatalogue(std::string fixedComponents, long level) const {
  // obtain list of elements within the given partial address
  EqAdr ea;
  EqCall eq;
  EqData src, propList;
  ea.adr((fixedComponents + "/*").c_str());
  int rc = eq.names(&ea, &propList);
  if(rc) {
    // if the enumeration failes, maybe the server is not available (but
    // exists in ENS) -> just ignore this address
    return;
  }

  // iterate over list
  for(int i = 0; i < propList.array_length() && (isCancelled() == false); ++i) {
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
    else { // this is a property: create RegisterInfo entry and set its name

      bool skipRegister;
      std::string pattern="";

      // skip unwanted properties.
      std::tie(skipRegister, pattern) = endsWith(name, ctk::IGNORE_PATTERNS);
      if(skipRegister) {
        continue;
      }

      // read property once to determine its length and data type
      ///@todo Is there a more efficient way to do this?
      std::string fqn = fixedComponents + "/" + name;
      EqData dst;
      ea.adr(fqn.c_str()); // strip leading slash
      rc = eq.get(&ea, &src, &dst);
      if(rc) {
        // if the property is not accessible, ignore it. This happens
        // frequently e.g. for archiver-related properties
        continue;
      }

      auto regPath = fqn.substr(std::string(serverAddress_).length());
      auto length = dst.array_length();
      auto doocsTypeId = dst.type();

      auto regInfolist = getRegInfo(regPath, length, doocsTypeId);
      for(auto &r: regInfolist){
          if(checkZmqAvailability(fixedComponents, name)) {r->accessModeFlags.add(ctk::AccessMode::wait_for_new_data);}
          catalogue_->addRegister(r);
      }
    }
  }
}

/********************************************************************************************************************/

std::vector<boost::shared_ptr<DoocsBackendRegisterInfo>>CatalogueFetcher::getRegInfo(const std::string &name, unsigned int length, int doocsType){
  std::vector<boost::shared_ptr<DoocsBackendRegisterInfo>> list;
  boost::shared_ptr<DoocsBackendRegisterInfo> info(new DoocsBackendRegisterInfo());

  info->name = name;
  info->length = length;
  info->doocsTypeId = doocsType;

  if (info->length == 0)
    info->length = 1; // DOOCS reports 0 if not an array
  if (doocsType == DATA_TEXT || doocsType == DATA_STRING || doocsType == DATA_STRING16 ||
      doocsType == DATA_USTR) { // in case of strings, DOOCS reports the length
                                // of the string
    info->length = 1;
    info->dataDescriptor = ChimeraTK::RegisterInfo::DataDescriptor(ChimeraTK::RegisterInfo::FundamentalType::string);
    list.push_back(info);

  } else if (doocsType == DATA_INT || doocsType == DATA_A_INT ||
             doocsType == DATA_A_SHORT || doocsType == DATA_A_LONG ||
             doocsType == DATA_A_BYTE ||
             doocsType == DATA_IIII) { // integral data types
    size_t digits;
    if (doocsType == DATA_A_SHORT) { // 16 bit signed
      digits = 6;
    } else if (doocsType == DATA_A_BYTE) { // 8 bit signed
      digits = 4;
    } else if (doocsType == DATA_A_LONG) { // 64 bit signed
      digits = 20;
    } else { // 32 bit signed
      digits = 11;
    }
    if (doocsType == DATA_IIII)
      info->length = 4;

    info->dataDescriptor = ChimeraTK::RegisterInfo::DataDescriptor(
        ChimeraTK::RegisterInfo::FundamentalType::numeric, true, true, digits);
    list.push_back(info);

  } else if (doocsType == DATA_IFFF) {
    info->name = name + "/I";
    info->dataDescriptor = ChimeraTK::RegisterInfo::DataDescriptor(
        ChimeraTK::RegisterInfo::FundamentalType::numeric, true, true, 11); // 32 bit integer

    boost::shared_ptr<DoocsBackendRegisterInfo> infoF1(new DoocsBackendRegisterInfo(*info));
    infoF1->name = name + "/F1";
    infoF1->dataDescriptor = ChimeraTK::RegisterInfo::DataDescriptor(
        ChimeraTK::RegisterInfo::FundamentalType::numeric, false, true, 320, 300); // float

    boost::shared_ptr<DoocsBackendRegisterInfo> infoF2(new DoocsBackendRegisterInfo(*infoF1));
    infoF2->name = name + "/F2";

    boost::shared_ptr<DoocsBackendRegisterInfo> infoF3(new DoocsBackendRegisterInfo(*infoF1));
    infoF3->name = name + "/F3";

    list.push_back(info);
    list.push_back(infoF1);
    list.push_back(infoF2);
    list.push_back(infoF3);

  } else { // floating point data types: always treat like double
    info->dataDescriptor = ChimeraTK::RegisterInfo::DataDescriptor(
        ChimeraTK::RegisterInfo::FundamentalType::numeric, false, true, 320, 300);
    list.push_back(info);
  }
  return list;
}

/********************************************************************************************************************/

bool CatalogueFetcher::checkZmqAvailability(
    const std::string& fullLocationPath, const std::string& propertyName) const {
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

namespace Cache {

  static std::unique_ptr<xmlpp::DomParser> createDomParser(const std::string& xmlfile);
  static xmlpp::Element* getRootNode(xmlpp::DomParser& parser);
  static unsigned int convertToUint(const std::string& s, int line);
  static int convertToInt(const std::string& s, int line);
  static std::vector<boost::shared_ptr<DoocsBackendRegisterInfo>> parseRegister(xmlpp::Element const* registerNode);
  static unsigned int parseLength(xmlpp::Element const* c);
  static int parseTypeId(xmlpp::Element const* c);
  static ctk::AccessModeFlags parseAccessMode(xmlpp::Element const* c);
  static void addRegInfoXmlNode(DoocsBackendRegisterInfo& r, xmlpp::Node* rootNode);

  /********************************************************************************************************************/

  std::unique_ptr<ctk::RegisterCatalogue> readCatalogue(const std::string& xmlfile) {
    auto catalogue = std::make_unique<ctk::RegisterCatalogue>();
    auto parser = createDomParser(xmlfile);
    auto registerList = getRootNode(*parser);

    for(auto const node : registerList->get_children()) {
      auto reg = dynamic_cast<const xmlpp::Element*>(node);
      if(reg == nullptr) {
        continue;
      }
      auto regInfolist = parseRegister(reg);
      for (auto & regInfo: regInfolist){
        catalogue->addRegister(regInfo);
      }
    }
    return catalogue;
  }

  /********************************************************************************************************************/

  void saveCatalogue(ctk::RegisterCatalogue& c, const std::string& xmlfile) {
    xmlpp::Document doc;

    auto rootNode = doc.create_root_node("catalogue");
    rootNode->set_attribute("version", "1.0");

    for(auto it = c.begin(); it != c.end(); ++it) {
      auto basePtr = it.get().get();
      auto doocsRegInfo = dynamic_cast<DoocsBackendRegisterInfo*>(basePtr);
      if(doocsRegInfo != nullptr) {
        addRegInfoXmlNode(*doocsRegInfo, rootNode);
      }
    }
    doc.write_to_file_formatted(xmlfile);
  }

  /********************************************************************************************************************/

  std::vector<boost::shared_ptr<DoocsBackendRegisterInfo>>
  parseRegister(xmlpp::Element const *registerNode) {
    std::string name;
    unsigned int len{};
    int doocsTypeId{};
    ctk::RegisterInfo::DataDescriptor descriptor{};
    ctk::AccessModeFlags flags{};

    for (auto &node : registerNode->get_children()) {
      auto e = dynamic_cast<const xmlpp::Element *>(node);
      if (e == nullptr) {
        continue;
      }
      std::string nodeName = e->get_name();

      if (nodeName == "name") {
        name = e->get_child_text()->get_content();
      } else if (nodeName == "length") {
        len = parseLength(e);
      } else if (nodeName == "access_mode") {
        flags = parseAccessMode(e);
      } else if (nodeName == "doocs_type_id") {
        doocsTypeId = parseTypeId(e);
      }
    }

    bool is_ifff = (doocsTypeId == DATA_IFFF);
    std::string pattern;
    std::vector<boost::shared_ptr<DoocsBackendRegisterInfo>> list;

    if (is_ifff) {
      std::tie(is_ifff, pattern) = CatalogueFetcher::endsWith(name, {"/I", "/F1", "/F2", "/F3"});
      // remove pattern from name for getRegInfo to work correctly;
      // precondition: patten is contained in name.
      name.erase(name.end() - pattern.length(), name.end());

      list = CatalogueFetcher::getRegInfo(name, len, doocsTypeId);
      // !!!
      list.erase(
          std::remove_if(list.begin(), list.end(), //
                         [&](boost::shared_ptr<DoocsBackendRegisterInfo> &e) {
                           return !boost::algorithm::ends_with(
                               static_cast<std::string>(e->name), pattern);
                         }),
          list.end());
    } else {
      list = CatalogueFetcher::getRegInfo(name, len, doocsTypeId);
    }
    for (auto e : list) {
      e->accessModeFlags = flags;
    }
    return list;
  }

  /********************************************************************************************************************/

  unsigned int parseLength(xmlpp::Element const* c) {
    return convertToUint(c->get_child_text()->get_content(), c->get_line());
  }

  /********************************************************************************************************************/

  int parseTypeId(xmlpp::Element const* c) { return convertToInt(c->get_child_text()->get_content(), c->get_line()); }

  /********************************************************************************************************************/

  ctk::AccessModeFlags parseAccessMode(xmlpp::Element const* c) {
    std::string accessMode{};
    auto t = c->get_child_text();
    if(t != nullptr) {
      accessMode = t->get_content();
    }
    return ctk::AccessModeFlags::deserialize(accessMode);
  }

  /********************************************************************************************************************/

  std::unique_ptr<xmlpp::DomParser> createDomParser(const std::string& xmlfile) {
    try {
      return std::make_unique<xmlpp::DomParser>(xmlfile);
    }
    catch(xmlpp::exception& e) {
      throw ChimeraTK::logic_error("Error opening " + xmlfile + ": " + e.what());
    }
  }

  /********************************************************************************************************************/

  xmlpp::Element* getRootNode(xmlpp::DomParser& parser) {
    auto root = parser.get_document()->get_root_node();
    if(root->get_name() != "catalogue") {
      ctk::logic_error("Expected tag 'catalog' got: " + root->get_name());
    }
    return root;
  }

  /********************************************************************************************************************/

  unsigned int convertToUint(const std::string& s, int line) {
    try {
      return std::stoul(s);
    }
    catch(std::invalid_argument& e) {
      throw ctk::logic_error("Failed to parse node at line " + std::to_string(line) + ":" + e.what());
    }
    catch(std::out_of_range& e) {
      throw ctk::logic_error("Failed to parse node at line " + std::to_string(line) + ":" + e.what());
    }
  }
  /********************************************************************************************************************/

  int convertToInt(const std::string& s, int line) {
    try {
      return std::stol(s);
    }
    catch(std::invalid_argument& e) {
      throw ctk::logic_error("Failed to parse node at line " + std::to_string(line) + ":" + e.what());
    }
    catch(std::out_of_range& e) {
      throw ctk::logic_error("Failed to parse node at line " + std::to_string(line) + ":" + e.what());
    }
  }

  /********************************************************************************************************************/

  void addRegInfoXmlNode(DoocsBackendRegisterInfo& r, xmlpp::Node* rootNode) {
    auto registerTag = rootNode->add_child("register");

    auto nameTag = registerTag->add_child("name");
    nameTag->set_child_text(static_cast<std::string>(r.name));

    auto lengthTag = registerTag->add_child("length");
    lengthTag->set_child_text(std::to_string(r.length));

    auto accessMode = registerTag->add_child("access_mode");
    accessMode->set_child_text(r.accessModeFlags.serialize());

    auto typeId = registerTag->add_child("doocs_type_id");
    typeId->set_child_text(std::to_string(r.doocsTypeId));

    std::string comment_text = std::string("doocs id: ") + EqData().type_string(r.doocsTypeId);
    registerTag->add_child_comment(comment_text);
  }

} // namespace Cache
