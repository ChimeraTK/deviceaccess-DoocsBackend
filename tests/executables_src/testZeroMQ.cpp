/*
 * testDoocsBackend.cpp
 *
 *  Created on: Apr 27, 2016
 *      Author: Martin Hierholzer
 */

#include <thread>

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_NO_MAIN      // main function is define in DOOCS
#include <boost/test/included/unit_test.hpp>

#include <ChimeraTK/Device.h>
#include <ChimeraTK/TransferGroup.h>
#include <doocs-server-test-helper/doocsServerTestHelper.h>

using namespace boost::unit_test_framework;
using namespace ChimeraTK;

/**********************************************************************************************************************/

class DoocsBackendTest {
  public:
    void testRoutine();

    void testZeroMQ();
};

/**********************************************************************************************************************/

class DoocsBackendTestSuite : public test_suite {
  public:
    DoocsBackendTestSuite() : test_suite("DoocsBackend test suite") {
      boost::shared_ptr<DoocsBackendTest> doocsBackendTest(new DoocsBackendTest);

      add( BOOST_CLASS_TEST_CASE(&DoocsBackendTest::testZeroMQ, doocsBackendTest) );
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

void DoocsBackendTest::testRoutine() {     // version to run the unit and integration tests

  // run update once to make sure the server is up and running
  DoocsServerTestHelper::runUpdate();

  // initialise BOOST test suite
  extern char **svr_argv;
  extern int svr_argc;
  framework::init([]{return true;},svr_argc,svr_argv);
  framework::master_test_suite().p_name.value = "DoocsBackend test suite";
  framework::master_test_suite().add( new DoocsBackendTestSuite() );

  // run the tests
  framework::run();

  // create report and exit with exit code. Note this ignores the runtime configuration "--result_code" as it always
  // sets the result code from the test result. The interface for determining the runtime configuration changed in
  // newer boost versions, which makes it difficult to obey it at this point.
  results_reporter::make_report();
  int result = results_collector.results( framework::master_test_suite().p_id ).result_code();
  exit(result);

}

/**********************************************************************************************************************/

void DoocsBackendTest::testZeroMQ() {

  BackendFactory::getInstance().setDMapFilePath("dummies.dmap");
  ChimeraTK::Device device;

  device.open("DoocsServer1");
  ScalarRegisterAccessor<int32_t> acc(device.getScalarRegisterAccessor<int32_t>("MYDUMMY/SOME_ZMQINT", 0,
      {AccessMode::wait_for_new_data}));

  BOOST_CHECK( acc.readNonBlocking() == false );

  // send updates until the ZMQ interface is initialised (this is done in the background unfortunately)
  while(acc.readNonBlocking() == false) {
    DoocsServerTestHelper::runUpdate();
  }
  // empty the queue
  usleep(100000);
  acc.readLatest();
  usleep(100000);

  BOOST_CHECK( acc.readNonBlocking() == false );

  // check a simple read
  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_ZMQINT",1);
  DoocsServerTestHelper::runUpdate();

  usleep(100000);
  BOOST_CHECK( acc.readNonBlocking() == true );
  BOOST_CHECK( acc == 2 );
  BOOST_CHECK( acc.readNonBlocking() == false );

  usleep(100000);

  // test having 3 updates in the queue
  DoocsServerTestHelper::runUpdate();
  DoocsServerTestHelper::runUpdate();
  DoocsServerTestHelper::runUpdate();
  usleep(100000);

  acc.read();
  BOOST_CHECK( acc == 3 );
  BOOST_CHECK( acc.readNonBlocking() == true );
  BOOST_CHECK( acc == 4 );
  acc.read();
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
