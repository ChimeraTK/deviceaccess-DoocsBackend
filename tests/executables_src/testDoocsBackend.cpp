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

    void testScalarInt();
};

/**********************************************************************************************************************/

class DoocsBackendTestSuite : public test_suite {
  public:
    DoocsBackendTestSuite() : test_suite("DoocsBackend test suite") {
      boost::shared_ptr<DoocsBackendTest> doocsBackendTest(new DoocsBackendTest);

      add( BOOST_CLASS_TEST_CASE(&DoocsBackendTest::testScalarInt, doocsBackendTest) );
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

void DoocsBackendTest::testScalarInt() {

  BackendFactory::getInstance().setDMapFilePath("dummies.dmap");
  mtca4u::Device device;

  device.open("DoocsServer1");

  TwoDRegisterAccessor<int32_t> acc_someInt_as_int(device.getTwoDRegisterAccessor<int32_t>("MYDUMMY/SOME_INT"));
  BOOST_CHECK( acc_someInt_as_int.getNChannels() == 1 );
  BOOST_CHECK( acc_someInt_as_int.getNElementsPerChannel() == 1 );
  acc_someInt_as_int.read();
  BOOST_CHECK( acc_someInt_as_int[0][0] == 42 );

  doocsServerTestHelper::doocsSet("//MYDUMMY/SOME_INT",120);

  BOOST_CHECK( acc_someInt_as_int[0][0] == 42 );
  acc_someInt_as_int.read();
  BOOST_CHECK( acc_someInt_as_int[0][0] == 120 );

  acc_someInt_as_int[0][0] = 1234;
  BOOST_CHECK( doocsServerTestHelper::doocsGet_int("//MYDUMMY/SOME_INT") == 120 );
  acc_someInt_as_int.write();
  BOOST_CHECK( doocsServerTestHelper::doocsGet_int("//MYDUMMY/SOME_INT") == 1234 );

  doocsServerTestHelper::doocsSet("//MYDUMMY/SOME_INT",42);

  TwoDRegisterAccessor<double> acc_someInt_as_double(device.getTwoDRegisterAccessor<double>("MYDUMMY/SOME_INT"));
  BOOST_CHECK( acc_someInt_as_double.getNChannels() == 1 );
  BOOST_CHECK( acc_someInt_as_double.getNElementsPerChannel() == 1 );
  acc_someInt_as_double.read();
  BOOST_CHECK_CLOSE( acc_someInt_as_double[0][0], 42, 0.001 );

  doocsServerTestHelper::doocsSet("//MYDUMMY/SOME_INT",120);

  BOOST_CHECK_CLOSE( acc_someInt_as_double[0][0], 42, 0.001 );
  acc_someInt_as_double.read();
  BOOST_CHECK_CLOSE( acc_someInt_as_double[0][0], 120, 0.001 );

  acc_someInt_as_double[0][0] = 1234.3;
  BOOST_CHECK( doocsServerTestHelper::doocsGet_int("//MYDUMMY/SOME_INT") == 120 );
  acc_someInt_as_double.write();
  BOOST_CHECK( doocsServerTestHelper::doocsGet_int("//MYDUMMY/SOME_INT") == 1234 );

  doocsServerTestHelper::doocsSet("//MYDUMMY/SOME_INT",42);

  TwoDRegisterAccessor<float> acc_someInt_as_float(device.getTwoDRegisterAccessor<float>("MYDUMMY/SOME_INT"));
  BOOST_CHECK( acc_someInt_as_float.getNChannels() == 1 );
  BOOST_CHECK( acc_someInt_as_float.getNElementsPerChannel() == 1 );
  acc_someInt_as_float.read();
  BOOST_CHECK_CLOSE( acc_someInt_as_float[0][0], 42, 0.001 );

  doocsServerTestHelper::doocsSet("//MYDUMMY/SOME_INT",120);

  BOOST_CHECK_CLOSE( acc_someInt_as_float[0][0], 42, 0.001 );
  acc_someInt_as_float.read();
  BOOST_CHECK_CLOSE( acc_someInt_as_float[0][0], 120, 0.001 );

  acc_someInt_as_float[0][0] = 1233.9;
  BOOST_CHECK( doocsServerTestHelper::doocsGet_int("//MYDUMMY/SOME_INT") == 120 );
  acc_someInt_as_float.write();
  BOOST_CHECK( doocsServerTestHelper::doocsGet_int("//MYDUMMY/SOME_INT") == 1234 );

  doocsServerTestHelper::doocsSet("//MYDUMMY/SOME_INT",42);

  TwoDRegisterAccessor<std::string> acc_someInt_as_string(device.getTwoDRegisterAccessor<std::string>("MYDUMMY/SOME_INT"));
  BOOST_CHECK( acc_someInt_as_string.getNChannels() == 1 );
  BOOST_CHECK( acc_someInt_as_string.getNElementsPerChannel() == 1 );
  acc_someInt_as_string.read();
  BOOST_CHECK( acc_someInt_as_string[0][0] == "42" );

  doocsServerTestHelper::doocsSet("//MYDUMMY/SOME_INT",120);

  BOOST_CHECK( acc_someInt_as_string[0][0] == "42" );
  acc_someInt_as_string.read();
  BOOST_CHECK( acc_someInt_as_string[0][0] == "120" );

  acc_someInt_as_string[0][0] = "1234";
  BOOST_CHECK( doocsServerTestHelper::doocsGet_int("//MYDUMMY/SOME_INT") == 120 );
  acc_someInt_as_string.write();
  BOOST_CHECK( doocsServerTestHelper::doocsGet_int("//MYDUMMY/SOME_INT") == 1234 );

  device.close();

}

/**********************************************************************************************************************/
