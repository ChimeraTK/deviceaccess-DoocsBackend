/*
 * testDoocsBackend.cpp
 *
 *  Created on: Apr 27, 2016
 *      Author: Martin Hierholzer
 */

#include <cstdlib>
#include <unistd.h>
#include <random>
#include <thread>
#include <iostream>

#define BOOST_TEST_MODULE testDoocsBackend
#include <boost/test/included/unit_test.hpp>
#include <boost/filesystem.hpp>

#include <ChimeraTK/Device.h>
#include <ChimeraTK/TransferGroup.h>
#include <doocs-server-test-helper/doocsServerTestHelper.h>
#include <doocs-server-test-helper/ThreadedDoocsServer.h>
#include <fstream>

#include <eq_client.h>

#include "eq_dummy.h"

using namespace boost::unit_test_framework;
using namespace ChimeraTK;

/**********************************************************************************************************************/

static bool file_exists(const std::string& name);
static void createCacheFileFromCdd(const std::string& cdd);
static void deleteFile(const std::string& filename);

class DoocsLauncher : public ThreadedDoocsServer {
 public:
  DoocsLauncher() :
    ThreadedDoocsServer("testDoocsBackend.conf", boost::unit_test::framework::master_test_suite().argc,
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

BOOST_AUTO_TEST_CASE(testScalarInt) {
  ChimeraTK::Device device;

  BOOST_CHECK(device.isOpened() == false);
  device.open(DoocsLauncher::DoocsServer1);
  BOOST_CHECK(device.isOpened() == true);

  TwoDRegisterAccessor<int32_t> acc_someInt_as_int(device.getTwoDRegisterAccessor<int32_t>("MYDUMMY/SOME_INT"));
  BOOST_CHECK(acc_someInt_as_int.getNChannels() == 1);
  BOOST_CHECK(acc_someInt_as_int.getNElementsPerChannel() == 1);
  acc_someInt_as_int.read();
  BOOST_CHECK(acc_someInt_as_int[0][0] == 42);

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_INT", 120);

  BOOST_CHECK(acc_someInt_as_int[0][0] == 42);
  auto oldVersion = acc_someInt_as_int.getVersionNumber();
  acc_someInt_as_int.read();
  BOOST_CHECK(acc_someInt_as_int[0][0] == 120);
  BOOST_CHECK(acc_someInt_as_int.getVersionNumber() > oldVersion);

  acc_someInt_as_int[0][0] = 1234;
  BOOST_CHECK(DoocsServerTestHelper::doocsGet<int>("//MYDUMMY/SOME_INT") == 120);
  VersionNumber nextVersion;
  acc_someInt_as_int.write(nextVersion);
  BOOST_CHECK(DoocsServerTestHelper::doocsGet<int>("//MYDUMMY/SOME_INT") == 1234);
  BOOST_CHECK(acc_someInt_as_int.getVersionNumber() == nextVersion);

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_INT", 42);

  TwoDRegisterAccessor<double> acc_someInt_as_double(device.getTwoDRegisterAccessor<double>("MYDUMMY/SOME_INT"));
  BOOST_CHECK(acc_someInt_as_double.getNChannels() == 1);
  BOOST_CHECK(acc_someInt_as_double.getNElementsPerChannel() == 1);
  acc_someInt_as_double.read();
  BOOST_CHECK_CLOSE(acc_someInt_as_double[0][0], 42, 0.001);

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_INT", 120);

  BOOST_CHECK_CLOSE(acc_someInt_as_double[0][0], 42, 0.001);
  acc_someInt_as_double.read();
  BOOST_CHECK_CLOSE(acc_someInt_as_double[0][0], 120, 0.001);

  acc_someInt_as_double[0][0] = 1234.3;
  BOOST_CHECK(DoocsServerTestHelper::doocsGet<int>("//MYDUMMY/SOME_INT") == 120);
  acc_someInt_as_double.write();
  BOOST_CHECK(DoocsServerTestHelper::doocsGet<int>("//MYDUMMY/SOME_INT") == 1234);

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_INT", 42);

  TwoDRegisterAccessor<float> acc_someInt_as_float(device.getTwoDRegisterAccessor<float>("MYDUMMY/SOME_INT"));
  BOOST_CHECK(acc_someInt_as_float.getNChannels() == 1);
  BOOST_CHECK(acc_someInt_as_float.getNElementsPerChannel() == 1);
  acc_someInt_as_float.read();
  BOOST_CHECK_CLOSE(acc_someInt_as_float[0][0], 42, 0.001);

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_INT", 120);

  BOOST_CHECK_CLOSE(acc_someInt_as_float[0][0], 42, 0.001);
  acc_someInt_as_float.read();
  BOOST_CHECK_CLOSE(acc_someInt_as_float[0][0], 120, 0.001);

  acc_someInt_as_float[0][0] = 1233.9;
  BOOST_CHECK(DoocsServerTestHelper::doocsGet<int>("//MYDUMMY/SOME_INT") == 120);
  acc_someInt_as_float.write();
  BOOST_CHECK(DoocsServerTestHelper::doocsGet<int>("//MYDUMMY/SOME_INT") == 1234);

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_INT", 42);

  TwoDRegisterAccessor<std::string> acc_someInt_as_string(
      device.getTwoDRegisterAccessor<std::string>("MYDUMMY/SOME_INT"));
  BOOST_CHECK(acc_someInt_as_string.getNChannels() == 1);
  BOOST_CHECK(acc_someInt_as_string.getNElementsPerChannel() == 1);
  acc_someInt_as_string.read();
  BOOST_CHECK(acc_someInt_as_string[0][0] == "42");

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_INT", 120);

  BOOST_CHECK(acc_someInt_as_string[0][0] == "42");
  acc_someInt_as_string.read();
  BOOST_CHECK(acc_someInt_as_string[0][0] == "120");

  acc_someInt_as_string[0][0] = "1234";
  BOOST_CHECK(DoocsServerTestHelper::doocsGet<int>("//MYDUMMY/SOME_INT") == 120);
  acc_someInt_as_string.write();
  BOOST_CHECK(DoocsServerTestHelper::doocsGet<int>("//MYDUMMY/SOME_INT") == 1234);

  device.close();
  BOOST_CHECK(device.isOpened() == false);
}

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testScalarFloat) {
  ChimeraTK::Device device;
  device.open(DoocsLauncher::DoocsServer1);

  TwoDRegisterAccessor<float> acc_someFloat_as_float(device.getTwoDRegisterAccessor<float>("MYDUMMY/SOME_FLOAT"));
  BOOST_CHECK(acc_someFloat_as_float.getNChannels() == 1);
  BOOST_CHECK(acc_someFloat_as_float.getNElementsPerChannel() == 1);
  acc_someFloat_as_float.read();
  BOOST_CHECK_CLOSE(acc_someFloat_as_float[0][0], 3.1415, 0.00001);

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_FLOAT", 123.456);

  BOOST_CHECK_CLOSE(acc_someFloat_as_float[0][0], 3.1415, 0.00001);
  auto oldVersion = acc_someFloat_as_float.getVersionNumber();
  acc_someFloat_as_float.read();
  BOOST_CHECK_CLOSE(acc_someFloat_as_float[0][0], 123.456, 0.00001);
  BOOST_CHECK(acc_someFloat_as_float.getVersionNumber() > oldVersion);

  acc_someFloat_as_float[0][0] = 666.333;
  BOOST_CHECK_CLOSE(DoocsServerTestHelper::doocsGet<float>("//MYDUMMY/SOME_FLOAT"), 123.456, 0.00001);
  VersionNumber nextVersion;
  acc_someFloat_as_float.write(nextVersion);
  BOOST_CHECK_CLOSE(DoocsServerTestHelper::doocsGet<float>("//MYDUMMY/SOME_FLOAT"), 666.333, 0.00001);
  BOOST_CHECK(acc_someFloat_as_float.getVersionNumber() == nextVersion);

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_FLOAT", 3.1415);

  TwoDRegisterAccessor<double> acc_someFloat_as_double(device.getTwoDRegisterAccessor<double>("MYDUMMY/SOME_FLOAT"));
  BOOST_CHECK(acc_someFloat_as_double.getNChannels() == 1);
  BOOST_CHECK(acc_someFloat_as_double.getNElementsPerChannel() == 1);
  acc_someFloat_as_double.read();
  BOOST_CHECK_CLOSE(acc_someFloat_as_double[0][0], 3.1415, 0.00001);

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_FLOAT", 123.456);

  BOOST_CHECK_CLOSE(acc_someFloat_as_double[0][0], 3.1415, 0.00001);
  acc_someFloat_as_double.read();
  BOOST_CHECK_CLOSE(acc_someFloat_as_double[0][0], 123.456, 0.00001);

  acc_someFloat_as_double[0][0] = 1234.3;
  BOOST_CHECK_CLOSE(DoocsServerTestHelper::doocsGet<float>("//MYDUMMY/SOME_FLOAT"), 123.456, 0.00001);
  acc_someFloat_as_double.write();
  BOOST_CHECK_CLOSE(DoocsServerTestHelper::doocsGet<float>("//MYDUMMY/SOME_FLOAT"), 1234.3, 0.00001);

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_FLOAT", 3.1415);

  TwoDRegisterAccessor<int> acc_someFloat_as_int(device.getTwoDRegisterAccessor<int>("MYDUMMY/SOME_FLOAT"));
  BOOST_CHECK(acc_someFloat_as_int.getNChannels() == 1);
  BOOST_CHECK(acc_someFloat_as_int.getNElementsPerChannel() == 1);
  acc_someFloat_as_int.read();
  BOOST_CHECK(acc_someFloat_as_int[0][0] == 3);

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_FLOAT", 119.9);

  BOOST_CHECK(acc_someFloat_as_int[0][0] == 3);
  acc_someFloat_as_int.read();
  BOOST_CHECK(acc_someFloat_as_int[0][0] == 120);

  acc_someFloat_as_int[0][0] = 1234;
  BOOST_CHECK_CLOSE(DoocsServerTestHelper::doocsGet<float>("//MYDUMMY/SOME_FLOAT"), 119.9, 0.00001);
  acc_someFloat_as_int.write();
  BOOST_CHECK_CLOSE(DoocsServerTestHelper::doocsGet<float>("//MYDUMMY/SOME_FLOAT"), 1234.0, 0.00001);

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_FLOAT", 3.1415);

  TwoDRegisterAccessor<std::string> acc_someFloat_as_string(
      device.getTwoDRegisterAccessor<std::string>("MYDUMMY/SOME_FLOAT"));
  BOOST_CHECK(acc_someFloat_as_string.getNChannels() == 1);
  BOOST_CHECK(acc_someFloat_as_string.getNElementsPerChannel() == 1);
  acc_someFloat_as_string.read();
  BOOST_CHECK_CLOSE(std::stod(acc_someFloat_as_string[0][0]), 3.1415, 0.00001);

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_FLOAT", 120);

  BOOST_CHECK_CLOSE(std::stod(acc_someFloat_as_string[0][0]), 3.1415, 0.00001);
  acc_someFloat_as_string.read();
  BOOST_CHECK(acc_someFloat_as_string[0][0] == "120");

  acc_someFloat_as_string[0][0] = "1234.5678";
  BOOST_CHECK_CLOSE(DoocsServerTestHelper::doocsGet<float>("//MYDUMMY/SOME_FLOAT"), 120, 0.00001);
  acc_someFloat_as_string.write();
  BOOST_CHECK_CLOSE(DoocsServerTestHelper::doocsGet<float>("//MYDUMMY/SOME_FLOAT"), 1234.5678, 0.00001);

  device.close();
}

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testScalarDouble) {
  ChimeraTK::Device device;
  device.open(DoocsLauncher::DoocsServer1);

  TwoDRegisterAccessor<float> acc_someDouble_as_float(device.getTwoDRegisterAccessor<float>("MYDUMMY/SOME_DOUBLE"));
  BOOST_CHECK(acc_someDouble_as_float.getNChannels() == 1);
  BOOST_CHECK(acc_someDouble_as_float.getNElementsPerChannel() == 1);
  acc_someDouble_as_float.read();
  BOOST_CHECK_CLOSE(acc_someDouble_as_float[0][0], 2.8, 0.00001);

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_DOUBLE", 123.456);

  BOOST_CHECK_CLOSE(acc_someDouble_as_float[0][0], 2.8, 0.00001);
  auto oldVersion = acc_someDouble_as_float.getVersionNumber();
  acc_someDouble_as_float.read();
  BOOST_CHECK_CLOSE(acc_someDouble_as_float[0][0], 123.456, 0.00001);
  BOOST_CHECK(acc_someDouble_as_float.getVersionNumber() > oldVersion);

  acc_someDouble_as_float[0][0] = 666.333;
  BOOST_CHECK_CLOSE(DoocsServerTestHelper::doocsGet<float>("//MYDUMMY/SOME_DOUBLE"), 123.456, 0.00001);
  VersionNumber nextVersion;
  acc_someDouble_as_float.write(nextVersion);
  BOOST_CHECK_CLOSE(DoocsServerTestHelper::doocsGet<float>("//MYDUMMY/SOME_DOUBLE"), 666.333, 0.00001);
  BOOST_CHECK(acc_someDouble_as_float.getVersionNumber() == nextVersion);

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_DOUBLE", 3.1415);

  TwoDRegisterAccessor<double> acc_someDouble_as_double(device.getTwoDRegisterAccessor<double>("MYDUMMY/SOME_DOUBLE"));
  BOOST_CHECK(acc_someDouble_as_double.getNChannels() == 1);
  BOOST_CHECK(acc_someDouble_as_double.getNElementsPerChannel() == 1);
  acc_someDouble_as_double.read();
  BOOST_CHECK_CLOSE(acc_someDouble_as_double[0][0], 3.1415, 0.00001);

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_DOUBLE", 123.456);

  BOOST_CHECK_CLOSE(acc_someDouble_as_double[0][0], 3.1415, 0.00001);
  acc_someDouble_as_double.read();
  BOOST_CHECK_CLOSE(acc_someDouble_as_double[0][0], 123.456, 0.00001);

  acc_someDouble_as_double[0][0] = 1234.3;
  BOOST_CHECK_CLOSE(DoocsServerTestHelper::doocsGet<float>("//MYDUMMY/SOME_DOUBLE"), 123.456, 0.00001);
  acc_someDouble_as_double.write();
  BOOST_CHECK_CLOSE(DoocsServerTestHelper::doocsGet<float>("//MYDUMMY/SOME_DOUBLE"), 1234.3, 0.00001);

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_FLOAT", 3.1415);

  TwoDRegisterAccessor<int> acc_someDouble_as_int(device.getTwoDRegisterAccessor<int>("MYDUMMY/SOME_FLOAT"));
  BOOST_CHECK(acc_someDouble_as_int.getNChannels() == 1);
  BOOST_CHECK(acc_someDouble_as_int.getNElementsPerChannel() == 1);
  acc_someDouble_as_int.read();
  BOOST_CHECK(acc_someDouble_as_int[0][0] == 3);

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_FLOAT", 119.9);

  BOOST_CHECK(acc_someDouble_as_int[0][0] == 3);
  acc_someDouble_as_int.read();
  BOOST_CHECK(acc_someDouble_as_int[0][0] == 120);

  acc_someDouble_as_int[0][0] = 1234;
  BOOST_CHECK_CLOSE(DoocsServerTestHelper::doocsGet<float>("//MYDUMMY/SOME_FLOAT"), 119.9, 0.00001);
  acc_someDouble_as_int.write();
  BOOST_CHECK_CLOSE(DoocsServerTestHelper::doocsGet<float>("//MYDUMMY/SOME_FLOAT"), 1234.0, 0.00001);

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_FLOAT", 3.1415);

  TwoDRegisterAccessor<std::string> acc_someDouble_as_string(
      device.getTwoDRegisterAccessor<std::string>("MYDUMMY/SOME_FLOAT"));
  BOOST_CHECK(acc_someDouble_as_string.getNChannels() == 1);
  BOOST_CHECK(acc_someDouble_as_string.getNElementsPerChannel() == 1);
  acc_someDouble_as_string.read();
  BOOST_CHECK_CLOSE(std::stod(acc_someDouble_as_string[0][0]), 3.1415, 0.00001);

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_FLOAT", 120);

  BOOST_CHECK_CLOSE(std::stod(acc_someDouble_as_string[0][0]), 3.1415, 0.00001);
  acc_someDouble_as_string.read();
  BOOST_CHECK(acc_someDouble_as_string[0][0] == "120");

  acc_someDouble_as_string[0][0] = "1234.5678";
  BOOST_CHECK_CLOSE(DoocsServerTestHelper::doocsGet<float>("//MYDUMMY/SOME_FLOAT"), 120, 0.00001);
  acc_someDouble_as_string.write();
  BOOST_CHECK_CLOSE(DoocsServerTestHelper::doocsGet<float>("//MYDUMMY/SOME_FLOAT"), 1234.5678, 0.00001);

  device.close();
}

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testString) {
  ChimeraTK::Device device;
  device.open(DoocsLauncher::DoocsServer1);

  TwoDRegisterAccessor<std::string> acc_someString(device.getTwoDRegisterAccessor<std::string>("MYDUMMY/SOME_STRING"));
  BOOST_CHECK(acc_someString.getNChannels() == 1);
  BOOST_CHECK(acc_someString.getNElementsPerChannel() == 1);
  auto oldVersion = acc_someString.getVersionNumber();
  acc_someString.read();
  BOOST_CHECK(acc_someString[0][0] == "The quick brown fox jumps over the lazy dog.");
  BOOST_CHECK(acc_someString.getVersionNumber() > oldVersion);

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_STRING", "Something else.");

  BOOST_CHECK(acc_someString[0][0] == "The quick brown fox jumps over the lazy dog.");
  acc_someString.read();
  BOOST_CHECK(acc_someString[0][0] == "Something else.");

  acc_someString[0][0] = "Even different!";
  BOOST_CHECK(DoocsServerTestHelper::doocsGet<std::string>("//MYDUMMY/SOME_STRING") == "Something else.");
  VersionNumber nextVersion;
  acc_someString.write(nextVersion);
  BOOST_CHECK(DoocsServerTestHelper::doocsGet<std::string>("//MYDUMMY/SOME_STRING") == "Even different!");
  BOOST_CHECK(acc_someString.getVersionNumber() == nextVersion);

  device.close();
}

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testArrayInt) {
  ChimeraTK::Device device;
  device.open(DoocsLauncher::DoocsServer1);

  TwoDRegisterAccessor<int> acc_someArray(device.getTwoDRegisterAccessor<int>("MYDUMMY/SOME_INT_ARRAY"));
  BOOST_CHECK(acc_someArray.getNChannels() == 1);
  BOOST_CHECK(acc_someArray.getNElementsPerChannel() == 42);
  acc_someArray.read();
  for(int i = 0; i < 42; i++) BOOST_CHECK(acc_someArray[0][i] == 3 * i + 120);

  std::vector<int> vals(42);
  for(int i = 0; i < 42; i++) vals[i] = -55 * i;
  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_INT_ARRAY", vals);

  for(int i = 0; i < 42; i++) BOOST_CHECK(acc_someArray[0][i] == 3 * i + 120);
  auto oldVersion = acc_someArray.getVersionNumber();
  acc_someArray.read();
  for(int i = 0; i < 42; i++) BOOST_CHECK(acc_someArray[0][i] == -55 * i);
  BOOST_CHECK(acc_someArray.getVersionNumber() > oldVersion);

  for(int i = 0; i < 42; i++) acc_someArray[0][i] = i - 21;
  vals = DoocsServerTestHelper::doocsGetArray<int>("//MYDUMMY/SOME_INT_ARRAY");
  for(int i = 0; i < 42; i++) BOOST_CHECK(vals[i] == -55 * i);
  VersionNumber nextVersion;
  acc_someArray.write(nextVersion);
  vals = DoocsServerTestHelper::doocsGetArray<int>("//MYDUMMY/SOME_INT_ARRAY");
  for(int i = 0; i < 42; i++) BOOST_CHECK(vals[i] == i - 21);
  BOOST_CHECK(acc_someArray.getVersionNumber() == nextVersion);

  // access via double
  TwoDRegisterAccessor<double> acc_someArrayAsDouble(device.getTwoDRegisterAccessor<double>("MYDUMMY/SOME_INT_ARRAY"));
  acc_someArrayAsDouble.read();
  for(int i = 0; i < 5; i++) BOOST_CHECK_CLOSE(acc_someArrayAsDouble[0][i], i - 21, 0.00001);
  for(int i = 0; i < 5; i++) acc_someArrayAsDouble[0][i] = 2.4 * i;
  acc_someArrayAsDouble.write();
  vals = DoocsServerTestHelper::doocsGetArray<int>("//MYDUMMY/SOME_INT_ARRAY");
  for(int i = 0; i < 5; i++) BOOST_CHECK_CLOSE(vals[i], round(2.4 * i), 0.00001);

  // access via string
  TwoDRegisterAccessor<std::string> acc_someArrayAsString(
      device.getTwoDRegisterAccessor<std::string>("MYDUMMY/SOME_INT_ARRAY"));
  acc_someArrayAsString.read();
  for(int i = 0; i < 5; i++) BOOST_CHECK_CLOSE(std::stod(acc_someArrayAsString[0][i]), round(2.4 * i), 0.00001);
  for(int i = 0; i < 5; i++) acc_someArrayAsString[0][i] = std::to_string(3 * i);
  acc_someArrayAsString.write();
  vals = DoocsServerTestHelper::doocsGetArray<int>("//MYDUMMY/SOME_INT_ARRAY");
  for(int i = 0; i < 5; i++) BOOST_CHECK(vals[i] == 3 * i);

  device.close();
}

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testArrayShort) {
  ChimeraTK::Device device;
  device.open(DoocsLauncher::DoocsServer1);

  TwoDRegisterAccessor<int> acc_someArray(device.getTwoDRegisterAccessor<int>("MYDUMMY/SOME_SHORT_ARRAY"));
  BOOST_CHECK(acc_someArray.getNChannels() == 1);
  BOOST_CHECK(acc_someArray.getNElementsPerChannel() == 5);
  acc_someArray.read();
  for(int i = 0; i < 5; i++) BOOST_CHECK(acc_someArray[0][i] == 10 + i);

  std::vector<short> vals(5);
  for(int i = 0; i < 5; i++) vals[i] = -55 * i;
  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_SHORT_ARRAY", vals);

  for(int i = 0; i < 5; i++) BOOST_CHECK(acc_someArray[0][i] == 10 + i);
  auto oldVersion = acc_someArray.getVersionNumber();
  acc_someArray.read();
  for(int i = 0; i < 5; i++) BOOST_CHECK(acc_someArray[0][i] == -55 * i);
  BOOST_CHECK(acc_someArray.getVersionNumber() > oldVersion);

  std::vector<int> vals2(5);
  for(int i = 0; i < 5; i++) acc_someArray[0][i] = i - 21;
  vals2 = DoocsServerTestHelper::doocsGetArray<int>("//MYDUMMY/SOME_SHORT_ARRAY");
  for(int i = 0; i < 5; i++) BOOST_CHECK(vals2[i] == -55 * i);
  VersionNumber nextVersion;
  acc_someArray.write(nextVersion);
  vals2 = DoocsServerTestHelper::doocsGetArray<int>("//MYDUMMY/SOME_SHORT_ARRAY");
  for(int i = 0; i < 5; i++) BOOST_CHECK(vals2[i] == i - 21);
  BOOST_CHECK(acc_someArray.getVersionNumber() == nextVersion);

  device.close();
}

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testArrayLong) {
  ChimeraTK::Device device;
  device.open(DoocsLauncher::DoocsServer1);

  TwoDRegisterAccessor<int> acc_someArray(device.getTwoDRegisterAccessor<int>("MYDUMMY/SOME_LONG_ARRAY"));
  BOOST_CHECK(acc_someArray.getNChannels() == 1);
  BOOST_CHECK(acc_someArray.getNElementsPerChannel() == 5);
  acc_someArray.read();
  for(int i = 0; i < 5; i++) BOOST_CHECK(acc_someArray[0][i] == 10 + i);

  std::vector<long long int> vals(5);
  for(int i = 0; i < 5; i++) vals[i] = -55 * i;
  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_LONG_ARRAY", vals);

  for(int i = 0; i < 5; i++) BOOST_CHECK(acc_someArray[0][i] == 10 + i);
  auto oldVersion = acc_someArray.getVersionNumber();
  acc_someArray.read();
  for(int i = 0; i < 5; i++) BOOST_CHECK(acc_someArray[0][i] == -55 * i);
  BOOST_CHECK(acc_someArray.getVersionNumber() > oldVersion);

  std::vector<int> vals2(5);
  for(int i = 0; i < 5; i++) acc_someArray[0][i] = i - 21;
  vals2 = DoocsServerTestHelper::doocsGetArray<int>("//MYDUMMY/SOME_LONG_ARRAY");
  for(int i = 0; i < 5; i++) BOOST_CHECK(vals2[i] == -55 * i);
  VersionNumber nextVersion;
  acc_someArray.write(nextVersion);
  vals2 = DoocsServerTestHelper::doocsGetArray<int>("//MYDUMMY/SOME_LONG_ARRAY");
  for(int i = 0; i < 5; i++) BOOST_CHECK(vals2[i] == i - 21);
  BOOST_CHECK(acc_someArray.getVersionNumber() == nextVersion);

  device.close();
}

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testArrayFloat) {
  ChimeraTK::Device device;
  device.open(DoocsLauncher::DoocsServer1);

  TwoDRegisterAccessor<float> acc_someArray(device.getTwoDRegisterAccessor<float>("MYDUMMY/SOME_FLOAT_ARRAY"));
  BOOST_CHECK(acc_someArray.getNChannels() == 1);
  BOOST_CHECK(acc_someArray.getNElementsPerChannel() == 5);
  auto oldVersion = acc_someArray.getVersionNumber();
  acc_someArray.read();
  for(int i = 0; i < 5; i++) BOOST_CHECK_CLOSE(acc_someArray[0][i], i / 1000., 0.00001);
  BOOST_CHECK(acc_someArray.getVersionNumber() > oldVersion);

  std::vector<float> vals(5);
  for(int i = 0; i < 5; i++) vals[i] = -3.14159265 * i;
  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_FLOAT_ARRAY", vals);

  for(int i = 0; i < 5; i++) BOOST_CHECK_CLOSE(acc_someArray[0][i], i / 1000., 0.00001);
  acc_someArray.read();
  for(int i = 0; i < 5; i++) BOOST_CHECK_CLOSE(acc_someArray[0][i], -3.14159265 * i, 0.00001);

  for(int i = 0; i < 5; i++) acc_someArray[0][i] = 2. / (i + 0.01);
  vals = DoocsServerTestHelper::doocsGetArray<float>("//MYDUMMY/SOME_FLOAT_ARRAY");
  for(int i = 0; i < 5; i++) BOOST_CHECK_CLOSE(vals[i], -3.14159265 * i, 0.00001);
  VersionNumber nextVersion;
  acc_someArray.write(nextVersion);
  vals = DoocsServerTestHelper::doocsGetArray<float>("//MYDUMMY/SOME_FLOAT_ARRAY");
  for(int i = 0; i < 5; i++) BOOST_CHECK_CLOSE(vals[i], 2. / (i + 0.01), 0.00001);
  BOOST_CHECK(acc_someArray.getVersionNumber() == nextVersion);

  device.close();
}

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testArrayDouble) {
  ChimeraTK::Device device;
  device.open(DoocsLauncher::DoocsServer1);

  TwoDRegisterAccessor<double> acc_someArray(device.getTwoDRegisterAccessor<double>("MYDUMMY/SOME_DOUBLE_ARRAY"));
  BOOST_CHECK(acc_someArray.getNChannels() == 1);
  BOOST_CHECK(acc_someArray.getNElementsPerChannel() == 5);
  auto oldVersion = acc_someArray.getVersionNumber();
  acc_someArray.read();
  for(int i = 0; i < 5; i++) BOOST_CHECK_CLOSE(acc_someArray[0][i], i / 333., 0.00001);
  BOOST_CHECK(acc_someArray.getVersionNumber() > oldVersion);

  std::vector<double> vals(5);
  for(int i = 0; i < 5; i++) vals[i] = -3.14159265 * i;
  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_DOUBLE_ARRAY", vals);

  for(int i = 0; i < 5; i++) BOOST_CHECK_CLOSE(acc_someArray[0][i], i / 333., 0.00001);
  acc_someArray.read();
  for(int i = 0; i < 5; i++) BOOST_CHECK_CLOSE(acc_someArray[0][i], -3.14159265 * i, 0.00001);

  for(int i = 0; i < 5; i++) acc_someArray[0][i] = 2. / (i + 0.01);
  vals = DoocsServerTestHelper::doocsGetArray<double>("//MYDUMMY/SOME_DOUBLE_ARRAY");
  for(int i = 0; i < 5; i++) BOOST_CHECK_CLOSE(vals[i], -3.14159265 * i, 0.00001);
  VersionNumber nextVersion;
  acc_someArray.write(nextVersion);
  vals = DoocsServerTestHelper::doocsGetArray<double>("//MYDUMMY/SOME_DOUBLE_ARRAY");
  for(int i = 0; i < 5; i++) BOOST_CHECK_CLOSE(vals[i], 2. / (i + 0.01), 0.00001);
  BOOST_CHECK(acc_someArray.getVersionNumber() == nextVersion);

  // access via int
  TwoDRegisterAccessor<int> acc_someArrayAsInt(device.getTwoDRegisterAccessor<int>("MYDUMMY/SOME_DOUBLE_ARRAY"));
  acc_someArrayAsInt.read();
  for(int i = 0; i < 5; i++) BOOST_CHECK(acc_someArrayAsInt[0][i] == round(2. / (i + 0.01)));
  for(int i = 0; i < 5; i++) acc_someArrayAsInt[0][i] = 2 * i;
  acc_someArrayAsInt.write();
  vals = DoocsServerTestHelper::doocsGetArray<double>("//MYDUMMY/SOME_DOUBLE_ARRAY");
  for(int i = 0; i < 5; i++) BOOST_CHECK_CLOSE(vals[i], 2 * i, 0.00001);

  // access via string
  TwoDRegisterAccessor<std::string> acc_someArrayAsString(
      device.getTwoDRegisterAccessor<std::string>("MYDUMMY/SOME_DOUBLE_ARRAY"));
  acc_someArrayAsString.read();
  for(int i = 0; i < 5; i++) BOOST_CHECK_CLOSE(std::stod(acc_someArrayAsString[0][i]), 2 * i, 0.00001);
  for(int i = 0; i < 5; i++) acc_someArrayAsString[0][i] = std::to_string(2.1 * i);
  acc_someArrayAsString.write();
  vals = DoocsServerTestHelper::doocsGetArray<double>("//MYDUMMY/SOME_DOUBLE_ARRAY");
  for(int i = 0; i < 5; i++) BOOST_CHECK_CLOSE(vals[i], 2.1 * i, 0.00001);

  device.close();
}

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testSpectrum) {
  ChimeraTK::Device device;
  device.open(DoocsLauncher::DoocsServer1);

  TwoDRegisterAccessor<float> acc_someArray(device.getTwoDRegisterAccessor<float>("MYDUMMY/SOME_SPECTRUM"));
  BOOST_CHECK(acc_someArray.getNChannels() == 1);
  BOOST_CHECK(acc_someArray.getNElementsPerChannel() == 100);
  auto oldVersion = acc_someArray.getVersionNumber();
  acc_someArray.read();
  for(int i = 0; i < 100; i++) BOOST_CHECK_CLOSE(acc_someArray[0][i], i / 42., 0.00001);
  BOOST_CHECK(acc_someArray.getVersionNumber() > oldVersion);

  device.close();
}

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testIIII) {
  ChimeraTK::Device device;
  device.open(DoocsLauncher::DoocsServer1);

  TwoDRegisterAccessor<int> acc_someArray(device.getTwoDRegisterAccessor<int>("MYDUMMY/SOME_IIII"));
  BOOST_CHECK(acc_someArray.getNChannels() == 1);
  BOOST_CHECK(acc_someArray.getNElementsPerChannel() == 4);
  acc_someArray.read();
  for(int i = 0; i < 4; i++) BOOST_CHECK(acc_someArray[0][i] == 10 + i);

  std::vector<int> vals(4);
  for(int i = 0; i < 4; i++) vals[i] = -55 * i;
  DoocsServerTestHelper::doocsSetIIII("//MYDUMMY/SOME_IIII", vals);

  for(int i = 0; i < 4; i++) BOOST_CHECK(acc_someArray[0][i] == 10 + i);
  auto oldVersion = acc_someArray.getVersionNumber();
  acc_someArray.read();
  for(int i = 0; i < 4; i++) BOOST_CHECK(acc_someArray[0][i] == -55 * i);
  BOOST_CHECK(acc_someArray.getVersionNumber() > oldVersion);

  for(int i = 0; i < 4; i++) acc_someArray[0][i] = i - 21;
  vals = DoocsServerTestHelper::doocsGetArray<int>("//MYDUMMY/SOME_IIII");
  for(int i = 0; i < 4; i++) BOOST_CHECK(vals[i] == -55 * i);
  VersionNumber nextVersion;
  acc_someArray.write(nextVersion);
  vals = DoocsServerTestHelper::doocsGetArray<int>("//MYDUMMY/SOME_IIII");
  for(int i = 0; i < 4; i++) BOOST_CHECK(vals[i] == i - 21);
  BOOST_CHECK(acc_someArray.getVersionNumber() == nextVersion);

  // access via double
  TwoDRegisterAccessor<double> acc_someArrayAsDouble(device.getTwoDRegisterAccessor<double>("MYDUMMY/SOME_IIII"));
  acc_someArrayAsDouble.read();
  for(int i = 0; i < 4; i++) BOOST_CHECK_CLOSE(acc_someArrayAsDouble[0][i], i - 21, 0.00001);
  for(int i = 0; i < 4; i++) acc_someArrayAsDouble[0][i] = 2.4 * i;
  acc_someArrayAsDouble.write();
  vals = DoocsServerTestHelper::doocsGetArray<int>("//MYDUMMY/SOME_IIII");
  for(int i = 0; i < 4; i++) BOOST_CHECK_CLOSE(vals[i], round(2.4 * i), 0.00001);

  // access via string
  TwoDRegisterAccessor<std::string> acc_someArrayAsString(
      device.getTwoDRegisterAccessor<std::string>("MYDUMMY/SOME_IIII"));
  acc_someArrayAsString.read();
  for(int i = 0; i < 4; i++) BOOST_CHECK_CLOSE(std::stod(acc_someArrayAsString[0][i]), round(2.4 * i), 0.00001);
  for(int i = 0; i < 4; i++) acc_someArrayAsString[0][i] = std::to_string(3 * i);
  acc_someArrayAsString.write();
  vals = DoocsServerTestHelper::doocsGetArray<int>("//MYDUMMY/SOME_IIII");
  for(int i = 0; i < 4; i++) BOOST_CHECK(vals[i] == 3 * i);

  device.close();
}

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testIFFF) {
  ChimeraTK::Device device;
  device.open(DoocsLauncher::DoocsServer1);

  // check catalogue
  auto catalogue = device.getRegisterCatalogue();
  auto regI = catalogue.getRegister("MYDUMMY/SOME_IFFF/I");
  auto regF1 = catalogue.getRegister("MYDUMMY/SOME_IFFF/F1");
  auto regF2 = catalogue.getRegister("MYDUMMY/SOME_IFFF/F2");
  auto regF3 = catalogue.getRegister("MYDUMMY/SOME_IFFF/F3");
  BOOST_CHECK(regI->getDataDescriptor().isIntegral());
  BOOST_CHECK(regI->getDataDescriptor().isSigned());
  BOOST_CHECK(regI->getDataDescriptor().nDigits() == 11);
  BOOST_CHECK(!regF1->getDataDescriptor().isIntegral());
  BOOST_CHECK(regF1->getDataDescriptor().isSigned());
  BOOST_CHECK(!regF2->getDataDescriptor().isIntegral());
  BOOST_CHECK(regF2->getDataDescriptor().isSigned());
  BOOST_CHECK(!regF3->getDataDescriptor().isIntegral());
  BOOST_CHECK(regF3->getDataDescriptor().isSigned());

  // read access via int/float
  {
    auto acc_I = device.getScalarRegisterAccessor<int>("MYDUMMY/SOME_IFFF/I");
    auto acc_F1 = device.getScalarRegisterAccessor<float>("MYDUMMY/SOME_IFFF/F1");
    auto acc_F2 = device.getScalarRegisterAccessor<float>("MYDUMMY/SOME_IFFF/F2");
    auto acc_F3 = device.getScalarRegisterAccessor<float>("MYDUMMY/SOME_IFFF/F3");

    acc_I.read();
    BOOST_CHECK_EQUAL(int(acc_I), 10);
    acc_F1.read();
    BOOST_CHECK_CLOSE(float(acc_F1), 2.71828, 0.00001);
    acc_F2.read();
    BOOST_CHECK_CLOSE(float(acc_F2), 3.14159, 0.00001);
    acc_F3.read();
    BOOST_CHECK_CLOSE(float(acc_F3), 197.327, 0.00001);
  }

  // read access via strings
  {
    auto acc_I = device.getScalarRegisterAccessor<std::string>("MYDUMMY/SOME_IFFF/I");
    auto acc_F1 = device.getScalarRegisterAccessor<std::string>("MYDUMMY/SOME_IFFF/F1");
    auto acc_F2 = device.getScalarRegisterAccessor<std::string>("MYDUMMY/SOME_IFFF/F2");
    auto acc_F3 = device.getScalarRegisterAccessor<std::string>("MYDUMMY/SOME_IFFF/F3");

    acc_I.read();
    BOOST_CHECK_EQUAL(std::string(acc_I), "10");
    acc_F1.read();
    BOOST_CHECK_CLOSE(std::stod(acc_F1), 2.71828, 0.00001);
    acc_F2.read();
    BOOST_CHECK_CLOSE(std::stod(acc_F2), 3.14159, 0.00001);
    acc_F3.read();
    BOOST_CHECK_CLOSE(std::stod(acc_F3), 197.327, 0.00001);
  }

  // write access via int/float
  {
    auto acc_I = device.getScalarRegisterAccessor<int>("MYDUMMY/SOME_IFFF/I");
    auto acc_F1 = device.getScalarRegisterAccessor<float>("MYDUMMY/SOME_IFFF/F1");
    auto acc_F2 = device.getScalarRegisterAccessor<float>("MYDUMMY/SOME_IFFF/F2");
    auto acc_F3 = device.getScalarRegisterAccessor<float>("MYDUMMY/SOME_IFFF/F3");

    acc_I = 42;
    acc_I.write();

    acc_I.read();
    BOOST_CHECK_EQUAL(int(acc_I), 42);
    acc_F1.read();
    BOOST_CHECK_CLOSE(float(acc_F1), 2.71828, 0.00001);
    acc_F2.read();
    BOOST_CHECK_CLOSE(float(acc_F2), 3.14159, 0.00001);
    acc_F3.read();
    BOOST_CHECK_CLOSE(float(acc_F3), 197.327, 0.00001);

    acc_F2 = 123.456;
    acc_F2.write();

    acc_F3 = -666.666;
    acc_F3.write();

    acc_I.read();
    BOOST_CHECK_EQUAL(int(acc_I), 42);
    acc_F1.read();
    BOOST_CHECK_CLOSE(float(acc_F1), 2.71828, 0.00001);
    acc_F2.read();
    BOOST_CHECK_CLOSE(float(acc_F2), 123.456, 0.00001);
    acc_F3.read();
    BOOST_CHECK_CLOSE(float(acc_F3), -666.666, 0.00001);
  }

  device.close();
}

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testBitAndStatus) {
  ChimeraTK::Device device;
  device.open(DoocsLauncher::DoocsServer1);

  TwoDRegisterAccessor<int> acc_someBit(device.getTwoDRegisterAccessor<int>("MYDUMMY/SOME_BIT"));
  TwoDRegisterAccessor<int> acc_someStatus(device.getTwoDRegisterAccessor<int>("MYDUMMY/SOME_STATUS"));
  TwoDRegisterAccessor<uint16_t> acc_someStatusU16(device.getTwoDRegisterAccessor<uint16_t>("MYDUMMY/SOME_STATUS"));

  BOOST_CHECK(acc_someBit.getNChannels() == 1);
  BOOST_CHECK(acc_someStatus.getNChannels() == 1);
  BOOST_CHECK(acc_someBit.getNElementsPerChannel() == 1);
  BOOST_CHECK(acc_someStatus.getNElementsPerChannel() == 1);

  auto oldVersion = acc_someBit.getVersionNumber();
  acc_someBit.read();
  BOOST_CHECK(acc_someBit[0][0] == 1);
  BOOST_CHECK(acc_someBit.getVersionNumber() > oldVersion);
  oldVersion = acc_someStatus.getVersionNumber();
  acc_someStatus.read();
  BOOST_CHECK(acc_someStatus[0][0] == 3);
  BOOST_CHECK(acc_someStatus.getVersionNumber() > oldVersion);

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_BIT", 0);

  BOOST_CHECK(acc_someBit[0][0] == 1);
  acc_someBit.read();
  BOOST_CHECK(acc_someBit[0][0] == 0);
  BOOST_CHECK(acc_someStatus[0][0] == 3);
  acc_someStatus.read();
  BOOST_CHECK(acc_someStatus[0][0] == 2);

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_STATUS", 0xFFFF);

  BOOST_CHECK(acc_someBit[0][0] == 0);
  acc_someBit.read();
  BOOST_CHECK(acc_someBit[0][0] == 1);
  BOOST_CHECK(acc_someStatus[0][0] == 2);
  acc_someStatus.read();
  BOOST_CHECK(acc_someStatus[0][0] == 0xFFFF);

  acc_someStatusU16.read();
  BOOST_CHECK(acc_someStatusU16[0][0] == 0xFFFF);

  acc_someBit[0][0] = 0;
  BOOST_CHECK(DoocsServerTestHelper::doocsGet<int>("//MYDUMMY/SOME_STATUS") == 0xFFFF);
  VersionNumber nextVersion;
  acc_someBit.write(nextVersion);
  BOOST_CHECK(DoocsServerTestHelper::doocsGet<int>("//MYDUMMY/SOME_STATUS") == 0xFFFE);
  BOOST_CHECK(acc_someBit.getVersionNumber() == nextVersion);

  acc_someStatus[0][0] = 123;
  BOOST_CHECK(DoocsServerTestHelper::doocsGet<int>("//MYDUMMY/SOME_STATUS") == 0xFFFE);
  nextVersion = {};
  acc_someStatus.write(nextVersion);
  BOOST_CHECK(DoocsServerTestHelper::doocsGet<int>("//MYDUMMY/SOME_STATUS") == 123);
  BOOST_CHECK(acc_someStatus.getVersionNumber() == nextVersion);

  device.close();
}
/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testPartialAccess) {
  ChimeraTK::Device device;
  device.open(DoocsLauncher::DoocsServer1);

  // int array, 20 elements, offset of 1
  {
    OneDRegisterAccessor<int> acc_someArray(device.getOneDRegisterAccessor<int>("MYDUMMY/SOME_INT_ARRAY", 20, 1));
    BOOST_CHECK(acc_someArray.getNElements() == 20);

    std::vector<int> vals(42);
    for(int i = 0; i < 42; i++) vals[i] = -55 * i;
    DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_INT_ARRAY", vals);

    acc_someArray.read();
    for(int i = 0; i < 20; i++) BOOST_CHECK(acc_someArray[i] == -55 * (i + 1));

    for(int i = 0; i < 20; i++) acc_someArray[i] = i;
    acc_someArray.write();
    vals = DoocsServerTestHelper::doocsGetArray<int>("//MYDUMMY/SOME_INT_ARRAY");
    for(int i = 0; i < 42; i++) {
      if(i == 0 || i > 20) {
        BOOST_CHECK(vals[i] == -55 * i);
      }
      else {
        BOOST_CHECK(vals[i] == i - 1);
      }
    }
  }

  // int array, 1 elements, offset of 10
  {
    OneDRegisterAccessor<int> acc_someArray(device.getOneDRegisterAccessor<int>("MYDUMMY/SOME_INT_ARRAY", 1, 10));
    BOOST_CHECK(acc_someArray.getNElements() == 1);

    std::vector<int> vals(42);
    for(int i = 0; i < 42; i++) vals[i] = 33 * i;
    DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_INT_ARRAY", vals);

    acc_someArray.read();
    BOOST_CHECK(acc_someArray[0] == 33 * 10);

    acc_someArray[0] = 42;
    acc_someArray.write();
    vals = DoocsServerTestHelper::doocsGetArray<int>("//MYDUMMY/SOME_INT_ARRAY");
    for(int i = 0; i < 42; i++) {
      if(i == 10) {
        BOOST_CHECK(vals[i] == 42);
      }
      else {
        BOOST_CHECK(vals[i] == 33 * i);
      }
    }
  }

  // double array with float user type, 3 elements, offset of 2
  {
    OneDRegisterAccessor<float> acc_someArray(device.getOneDRegisterAccessor<float>("MYDUMMY/SOME_DOUBLE_ARRAY", 3, 2));
    BOOST_CHECK(acc_someArray.getNElements() == 3);

    std::vector<double> vals(5);
    for(int i = 0; i < 5; i++) vals[i] = 3.14 * i;
    DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_DOUBLE_ARRAY", vals);

    acc_someArray.read();
    for(int i = 0; i < 3; i++) BOOST_CHECK_CLOSE(acc_someArray[i], 3.14 * (i + 2), 0.00001);

    for(int i = 0; i < 3; i++) acc_someArray[i] = 1.2 - i;
    acc_someArray.write();
    vals = DoocsServerTestHelper::doocsGetArray<double>("//MYDUMMY/SOME_DOUBLE_ARRAY");
    for(int i = 0; i < 5; i++) {
      if(i > 1) {
        BOOST_CHECK_CLOSE(vals[i], 1.2 - (i - 2), 0.00001);
      }
      else {
        BOOST_CHECK_CLOSE(vals[i], 3.14 * i, 0.00001);
      }
    }
  }

  // float array with string user type, 1 elements, offset of 0
  {
    ScalarRegisterAccessor<std::string> acc_someArray(
        device.getScalarRegisterAccessor<std::string>("MYDUMMY/SOME_FLOAT_ARRAY", 0));

    std::vector<float> vals(5);
    for(int i = 0; i < 5; i++) vals[i] = 2.82 * (i + 1);
    DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_FLOAT_ARRAY", vals);

    acc_someArray.read();
    BOOST_CHECK_CLOSE(std::stod(acc_someArray), 2.82, 0.00001);

    acc_someArray = "-11.111";
    acc_someArray.write();
    vals = DoocsServerTestHelper::doocsGetArray<float>("//MYDUMMY/SOME_FLOAT_ARRAY");
    for(int i = 0; i < 5; i++) {
      if(i == 0) {
        BOOST_CHECK_CLOSE(vals[i], -11.111, 0.00001);
      }
      else {
        BOOST_CHECK_CLOSE(vals[i], 2.82 * (i + 1), 0.00001);
      }
    }
  }

  device.close();
}

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testExceptions) {
  ChimeraTK::Device device;
  device.open(DoocsLauncher::DoocsServer1);

  // unsupported data type
  BOOST_CHECK_THROW(device.getTwoDRegisterAccessor<int>("MYDUMMY/UNSUPPORTED_DATA_TYPE"), // D_fiii
      ChimeraTK::logic_error);

  // non-existing DOOCS property
  BOOST_CHECK_THROW(device.getTwoDRegisterAccessor<int>("MYDUMMY/NOT_EXISTING"), ChimeraTK::runtime_error);

  // read string with non-string user type
  BOOST_CHECK_THROW(device.getTwoDRegisterAccessor<int>("MYDUMMY/SOME_STRING"), ChimeraTK::logic_error);

  // access too many elements (double register)
  BOOST_CHECK_THROW(device.getOneDRegisterAccessor<float>("MYDUMMY/SOME_DOUBLE_ARRAY", 10, 1), ChimeraTK::logic_error);

  // access too many elements (int register)
  BOOST_CHECK_THROW(device.getOneDRegisterAccessor<float>("MYDUMMY/SOME_INT_ARRAY", 100, 1), ChimeraTK::logic_error);

  device.close();
}

/**********************************************************************************************************************/

void createCacheFileFromCdd(const std::string& cdd) {
  namespace ctk = ChimeraTK;
  auto d = ctk::Device(cdd);
  d.open();
  d.getRegisterCatalogue();
}

void deleteFile(const std::string& filename) {
  errno = 0;
  if (unlink(filename.c_str()) < 0)
    throw std::system_error(std::make_error_code(static_cast<std::errc>(errno)));
}

BOOST_AUTO_TEST_CASE(testCatalogue) {
  {
    createCacheFileFromCdd(DoocsLauncher::DoocsServer1_cached);
    createCacheFileFromCdd(DoocsLauncher::DoocsServer2_cached);

    ChimeraTK::Device device;
    device.open(DoocsLauncher::DoocsServer2);

    ChimeraTK::Device device_cached;
    device_cached.open(DoocsLauncher::DoocsServer2_cached);

    std::vector<ChimeraTK::RegisterCatalogue> catalogueList2;
    catalogueList2.push_back(device.getRegisterCatalogue());
    catalogueList2.push_back(device_cached.getRegisterCatalogue());

    for(auto catalogue : catalogueList2) {
      // check number of registers, but not with the exact number, since DOOCS adds
      // some registers!
      BOOST_CHECK(catalogue.getNumberOfRegisters() > 13);

      // check for the presence of known registers
      BOOST_CHECK(catalogue.hasRegister("SOME_INT"));
      BOOST_CHECK(catalogue.hasRegister("SOME_FLOAT"));
      BOOST_CHECK(catalogue.hasRegister("SOME_DOUBLE"));
      BOOST_CHECK(catalogue.hasRegister("SOME_STRING"));
      BOOST_CHECK(catalogue.hasRegister("SOME_STATUS"));
      BOOST_CHECK(catalogue.hasRegister("SOME_BIT"));
      BOOST_CHECK(catalogue.hasRegister("SOME_INT_ARRAY"));
      BOOST_CHECK(catalogue.hasRegister("SOME_SHORT_ARRAY"));
      BOOST_CHECK(catalogue.hasRegister("SOME_LONG_ARRAY"));
      BOOST_CHECK(catalogue.hasRegister("SOME_FLOAT_ARRAY"));
      BOOST_CHECK(catalogue.hasRegister("SOME_DOUBLE_ARRAY"));
      BOOST_CHECK(catalogue.hasRegister("SOME_SPECTRUM"));
      BOOST_CHECK(catalogue.hasRegister("SOME_IFFF/I"));
      BOOST_CHECK(catalogue.hasRegister("SOME_IFFF/F1"));
      BOOST_CHECK(catalogue.hasRegister("SOME_IFFF/F2"));
      BOOST_CHECK(catalogue.hasRegister("SOME_IFFF/F3"));

      // check the properties of some registers
      auto r1 = catalogue.getRegister("SOME_INT");
      BOOST_CHECK(r1->getRegisterName() == "SOME_INT");
      BOOST_CHECK(r1->getNumberOfElements() == 1);
      BOOST_CHECK(r1->getNumberOfChannels() == 1);
      BOOST_CHECK(r1->getNumberOfDimensions() == 0);
      BOOST_CHECK(not r1->getSupportedAccessModes().has(ChimeraTK::AccessMode::wait_for_new_data));

      auto r2 = catalogue.getRegister("SOME_STRING");
      BOOST_CHECK(r2->getRegisterName() == "SOME_STRING");
      BOOST_CHECK(r2->getNumberOfElements() == 1);
      BOOST_CHECK(r2->getNumberOfChannels() == 1);
      BOOST_CHECK(r2->getNumberOfDimensions() == 0);
      BOOST_CHECK(not r2->getSupportedAccessModes().has(ChimeraTK::AccessMode::wait_for_new_data));

      auto r3 = catalogue.getRegister("SOME_INT_ARRAY");
      BOOST_CHECK(r3->getRegisterName() == "SOME_INT_ARRAY");
      BOOST_CHECK(r3->getNumberOfElements() == 42);
      BOOST_CHECK(r3->getNumberOfChannels() == 1);
      BOOST_CHECK(r3->getNumberOfDimensions() == 1);
      BOOST_CHECK(not r3->getSupportedAccessModes().has(ChimeraTK::AccessMode::wait_for_new_data));

      auto r4 = catalogue.getRegister("SOME_FLOAT_ARRAY");
      BOOST_CHECK(r4->getRegisterName() == "SOME_FLOAT_ARRAY");
      BOOST_CHECK(r4->getNumberOfElements() == 5);
      BOOST_CHECK(r4->getNumberOfChannels() == 1);
      BOOST_CHECK(r4->getNumberOfDimensions() == 1);
      BOOST_CHECK(not r4->getSupportedAccessModes().has(ChimeraTK::AccessMode::wait_for_new_data));

      auto r5 = catalogue.getRegister("SOME_SPECTRUM");
      BOOST_CHECK(r5->getRegisterName() == "SOME_SPECTRUM");
      BOOST_CHECK(r5->getNumberOfElements() == 100);
      BOOST_CHECK(r5->getNumberOfChannels() == 1);
      BOOST_CHECK(r5->getNumberOfDimensions() == 1);
      BOOST_CHECK(not r5->getSupportedAccessModes().has(ChimeraTK::AccessMode::wait_for_new_data));

      auto r6 = catalogue.getRegister("SOME_ZMQINT");
      BOOST_CHECK(r6->getRegisterName() == "SOME_ZMQINT");
      BOOST_CHECK(r6->getNumberOfElements() == 1);
      BOOST_CHECK(r6->getNumberOfChannels() == 1);
      BOOST_CHECK(r6->getNumberOfDimensions() == 0);
      BOOST_CHECK(r6->getSupportedAccessModes().has(ChimeraTK::AccessMode::wait_for_new_data));

      auto r7 = catalogue.getRegister("SOME_IIII");
      BOOST_CHECK(r7->getRegisterName() == "SOME_IIII");
      BOOST_CHECK(r7->getNumberOfElements() == 4);
      BOOST_CHECK(r7->getNumberOfChannels() == 1);
      BOOST_CHECK(r7->getNumberOfDimensions() == 1);
      BOOST_CHECK(not r7->getSupportedAccessModes().has(ChimeraTK::AccessMode::wait_for_new_data));

      auto regI = catalogue.getRegister("SOME_IFFF/I");
      auto regF1 = catalogue.getRegister("SOME_IFFF/F1");
      auto regF2 = catalogue.getRegister("SOME_IFFF/F2");
      auto regF3 = catalogue.getRegister("SOME_IFFF/F3");
      BOOST_CHECK(regI->getDataDescriptor().isIntegral());
      BOOST_CHECK(regI->getDataDescriptor().isSigned());
      BOOST_CHECK(regI->getDataDescriptor().nDigits() == 11);
      BOOST_CHECK(!regF1->getDataDescriptor().isIntegral());
      BOOST_CHECK(regF1->getDataDescriptor().isSigned());
      BOOST_CHECK(!regF2->getDataDescriptor().isIntegral());
      BOOST_CHECK(regF2->getDataDescriptor().isSigned());
      BOOST_CHECK(!regF3->getDataDescriptor().isIntegral());
      BOOST_CHECK(regF3->getDataDescriptor().isSigned());
    }
    device.close();
    device_cached.close();

    // quick check with the other level (location is included in the register
    // name)
    device.open(DoocsLauncher::DoocsServer1);
    device_cached.open(DoocsLauncher::DoocsServer1_cached);

    std::vector<ChimeraTK::RegisterCatalogue> catalogueList1;
    catalogueList1.push_back(device.getRegisterCatalogue());
    catalogueList1.push_back(device_cached.getRegisterCatalogue());

    for(auto catalogue : catalogueList1) {
      // check number of registers, but not with the exact number, since DOOCS adds
      // some registers!
      BOOST_CHECK(catalogue.getNumberOfRegisters() > 13);

      // check for the presence of known registers
      BOOST_CHECK(catalogue.hasRegister("MYDUMMY/SOME_INT"));
      BOOST_CHECK(catalogue.hasRegister("MYDUMMY/SOME_ZMQINT"));
      BOOST_CHECK(catalogue.hasRegister("DUMMY._SVR/SVR.BPN"));
      BOOST_CHECK(catalogue.hasRegister("MYDUMMY/SOME_IFFF/I"));
      BOOST_CHECK(catalogue.hasRegister("MYDUMMY/SOME_IFFF/F1"));
      BOOST_CHECK(catalogue.hasRegister("MYDUMMY/SOME_IFFF/F2"));
      BOOST_CHECK(catalogue.hasRegister("MYDUMMY/SOME_IFFF/F3"));

      // check the properties of some registers
      auto r8 = catalogue.getRegister("MYDUMMY/SOME_INT");
      BOOST_CHECK(r8->getRegisterName() == "MYDUMMY/SOME_INT");
      BOOST_CHECK(r8->getNumberOfElements() == 1);
      BOOST_CHECK(r8->getNumberOfChannels() == 1);
      BOOST_CHECK(r8->getNumberOfDimensions() == 0);
      BOOST_CHECK(not r8->getSupportedAccessModes().has(ChimeraTK::AccessMode::wait_for_new_data));

      auto r9 = catalogue.getRegister("DUMMY._SVR/SVR.BPN");
      BOOST_CHECK(r9->getRegisterName() == "DUMMY._SVR/SVR.BPN");
      BOOST_CHECK(r9->getNumberOfElements() == 1);
      BOOST_CHECK(r9->getNumberOfChannels() == 1);
      BOOST_CHECK(r9->getNumberOfDimensions() == 0);
      BOOST_CHECK(not r9->getSupportedAccessModes().has(ChimeraTK::AccessMode::wait_for_new_data));

      auto r10 = catalogue.getRegister("MYDUMMY/SOME_ZMQINT");
      BOOST_CHECK(r10->getRegisterName() == "MYDUMMY/SOME_ZMQINT");
      BOOST_CHECK(r10->getNumberOfElements() == 1);
      BOOST_CHECK(r10->getNumberOfChannels() == 1);
      BOOST_CHECK(r10->getNumberOfDimensions() == 0);
      BOOST_CHECK(r10->getSupportedAccessModes().has(ChimeraTK::AccessMode::wait_for_new_data));

      auto regI = catalogue.getRegister("MYDUMMY/SOME_IFFF/I");
      auto regF1 = catalogue.getRegister("MYDUMMY/SOME_IFFF/F1");
      auto regF2 = catalogue.getRegister("MYDUMMY/SOME_IFFF/F2");
      auto regF3 = catalogue.getRegister("MYDUMMY/SOME_IFFF/F3");
      BOOST_CHECK(regI->getDataDescriptor().isIntegral());
      BOOST_CHECK(regI->getDataDescriptor().isSigned());
      BOOST_CHECK(regI->getDataDescriptor().nDigits() == 11);
      BOOST_CHECK(!regF1->getDataDescriptor().isIntegral());
      BOOST_CHECK(regF1->getDataDescriptor().isSigned());
      BOOST_CHECK(!regF2->getDataDescriptor().isIntegral());
      BOOST_CHECK(regF2->getDataDescriptor().isSigned());
      BOOST_CHECK(!regF3->getDataDescriptor().isIntegral());
      BOOST_CHECK(regF3->getDataDescriptor().isSigned());
    }
    device.close();
    device_cached.close();
  } // ensure we delete the cache file written on destruction

  deleteFile(DoocsLauncher::cacheFile1);
  deleteFile(DoocsLauncher::cacheFile2);
}

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testOther) {
  ChimeraTK::Device device;
  device.open(DoocsLauncher::DoocsServer1);

  // device info string
  BOOST_CHECK(device.readDeviceInfo() == "DOOCS server address: doocs://localhost:" + DoocsLauncher::rpc_no + "/F/D");

  // test in TransferGroup
  TwoDRegisterAccessor<int32_t> acc1(device.getTwoDRegisterAccessor<int32_t>("MYDUMMY/SOME_INT"));
  TwoDRegisterAccessor<int32_t> acc2(device.getTwoDRegisterAccessor<int32_t>("MYDUMMY/SOME_INT"));
  TransferGroup group;
  group.addAccessor(acc1);
  group.addAccessor(acc2);

  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_INT", 123);
  group.read();
  BOOST_CHECK_EQUAL(acc1[0][0], 123);
  BOOST_CHECK_EQUAL(acc2[0][0], 123);
  acc1[0][0] = 42;

  device.close();

  // compatibility with SDM syntax
  device.open("sdm://./doocs=TEST.DOOCS,NOT_EXISTING,SOME_LOCATION");
  BOOST_CHECK(device.readDeviceInfo() == "DOOCS server address: TEST.DOOCS/NOT_EXISTING/SOME_LOCATION");
  device.close();
}

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testDestruction) {
  auto server = find_device("MYDUMMY");
  server->lock();
  {
    auto d = ChimeraTK::Device(DoocsLauncher::DoocsServer2_cached);
    std::async(std::launch::async, [=]() { server->unlock(); });
  } // should not block here on instance destruction.
}

/**********************************************************************************************************************/

bool file_exists(const std::string& name) {
  std::ifstream f(name.c_str());
  return f.good();
}

BOOST_AUTO_TEST_CASE(testCacheFileCreation) {
  {
    BOOST_CHECK(file_exists(DoocsLauncher::cacheFile1) == false);
    BOOST_CHECK(file_exists(DoocsLauncher::cacheFile2) == false);

    auto d1 = ChimeraTK::Device(DoocsLauncher::DoocsServer1_cached);
    auto d2 = ChimeraTK::Device(DoocsLauncher::DoocsServer2_cached);

    d1.getRegisterCatalogue();
    d2.getRegisterCatalogue();

    BOOST_CHECK(file_exists(DoocsLauncher::cacheFile1) == true);
    BOOST_CHECK(file_exists(DoocsLauncher::cacheFile2) == true);
  }
  deleteFile(DoocsLauncher::cacheFile1);
  deleteFile(DoocsLauncher::cacheFile2);
}

std::time_t createDummyXml(const std::string& filename){
  std::string xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                    "<catalogue version=\"1.0\">"
                    "  <register>\n"
                    "    <name>/DUMMY</name>\n"
                    "    <length>1</length>\n"
                    "    <access_mode></access_mode>\n"
                    "    <doocs_type_id>1</doocs_type_id>\n" // DATA_TEXT
                    "    <!--doocs id: INT-->\n"
                    "  </register>\n"
                    "</catalogue>\n";
  std::ofstream o(filename);
  o << xml;
  o.close();

  auto creation_time = boost::filesystem::last_write_time(DoocsLauncher::cacheFile2);

  // boost::filesystem::last_write_time timestamp resolution is not high
  // enough to pick modifications without this wait.
  std::this_thread::sleep_for(std::chrono::seconds(1));
  return creation_time;
}

BOOST_AUTO_TEST_CASE(testCacheXmlReplacement) {
  // test local copy is replaced when catalogue fetching is complete.
  auto creation_time = createDummyXml(DoocsLauncher::cacheFile2);
  {
    auto d = ChimeraTK::Device(DoocsLauncher::DoocsServer2_cached);
    std::atomic<bool> cancelTimeOutTask{false};

    std::future<void> timeoutTask = std::async(std::launch::async, [&] () {
          unsigned int count = 60;
          while (count > 0 && not cancelTimeOutTask) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            count --;
          }
        }) ;

    auto hasTimedOut = [&](){
       return timeoutTask.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    };

    auto modification_time = creation_time;
    while ((modification_time <= creation_time) && not hasTimedOut()) {
      modification_time = boost::filesystem::last_write_time(DoocsLauncher::cacheFile2);
      std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    // cancel timeout task
    cancelTimeOutTask = true;
    BOOST_CHECK(modification_time > creation_time); // given the percondition,
                                                    // this should fail if loop
                                                    // times out.
    deleteFile(DoocsLauncher::cacheFile2);
  }

}

BOOST_AUTO_TEST_CASE(testCacheXmlReplacementBehaviorOnFailure) {
  // local copy unchanged if app exits before.
  auto creation_time = createDummyXml(DoocsLauncher::cacheFile2);
  auto server = find_device("MYDUMMY");
  server->lock();
  {
    auto d = ChimeraTK::Device(DoocsLauncher::DoocsServer2_cached);
  }
  server->unlock();
  auto modification_time = boost::filesystem::last_write_time(DoocsLauncher::cacheFile2);
  BOOST_CHECK(creation_time == modification_time);
  deleteFile(DoocsLauncher::cacheFile2);

}

BOOST_AUTO_TEST_CASE(testAccessorForCachedMode){
  createCacheFileFromCdd(DoocsLauncher::DoocsServer1_cached);
  auto d = ChimeraTK::Device(DoocsLauncher::DoocsServer1_cached);
  
  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_INT", 120);
  TwoDRegisterAccessor<int32_t> acc_someInt_as_int(d.getTwoDRegisterAccessor<int32_t>("MYDUMMY/SOME_INT"));
  BOOST_CHECK(acc_someInt_as_int.getNChannels() == 1);
  BOOST_CHECK(acc_someInt_as_int.getNElementsPerChannel() == 1);

  d.open();

  acc_someInt_as_int.read();
  BOOST_CHECK(acc_someInt_as_int[0][0] == 120);

  acc_someInt_as_int[0][0] = 50;
  acc_someInt_as_int.write();
  BOOST_CHECK(DoocsServerTestHelper::doocsGet<int>("//MYDUMMY/SOME_INT") == 50);
  deleteFile(DoocsLauncher::cacheFile1);
}

BOOST_AUTO_TEST_CASE(testBlankXMLThrow){

  std::string xml = "";
  std::ofstream o(DoocsLauncher::cacheFile2);
  o << xml;
  o.close();

  BOOST_CHECK_THROW(ChimeraTK::Device x(DoocsLauncher::DoocsServer2_cached), ChimeraTK::logic_error);
  deleteFile(DoocsLauncher::cacheFile2);

}

BOOST_AUTO_TEST_CASE(testEventId) {
  ChimeraTK::Device device;
  device.open(DoocsLauncher::DoocsServer1);
  auto catalogue = device.getRegisterCatalogue();

  // Run update once to have the ZMQ variable's mp number updated
  DoocsServerTestHelper::runUpdate();

  // Get EqFct to access the mp counter for ZMQ
  auto eqfct = reinterpret_cast<eq_dummy*>(find_device("MYDUMMY"));

  // device info string
  BOOST_CHECK(device.readDeviceInfo() == "DOOCS server address: doocs://localhost:" + DoocsLauncher::rpc_no + "/F/D");

  for(std::string param : {"SOME_INT", "SOME_FLOAT", "SOME_BIT", "SOME_INT_ARRAY", "SOME_SHORT_ARRAY",
          "SOME_LONG_ARRAY", "SOME_FLOAT_ARRAY", "SOME_DOUBLE_ARRAY", "SOME_SPECTRUM", "SOME_IIII", "SOME_IFFF"}) {
    auto path = RegisterPath("MYDUMMY/" + param + "/eventId");
    BOOST_CHECK(catalogue.hasRegister(path));

    auto regInfo = catalogue.getRegister(path);
    BOOST_CHECK(regInfo->isReadable());
    BOOST_CHECK(!regInfo->isWriteable());
    BOOST_CHECK_EQUAL(regInfo->getNumberOfElements(), 1);
    BOOST_CHECK_EQUAL(regInfo->getNumberOfChannels(), 1);
    BOOST_CHECK_EQUAL(regInfo->getNumberOfDimensions(), 0);
    auto descriptor = regInfo->getDataDescriptor();
    BOOST_CHECK(descriptor.fundamentalType() == ChimeraTK::RegisterInfo::FundamentalType::numeric);
    BOOST_CHECK_EQUAL(descriptor.nDigits(), 20);
    BOOST_CHECK(descriptor.isSigned());
    BOOST_CHECK(descriptor.isIntegral());

    auto acc1 = device.getScalarRegisterAccessor<int64_t>(path);
    acc1.read();
    BOOST_CHECK_EQUAL(static_cast<int64_t>(acc1), eqfct->counter - 1);
    BOOST_CHECK_EQUAL(acc1.isReadOnly(), true);
    BOOST_CHECK_EQUAL(acc1.isWriteable(), false);
    BOOST_CHECK_EQUAL(acc1.isReadable(), true);
    BOOST_CHECK_THROW(acc1.write(), ChimeraTK::logic_error);

    auto acc2 = device.getScalarRegisterAccessor<std::string>(path);
    acc2.read();
    BOOST_CHECK_EQUAL(static_cast<std::string>(acc2), std::to_string(eqfct->counter - 1));
    BOOST_CHECK_EQUAL(acc2.isReadOnly(), true);
    BOOST_CHECK_EQUAL(acc2.isWriteable(), false);
    BOOST_CHECK_EQUAL(acc2.isReadable(), true);
    BOOST_CHECK_THROW(acc2.write(), ChimeraTK::logic_error);

    auto acc3 = device.getScalarRegisterAccessor<uint8_t>(path);
    BOOST_CHECK_THROW(acc3.read(), boost::numeric::positive_overflow);
  }

  // Check that we get the accessor for some unsupported type as well
  // Whether or not eventId retuns something usefull here is completely undefined
  auto acc1 = device.getScalarRegisterAccessor<int64_t>("MYDUMMY/UNSUPPORTED_DATA_TYPE/eventId");
  BOOST_CHECK(acc1.isInitialised());

  // Run update once to have the ZMQ variable's mp number updated
  DoocsServerTestHelper::runUpdate();

  auto path = RegisterPath("MYDUMMY/SOME_ZMQINT/eventId");
  BOOST_CHECK(catalogue.hasRegister(path));
  auto acc2 = device.getScalarRegisterAccessor<int64_t>(path);
  acc2.read();
  BOOST_CHECK_EQUAL(static_cast<int64_t>(acc2), eqfct->counter - 1);
  BOOST_CHECK_EQUAL(acc2.isReadOnly(), true);
  BOOST_CHECK_EQUAL(acc2.isWriteable(), false);
  BOOST_CHECK_EQUAL(acc2.isReadable(), true);
  BOOST_CHECK_THROW(acc2.write(), ChimeraTK::logic_error);

  device.close();
}

BOOST_AUTO_TEST_CASE(testTimeStamp) {
  ChimeraTK::Device device;
  device.open(DoocsLauncher::DoocsServer1);
  auto catalogue = device.getRegisterCatalogue();

  // Run update once to have the ZMQ variable's mp number updated
  DoocsServerTestHelper::runUpdate();

  // Get EqFct to access the mp counter for ZMQ
  auto eqfct = reinterpret_cast<eq_dummy*>(find_device("MYDUMMY"));

  // device info string
  BOOST_CHECK(device.readDeviceInfo() == "DOOCS server address: doocs://localhost:" + DoocsLauncher::rpc_no + "/F/D");

  for(std::string param : {"SOME_INT", "SOME_FLOAT", "SOME_BIT", "SOME_INT_ARRAY", "SOME_SHORT_ARRAY",
          "SOME_LONG_ARRAY", "SOME_FLOAT_ARRAY", "SOME_DOUBLE_ARRAY", "SOME_SPECTRUM", "SOME_IIII", "SOME_IFFF"}) {
    auto path = RegisterPath("MYDUMMY/" + param + "/timeStamp");
    BOOST_CHECK(catalogue.hasRegister(path));

    auto regInfo = catalogue.getRegister(path);
    BOOST_CHECK(regInfo->isReadable());
    BOOST_CHECK(!regInfo->isWriteable());
    BOOST_CHECK_EQUAL(regInfo->getNumberOfElements(), 1);
    BOOST_CHECK_EQUAL(regInfo->getNumberOfChannels(), 1);
    BOOST_CHECK_EQUAL(regInfo->getNumberOfDimensions(), 0);
    auto descriptor = regInfo->getDataDescriptor();
    BOOST_CHECK(descriptor.fundamentalType() == ChimeraTK::RegisterInfo::FundamentalType::numeric);
    BOOST_CHECK_EQUAL(descriptor.nDigits(), 20);
    BOOST_CHECK(descriptor.isSigned());
    BOOST_CHECK(descriptor.isIntegral());

    auto acc1 = device.getScalarRegisterAccessor<int64_t>(path);
    acc1.read();
    BOOST_CHECK_EQUAL(static_cast<int64_t>(acc1), eqfct->startTime - 1);
    BOOST_CHECK_EQUAL(acc1.isReadOnly(), true);
    BOOST_CHECK_EQUAL(acc1.isWriteable(), false);
    BOOST_CHECK_EQUAL(acc1.isReadable(), true);
    BOOST_CHECK_THROW(acc1.write(), ChimeraTK::logic_error);

    auto acc2 = device.getScalarRegisterAccessor<std::string>(path);
    acc2.read();
    BOOST_CHECK_EQUAL(static_cast<std::string>(acc2), std::to_string(eqfct->startTime - 1));
    BOOST_CHECK_EQUAL(acc2.isReadOnly(), true);
    BOOST_CHECK_EQUAL(acc2.isWriteable(), false);
    BOOST_CHECK_EQUAL(acc2.isReadable(), true);
    BOOST_CHECK_THROW(acc2.write(), ChimeraTK::logic_error);

    auto acc3 = device.getScalarRegisterAccessor<uint8_t>(path);
    BOOST_CHECK_THROW(acc3.read(), boost::numeric::positive_overflow);
  }

  // Check that we get the accessor for some unsupported type as well
  // Whether or not eventId retuns something usefull here is completely undefined
  auto acc1 = device.getScalarRegisterAccessor<int64_t>("MYDUMMY/UNSUPPORTED_DATA_TYPE/timeStamp");
  BOOST_CHECK(acc1.isInitialised());

  // Run update once to have the ZMQ variable's mp number updated
  DoocsServerTestHelper::runUpdate();

  auto path = RegisterPath("MYDUMMY/SOME_ZMQINT/timeStamp");
  BOOST_CHECK(catalogue.hasRegister(path));
  auto acc2 = device.getScalarRegisterAccessor<int64_t>(path);
  acc2.read();
  BOOST_CHECK_EQUAL(static_cast<int64_t>(acc2), eqfct->startTime - 1);
  BOOST_CHECK_EQUAL(acc2.isReadOnly(), true);
  BOOST_CHECK_EQUAL(acc2.isWriteable(), false);
  BOOST_CHECK_EQUAL(acc2.isReadable(), true);
  BOOST_CHECK_THROW(acc2.write(), ChimeraTK::logic_error);

  device.close();
}

BOOST_AUTO_TEST_CASE(testEventIdMapping) {
  ChimeraTK::Device device;
  device.open(DoocsLauncher::DoocsServer1);
  auto catalogue = device.getRegisterCatalogue();

  // Run update once to have the ZMQ variable's mp number updated
  DoocsServerTestHelper::runUpdate();

  auto acc1 = device.getScalarRegisterAccessor<int>("MYDUMMY/SOME_INT");
  auto acc2 = device.getScalarRegisterAccessor<float>("MYDUMMY/SOME_FLOAT");

  acc1.read();
  acc2.read();

  BOOST_CHECK_MESSAGE(acc1.getVersionNumber() == acc2.getVersionNumber(),
      static_cast<std::string>(acc1.getVersionNumber()) + " should be equal to " +
          static_cast<std::string>(acc2.getVersionNumber()));

  // update() will set a new eventId
  DoocsServerTestHelper::runUpdate();

  // After reading only one of the accessors, the version should now be different
  acc2.read();
  BOOST_CHECK_MESSAGE(acc1.getVersionNumber() != acc2.getVersionNumber(),
      static_cast<std::string>(acc1.getVersionNumber()) + " should not be equal to " +
          static_cast<std::string>(acc2.getVersionNumber()));

  // After reading acc1 as well, they should match up now
  acc1.read();
  BOOST_CHECK_MESSAGE(acc1.getVersionNumber() == acc2.getVersionNumber(),
      static_cast<std::string>(acc1.getVersionNumber()) + " should be equal to " +
          static_cast<std::string>(acc2.getVersionNumber()));
}
