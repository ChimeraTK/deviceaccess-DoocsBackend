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

static eq_dummy* location{nullptr};

static size_t mpn = 10000;
static size_t seconds = 0;
static size_t microseconds = 0;

BOOST_GLOBAL_FIXTURE(DoocsLauncher);

/**********************************************************************************************************************/

void setRemoteValue(std::string registerName, bool disableZeroMQ = false) {
  ++mpn;
  microseconds += 12345;
  if(microseconds > 999999) {
    microseconds -= 1000000;
    ++seconds;
  }

  location->lock();
  if(registerName == "MYDUMMY/SOME_INT") {
    auto newval = location->prop_someInt.value() + 2;
    location->prop_someInt.set_value(newval);
    location->prop_someInt.set_tmstmp(seconds, microseconds);
    location->prop_someInt.set_mpnum(mpn);
  }
  else if(registerName == "MYDUMMY/SOME_ZMQINT") {
    auto newval = location->prop_someZMQInt.value() + 3;
    location->prop_someZMQInt.set_value(newval);
    location->prop_someZMQInt.set_tmstmp(seconds, microseconds);
    location->prop_someZMQInt.set_mpnum(mpn);

    if(!disableZeroMQ) {
      dmsg_info_t info;
      memset(&info, 0, sizeof(info));
      info.sec = seconds;
      info.usec = microseconds;
      info.ident = mpn;
      location->prop_someZMQInt.send(&info);
      usleep(10000); // FIXME: DOOCS-ZeroMQ seems to lose data when sending too fast...
    }
  }
  location->unlock();
}

/**********************************************************************************************************************/

template<typename UserType>
std::vector<std::vector<UserType>> getRemoteValue(std::string registerName) {
  std::vector<UserType> ret; // always scalar or 1D

  location->lock();
  if(registerName == "MYDUMMY/SOME_INT") {
    ret.push_back(ctk::numericToUserType<UserType>(location->prop_someInt.value()));
  }
  else if(registerName == "MYDUMMY/SOME_ZMQINT") {
    ret.push_back(ctk::numericToUserType<UserType>(location->prop_someZMQInt.value()));
  }
  location->unlock();

  return {ret};
}

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(unifiedBackendTest) {
  location = dynamic_cast<eq_dummy*>(find_device("MYDUMMY"));
  assert(location != nullptr);

  doocs::Timestamp timestamp = get_global_timestamp();
  auto sinceEpoch = timestamp.get_seconds_and_microseconds_since_epoch();
  seconds = sinceEpoch.seconds + 1;

  doocs::zmq_set_subscription_timeout(10); // reduce timout to make test faster

  auto ubt = makeUnifiedBackendTest(
      [](std::string registerName, auto dummy) -> std::vector<std::vector<decltype(dummy)>> {
        return getRemoteValue<decltype(dummy)>(registerName);
      },
      [](std::string registerName) { setRemoteValue(registerName); });

  ubt.addSyncReadTestRegister<int32_t>("MYDUMMY/SOME_INT", false, false);
  ubt.addWriteTestRegister<int32_t>("MYDUMMY/SOME_INT", false, false);
  ubt.addAsyncReadTestRegister<int32_t>("MYDUMMY/SOME_ZMQINT", false, false);

  ubt.forceRuntimeErrorOnRead({{[&] { location->lock(); }, [&] { location->unlock(); }}});
  ubt.forceRuntimeErrorOnWrite({{[&] { location->lock(); }, [&] { location->unlock(); }}});
  ubt.forceAsyncReadInconsistency([&](std::string registerName) { setRemoteValue(registerName, true); });

  ubt.runTests(DoocsLauncher::DoocsServer1);
}

/**********************************************************************************************************************/
