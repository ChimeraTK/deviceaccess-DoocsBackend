/*
 * testDoocsBackend.cpp
 *
 *  Created on: Apr 27, 2016
 *      Author: Martin Hierholzer
 */

#include <boost/test/included/unit_test.hpp>

#include <mtca4u/Device.h>

using namespace boost::unit_test_framework;
using namespace mtca4u;


class DoocsBackendTest {
  public:
    void testSimpleCase();
};

class DoocsBackendTestSuite : public test_suite {
  public:
    DoocsBackendTestSuite() : test_suite("DoocsBackend test suite") {
      boost::shared_ptr<DoocsBackendTest> doocsBackendTest(new DoocsBackendTest);

      add( BOOST_CLASS_TEST_CASE(&DoocsBackendTest::testSimpleCase, doocsBackendTest) );
    }
};

test_suite* init_unit_test_suite(int /*argc*/, char * /*argv*/ []) {
  framework::master_test_suite().p_name.value = "DoocsBackend class test suite";
  framework::master_test_suite().add(new DoocsBackendTestSuite());

  return NULL;
}

void DoocsBackendTest::testSimpleCase() {

  BackendFactory::getInstance().setDMapFilePath("test.dmap");
  mtca4u::Device device;

  device.open("PCIE2");

  device.close();

}
