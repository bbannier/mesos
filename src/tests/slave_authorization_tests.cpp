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
// Unless required by applicable law or agreed to in writiDng, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <string>

#include <gmock/gmock.h>

#include <process/clock.hpp>
#include <process/http.hpp>
#include <process/owned.hpp>

#include <stout/try.hpp>

#include "master/master.hpp"

#include "tests/mesos.hpp"

namespace http = process::http;

using mesos::internal::master::Master;

using mesos::internal::slave::Slave;

using mesos::master::detector::MasterDetector;

using process::Clock;
using process::Future;
using process::Owned;

using process::http::Forbidden;
using process::http::OK;
using process::http::Response;

using std::string;

using testing::DoAll;

namespace mesos {
namespace internal {
namespace tests {

class SlaveAuthorizationTest : public MesosTest {};


// This test verifies that only authorized principals can access the
// '/flags' endpoint.
TEST_F(SlaveAuthorizationTest, AuthorizeFlagsEndpoint)
{
  // Setup ACLs so that only the default principal can access the '/flags'
  // endpoint.
  ACLs acls;
  acls.set_permissive(false);

  mesos::ACL::AccessEndpoint* acl1 = acls.add_access_endpoints();
  acl1->mutable_principals()->add_values(DEFAULT_CREDENTIAL.principal());
  acl1->mutable_paths()->add_values("/flags");

  slave::Flags slaveFlags = CreateSlaveFlags();
  slaveFlags.acls = acls;

  Try<Owned<cluster::Master>> master = StartMaster();
  ASSERT_SOME(master);

  Owned<MasterDetector> detector = master.get()->createDetector();
  Try<Owned<cluster::Slave>> slave = StartSlave(detector.get(), slaveFlags);
  ASSERT_SOME(slave);

  Future<Response> response = http::get(
      slave.get()->pid,
      "flags",
      None(),
      createBasicAuthHeaders(DEFAULT_CREDENTIAL));

  AWAIT_EXPECT_RESPONSE_STATUS_EQ(OK().status, response)
    << response.get().body;

  response = http::get(
      slave.get()->pid,
      "flags",
      None(),
      createBasicAuthHeaders(DEFAULT_CREDENTIAL_2));

  AWAIT_EXPECT_RESPONSE_STATUS_EQ(Forbidden().status, response)
    << response.get().body;
}


TEST_F(SlaveAuthorizationTest, AuthorizeFlagsEndpointWithoutPrincipal)
{
  Try<Owned<cluster::Master>> master = StartMaster();
  ASSERT_SOME(master);

  // Because the authenticators' lifetime is tied to libprocess's lifetime,
  // it may already be set by other tests. We have to unset it here to disable
  // HTTP authentication.
  http::authentication::unsetAuthenticator(
      slave::DEFAULT_HTTP_AUTHENTICATION_REALM);

  slave::Flags slaveFlags = CreateSlaveFlags();
  slaveFlags.authenticate_http = false;
  slaveFlags.http_credentials = None();

  Owned<MasterDetector> detector = master.get()->createDetector();
  Try<Owned<cluster::Slave>> slave = StartSlave(detector.get(), slaveFlags);
  ASSERT_SOME(slave);

  Future<Response> response = http::get(slave.get()->pid, "flags");

  AWAIT_EXPECT_RESPONSE_STATUS_EQ(OK().status, response)
    << response.get().body;
}


// This test checks that an agent's statistics endpoint is authorized.
TEST_F(SlaveAuthorizationTest, StatisticsEndpointAuthorization)
{
  Try<Owned<cluster::Master>> master = StartMaster();
  ASSERT_SOME(master);

  Owned<MasterDetector> detector = master.get()->createDetector();

  MockAuthorizer mockAuthorizer;

  Try<Owned<cluster::Slave>> agent =
    StartSlave(detector.get(), &mockAuthorizer);
  ASSERT_SOME(agent);

  const string statisticsEndpoints[] =
    {"monitor/statistics", "monitor/statistics.json"};

  foreach (const string& statisticsEndpoint, statisticsEndpoints) {
    // Test that the endpoint handler forms correct queries against
    // the authorizer.
    {
      Future<authorization::Request> request;
      EXPECT_CALL(mockAuthorizer, authorized(_))
        .WillOnce(DoAll(FutureArg<0>(&request), Return(true)));

      Future<Response> response = process::http::get(
          agent.get()->pid,
          statisticsEndpoint,
          None(),
          createBasicAuthHeaders(DEFAULT_CREDENTIAL));

      AWAIT_EXPECT_RESPONSE_STATUS_EQ(OK().status, response)
          << response.get().body;

      AWAIT_READY(request);

      const string principal = DEFAULT_CREDENTIAL.principal();
      EXPECT_EQ(principal, request.get().subject().value());

      EXPECT_EQ(authorization::ACCESS_ENDPOINT_WITH_PATH,
                request.get().action());

      EXPECT_EQ("/" + statisticsEndpoint, request.get().object().value());
    }

    // Test that unauthorized requests are properly rejected.
    {
      EXPECT_CALL(mockAuthorizer, authorized(_))
        .WillOnce(Return(false));

      Future<Response> response = process::http::get(
          agent.get()->pid,
          statisticsEndpoint,
          None(),
          createBasicAuthHeaders(DEFAULT_CREDENTIAL));

      AWAIT_EXPECT_RESPONSE_STATUS_EQ(Forbidden().status, response);
    }

    // Test that without an active authorizer authorizations always succeed.
    {
      Try<Owned<cluster::Slave>> agent = cluster::Slave::start(
          detector.get(),
          CreateSlaveFlags(),
          None(),
          None(),
          None(),
          None(),
          None(),
          None(),
          None());
      ASSERT_SOME(agent);

      Future<Response> response = process::http::get(
          agent.get()->pid,
          statisticsEndpoint,
          None(),
          createBasicAuthHeaders(DEFAULT_CREDENTIAL));

      AWAIT_EXPECT_RESPONSE_STATUS_EQ(OK().status, response);
    }
  }
}

} // namespace tests {
} // namespace internal {
} // namespace mesos {
