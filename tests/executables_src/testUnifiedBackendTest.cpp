/*
 * testUnifiedBackendTest.cpp
 *
 *  Created on: Jun 03, 2020
 *      Author: Martin Hierholzer
 */

#include <cstdlib>
#include <unistd.h>
#include <random>
#include <thread>
#include <iostream>

#define BOOST_TEST_MODULE testUnifiedBackendTest
#include <boost/test/included/unit_test.hpp>
#include <boost/filesystem.hpp>

#include <ChimeraTK/Device.h>
#include <ChimeraTK/TransferGroup.h>
#include <ChimeraTK/UnifiedBackendTest.h>
#include <doocs-server-test-helper/doocsServerTestHelper.h>
#include <doocs-server-test-helper/ThreadedDoocsServer.h>
#include <fstream>

#include <eq_client.h>

#include "eq_dummy.h"

using namespace boost::unit_test_framework;
using namespace ChimeraTK;

/**********************************************************************************************************************/

class DoocsLauncher : public ThreadedDoocsServer {
 public:
  DoocsLauncher()
  : ThreadedDoocsServer("testUnifiedBackendTest.conf", boost::unit_test::framework::master_test_suite().argc,
        boost::unit_test::framework::master_test_suite().argv) {
    // set CDDs for the two doocs addresses used in the test
    rpc_no = rpcNo();
    DoocsServer1 = "(doocs:doocs://localhost:" + rpcNo() + "/F/D)";
    DoocsServer1_cached = "(doocs:doocs://localhost:" + rpcNo() + "/F/D?cacheFile=" + cacheFile1 + ")";
    DoocsServer2 = "(doocs:doocs://localhost:" + rpcNo() + "/F/D/MYDUMMY)";
    DoocsServer2_cached = "(doocs:doocs://localhost:" + rpcNo() + "/F/D/MYDUMMY?cacheFile=" + cacheFile2 + ")";

    // wait until server has started (both the update thread and the rpc thread)
    EqCall eq;
    EqAdr ea;
    EqData src, dst;
    ea.adr("doocs://localhost:" + rpcNo() + "/F/D/MYDUMMY/SOME_ZMQINT");
    while(eq.get(&ea, &src, &dst)) usleep(100000);
  }

  static std::string rpc_no;
  static std::string DoocsServer1;
  static std::string DoocsServer1_cached;
  static std::string cacheFile1;

  static std::string DoocsServer2;
  static std::string DoocsServer2_cached;
  static std::string cacheFile2;
};

std::string DoocsLauncher::rpc_no;
std::string DoocsLauncher::DoocsServer1;
std::string DoocsLauncher::DoocsServer1_cached;
std::string DoocsLauncher::DoocsServer2;
std::string DoocsLauncher::DoocsServer2_cached;
std::string DoocsLauncher::cacheFile1{"cache1.xml"};
std::string DoocsLauncher::cacheFile2{"cache2.xml"};

BOOST_GLOBAL_FIXTURE(DoocsLauncher);

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(unifiedBackendTest) {
  auto location = find_device("MYDUMMY");
  assert(location != nullptr);

  UnifiedBackendTest ubt;

  ubt.setSyncReadTestRegisters<int32_t>({"MYDUMMY/SOME_INT"});
  ubt.setWriteTestRegisters<int32_t>({"MYDUMMY/SOME_INT"});
  ubt.setAsyncReadTestRegisters<int32_t>({"MYDUMMY/SOME_ZMQINT"});

  ubt.forceRuntimeErrorOnRead({{[&] { location->lock(); }, [&] { location->unlock(); }}});
  ubt.forceRuntimeErrorOnWrite({{[&] { location->lock(); }, [&] { location->unlock(); }}});

  ubt.runTests(DoocsLauncher::DoocsServer1);
}

/**********************************************************************************************************************/
