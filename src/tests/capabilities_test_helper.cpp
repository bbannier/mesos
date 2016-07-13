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

#include <ostream>
#include <vector>

#include <stout/subcommand.hpp>
#include <stout/try.hpp>

#include <process/owned.hpp>

#include "common/parse.hpp"

#include "tests/capabilities_test_helper.hpp"

using std::cerr;
using std::endl;
using std::string;
using std::vector;

using process::Owned;

using namespace mesos::internal::capabilities;

namespace mesos {
namespace internal {
namespace tests {

const string CapabilitiesTestHelper::NAME = "CapabilitiesTestHelper";


CapabilitiesTestHelper::Flags::Flags()
{
  add(&userId,
      "userid",
      "Userid to be used for the test.");

  add(&net_capabilities,
      "net_capabilities",
      "Net process capabilities for the test.");

  // TODO(jojy): Add command as one of the flags.
}


static int executeTest(const CapabilitiesTestHelper::Flags& flags)
{
  Try<Owned<Capabilities>> capabilities = Capabilities::create();
  if (capabilities.isError()) {
    cerr << "Failed to instantiate `Capabilities`: "
         << capabilities.error() << endl;

    return -1;
  }

  if (flags.net_capabilities.isNone()) {
    cerr << "Missing net capabilities argument for the test" << endl;
    return -1;
  }

  if (flags.userId.isSome()) {
    Try<Nothing> keepCaps = capabilities.get()->keepCapabilitiesOnSetUid();
    if (capabilities.isError()) {
      cerr << "Failed to set process capabilities on uid change: "
           << keepCaps.error() << endl;

      return -1;
    }

    int rc = ::setuid(flags.userId.get());
    if (rc) {
      cerr << "Failed to set userid, return code: " << rc << endl;
      return -1;
    }
  }

  Try<Nothing> setExecCaps =
    capabilities.get()->setCapabilitiesOnExec(flags.net_capabilities.get());

  if (setExecCaps.isError()) {
    cerr << "Failed to set process's execute capabilities: "
         << setExecCaps.error() << endl;

    return -1;
  }

  // TODO(jojy): Get the command from flag.
  char const *execArgs[] = {"ping", "-c", "1", "localhost", NULL};

  ::execvp("ping", const_cast<char**>(execArgs));

  UNREACHABLE();
}


int CapabilitiesTestHelper::execute()
{
  return executeTest(flags);
}

} // namespace tests {
} // namespace internal {
} // namespace mesos {
