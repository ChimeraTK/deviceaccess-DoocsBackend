/*
 * testDoocsBackend.cpp
 *
 *  Created on: Apr 27, 2016
 *      Author: Martin Hierholzer
 */

#include <thread>

#define BOOST_TEST_NO_MAIN      // main function is define in DOOCS
#include <boost/test/included/unit_test.hpp>

#include <mtca4u/Device.h>
#include <mtca4u-doocsServerTestHelper/doocsServerTestHelper.h>

using namespace boost::unit_test_framework;
using namespace mtca4u;

/**********************************************************************************************************************/

class DoocsBackendTest {
  public:
    void testRoutine();

    void testSimpleCase();
};

/**********************************************************************************************************************/

class DoocsBackendTestSuite : public test_suite {
  public:
    DoocsBackendTestSuite() : test_suite("DoocsBackend test suite") {
      boost::shared_ptr<DoocsBackendTest> doocsBackendTest(new DoocsBackendTest);

      add( BOOST_CLASS_TEST_CASE(&DoocsBackendTest::testSimpleCase, doocsBackendTest) );
    }
};

/**********************************************************************************************************************/

// test lanucher class, constructor will be called on start of the server
class TestLauncher {
  public:
    TestLauncher() {

      // start the test thread
      DoocsBackendTest *test = new DoocsBackendTest();
      serverTest = boost::shared_ptr<DoocsBackendTest>(test);
      theThread = std::thread(&DoocsBackendTest::testRoutine, test);
      theThread.detach();

    }

    /// server test object
    boost::shared_ptr<DoocsBackendTest> serverTest;

    /// thread running the test routine resp. controlling the timing
    std::thread theThread;

};
static TestLauncher testLauncher;

/**********************************************************************************************************************/

test_suite* myInit( int /*argc*/, char* /*argv*/ [] ) {
    return NULL;
}

/**********************************************************************************************************************/

void DoocsBackendTest::testRoutine() {     // version to run the unit and integration tests

  // initialise virtual timing system and wait until server has started
  doocsServerTestHelper::initialise(false);

  // initialise BOOST test suite
  extern char **svr_argv;
  extern int svr_argc;
  framework::init(&myInit,svr_argc,svr_argv);
  framework::master_test_suite().p_name.value = "DoocsBackend test suite";
  framework::master_test_suite().add( new DoocsBackendTestSuite() );

  // run the tests
  framework::run();

  // create report and exit with exit code
  results_reporter::make_report();
  int result = ( runtime_config::no_result_code()
                      ? boost::exit_success
                      : results_collector.results( framework::master_test_suite().p_id ).result_code() );
  exit(result);

}

/**********************************************************************************************************************/

void DoocsBackendTest::testSimpleCase() {

  BackendFactory::getInstance().setDMapFilePath("dummies.dmap");
  mtca4u::Device device;

  device.open("DoocsServer1");

  //while(true) doocsServerTestHelper::runUpdate();

  ScalarRegisterAccessor<int32_t> acc_someInt(device.getScalarRegisterAccessor<int32_t>("MYDUMMY/SOME_INT"));
  acc_someInt.read();
  BOOST_CHECK( acc_someInt == 42 );

  device.close();

}

/**********************************************************************************************************************/
