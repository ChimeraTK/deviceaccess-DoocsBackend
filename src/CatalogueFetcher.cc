#include "CatalogueFetcher.h"
#include "StringUtility.h"
#include "RegisterInfo.h"

#include <eq_client.h>

const std::vector<std::string> IGNORE_PATTERNS = {".HIST", ".FILT", "._FILT", ".EGU", ".DESC", ".HSTAT", "._HSTAT", "._HIST",
    ".LIST", ".SAVE", ".COMMENT", ".XEGU", ".POLYPARA"};


/********************************************************************************************************************/

std::unique_ptr<ChimeraTK::RegisterCatalogue> CatalogueFetcher::fetch() {
  catalogue_ = std::make_unique<ChimeraTK::RegisterCatalogue>();

  auto nSlashes = detail::slashes(serverAddress_);
  fillCatalogue(serverAddress_, nSlashes);

  return isCancelled() ? std::unique_ptr<ChimeraTK::RegisterCatalogue>{nullptr} : std::move(catalogue_);
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
      std::tie(skipRegister, pattern) = detail::endsWith(name, IGNORE_PATTERNS);
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

      auto regInfolist = DoocsBackendRegisterInfo::create(regPath, length, doocsTypeId);
      for(auto &r: regInfolist){
        if(checkZmqAvailability(fixedComponents, name)) {r->accessModeFlags.add(ChimeraTK::AccessMode::wait_for_new_data);}
        catalogue_->addRegister(r);
      }
    }
  }
}

/********************************************************************************************************************/


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

