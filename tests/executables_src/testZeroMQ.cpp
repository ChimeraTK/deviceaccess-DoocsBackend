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

    void testZeroMQ();
    void testExceptions();
};

/**********************************************************************************************************************/

class DoocsBackendTestSuite : public test_suite {
  public:
    DoocsBackendTestSuite() : test_suite("DoocsBackend test suite") {
      boost::shared_ptr<DoocsBackendTest> doocsBackendTest(new DoocsBackendTest);

      add( BOOST_CLASS_TEST_CASE(&DoocsBackendTest::testZeroMQ, doocsBackendTest) );
      add( BOOST_CLASS_TEST_CASE(&DoocsBackendTest::testExceptions, doocsBackendTest) );
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
  DoocsServerTestHelper::initialise();

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

void DoocsBackendTest::testZeroMQ() {

  BackendFactory::getInstance().setDMapFilePath("dummies.dmap");
  mtca4u::Device device;

  device.open("DoocsServer1");

  ScalarRegisterAccessor<int32_t> acc(device.getScalarRegisterAccessor<int32_t>("MYDUMMY/SOME_ZMQINT", 0,
      {AccessMode::wait_for_new_data}));

  BOOST_CHECK( acc.getNInputQueueElements() == 0 );

  // send updates until the ZMQ interface is initialised (this is done in the background unfortunately)
  while(acc.getNInputQueueElements() == 0) {
    DoocsServerTestHelper::runUpdate();
  }
  // empty the queue
  usleep(100000);
  while(acc.getNInputQueueElements() != 0) {
    acc.read();
  }
  usleep(100000);

  BOOST_CHECK( acc.getNInputQueueElements() == 0 );

  // check a simple read
  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_ZMQINT",1);
  DoocsServerTestHelper::runUpdate();

  usleep(100000);
  BOOST_CHECK( acc.getNInputQueueElements() == 1 );
  acc.read();
  BOOST_CHECK( acc.getNInputQueueElements() == 0 );
  BOOST_CHECK( acc == 2 );

  usleep(100000);

  // test having 3 updates in the queue
  DoocsServerTestHelper::runUpdate();
  DoocsServerTestHelper::runUpdate();
  DoocsServerTestHelper::runUpdate();
  usleep(100000);

  BOOST_CHECK( acc.getNInputQueueElements() == 3 );
  acc.read();
  BOOST_CHECK( acc.getNInputQueueElements() == 2 );
  BOOST_CHECK( acc == 3 );
  acc.read();
  BOOST_CHECK( acc.getNInputQueueElements() == 1 );
  BOOST_CHECK( acc == 4 );
  acc.read();
  BOOST_CHECK( acc.getNInputQueueElements() == 0 );
  BOOST_CHECK( acc == 5 );

  // test if read really blocks when having no update in the queue
  std::atomic<bool> readFinished(false);
  std::promise<void> prom;
  std::future<void> fut = prom.get_future();
  std::thread readAsync( [&acc, &prom, &readFinished]() {
    acc.read();
    prom.set_value();
    readFinished = true;
  } );
  fut.wait_for(std::chrono::milliseconds(500));
  BOOST_CHECK( readFinished == false );
  DoocsServerTestHelper::runUpdate();
  fut.wait_for(std::chrono::milliseconds(500));
  BOOST_CHECK( readFinished == true );
  readAsync.join();

  device.close();
}

/**********************************************************************************************************************/

void DoocsBackendTest::testExceptions() {

  BackendFactory::getInstance().setDMapFilePath("dummies.dmap");
  mtca4u::Device device;

  device.open("DoocsServer1");

  // non-ZeroMQ variable
  ScalarRegisterAccessor<int32_t> acc(device.getScalarRegisterAccessor<int32_t>("MYDUMMY/SOME_INT", 0,
      {AccessMode::wait_for_new_data}));

  usleep(100000);

  try {
    acc.read();
    BOOST_ERROR("Exception expected.");
  }
  catch(DeviceException &ex) {
    BOOST_CHECK(ex.getID() == DeviceException::CANNOT_OPEN_DEVICEBACKEND);
  }

  device.close();

}

/**********************************************************************************************************************/
