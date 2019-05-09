#define BOOST_TEST_MODULE testDoocsBackend
#include <boost/test/included/unit_test.hpp>

#include <fstream>
#include <ChimeraTK/Device.h>

/**********************************************************************************************************************/

static std::string cacheFile = "cache.xml";

/**********************************************************************************************************************/

void generateCacheFile() {
  // create xml cache file
  std::string xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                    "<catalogue version=\"1.0\">"
                    "  <register>\n"
                    "    <name>/DUMMY</name>\n"
                    "    <length>1</length>\n"
                    "    <descriptor>\n"
                    "      <type>string</type>\n"
                    "      <raw_type>none</raw_type>\n"
                    "      <transport_type>none</transport_type>\n"
                    "    </descriptor>\n"
                    "    <access_mode></access_mode>\n"
                    "    <doocs_type_id>7</doocs_type_id>\n" // DATA_TEXT
                    "  </register>\n"
                    "</catalogue>\n";
  std::ofstream o(cacheFile);
  o << xml;
  o.close();
}

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testCacheReading) {
  generateCacheFile();

  // make device pick the xml we have just created
  std::string address = "(doocs:doocs://localhost:212/F/D/L?cacheFile=" + cacheFile + ")";
  auto d = ChimeraTK::Device(address);
  auto catalogue = d.getRegisterCatalogue();

  BOOST_CHECK(catalogue.hasRegister("/DUMMY"));

  // cleanup
  std::string command = "rm " + cacheFile;
  std::system(command.c_str());
}

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testGetAccessorWhileClosed) {
  generateCacheFile();

  // make device pick the xml we have just created
  std::string address = "(doocs:doocs://localhost:212/F/D/L?cacheFile=" + cacheFile + ")";
  auto d = ChimeraTK::Device(address);
  auto catalogue = d.getRegisterCatalogue();

  BOOST_CHECK(catalogue.hasRegister("/DUMMY"));

  // obtain accessor
  auto acc = d.getOneDRegisterAccessor<std::string>("DUMMY");
  BOOST_CHECK(acc.getNElements() == 1);
  BOOST_CHECK_THROW(acc.read(), ChimeraTK::logic_error); // device closed

  // cleanup
  std::string command = "rm " + cacheFile;
  std::system(command.c_str());
}

/**********************************************************************************************************************/