#include "CatalogueCache.h"
#include "RegisterInfo.h"
#include "StringUtility.h"

#include <boost/algorithm/string.hpp>
#include <fstream>
#include <libxml++/libxml++.h>
#include <eq_types.h>
#include <eq_data.h>

#include <cstdlib>
#include <cerrno>
#include <cstring>

/********************************************************************************************************************/

namespace Cache {

  static std::unique_ptr<xmlpp::DomParser> createDomParser(const std::string& xmlfile);
  static xmlpp::Element* getRootNode(xmlpp::DomParser& parser);
  static unsigned int convertToUint(const std::string& s, int line);
  static int convertToInt(const std::string& s, int line);
  static std::vector<boost::shared_ptr<DoocsBackendRegisterInfo>> parseRegister(xmlpp::Element const* registerNode);
  static unsigned int parseLength(xmlpp::Element const* c);
  static int parseTypeId(xmlpp::Element const* c);
  static ChimeraTK::AccessModeFlags parseAccessMode(xmlpp::Element const* c);
  static void addRegInfoXmlNode(DoocsBackendRegisterInfo& r, xmlpp::Node* rootNode);

  /********************************************************************************************************************/

  std::unique_ptr<ChimeraTK::RegisterCatalogue> readCatalogue(const std::string& xmlfile) {
    auto catalogue = std::make_unique<ChimeraTK::RegisterCatalogue>();
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
  bool is_empty(std::ifstream& f){
    return f.peek() == std::ifstream::traits_type::eof();
  }

  bool is_empty(std::ifstream&& f){
     return is_empty(f);
  }

  /********************************************************************************************************************/

  void saveCatalogue(ChimeraTK::RegisterCatalogue& c, const std::string& xmlfile) {
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
    std::string temporary_name = xmlfile + ".new";
    doc.write_to_file_formatted(temporary_name);

    // check for empty tmp file:
    // xmlpp::Document::write_to_file_formatted sometimes misbehaves on exceptions, creating
    // empty files.
    if(is_empty(std::ifstream(temporary_name))){
      throw ChimeraTK::runtime_error(std::string{"Failed to save cache File"});
    }

    if(std::rename(temporary_name.c_str(), xmlfile.c_str()) < 0) {
      int savedErrno = errno;
      char reason[255] = {0};
      strerror_r(savedErrno, reason, sizeof(reason) - 1);
      throw ChimeraTK::runtime_error(std::string{"Failed to replace cache file: "} + reason);
    }
  }

  /********************************************************************************************************************/

  std::vector<boost::shared_ptr<DoocsBackendRegisterInfo>>
      parseRegister(xmlpp::Element const *registerNode) {
    std::string name;
    unsigned int len{};
    int doocsTypeId{};
    ChimeraTK::RegisterInfo::DataDescriptor descriptor{};
    ChimeraTK::AccessModeFlags flags{};

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
    std::vector<boost::shared_ptr<DoocsBackendRegisterInfo>> list;

    if (is_ifff) {
      auto pattern = detail::endsWith(name, {"/I", "/F1", "/F2", "/F3"}).second;
      // remove pattern from name for getRegInfo to work correctly;
      // precondition: patten is contained in name.
      name.erase(name.end() - pattern.length(), name.end());

      list = DoocsBackendRegisterInfo::create(name, len, doocsTypeId);
      // !!!
      list.erase(
          std::remove_if(list.begin(), list.end(), //
              [&pattern](boost::shared_ptr<DoocsBackendRegisterInfo> &e) {
                return !boost::algorithm::ends_with(
                    static_cast<std::string>(e->_name), pattern);
              }),
          list.end());
    } else {
      list = DoocsBackendRegisterInfo::create(name, len, doocsTypeId);
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

  ChimeraTK::AccessModeFlags parseAccessMode(xmlpp::Element const* c) {
    std::string accessMode{};
    auto t = c->get_child_text();
    if(t != nullptr) {
      accessMode = t->get_content();
    }
    return ChimeraTK::AccessModeFlags::deserialize(accessMode);
  }

  /********************************************************************************************************************/

  std::unique_ptr<xmlpp::DomParser> createDomParser(const std::string& xmlfile) {
    try {
      return std::make_unique<xmlpp::DomParser>(xmlfile);
    }
    catch(std::exception& e) {
      throw ChimeraTK::logic_error("Error opening " + xmlfile + ": " + e.what());
    }
  }

  /********************************************************************************************************************/

  xmlpp::Element* getRootNode(xmlpp::DomParser& parser) {
    try {
      auto root = parser.get_document()->get_root_node();
      if(root->get_name() != "catalogue") {
        ChimeraTK::logic_error("Expected tag 'catalog' got: " + root->get_name());
      }
      return root;
    }
    catch(std::exception& e){
      throw ChimeraTK::logic_error(e.what());
    }
  }

  /********************************************************************************************************************/

  unsigned int convertToUint(const std::string& s, int line) {
    try {
      return std::stoul(s);
    }
    catch(std::invalid_argument& e) {
      throw ChimeraTK::logic_error("Failed to parse node at line " + std::to_string(line) + ":" + e.what());
    }
    catch(std::out_of_range& e) {
      throw ChimeraTK::logic_error("Failed to parse node at line " + std::to_string(line) + ":" + e.what());
    }
  }
  /********************************************************************************************************************/

  int convertToInt(const std::string& s, int line) {
    try {
      return std::stol(s);
    }
    catch(std::invalid_argument& e) {
      throw ChimeraTK::logic_error("Failed to parse node at line " + std::to_string(line) + ":" + e.what());
    }
    catch(std::out_of_range& e) {
      throw ChimeraTK::logic_error("Failed to parse node at line " + std::to_string(line) + ":" + e.what());
    }
  }

  /********************************************************************************************************************/

  void addRegInfoXmlNode(DoocsBackendRegisterInfo& r, xmlpp::Node* rootNode) {
    auto registerTag = rootNode->add_child("register");

    auto nameTag = registerTag->add_child("name");
    nameTag->set_child_text(static_cast<std::string>(r.getRegisterName()));

    auto lengthTag = registerTag->add_child("length");
    lengthTag->set_child_text(std::to_string(r.getNumberOfElements()));

    auto accessMode = registerTag->add_child("access_mode");
    accessMode->set_child_text(r.accessModeFlags.serialize());

    auto typeId = registerTag->add_child("doocs_type_id");
    typeId->set_child_text(std::to_string(r.doocsTypeId));

    std::string comment_text = std::string("doocs id: ") + EqData().type_string(r.doocsTypeId);
    registerTag->add_child_comment(comment_text);
  }

} // namespace Cache
