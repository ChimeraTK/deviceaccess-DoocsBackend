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
#include <mtca4u/TransferGroup.h>
#include <doocs-server-test-helper/doocsServerTestHelper.h>

using namespace boost::unit_test_framework;
using namespace mtca4u;

/**********************************************************************************************************************/

class DoocsBackendTest {
  public:
    void testRoutine();

    void testScalarInt();
    void testScalarFloat();
    void testScalarDouble();
    void testString();
    void testArrayInt();
    void testArrayShort();
    void testArrayLong();
    void testArrayFloat();
    void testArrayDouble();
    void testBitAndStatus();
    void testPartialAccess();
    void testExceptions();
    void testCatalogue();
    void testOther();
};

/**********************************************************************************************************************/

class DoocsBackendTestSuite : public test_suite {
  public:
    DoocsBackendTestSuite() : test_suite("DoocsBackend test suite") {
      boost::shared_ptr<DoocsBackendTest> doocsBackendTest(new DoocsBackendTest);

      add( BOOST_CLASS_TEST_CASE(&DoocsBackendTest::testScalarInt, doocsBackendTest) );
      add( BOOST_CLASS_TEST_CASE(&DoocsBackendTest::testScalarFloat, doocsBackendTest) );
      add( BOOST_CLASS_TEST_CASE(&DoocsBackendTest::testScalarDouble, doocsBackendTest) );
      add( BOOST_CLASS_TEST_CASE(&DoocsBackendTest::testString, doocsBackendTest) );
      add( BOOST_CLASS_TEST_CASE(&DoocsBackendTest::testArrayInt, doocsBackendTest) );
      add( BOOST_CLASS_TEST_CASE(&DoocsBackendTest::testArrayShort, doocsBackendTest) );
      add( BOOST_CLASS_TEST_CASE(&DoocsBackendTest::testArrayLong, doocsBackendTest) );
      add( BOOST_CLASS_TEST_CASE(&DoocsBackendTest::testArrayFloat, doocsBackendTest) );
      add( BOOST_CLASS_TEST_CASE(&DoocsBackendTest::testArrayDouble, doocsBackendTest) );
      add( BOOST_CLASS_TEST_CASE(&DoocsBackendTest::testBitAndStatus, doocsBackendTest) );
      add( BOOST_CLASS_TEST_CASE(&DoocsBackendTest::testPartialAccess, doocsBackendTest) );
      add( BOOST_CLASS_TEST_CASE(&DoocsBackendTest::testExceptions, doocsBackendTest) );
      add( BOOST_CLASS_TEST_CASE(&DoocsBackendTest::testCatalogue, doocsBackendTest) );
      add( BOOST_CLASS_TEST_CASE(&DoocsBackendTest::testOther, doocsBackendTest) );
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
  
  // run update once to make sure the server is up and running
  DoocsServerTestHelper::runUpdate();

  BackendFactory::getInstance().setDMapFilePath("dummies.dmap");
  mtca4u::Device device;

  device.open("DoocsServer1");

  TwoDRegisterAccessor<int32_t> acc_someInt_as_int(device.getTwoDRegisterAccessor<int32_t>("MYDUMMY/SOME_INT"));
  BOOST_CHECK( acc_someInt_as_int.getNChannels() == 1 );
  BOOST_CHECK( acc_someInt_as_int.getNElementsPerChannel() == 1 );
  acc_someInt_as_int.read();
  BOOST_CHECK( acc_someInt_as_int[0][0] == 42 );

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_INT",120);

  BOOST_CHECK( acc_someInt_as_int[0][0] == 42 );
  acc_someInt_as_int.read();
  BOOST_CHECK( acc_someInt_as_int[0][0] == 120 );

  acc_someInt_as_int[0][0] = 1234;
  BOOST_CHECK( DoocsServerTestHelper::doocsGet<int>("//MYDUMMY/SOME_INT") == 120 );
  acc_someInt_as_int.write();
  BOOST_CHECK( DoocsServerTestHelper::doocsGet<int>("//MYDUMMY/SOME_INT") == 1234 );

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_INT",42);

  TwoDRegisterAccessor<double> acc_someInt_as_double(device.getTwoDRegisterAccessor<double>("MYDUMMY/SOME_INT"));
  BOOST_CHECK( acc_someInt_as_double.getNChannels() == 1 );
  BOOST_CHECK( acc_someInt_as_double.getNElementsPerChannel() == 1 );
  acc_someInt_as_double.read();
  BOOST_CHECK_CLOSE( acc_someInt_as_double[0][0], 42, 0.001 );

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_INT",120);

  BOOST_CHECK_CLOSE( acc_someInt_as_double[0][0], 42, 0.001 );
  acc_someInt_as_double.read();
  BOOST_CHECK_CLOSE( acc_someInt_as_double[0][0], 120, 0.001 );

  acc_someInt_as_double[0][0] = 1234.3;
  BOOST_CHECK( DoocsServerTestHelper::doocsGet<int>("//MYDUMMY/SOME_INT") == 120 );
  acc_someInt_as_double.write();
  BOOST_CHECK( DoocsServerTestHelper::doocsGet<int>("//MYDUMMY/SOME_INT") == 1234 );

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_INT",42);

  TwoDRegisterAccessor<float> acc_someInt_as_float(device.getTwoDRegisterAccessor<float>("MYDUMMY/SOME_INT"));
  BOOST_CHECK( acc_someInt_as_float.getNChannels() == 1 );
  BOOST_CHECK( acc_someInt_as_float.getNElementsPerChannel() == 1 );
  acc_someInt_as_float.read();
  BOOST_CHECK_CLOSE( acc_someInt_as_float[0][0], 42, 0.001 );

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_INT",120);

  BOOST_CHECK_CLOSE( acc_someInt_as_float[0][0], 42, 0.001 );
  acc_someInt_as_float.read();
  BOOST_CHECK_CLOSE( acc_someInt_as_float[0][0], 120, 0.001 );

  acc_someInt_as_float[0][0] = 1233.9;
  BOOST_CHECK( DoocsServerTestHelper::doocsGet<int>("//MYDUMMY/SOME_INT") == 120 );
  acc_someInt_as_float.write();
  BOOST_CHECK( DoocsServerTestHelper::doocsGet<int>("//MYDUMMY/SOME_INT") == 1234 );

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_INT",42);

  TwoDRegisterAccessor<std::string> acc_someInt_as_string(device.getTwoDRegisterAccessor<std::string>("MYDUMMY/SOME_INT"));
  BOOST_CHECK( acc_someInt_as_string.getNChannels() == 1 );
  BOOST_CHECK( acc_someInt_as_string.getNElementsPerChannel() == 1 );
  acc_someInt_as_string.read();
  BOOST_CHECK( acc_someInt_as_string[0][0] == "42" );

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_INT",120);

  BOOST_CHECK( acc_someInt_as_string[0][0] == "42" );
  acc_someInt_as_string.read();
  BOOST_CHECK( acc_someInt_as_string[0][0] == "120" );

  acc_someInt_as_string[0][0] = "1234";
  BOOST_CHECK( DoocsServerTestHelper::doocsGet<int>("//MYDUMMY/SOME_INT") == 120 );
  acc_someInt_as_string.write();
  BOOST_CHECK( DoocsServerTestHelper::doocsGet<int>("//MYDUMMY/SOME_INT") == 1234 );

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

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_FLOAT",123.456);

  BOOST_CHECK_CLOSE( acc_someFloat_as_float[0][0], 3.1415, 0.00001 );
  acc_someFloat_as_float.read();
  BOOST_CHECK_CLOSE( acc_someFloat_as_float[0][0], 123.456, 0.00001 );

  acc_someFloat_as_float[0][0] = 666.333;
  BOOST_CHECK_CLOSE( DoocsServerTestHelper::doocsGet<float>("//MYDUMMY/SOME_FLOAT"), 123.456, 0.00001 );
  acc_someFloat_as_float.write();
  BOOST_CHECK_CLOSE( DoocsServerTestHelper::doocsGet<float>("//MYDUMMY/SOME_FLOAT"), 666.333, 0.00001 );

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_FLOAT",3.1415);

  TwoDRegisterAccessor<double> acc_someFloat_as_double(device.getTwoDRegisterAccessor<double>("MYDUMMY/SOME_FLOAT"));
  BOOST_CHECK( acc_someFloat_as_double.getNChannels() == 1 );
  BOOST_CHECK( acc_someFloat_as_double.getNElementsPerChannel() == 1 );
  acc_someFloat_as_double.read();
  BOOST_CHECK_CLOSE( acc_someFloat_as_double[0][0], 3.1415, 0.00001 );

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_FLOAT",123.456);

  BOOST_CHECK_CLOSE( acc_someFloat_as_double[0][0], 3.1415, 0.00001 );
  acc_someFloat_as_double.read();
  BOOST_CHECK_CLOSE( acc_someFloat_as_double[0][0], 123.456, 0.00001 );

  acc_someFloat_as_double[0][0] = 1234.3;
  BOOST_CHECK_CLOSE( DoocsServerTestHelper::doocsGet<float>("//MYDUMMY/SOME_FLOAT"), 123.456, 0.00001 );
  acc_someFloat_as_double.write();
  BOOST_CHECK_CLOSE( DoocsServerTestHelper::doocsGet<float>("//MYDUMMY/SOME_FLOAT"), 1234.3, 0.00001 );

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_FLOAT",3.1415);

  TwoDRegisterAccessor<int> acc_someFloat_as_int(device.getTwoDRegisterAccessor<int>("MYDUMMY/SOME_FLOAT"));
  BOOST_CHECK( acc_someFloat_as_int.getNChannels() == 1 );
  BOOST_CHECK( acc_someFloat_as_int.getNElementsPerChannel() == 1 );
  acc_someFloat_as_int.read();
  BOOST_CHECK( acc_someFloat_as_int[0][0] == 3 );

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_FLOAT",119.9);

  BOOST_CHECK( acc_someFloat_as_int[0][0] == 3 );
  acc_someFloat_as_int.read();
  BOOST_CHECK( acc_someFloat_as_int[0][0] == 120 );

  acc_someFloat_as_int[0][0] = 1234;
  BOOST_CHECK_CLOSE( DoocsServerTestHelper::doocsGet<float>("//MYDUMMY/SOME_FLOAT"), 119.9, 0.00001 );
  acc_someFloat_as_int.write();
  BOOST_CHECK_CLOSE( DoocsServerTestHelper::doocsGet<float>("//MYDUMMY/SOME_FLOAT"), 1234.0, 0.00001 );

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_FLOAT",3.1415);

  TwoDRegisterAccessor<std::string> acc_someFloat_as_string(device.getTwoDRegisterAccessor<std::string>("MYDUMMY/SOME_FLOAT"));
  BOOST_CHECK( acc_someFloat_as_string.getNChannels() == 1 );
  BOOST_CHECK( acc_someFloat_as_string.getNElementsPerChannel() == 1 );
  acc_someFloat_as_string.read();
  BOOST_CHECK_CLOSE( std::stod(acc_someFloat_as_string[0][0]), 3.1415, 0.00001 );

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_FLOAT",120);

  BOOST_CHECK_CLOSE( std::stod(acc_someFloat_as_string[0][0]), 3.1415, 0.00001 );
  acc_someFloat_as_string.read();
  BOOST_CHECK( acc_someFloat_as_string[0][0] == "120" );

  acc_someFloat_as_string[0][0] = "1234.5678";
  BOOST_CHECK_CLOSE( DoocsServerTestHelper::doocsGet<float>("//MYDUMMY/SOME_FLOAT"), 120, 0.00001 );
  acc_someFloat_as_string.write();
  BOOST_CHECK_CLOSE( DoocsServerTestHelper::doocsGet<float>("//MYDUMMY/SOME_FLOAT"), 1234.5678, 0.00001 );

  device.close();

}

/**********************************************************************************************************************/

void DoocsBackendTest::testScalarDouble() {

  BackendFactory::getInstance().setDMapFilePath("dummies.dmap");
  mtca4u::Device device;

  device.open("DoocsServer1");

  TwoDRegisterAccessor<float> acc_someDouble_as_float(device.getTwoDRegisterAccessor<float>("MYDUMMY/SOME_DOUBLE"));
  BOOST_CHECK( acc_someDouble_as_float.getNChannels() == 1 );
  BOOST_CHECK( acc_someDouble_as_float.getNElementsPerChannel() == 1 );
  acc_someDouble_as_float.read();
  BOOST_CHECK_CLOSE( acc_someDouble_as_float[0][0], 2.8, 0.00001 );

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_DOUBLE",123.456);

  BOOST_CHECK_CLOSE( acc_someDouble_as_float[0][0], 2.8, 0.00001 );
  acc_someDouble_as_float.read();
  BOOST_CHECK_CLOSE( acc_someDouble_as_float[0][0], 123.456, 0.00001 );

  acc_someDouble_as_float[0][0] = 666.333;
  BOOST_CHECK_CLOSE( DoocsServerTestHelper::doocsGet<float>("//MYDUMMY/SOME_DOUBLE"), 123.456, 0.00001 );
  acc_someDouble_as_float.write();
  BOOST_CHECK_CLOSE( DoocsServerTestHelper::doocsGet<float>("//MYDUMMY/SOME_DOUBLE"), 666.333, 0.00001 );

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_DOUBLE",3.1415);

  TwoDRegisterAccessor<double> acc_someDouble_as_double(device.getTwoDRegisterAccessor<double>("MYDUMMY/SOME_DOUBLE"));
  BOOST_CHECK( acc_someDouble_as_double.getNChannels() == 1 );
  BOOST_CHECK( acc_someDouble_as_double.getNElementsPerChannel() == 1 );
  acc_someDouble_as_double.read();
  BOOST_CHECK_CLOSE( acc_someDouble_as_double[0][0], 3.1415, 0.00001 );

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_DOUBLE",123.456);

  BOOST_CHECK_CLOSE( acc_someDouble_as_double[0][0], 3.1415, 0.00001 );
  acc_someDouble_as_double.read();
  BOOST_CHECK_CLOSE( acc_someDouble_as_double[0][0], 123.456, 0.00001 );

  acc_someDouble_as_double[0][0] = 1234.3;
  BOOST_CHECK_CLOSE( DoocsServerTestHelper::doocsGet<float>("//MYDUMMY/SOME_DOUBLE"), 123.456, 0.00001 );
  acc_someDouble_as_double.write();
  BOOST_CHECK_CLOSE( DoocsServerTestHelper::doocsGet<float>("//MYDUMMY/SOME_DOUBLE"), 1234.3, 0.00001 );

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_FLOAT",3.1415);

  TwoDRegisterAccessor<int> acc_someDouble_as_int(device.getTwoDRegisterAccessor<int>("MYDUMMY/SOME_FLOAT"));
  BOOST_CHECK( acc_someDouble_as_int.getNChannels() == 1 );
  BOOST_CHECK( acc_someDouble_as_int.getNElementsPerChannel() == 1 );
  acc_someDouble_as_int.read();
  BOOST_CHECK( acc_someDouble_as_int[0][0] == 3 );

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_FLOAT",119.9);

  BOOST_CHECK( acc_someDouble_as_int[0][0] == 3 );
  acc_someDouble_as_int.read();
  BOOST_CHECK( acc_someDouble_as_int[0][0] == 120 );

  acc_someDouble_as_int[0][0] = 1234;
  BOOST_CHECK_CLOSE( DoocsServerTestHelper::doocsGet<float>("//MYDUMMY/SOME_FLOAT"), 119.9, 0.00001 );
  acc_someDouble_as_int.write();
  BOOST_CHECK_CLOSE( DoocsServerTestHelper::doocsGet<float>("//MYDUMMY/SOME_FLOAT"), 1234.0, 0.00001 );

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_FLOAT",3.1415);

  TwoDRegisterAccessor<std::string> acc_someDouble_as_string(device.getTwoDRegisterAccessor<std::string>("MYDUMMY/SOME_FLOAT"));
  BOOST_CHECK( acc_someDouble_as_string.getNChannels() == 1 );
  BOOST_CHECK( acc_someDouble_as_string.getNElementsPerChannel() == 1 );
  acc_someDouble_as_string.read();
  BOOST_CHECK_CLOSE( std::stod(acc_someDouble_as_string[0][0]), 3.1415, 0.00001 );

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_FLOAT",120);

  BOOST_CHECK_CLOSE( std::stod(acc_someDouble_as_string[0][0]), 3.1415, 0.00001 );
  acc_someDouble_as_string.read();
  BOOST_CHECK( acc_someDouble_as_string[0][0] == "120" );

  acc_someDouble_as_string[0][0] = "1234.5678";
  BOOST_CHECK_CLOSE( DoocsServerTestHelper::doocsGet<float>("//MYDUMMY/SOME_FLOAT"), 120, 0.00001 );
  acc_someDouble_as_string.write();
  BOOST_CHECK_CLOSE( DoocsServerTestHelper::doocsGet<float>("//MYDUMMY/SOME_FLOAT"), 1234.5678, 0.00001 );

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

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_STRING","Something else.");

  BOOST_CHECK(acc_someString[0][0] == "The quick brown fox jumps over the lazy dog.");
  acc_someString.read();
  BOOST_CHECK(acc_someString[0][0] == "Something else.");

  acc_someString[0][0] = "Even different!";
  BOOST_CHECK( DoocsServerTestHelper::doocsGet<string>("//MYDUMMY/SOME_STRING") == "Something else." );
  acc_someString.write();
  BOOST_CHECK( DoocsServerTestHelper::doocsGet<string>("//MYDUMMY/SOME_STRING") == "Even different!" );

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
  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_INT_ARRAY", vals);

  for(int i=0; i<42; i++) BOOST_CHECK(acc_someArray[0][i] == 3*i+120);
  acc_someArray.read();
  for(int i=0; i<42; i++) BOOST_CHECK(acc_someArray[0][i] == -55*i);

  for(int i=0; i<42; i++) acc_someArray[0][i] = i-21;
  vals = DoocsServerTestHelper::doocsGetArray<int>("//MYDUMMY/SOME_INT_ARRAY");
  for(int i=0; i<42; i++) BOOST_CHECK(vals[i] == -55*i);
  acc_someArray.write();
  vals = DoocsServerTestHelper::doocsGetArray<int>("//MYDUMMY/SOME_INT_ARRAY");
  for(int i=0; i<42; i++) BOOST_CHECK(vals[i] == i-21);

  // access via double
  TwoDRegisterAccessor<double> acc_someArrayAsDouble(device.getTwoDRegisterAccessor<double>("MYDUMMY/SOME_INT_ARRAY"));
  acc_someArrayAsDouble.read();
  for(int i=0; i<5; i++) BOOST_CHECK_CLOSE( acc_someArrayAsDouble[0][i], i-21, 0.00001 );
  for(int i=0; i<5; i++) acc_someArrayAsDouble[0][i] = 2.4*i;
  acc_someArrayAsDouble.write();
  vals = DoocsServerTestHelper::doocsGetArray<int>("//MYDUMMY/SOME_INT_ARRAY");
  for(int i=0; i<5; i++) BOOST_CHECK_CLOSE(vals[i], round(2.4*i), 0.00001 );

  // access via string
  TwoDRegisterAccessor<std::string> acc_someArrayAsString(device.getTwoDRegisterAccessor<std::string>("MYDUMMY/SOME_INT_ARRAY"));
  acc_someArrayAsString.read();
  for(int i=0; i<5; i++) BOOST_CHECK_CLOSE( std::stod(acc_someArrayAsString[0][i]),round(2.4*i), 0.00001 );
  for(int i=0; i<5; i++) acc_someArrayAsString[0][i] = std::to_string(3*i);
  acc_someArrayAsString.write();
  vals = DoocsServerTestHelper::doocsGetArray<int>("//MYDUMMY/SOME_INT_ARRAY");
  for(int i=0; i<5; i++) BOOST_CHECK(vals[i] == 3*i);

  device.close();

}

/**********************************************************************************************************************/

void DoocsBackendTest::testArrayShort() {

  BackendFactory::getInstance().setDMapFilePath("dummies.dmap");
  mtca4u::Device device;

  device.open("DoocsServer1");

  TwoDRegisterAccessor<int> acc_someArray(device.getTwoDRegisterAccessor<int>("MYDUMMY/SOME_SHORT_ARRAY"));
  BOOST_CHECK( acc_someArray.getNChannels() == 1 );
  BOOST_CHECK( acc_someArray.getNElementsPerChannel() == 5 );
  acc_someArray.read();
  for(int i=0; i<5; i++) BOOST_CHECK(acc_someArray[0][i] == 10+i);

  std::vector<short> vals(5);
  for(int i=0; i<5; i++) vals[i] = -55*i;
  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_SHORT_ARRAY", vals);

  for(int i=0; i<5; i++) BOOST_CHECK(acc_someArray[0][i] == 10+i);
  acc_someArray.read();
  for(int i=0; i<5; i++) BOOST_CHECK(acc_someArray[0][i] == -55*i);

  std::vector<int> vals2(5);
  for(int i=0; i<5; i++) acc_someArray[0][i] = i-21;
  vals2 = DoocsServerTestHelper::doocsGetArray<int>("//MYDUMMY/SOME_SHORT_ARRAY");
  for(int i=0; i<5; i++) BOOST_CHECK(vals2[i] == -55*i);
  acc_someArray.write();
  vals2 = DoocsServerTestHelper::doocsGetArray<int>("//MYDUMMY/SOME_SHORT_ARRAY");
  for(int i=0; i<5; i++) BOOST_CHECK(vals2[i] == i-21);

  device.close();

}

/**********************************************************************************************************************/

void DoocsBackendTest::testArrayLong() {

  BackendFactory::getInstance().setDMapFilePath("dummies.dmap");
  mtca4u::Device device;

  device.open("DoocsServer1");

  TwoDRegisterAccessor<int> acc_someArray(device.getTwoDRegisterAccessor<int>("MYDUMMY/SOME_LONG_ARRAY"));
  BOOST_CHECK( acc_someArray.getNChannels() == 1 );
  BOOST_CHECK( acc_someArray.getNElementsPerChannel() == 5 );
  acc_someArray.read();
  for(int i=0; i<5; i++) BOOST_CHECK(acc_someArray[0][i] == 10+i);

  std::vector<long long int> vals(5);
  for(int i=0; i<5; i++) vals[i] = -55*i;
  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_LONG_ARRAY", vals);

  for(int i=0; i<5; i++) BOOST_CHECK(acc_someArray[0][i] == 10+i);
  acc_someArray.read();
  for(int i=0; i<5; i++) BOOST_CHECK(acc_someArray[0][i] == -55*i);

  std::vector<int> vals2(5);
  for(int i=0; i<5; i++) acc_someArray[0][i] = i-21;
  vals2 = DoocsServerTestHelper::doocsGetArray<int>("//MYDUMMY/SOME_LONG_ARRAY");
  for(int i=0; i<5; i++) BOOST_CHECK(vals2[i] == -55*i);
  acc_someArray.write();
  vals2 = DoocsServerTestHelper::doocsGetArray<int>("//MYDUMMY/SOME_LONG_ARRAY");
  for(int i=0; i<5; i++) BOOST_CHECK(vals2[i] == i-21);

  device.close();

}

/**********************************************************************************************************************/

void DoocsBackendTest::testArrayFloat() {

  BackendFactory::getInstance().setDMapFilePath("dummies.dmap");
  mtca4u::Device device;

  device.open("DoocsServer1");

  TwoDRegisterAccessor<float> acc_someArray(device.getTwoDRegisterAccessor<float>("MYDUMMY/SOME_FLOAT_ARRAY"));
  BOOST_CHECK( acc_someArray.getNChannels() == 1 );
  BOOST_CHECK( acc_someArray.getNElementsPerChannel() == 5 );
  acc_someArray.read();
  for(int i=0; i<5; i++) BOOST_CHECK_CLOSE(acc_someArray[0][i], i/1000., 0.00001);

  std::vector<float> vals(5);
  for(int i=0; i<5; i++) vals[i] = -3.14159265*i;
  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_FLOAT_ARRAY", vals);

  for(int i=0; i<5; i++) BOOST_CHECK_CLOSE(acc_someArray[0][i], i/1000., 0.00001);
  acc_someArray.read();
  for(int i=0; i<5; i++) BOOST_CHECK_CLOSE(acc_someArray[0][i], -3.14159265*i, 0.00001);

  for(int i=0; i<5; i++) acc_someArray[0][i] = 2./(i+0.01);
  vals = DoocsServerTestHelper::doocsGetArray<float>("//MYDUMMY/SOME_FLOAT_ARRAY");
  for(int i=0; i<5; i++) BOOST_CHECK_CLOSE(vals[i], -3.14159265*i, 0.00001);
  acc_someArray.write();
  vals = DoocsServerTestHelper::doocsGetArray<float>("//MYDUMMY/SOME_FLOAT_ARRAY");
  for(int i=0; i<5; i++) BOOST_CHECK_CLOSE(vals[i], 2./(i+0.01), 0.00001);

  device.close();

}

/**********************************************************************************************************************/

void DoocsBackendTest::testArrayDouble() {

  BackendFactory::getInstance().setDMapFilePath("dummies.dmap");
  mtca4u::Device device;

  device.open("DoocsServer1");

  TwoDRegisterAccessor<double> acc_someArray(device.getTwoDRegisterAccessor<double>("MYDUMMY/SOME_DOUBLE_ARRAY"));
  BOOST_CHECK( acc_someArray.getNChannels() == 1 );
  BOOST_CHECK( acc_someArray.getNElementsPerChannel() == 5 );
  acc_someArray.read();
  for(int i=0; i<5; i++) BOOST_CHECK_CLOSE(acc_someArray[0][i], i/333., 0.00001);

  std::vector<double> vals(5);
  for(int i=0; i<5; i++) vals[i] = -3.14159265*i;
  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_DOUBLE_ARRAY", vals);

  for(int i=0; i<5; i++) BOOST_CHECK_CLOSE(acc_someArray[0][i], i/333., 0.00001);
  acc_someArray.read();
  for(int i=0; i<5; i++) BOOST_CHECK_CLOSE(acc_someArray[0][i], -3.14159265*i, 0.00001);

  for(int i=0; i<5; i++) acc_someArray[0][i] = 2./(i+0.01);
  vals = DoocsServerTestHelper::doocsGetArray<double>("//MYDUMMY/SOME_DOUBLE_ARRAY");
  for(int i=0; i<5; i++) BOOST_CHECK_CLOSE(vals[i], -3.14159265*i, 0.00001);
  acc_someArray.write();
  vals = DoocsServerTestHelper::doocsGetArray<double>("//MYDUMMY/SOME_DOUBLE_ARRAY");
  for(int i=0; i<5; i++) BOOST_CHECK_CLOSE(vals[i], 2./(i+0.01), 0.00001);

  // access via int
  TwoDRegisterAccessor<int> acc_someArrayAsInt(device.getTwoDRegisterAccessor<int>("MYDUMMY/SOME_DOUBLE_ARRAY"));
  acc_someArrayAsInt.read();
  for(int i=0; i<5; i++) BOOST_CHECK( acc_someArrayAsInt[0][i] == round(2./(i+0.01)));
  for(int i=0; i<5; i++) acc_someArrayAsInt[0][i] = 2*i;
  acc_someArrayAsInt.write();
  vals = DoocsServerTestHelper::doocsGetArray<double>("//MYDUMMY/SOME_DOUBLE_ARRAY");
  for(int i=0; i<5; i++) BOOST_CHECK_CLOSE(vals[i], 2*i, 0.00001);

  // access via string
  TwoDRegisterAccessor<std::string> acc_someArrayAsString(device.getTwoDRegisterAccessor<std::string>("MYDUMMY/SOME_DOUBLE_ARRAY"));
  acc_someArrayAsString.read();
  for(int i=0; i<5; i++) BOOST_CHECK_CLOSE( std::stod(acc_someArrayAsString[0][i]), 2*i, 0.00001 );
  for(int i=0; i<5; i++) acc_someArrayAsString[0][i] = std::to_string(2.1*i);
  acc_someArrayAsString.write();
  vals = DoocsServerTestHelper::doocsGetArray<double>("//MYDUMMY/SOME_DOUBLE_ARRAY");
  for(int i=0; i<5; i++) BOOST_CHECK_CLOSE(vals[i], 2.1*i, 0.00001);

  device.close();

}

/**********************************************************************************************************************/

void DoocsBackendTest::testBitAndStatus() {

  BackendFactory::getInstance().setDMapFilePath("dummies.dmap");
  mtca4u::Device device;

  device.open("DoocsServer1");

  TwoDRegisterAccessor<int> acc_someBit(device.getTwoDRegisterAccessor<int>("MYDUMMY/SOME_BIT"));
  TwoDRegisterAccessor<int> acc_someStatus(device.getTwoDRegisterAccessor<int>("MYDUMMY/SOME_STATUS"));
  TwoDRegisterAccessor<uint16_t> acc_someStatusU16(device.getTwoDRegisterAccessor<uint16_t>("MYDUMMY/SOME_STATUS"));

  BOOST_CHECK( acc_someBit.getNChannels() == 1 );
  BOOST_CHECK( acc_someStatus.getNChannels() == 1 );
  BOOST_CHECK( acc_someBit.getNElementsPerChannel() == 1 );
  BOOST_CHECK( acc_someStatus.getNElementsPerChannel() == 1 );

  acc_someBit.read();
  BOOST_CHECK( acc_someBit[0][0] == 1 );
  acc_someStatus.read();
  BOOST_CHECK( acc_someStatus[0][0] == 3 );

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_BIT", 0);

  BOOST_CHECK( acc_someBit[0][0] == 1 );
  acc_someBit.read();
  BOOST_CHECK( acc_someBit[0][0] == 0 );
  BOOST_CHECK( acc_someStatus[0][0] == 3 );
  acc_someStatus.read();
  BOOST_CHECK( acc_someStatus[0][0] == 2 );

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_STATUS", 0xFFFF);

  BOOST_CHECK( acc_someBit[0][0] == 0 );
  acc_someBit.read();
  BOOST_CHECK( acc_someBit[0][0] == 1 );
  BOOST_CHECK( acc_someStatus[0][0] == 2 );
  acc_someStatus.read();
  BOOST_CHECK( acc_someStatus[0][0] == 0xFFFF );

  acc_someStatusU16.read();
  BOOST_CHECK( acc_someStatusU16[0][0] == 0xFFFF );

  acc_someBit[0][0] = 0;
  BOOST_CHECK( DoocsServerTestHelper::doocsGet<int>("//MYDUMMY/SOME_STATUS") == 0xFFFF );
  acc_someBit.write();
  BOOST_CHECK( DoocsServerTestHelper::doocsGet<int>("//MYDUMMY/SOME_STATUS") == 0xFFFE );

  acc_someStatus[0][0] = 123;
  BOOST_CHECK( DoocsServerTestHelper::doocsGet<int>("//MYDUMMY/SOME_STATUS") == 0xFFFE );
  acc_someStatus.write();
  BOOST_CHECK( DoocsServerTestHelper::doocsGet<int>("//MYDUMMY/SOME_STATUS") == 123 );

  device.close();

}
/**********************************************************************************************************************/

void DoocsBackendTest::testPartialAccess() {

  BackendFactory::getInstance().setDMapFilePath("dummies.dmap");
  mtca4u::Device device;

  device.open("DoocsServer1");

  // int array, 20 elements, offset of 1
  {
    OneDRegisterAccessor<int> acc_someArray(device.getOneDRegisterAccessor<int>("MYDUMMY/SOME_INT_ARRAY", 20, 1));
    BOOST_CHECK( acc_someArray.getNElements() == 20 );

    std::vector<int> vals(42);
    for(int i=0; i<42; i++) vals[i] = -55*i;
    DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_INT_ARRAY", vals);

    acc_someArray.read();
    for(int i=0; i<20; i++) BOOST_CHECK( acc_someArray[i] == -55*(i+1) );

    for(int i=0; i<20; i++) acc_someArray[i] = i;
    acc_someArray.write();
    vals = DoocsServerTestHelper::doocsGetArray<int>("//MYDUMMY/SOME_INT_ARRAY");
    for(int i=0; i<42; i++) {
      if(i == 0 || i > 20) {
        BOOST_CHECK(vals[i] == -55*i );
      }
      else {
        BOOST_CHECK(vals[i] == i-1 );
      }
    }
  }

  // int array, 1 elements, offset of 10
  {
    OneDRegisterAccessor<int> acc_someArray(device.getOneDRegisterAccessor<int>("MYDUMMY/SOME_INT_ARRAY", 1, 10));
    BOOST_CHECK( acc_someArray.getNElements() == 1 );

    std::vector<int> vals(42);
    for(int i=0; i<42; i++) vals[i] = 33*i;
    DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_INT_ARRAY", vals);

    acc_someArray.read();
    BOOST_CHECK( acc_someArray[0] == 33*10 );

    acc_someArray[0] = 42;
    acc_someArray.write();
    vals = DoocsServerTestHelper::doocsGetArray<int>("//MYDUMMY/SOME_INT_ARRAY");
    for(int i=0; i<42; i++) {
      if(i == 10) {
        BOOST_CHECK(vals[i] == 42 );
      }
      else {
        BOOST_CHECK(vals[i] == 33*i );
      }
    }
  }

  // double array with float user type, 3 elements, offset of 2
  {
    OneDRegisterAccessor<float> acc_someArray(device.getOneDRegisterAccessor<float>("MYDUMMY/SOME_DOUBLE_ARRAY", 3, 2));
    BOOST_CHECK( acc_someArray.getNElements() == 3 );

    std::vector<double> vals(5);
    for(int i=0; i<5; i++) vals[i] = 3.14*i;
    DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_DOUBLE_ARRAY", vals);

    acc_someArray.read();
    for(int i=0; i<3; i++) BOOST_CHECK_CLOSE( acc_someArray[i], 3.14*(i+2), 0.00001 );

    for(int i=0; i<3; i++) acc_someArray[i] = 1.2-i;
    acc_someArray.write();
    vals = DoocsServerTestHelper::doocsGetArray<double>("//MYDUMMY/SOME_DOUBLE_ARRAY");
    for(int i=0; i<5; i++) {
      if(i > 1) {
        BOOST_CHECK_CLOSE(vals[i], 1.2-(i-2), 0.00001 );
      }
      else {
        BOOST_CHECK_CLOSE(vals[i], 3.14*i, 0.00001 );
      }
    }
  }

  // float array with string user type, 1 elements, offset of 0
  {
    ScalarRegisterAccessor<std::string> acc_someArray(device.getScalarRegisterAccessor<std::string>("MYDUMMY/SOME_FLOAT_ARRAY", 0));

    std::vector<float> vals(5);
    for(int i=0; i<5; i++) vals[i] = 2.82*(i+1);
    DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_FLOAT_ARRAY", vals);

    acc_someArray.read();
    BOOST_CHECK_CLOSE( std::stod(acc_someArray), 2.82, 0.00001 );

    acc_someArray = "-11.111";
    acc_someArray.write();
    vals = DoocsServerTestHelper::doocsGetArray<float>("//MYDUMMY/SOME_FLOAT_ARRAY");
    for(int i=0; i<5; i++) {
      if(i == 0) {
        BOOST_CHECK_CLOSE(vals[i], -11.111, 0.00001 );
      }
      else {
        BOOST_CHECK_CLOSE(vals[i], 2.82*(i+1), 0.00001 );
      }
    }
  }

  device.close();

}

/**********************************************************************************************************************/

void DoocsBackendTest::testExceptions() {

  BackendFactory::getInstance().setDMapFilePath("dummies.dmap");
  mtca4u::Device device;

  device.open("DoocsServer1");

  // unsupported data type
  try {
    device.getTwoDRegisterAccessor<int>("MYDUMMY/SOME_SPECTRUM");
    BOOST_ERROR("Exception expected.");
  }
  catch(DeviceException &ex) {
    BOOST_CHECK(ex.getID() == DeviceException::WRONG_PARAMETER);
  }

  // non-existing DOOCS property
  try {
    device.getTwoDRegisterAccessor<int>("MYDUMMY/NOT_EXISTING");
    BOOST_ERROR("Exception expected.");
  }
  catch(DeviceException &ex) {
    BOOST_CHECK(ex.getID() == DeviceException::CANNOT_OPEN_DEVICEBACKEND);
  }

  // read string with non-string user type
  try {
    device.getTwoDRegisterAccessor<int>("MYDUMMY/SOME_STRING");
    BOOST_ERROR("Exception expected.");
  }
  catch(DeviceException &ex) {
    BOOST_CHECK(ex.getID() == DeviceException::WRONG_PARAMETER);
  }

  // access too many elements (double register)
  try {
    device.getOneDRegisterAccessor<float>("MYDUMMY/SOME_DOUBLE_ARRAY", 10, 1);
    BOOST_ERROR("Exception expected.");
  }
  catch(DeviceException &ex) {
    BOOST_CHECK(ex.getID() == DeviceException::WRONG_PARAMETER);
  }

  // access too many elements (int register)
  try {
    device.getOneDRegisterAccessor<float>("MYDUMMY/SOME_INT_ARRAY", 100, 1);
    BOOST_ERROR("Exception expected.");
  }
  catch(DeviceException &ex) {
    BOOST_CHECK(ex.getID() == DeviceException::WRONG_PARAMETER);
  }

  device.close();

}

/**********************************************************************************************************************/

void DoocsBackendTest::testCatalogue() {

  BackendFactory::getInstance().setDMapFilePath("dummies.dmap");
  mtca4u::Device device;

  device.open("DoocsServer2");

  auto catalogue = device.getRegisterCatalogue();
  
  // check number of registers, but not with the exact number, since DOOCS adds some registers!
  BOOST_CHECK( catalogue.getNumberOfRegisters() > 13);
  
  // check for the presence of known registers
  BOOST_CHECK( catalogue.hasRegister("SOME_INT") );
  BOOST_CHECK( catalogue.hasRegister("SOME_FLOAT") );
  BOOST_CHECK( catalogue.hasRegister("SOME_DOUBLE") );
  BOOST_CHECK( catalogue.hasRegister("SOME_STRING") );
  BOOST_CHECK( catalogue.hasRegister("SOME_STATUS") );
  BOOST_CHECK( catalogue.hasRegister("SOME_BIT") );
  BOOST_CHECK( catalogue.hasRegister("SOME_INT_ARRAY") );
  BOOST_CHECK( catalogue.hasRegister("SOME_SHORT_ARRAY") );
  BOOST_CHECK( catalogue.hasRegister("SOME_LONG_ARRAY") );
  BOOST_CHECK( catalogue.hasRegister("SOME_FLOAT_ARRAY") );
  BOOST_CHECK( catalogue.hasRegister("SOME_DOUBLE_ARRAY") );
  BOOST_CHECK( catalogue.hasRegister("SOME_SPECTRUM") );
  BOOST_CHECK( catalogue.hasRegister("SOME_ZMQINT") );
  
  // check the properties of some registers
  auto r1 = catalogue.getRegister("SOME_INT");
  BOOST_CHECK(r1->getRegisterName() == "SOME_INT");
  BOOST_CHECK(r1->getNumberOfElements() == 1);
  BOOST_CHECK(r1->getNumberOfChannels() == 1);
  BOOST_CHECK(r1->getNumberOfDimensions() == 0);

  auto r2 = catalogue.getRegister("SOME_STRING");
  BOOST_CHECK(r2->getRegisterName() == "SOME_STRING");
  BOOST_CHECK(r2->getNumberOfElements() == 1);
  BOOST_CHECK(r2->getNumberOfChannels() == 1);
  BOOST_CHECK(r2->getNumberOfDimensions() == 0);

  auto r3 = catalogue.getRegister("SOME_INT_ARRAY");
  BOOST_CHECK(r3->getRegisterName() == "SOME_INT_ARRAY");
  BOOST_CHECK(r3->getNumberOfElements() == 42);
  BOOST_CHECK(r3->getNumberOfChannels() == 1);
  BOOST_CHECK(r3->getNumberOfDimensions() == 1);

  auto r4 = catalogue.getRegister("SOME_FLOAT_ARRAY");
  BOOST_CHECK(r4->getRegisterName() == "SOME_FLOAT_ARRAY");
  BOOST_CHECK(r4->getNumberOfElements() == 5);
  BOOST_CHECK(r4->getNumberOfChannels() == 1);
  BOOST_CHECK(r4->getNumberOfDimensions() == 1);

  auto r5 = catalogue.getRegister("SOME_SPECTRUM");
  BOOST_CHECK(r5->getRegisterName() == "SOME_SPECTRUM");
  BOOST_CHECK(r5->getNumberOfElements() == 100);
  BOOST_CHECK(r5->getNumberOfChannels() == 1);
  BOOST_CHECK(r5->getNumberOfDimensions() == 1);

  auto r6 = catalogue.getRegister("SOME_ZMQINT");
  BOOST_CHECK(r6->getRegisterName() == "SOME_ZMQINT");
  BOOST_CHECK(r6->getNumberOfElements() == 1);
  BOOST_CHECK(r6->getNumberOfChannels() == 1);
  BOOST_CHECK(r6->getNumberOfDimensions() == 0);

  device.close();

  // quick check with the other level (location is included in the register name)
  device.open("DoocsServer1");

  auto catalogue2 = device.getRegisterCatalogue();
  
  // check number of registers, but not with the exact number, since DOOCS adds some registers!
  BOOST_CHECK( catalogue2.getNumberOfRegisters() > 13);
  
  // check for the presence of known registers
  BOOST_CHECK( catalogue2.hasRegister("MYDUMMY/SOME_INT") );
  BOOST_CHECK( catalogue2.hasRegister("DUMMY._SVR/SVR.BPN") );
  
  // check the properties of some registers
  auto r7 = catalogue2.getRegister("MYDUMMY/SOME_INT");
  BOOST_CHECK(r7->getRegisterName() == "MYDUMMY/SOME_INT");
  BOOST_CHECK(r7->getNumberOfElements() == 1);
  BOOST_CHECK(r7->getNumberOfChannels() == 1);
  BOOST_CHECK(r7->getNumberOfDimensions() == 0);
  
  auto r8 = catalogue2.getRegister("DUMMY._SVR/SVR.BPN");
  BOOST_CHECK(r8->getRegisterName() == "DUMMY._SVR/SVR.BPN");
  BOOST_CHECK(r8->getNumberOfElements() == 1);
  BOOST_CHECK(r8->getNumberOfChannels() == 1);
  BOOST_CHECK(r8->getNumberOfDimensions() == 0);

  device.close();
  
}

/**********************************************************************************************************************/

void DoocsBackendTest::testOther() {

  BackendFactory::getInstance().setDMapFilePath("dummies.dmap");
  mtca4u::Device device;

  device.open("DoocsServer1");

  // device info string
  BOOST_CHECK( device.readDeviceInfo() == "DOOCS server address: /TEST.DOOCS/LOCALHOST_610498009" );

  // test in TransferGroup
  TwoDRegisterAccessor<int32_t> acc1(device.getTwoDRegisterAccessor<int32_t>("MYDUMMY/SOME_INT"));
  TwoDRegisterAccessor<int32_t> acc2(device.getTwoDRegisterAccessor<int32_t>("MYDUMMY/SOME_INT"));
  TransferGroup group;
  group.addAccessor(acc1);
  group.addAccessor(acc2);

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_INT", 123);
  group.read();
  BOOST_CHECK(acc1[0][0] == 123);
  BOOST_CHECK(acc2[0][0] == 123);
  acc1[0][0] = 42;
  BOOST_CHECK(acc2[0][0] == 42);    // now sharing the buffer
  acc1.write();
  BOOST_CHECK( DoocsServerTestHelper::doocsGet<int>("//MYDUMMY/SOME_INT") == 42 );


  device.close();

}

/**********************************************************************************************************************/
