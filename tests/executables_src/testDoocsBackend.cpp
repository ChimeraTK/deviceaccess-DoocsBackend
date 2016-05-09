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
    void testScalarFloat();
    void testString();
    void testArrayInt();
};

/**********************************************************************************************************************/

class DoocsBackendTestSuite : public test_suite {
  public:
    DoocsBackendTestSuite() : test_suite("DoocsBackend test suite") {
      boost::shared_ptr<DoocsBackendTest> doocsBackendTest(new DoocsBackendTest);

      add( BOOST_CLASS_TEST_CASE(&DoocsBackendTest::testScalarInt, doocsBackendTest) );
      add( BOOST_CLASS_TEST_CASE(&DoocsBackendTest::testScalarFloat, doocsBackendTest) );
      add( BOOST_CLASS_TEST_CASE(&DoocsBackendTest::testString, doocsBackendTest) );
      add( BOOST_CLASS_TEST_CASE(&DoocsBackendTest::testArrayInt, doocsBackendTest) );
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

void DoocsBackendTest::testScalarFloat() {

  BackendFactory::getInstance().setDMapFilePath("dummies.dmap");
  mtca4u::Device device;

  device.open("DoocsServer1");

  TwoDRegisterAccessor<float> acc_someFloat_as_float(device.getTwoDRegisterAccessor<float>("MYDUMMY/SOME_FLOAT"));
  BOOST_CHECK( acc_someFloat_as_float.getNChannels() == 1 );
  BOOST_CHECK( acc_someFloat_as_float.getNElementsPerChannel() == 1 );
  acc_someFloat_as_float.read();
  BOOST_CHECK_CLOSE( acc_someFloat_as_float[0][0], 3.1415, 0.00001 );

  doocsServerTestHelper::doocsSet("//MYDUMMY/SOME_FLOAT",123.456);

  BOOST_CHECK_CLOSE( acc_someFloat_as_float[0][0], 3.1415, 0.00001 );
  acc_someFloat_as_float.read();
  BOOST_CHECK_CLOSE( acc_someFloat_as_float[0][0], 123.456, 0.00001 );

  acc_someFloat_as_float[0][0] = 666.333;
  BOOST_CHECK_CLOSE( doocsServerTestHelper::doocsGet_float("//MYDUMMY/SOME_FLOAT"), 123.456, 0.00001 );
  acc_someFloat_as_float.write();
  BOOST_CHECK_CLOSE( doocsServerTestHelper::doocsGet_float("//MYDUMMY/SOME_FLOAT"), 666.333, 0.00001 );

  doocsServerTestHelper::doocsSet("//MYDUMMY/SOME_FLOAT",3.1415);

  TwoDRegisterAccessor<double> acc_someFloat_as_double(device.getTwoDRegisterAccessor<double>("MYDUMMY/SOME_FLOAT"));
  BOOST_CHECK( acc_someFloat_as_double.getNChannels() == 1 );
  BOOST_CHECK( acc_someFloat_as_double.getNElementsPerChannel() == 1 );
  acc_someFloat_as_double.read();
  BOOST_CHECK_CLOSE( acc_someFloat_as_double[0][0], 3.1415, 0.00001 );

  doocsServerTestHelper::doocsSet("//MYDUMMY/SOME_FLOAT",123.456);

  BOOST_CHECK_CLOSE( acc_someFloat_as_double[0][0], 3.1415, 0.00001 );
  acc_someFloat_as_double.read();
  BOOST_CHECK_CLOSE( acc_someFloat_as_double[0][0], 123.456, 0.00001 );

  acc_someFloat_as_double[0][0] = 1234.3;
  BOOST_CHECK_CLOSE( doocsServerTestHelper::doocsGet_float("//MYDUMMY/SOME_FLOAT"), 123.456, 0.00001 );
  acc_someFloat_as_double.write();
  BOOST_CHECK_CLOSE( doocsServerTestHelper::doocsGet_float("//MYDUMMY/SOME_FLOAT"), 1234.3, 0.00001 );

  doocsServerTestHelper::doocsSet("//MYDUMMY/SOME_FLOAT",3.1415);

  TwoDRegisterAccessor<int> acc_someFloat_as_int(device.getTwoDRegisterAccessor<int>("MYDUMMY/SOME_FLOAT"));
  BOOST_CHECK( acc_someFloat_as_int.getNChannels() == 1 );
  BOOST_CHECK( acc_someFloat_as_int.getNElementsPerChannel() == 1 );
  acc_someFloat_as_int.read();
  BOOST_CHECK( acc_someFloat_as_int[0][0] == 3 );

  doocsServerTestHelper::doocsSet("//MYDUMMY/SOME_FLOAT",119.9);

  BOOST_CHECK( acc_someFloat_as_int[0][0] == 3 );
  acc_someFloat_as_int.read();
  BOOST_CHECK( acc_someFloat_as_int[0][0] == 120 );

  acc_someFloat_as_int[0][0] = 1234;
  BOOST_CHECK_CLOSE( doocsServerTestHelper::doocsGet_float("//MYDUMMY/SOME_FLOAT"), 119.9, 0.00001 );
  acc_someFloat_as_int.write();
  BOOST_CHECK_CLOSE( doocsServerTestHelper::doocsGet_float("//MYDUMMY/SOME_FLOAT"), 1234.0, 0.00001 );

  doocsServerTestHelper::doocsSet("//MYDUMMY/SOME_FLOAT",3.1415);

  TwoDRegisterAccessor<std::string> acc_someFloat_as_string(device.getTwoDRegisterAccessor<std::string>("MYDUMMY/SOME_FLOAT"));
  BOOST_CHECK( acc_someFloat_as_string.getNChannels() == 1 );
  BOOST_CHECK( acc_someFloat_as_string.getNElementsPerChannel() == 1 );
  acc_someFloat_as_string.read();
  BOOST_CHECK_CLOSE( std::stod(acc_someFloat_as_string[0][0]), 3.1415, 0.00001 );

  doocsServerTestHelper::doocsSet("//MYDUMMY/SOME_FLOAT",120);

  BOOST_CHECK_CLOSE( std::stod(acc_someFloat_as_string[0][0]), 3.1415, 0.00001 );
  acc_someFloat_as_string.read();
  BOOST_CHECK( acc_someFloat_as_string[0][0] == "120" );

  acc_someFloat_as_string[0][0] = "1234.5678";
  BOOST_CHECK_CLOSE( doocsServerTestHelper::doocsGet_float("//MYDUMMY/SOME_FLOAT"), 120, 0.00001 );
  acc_someFloat_as_string.write();
  BOOST_CHECK_CLOSE( doocsServerTestHelper::doocsGet_float("//MYDUMMY/SOME_FLOAT"), 1234.5678, 0.00001 );

  device.close();

}

/**********************************************************************************************************************/

void DoocsBackendTest::testString() {

  BackendFactory::getInstance().setDMapFilePath("dummies.dmap");
  mtca4u::Device device;

  device.open("DoocsServer1");

  TwoDRegisterAccessor<std::string> acc_someString(device.getTwoDRegisterAccessor<std::string>("MYDUMMY/SOME_STRING"));
  BOOST_CHECK( acc_someString.getNChannels() == 1 );
  BOOST_CHECK( acc_someString.getNElementsPerChannel() == 1 );
  acc_someString.read();
  BOOST_CHECK(acc_someString[0][0] == "The quick brown fox jumps over the lazy dog.");

  doocsServerTestHelper::doocsSet("//MYDUMMY/SOME_STRING","Something else.");

  BOOST_CHECK(acc_someString[0][0] == "The quick brown fox jumps over the lazy dog.");
  acc_someString.read();
  BOOST_CHECK(acc_someString[0][0] == "Something else.");

  acc_someString[0][0] = "Even different!";
  BOOST_CHECK( doocsServerTestHelper::doocsGet_string("//MYDUMMY/SOME_STRING") == "Something else." );
  acc_someString.write();
  BOOST_CHECK( doocsServerTestHelper::doocsGet_string("//MYDUMMY/SOME_STRING") == "Even different!" );

  device.close();

}

/**********************************************************************************************************************/

void DoocsBackendTest::testArrayInt() {

  BackendFactory::getInstance().setDMapFilePath("dummies.dmap");
  mtca4u::Device device;

  device.open("DoocsServer1");

  TwoDRegisterAccessor<int> acc_someArray(device.getTwoDRegisterAccessor<int>("MYDUMMY/SOME_INT_ARRAY"));
  BOOST_CHECK( acc_someArray.getNChannels() == 1 );
  BOOST_CHECK( acc_someArray.getNElementsPerChannel() == 42 );
  acc_someArray.read();
  for(int i=0; i<42; i++) BOOST_CHECK(acc_someArray[0][i] == 3*i+120);

  std::vector<int> vals(42);
  for(int i=0; i<42; i++) vals[i] = -55*i;
  doocsServerTestHelper::doocsSet("//MYDUMMY/SOME_INT_ARRAY", vals);

  for(int i=0; i<42; i++) BOOST_CHECK(acc_someArray[0][i] == 3*i+120);
  acc_someArray.read();
  for(int i=0; i<42; i++) BOOST_CHECK(acc_someArray[0][i] == -55*i);

  for(int i=0; i<42; i++) acc_someArray[0][i] = i-21;
  vals = doocsServerTestHelper::doocsGet_intArray("//MYDUMMY/SOME_INT_ARRAY");
  for(int i=0; i<42; i++) vals[i] = -55*i;
  acc_someArray.write();
  vals = doocsServerTestHelper::doocsGet_intArray("//MYDUMMY/SOME_INT_ARRAY");
  for(int i=0; i<42; i++) vals[i] = i-21;

  device.close();

}

/**********************************************************************************************************************/
