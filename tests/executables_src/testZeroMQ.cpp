/*
 * testDoocsBackend.cpp
 *
 *  Created on: Apr 27, 2016
 *      Author: Martin Hierholzer
 */

#include <thread>
#include <random>

#define BOOST_TEST_MODULE testZeroMQ
#include <boost/test/included/unit_test.hpp>

#include <ChimeraTK/Device.h>
#include <ChimeraTK/TransferGroup.h>
#include <doocs-server-test-helper/doocsServerTestHelper.h>

#include <eq_client.h>

using namespace boost::unit_test_framework;
using namespace ChimeraTK;

/**********************************************************************************************************************/

extern int eq_server(int, char **);

struct DoocsLauncher {
    DoocsLauncher() {
      // choose random RPC number
      std::random_device rd;
      std::uniform_int_distribution<int> dist(620000000, 999999999);
      rpc_no = std::to_string(dist(rd));
      // update config file with the RPC number
      std::string command = "sed -i testZeroMQ.conf -e 's/^SVR.RPC_NUMBER:.*$/SVR.RPC_NUMBER: "+rpc_no+"/'";
      auto rc = std::system(command.c_str());
      (void)rc;

      // start the server
      std::thread( eq_server,
                   boost::unit_test::framework::master_test_suite().argc,
                   boost::unit_test::framework::master_test_suite().argv ).detach();
      // set CDDs for the two doocs addresses used in the test
      DoocsServer1 = "(doocs:doocs://localhost:"+rpc_no+"/F/D)";
      DoocsServer2 = "(doocs:doocs://localhost:"+rpc_no+"/F/D/MYDUMMY)";
      // wait until server has started (both the update thread and the rpc thread)
      DoocsServerTestHelper::runUpdate();
      EqCall eq;
      EqAdr ea;
      EqData src,dst;
      ea.adr("doocs://localhost:"+rpc_no+"/F/D/MYDUMMY/SOME_ZMQINT");
      while(eq.get(&ea,&src,&dst)) usleep(100000);
    }

    static void launchIfNotYetLaunched() {
      static DoocsLauncher launcher;
    }

    static std::string rpc_no;
    static std::string DoocsServer1;
    static std::string DoocsServer2;
};
std::string DoocsLauncher::rpc_no;
std::string DoocsLauncher::DoocsServer1;
std::string DoocsLauncher::DoocsServer2;

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE( testZeroMQ ) {
  DoocsLauncher::launchIfNotYetLaunched();

  ChimeraTK::Device device;
  device.open(DoocsLauncher::DoocsServer1);

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
  {
    std::atomic<bool> readFinished(false);
    std::promise<void> prom;
    std::future<void> fut = prom.get_future();
    std::thread readAsync( [&acc, &prom, &readFinished]() {
      acc.read();
      readFinished = true;
      prom.set_value();
    } );
    fut.wait_for(std::chrono::milliseconds(500));
    BOOST_CHECK( readFinished == false );
    DoocsServerTestHelper::runUpdate();
    fut.wait_for(std::chrono::milliseconds(500));
    BOOST_CHECK( readFinished == true );
    readAsync.join();
  }

  // test shutdown procedure
  {
    std::atomic<bool> threadInterrupted(false);
    std::promise<void> prom;
    std::future<void> fut = prom.get_future();
    std::thread readAsync( [&acc, &prom, &threadInterrupted]() {
      try {
        acc.read();
      }
      catch(boost::thread_interrupted&) {
        // should end up here!
        threadInterrupted = true;
        prom.set_value();
      }
    } );
    fut.wait_for(std::chrono::milliseconds(500));
    BOOST_CHECK( threadInterrupted == false );
    acc.getHighLevelImplElement()->interrupt();
    fut.wait_for(std::chrono::milliseconds(500));
    BOOST_CHECK( threadInterrupted == true );
    readAsync.join();
  }

  device.close();
}

/**********************************************************************************************************************/
