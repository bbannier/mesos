// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <pwd.h>

#include <iostream>
#include <string>
#include <tuple>
#include <vector>

#include <gmock/gmock.h>

#include <gtest/gtest.h>

#include <process/collect.hpp>
#include <process/io.hpp>
#include <process/owned.hpp>
#include <process/subprocess.hpp>

#include <stout/os.hpp>
#include <stout/path.hpp>

#include "common/status_utils.hpp"

#include "linux/capabilities.hpp"

#include "tests/capabilities_test_helper.hpp"
#include "tests/mesos.hpp"
#include "tests/utils.hpp"

using std::string;
using std::tuple;
using std::vector;

using process::Failure;
using process::Future;
using process::Owned;
using process::Subprocess;

namespace mesos {
namespace internal {
namespace tests {

using namespace capabilities;

// Test fixture for testing capabilities API.
class CapabilitiesTest : public ::testing::Test
{
protected:
  const string CAPS_TEST_UNPRIVILEGED_USER1 = "mesos.test.caps.user1";

  Try<Set<Capability>> getPermittedCapabilities() const
  {
    Try<Owned<Capabilities>> capabilities = Capabilities::create();
    if (capabilities.isError()) {
      return Error(
          "Failed to instantiate `Capabilities`: " + capabilities.error());
    }

    Try<ProcessCapabilities> procCaps = capabilities.get()->get();
    if (procCaps.isError()) {
      return Error("Failed to get process capabilities: " + procCaps.error());
    }

    return procCaps.get().get(PERMITTED);
  }

  Try<Subprocess> launchTest(const CapabilitiesTestHelper::Flags& flags)
  {
    vector<string> commandArgv;
    commandArgv.push_back("capabilities-test-helper");
    commandArgv.push_back(CapabilitiesTestHelper::NAME);

    return subprocess(
        getTestHelperPath("capabilities-test-helper"),
        commandArgv,
        Subprocess::FD(STDIN_FILENO),
        Subprocess::FD(STDOUT_FILENO),
        Subprocess::FD(STDERR_FILENO),
        process::NO_SETSID,
        flags,
        None(),
        lambda::bind(&os::clone, lambda::_1, SIGCHLD));
  }

  void addUser(const string& user)
  {
    // Currently supporting only 1 user. But the idea is to allow
    // tests that support multiple users.
    ASSERT_TRUE(user == CAPS_TEST_UNPRIVILEGED_USER1);

    ASSERT_EQ(0, os::system("useradd " + user));

    usersAdded.emplace_back(user);
  }

  void removeUser(const string& user)
  {
    os::system("userdel -r " + user + " > /dev/null");
  }

  void TearDown()
  {
    // Remove all users created during test run.
    foreach (const string& user, usersAdded) {
      removeUser(user);
    }
  }

private:
  vector<string> usersAdded;
};


// Test for verifying that an operation that needs NET_RAW capability does
// not succeed if the capability NET_RAW is dropped for the process.
TEST_F(CapabilitiesTest, ROOT_PingWithNoNetRawCaps)
{
  removeUser(CAPS_TEST_UNPRIVILEGED_USER1);

  addUser(CAPS_TEST_UNPRIVILEGED_USER1);

  CapabilitiesTestHelper::Flags flags;

  Try<Set<Capability>> _permSet = getPermittedCapabilities();
  ASSERT_SOME(_permSet);

  Set<Capability> &permSet = _permSet.get();

  // Disable NET_RAW capability flag.
  permSet.erase(NET_RAW);

  flags.net_capabilities = capabilities::toCapabilityInfo(permSet);

  Try<Subprocess> s = launchTest(flags);
  ASSERT_SOME(s);

  Future<Option<int>> status = s.get().status();
  AWAIT_READY(status);

  ASSERT_SOME(status.get());
  EXPECT_TRUE(WIFEXITED(status.get().get()));

  // The exit status of test helper process is the underlying
  // command's return status.
  // Verify that test exits with status 2(Operation not permitted).
  EXPECT_EQ(2, WEXITSTATUS(status.get().get()));
}


// Test for verifying that capabilities can be inherited after 'setuid'
// system call. This test verifies the `keepCapabilitiesOnSetUid` API.
TEST_F(CapabilitiesTest, ROOT_PingWithInheritedCaps)
{
  removeUser(CAPS_TEST_UNPRIVILEGED_USER1);

  addUser(CAPS_TEST_UNPRIVILEGED_USER1);

  Result<uid_t> uid = os::getuid(CAPS_TEST_UNPRIVILEGED_USER1);
  ASSERT_SOME(uid);

  CapabilitiesTestHelper::Flags flags;

  flags.userId = uid.get();

  Try<Set<Capability>> _permSet = getPermittedCapabilities();
  ASSERT_SOME(_permSet);

  Set<Capability> &permSet = _permSet.get();

  flags.net_capabilities = capabilities::toCapabilityInfo(permSet);

  Try<Subprocess> s = launchTest(flags);
  ASSERT_SOME(s);

  Future<Option<int>> status = s.get().status();
  AWAIT_READY(status);

  ASSERT_SOME(status.get());
  EXPECT_TRUE(WIFEXITED(status.get().get()));

  // The exit status of test helper process is the underlying
  // command's return status.
  // Verify that test exits with status 0.
  EXPECT_EQ(0, WEXITSTATUS(status.get().get()));
}


// Test for verifying that 'effective' capabilities of a process can
// be controlled after 'setuid' system call.
TEST_F(CapabilitiesTest, ROOT_PingWithInheritedCapsNetRawDropped)
{
  removeUser(CAPS_TEST_UNPRIVILEGED_USER1);

  addUser(CAPS_TEST_UNPRIVILEGED_USER1);

  Result<uid_t> uid = os::getuid(CAPS_TEST_UNPRIVILEGED_USER1);
  ASSERT_SOME(uid);

  CapabilitiesTestHelper::Flags flags;
  flags.userId = uid.get();

  Try<Set<Capability>> _permSet = getPermittedCapabilities();
  ASSERT_SOME(_permSet);

  Set<Capability> &permSet = _permSet.get();

  // Disable NET_RAW capability flag.
  permSet.erase(NET_RAW);

  flags.net_capabilities = capabilities::toCapabilityInfo(permSet);

  Try<Subprocess> s = launchTest(flags);
  ASSERT_SOME(s);

  Future<Option<int>> status = s.get().status();
  AWAIT_READY(status);

  ASSERT_SOME(status.get());
  EXPECT_TRUE(WIFEXITED(status.get().get()));

  // The exit status of test helper process is the underlying
  // command's return status.
  // Verify that test exits with status 2(Operation not permitted).
  EXPECT_EQ(2, WEXITSTATUS(status.get().get()));
}


// Test for verifying that `ping` would work with just the minimum
// capability that itt requires.
TEST_F(CapabilitiesTest, ROOT_PingWithJustNetRawCap)
{
  removeUser(CAPS_TEST_UNPRIVILEGED_USER1);

  addUser(CAPS_TEST_UNPRIVILEGED_USER1);

  Result<uid_t> uid = os::getuid(CAPS_TEST_UNPRIVILEGED_USER1);
  ASSERT_SOME(uid);

  CapabilitiesTestHelper::Flags flags;
  flags.userId = uid.get();

  // Just set `NET_RAW` capability for the process.
  Set<Capability> capSet = {NET_RAW};

  flags.net_capabilities = capabilities::toCapabilityInfo(capSet);

  Try<Subprocess> s = launchTest(flags);
  ASSERT_SOME(s);

  Future<Option<int>> status = s.get().status();
  AWAIT_READY(status);

  ASSERT_SOME(status.get());
  EXPECT_TRUE(WIFEXITED(status.get().get()));

  // The exit status of test helper process is the underlying
  // command's return status.
  // Verify that test exits with status 2(Operation not permitted).
  EXPECT_EQ(0, WEXITSTATUS(status.get().get()));
}

} // namespace tests {
} // namespace internal {
} // namespace mesos {
