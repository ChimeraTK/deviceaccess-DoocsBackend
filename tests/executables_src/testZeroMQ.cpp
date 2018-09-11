/*
 * testDoocsBackend.cpp
 *
 *  Created on: Apr 27, 2016
 *      Author: Martin Hierholzer
 */

#include <thread>

#define BOOST_TEST_MODULE testZeroMQ
#include <boost/test/included/unit_test.hpp>

#include <ChimeraTK/Device.h>
#include <ChimeraTK/TransferGroup.h>
#include <doocs-server-test-helper/doocsServerTestHelper.h>

using namespace boost::unit_test_framework;
using namespace ChimeraTK;

/**********************************************************************************************************************/

extern int eq_server(int, char **);

struct DoocsLauncher {
    DoocsLauncher()
    : doocsServerThread( eq_server,
                         boost::unit_test::framework::master_test_suite().argc,
                         boost::unit_test::framework::master_test_suite().argv )
    {
      doocsServerThread.detach();
    }
    std::thread doocsServerThread;

    static void launchIfNotYetLaunched() {
      static DoocsLauncher launcher;
    }
};

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE( testZeroMQ ) {
  DoocsLauncher::launchIfNotYetLaunched();

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
