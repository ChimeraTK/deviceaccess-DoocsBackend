#define BOOST_TEST_MODULE testDoocsBackend
#include <boost/test/included/unit_test.hpp>

#include <fstream>
#include <ChimeraTK/Device.h>

BOOST_AUTO_TEST_CASE(testCaching) {
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
                    "  </register>\n"
                    "</catalogue>\n";
  std::string cacheFile = "cache.xml";
  std::ofstream o(cacheFile);
  o << xml;
  o.close();


  // make device pick the xml we have just created
  std::string address = "(doocs:doocs://localhost:212/F/D?cacheFile=" + cacheFile + ")";
  auto d = ChimeraTK::Device(address);
  auto catalogue = d.getRegisterCatalogue();

  BOOST_CHECK(catalogue.hasRegister("/DUMMY"));

  // cleanup
  std::string command = "rm " + cacheFile;
  std::system(command.c_str());
}
