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

#include <string>

#include <gtest/gtest.h>

#include <mesos/http.hpp>
#include <mesos/resources.hpp>

#include <mesos/state/protobuf.hpp>

#include <mesos/v1/mesos.hpp>

#include <mesos/v1/resource_provider/resource_provider.hpp>

#include <process/clock.hpp>
#include <process/gmock.hpp>
#include <process/gtest.hpp>
#include <process/http.hpp>

#include <stout/duration.hpp>
#include <stout/error.hpp>
#include <stout/gtest.hpp>
#include <stout/lambda.hpp>
#include <stout/option.hpp>
#include <stout/protobuf.hpp>
#include <stout/recordio.hpp>
#include <stout/result.hpp>
#include <stout/stringify.hpp>
#include <stout/try.hpp>
#include <stout/unimplemented.hpp>

#include "common/http.hpp"
#include "common/recordio.hpp"

#include "internal/devolve.hpp"

#include "resource_provider/manager.hpp"

#include "slave/slave.hpp"

#include "tests/mesos.hpp"

namespace http = process::http;

using mesos::internal::slave::Slave;

using mesos::master::detector::MasterDetector;

using mesos::v1::resource_provider::Call;
using mesos::v1::resource_provider::Event;

using process::Clock;
using process::Future;
using process::Owned;
using process::PID;

using process::http::BadRequest;
using process::http::OK;
using process::http::UnsupportedMediaType;

using std::string;

using testing::Values;
using testing::WithParamInterface;

namespace mesos {
namespace internal {
namespace tests {

class ResourceProviderManagerHttpApiTest
  : public MesosTest,
    public WithParamInterface<ContentType> {};


// The tests are parameterized by the content type of the request.
INSTANTIATE_TEST_CASE_P(
    ContentType,
    ResourceProviderManagerHttpApiTest,
    Values(ContentType::PROTOBUF, ContentType::JSON));


TEST_F(ResourceProviderManagerHttpApiTest, NoContentType)
{
  http::Request request;
  request.method = "POST";
  request.headers = createBasicAuthHeaders(DEFAULT_CREDENTIAL);

  ResourceProviderManager manager;

  Future<http::Response> response = manager.api(request, None());

  AWAIT_EXPECT_RESPONSE_STATUS_EQ(BadRequest().status, response);
  AWAIT_EXPECT_RESPONSE_BODY_EQ(
      "Expecting 'Content-Type' to be present",
      response);
}


// This test sends a valid JSON blob that cannot be deserialized
// into a valid protobuf resulting in a BadRequest.
TEST_F(ResourceProviderManagerHttpApiTest, ValidJsonButInvalidProtobuf)
{
  JSON::Object object;
  object.values["string"] = "valid_json";

  http::Request request;
  request.method = "POST";
  request.headers = createBasicAuthHeaders(DEFAULT_CREDENTIAL);
  request.headers["Accept"] = APPLICATION_JSON;
  request.headers["Content-Type"] = APPLICATION_JSON;
  request.body = stringify(object);

  ResourceProviderManager manager;

  Future<http::Response> response = manager.api(request, None());

  AWAIT_EXPECT_RESPONSE_STATUS_EQ(BadRequest().status, response);
  AWAIT_EXPECT_RESPONSE_BODY_EQ(
      "Failed to validate resource_provider::Call: "
      "Expecting 'type' to be present",
      response);
}


TEST_P(ResourceProviderManagerHttpApiTest, MalformedContent)
{
  const ContentType contentType = GetParam();

  http::Request request;
  request.method = "POST";
  request.headers = createBasicAuthHeaders(DEFAULT_CREDENTIAL);
  request.headers["Accept"] = stringify(contentType);
  request.headers["Content-Type"] = stringify(contentType);
  request.body = "MALFORMED_CONTENT";

  ResourceProviderManager manager;

  Future<http::Response> response = manager.api(request, None());

  AWAIT_EXPECT_RESPONSE_STATUS_EQ(BadRequest().status, response);
  switch (contentType) {
    case ContentType::PROTOBUF:
      AWAIT_EXPECT_RESPONSE_BODY_EQ(
          "Failed to parse body into Call protobuf",
          response);
      break;
    case ContentType::JSON:
      AWAIT_EXPECT_RESPONSE_BODY_EQ(
          "Failed to parse body into JSON: "
          "syntax error at line 1 near: MALFORMED_CONTENT",
          response);
      break;
    case ContentType::RECORDIO:
      break;
  }
}


TEST_P(ResourceProviderManagerHttpApiTest, UnsupportedContentMediaType)
{
  Call call;
  call.set_type(Call::SUBSCRIBE);

  Call::Subscribe* subscribe = call.mutable_subscribe();

  mesos::v1::ResourceProviderInfo* info =
    subscribe->mutable_resource_provider_info();

  info->set_type("org.apache.mesos.rp.test");
  info->set_name("test");

  const ContentType contentType = GetParam();
  const string unknownMediaType = "application/unknown-media-type";

  http::Request request;
  request.method = "POST";
  request.headers = createBasicAuthHeaders(DEFAULT_CREDENTIAL);
  request.headers["Accept"] = stringify(contentType);
  request.headers["Content-Type"] = unknownMediaType;
  request.body = serialize(contentType, call);

  ResourceProviderManager manager;

  Future<http::Response> response = manager.api(request, None());

  AWAIT_EXPECT_RESPONSE_STATUS_EQ(UnsupportedMediaType().status, response)
    << response->body;
}


TEST_P(ResourceProviderManagerHttpApiTest, Subscribe)
{
  Call call;
  call.set_type(Call::SUBSCRIBE);

  Call::Subscribe* subscribe = call.mutable_subscribe();

  mesos::v1::ResourceProviderInfo* info =
    subscribe->mutable_resource_provider_info();

  info->set_type("org.apache.mesos.rp.test");
  info->set_name("test");

  const ContentType contentType = GetParam();

  http::Request request;
  request.method = "POST";
  request.headers = createBasicAuthHeaders(DEFAULT_CREDENTIAL);
  request.headers["Accept"] = stringify(contentType);
  request.headers["Content-Type"] = stringify(contentType);
  request.body = serialize(contentType, call);

  ResourceProviderManager manager;

  Future<http::Response> response = manager.api(request, None());

  AWAIT_EXPECT_RESPONSE_STATUS_EQ(OK().status, response) << response->body;
  ASSERT_EQ(http::Response::PIPE, response->type);

  Option<http::Pipe::Reader> reader = response->reader;
  ASSERT_SOME(reader);

  recordio::Reader<Event> responseDecoder(
      ::recordio::Decoder<Event>(
          lambda::bind(deserialize<Event>, contentType, lambda::_1)),
      reader.get());

  Future<Result<Event>> event = responseDecoder.read();
  AWAIT_READY(event);
  ASSERT_SOME(event.get());

  // Check event type is subscribed and the resource provider id is set.
  ASSERT_EQ(Event::SUBSCRIBED, event->get().type());
  ASSERT_NE("", event->get().subscribed().provider_id().value());
}


// This test starts an agent and connects directly with its resource
// provider endpoint.
TEST_P(ResourceProviderManagerHttpApiTest, AgentEndpoint)
{
  Try<Owned<cluster::Master>> master = StartMaster();
  ASSERT_SOME(master);

  Future<Nothing> __recover = FUTURE_DISPATCH(_, &Slave::__recover);

  Owned<MasterDetector> detector = master.get()->createDetector();

  Try<Owned<cluster::Slave>> agent = StartSlave(detector.get());
  ASSERT_SOME(agent);

  AWAIT_READY(__recover);

  // Wait for recovery to be complete.
  Clock::pause();
  Clock::settle();

  Call call;
  call.set_type(Call::SUBSCRIBE);

  Call::Subscribe* subscribe = call.mutable_subscribe();

  mesos::v1::ResourceProviderInfo* info =
    subscribe->mutable_resource_provider_info();

  info->set_type("org.apache.mesos.rp.test");
  info->set_name("test");

  const ContentType contentType = GetParam();

  http::Headers headers = createBasicAuthHeaders(DEFAULT_CREDENTIAL);
  headers["Accept"] = stringify(contentType);

  Future<http::Response> response = http::streaming::post(
      agent.get()->pid,
      "api/v1/resource_provider",
      headers,
      serialize(contentType, call),
      stringify(contentType));

  AWAIT_EXPECT_RESPONSE_STATUS_EQ(OK().status, response);
  AWAIT_EXPECT_RESPONSE_HEADER_EQ("chunked", "Transfer-Encoding", response);
  ASSERT_EQ(http::Response::PIPE, response->type);

  Option<http::Pipe::Reader> reader = response->reader;
  ASSERT_SOME(reader);

  recordio::Reader<Event> responseDecoder(
      ::recordio::Decoder<Event>(
          lambda::bind(deserialize<Event>, contentType, lambda::_1)),
      reader.get());

  Future<Result<Event>> event = responseDecoder.read();
  AWAIT_READY(event);
  ASSERT_SOME(event.get());

  // Check event type is subscribed and the resource provider id is set.
  ASSERT_EQ(Event::SUBSCRIBED, event->get().type());
  ASSERT_NE("", event->get().subscribed().provider_id().value());
}


class ResourceProviderManagerTest : public ::testing::Test
{
public:
  ResourceProviderManagerTest()
    : endpointProcess(&resourceProviderManager),
      pid(process::spawn(endpointProcess, false)) {}

  ~ResourceProviderManagerTest()
  {
    process::terminate(pid);
    process::wait(pid);
  }

  struct EndpointProcess : process::Process<EndpointProcess>
  {
    explicit EndpointProcess(ResourceProviderManager* _resourceProviderManager)
      : resourceProviderManager(_resourceProviderManager) {}

    void initialize() override
    {
      route(
          "/api/v1/resource_provider",
          None(),
          defer(self(), [this](const http::Request& request) {
            return resourceProviderManager->api(request, None());
          }));
    }

    ResourceProviderManager* resourceProviderManager;
  };

  ResourceProviderManager resourceProviderManager;
  EndpointProcess endpointProcess;
  const PID<EndpointProcess> pid;
};


// This test checks that subscribed resource providers are reflected in resource
// provider manager messages.
TEST_F(ResourceProviderManagerTest, Subscribe)
{
  auto resourceProvider =
    std::make_shared<v1::MockResourceProvider>();

  Future<Nothing> connected;
  EXPECT_CALL(*resourceProvider, connected())
    .WillOnce(FutureSatisfy(&connected));

  v1::resource_provider::TestDriver driver(
      pid,
      ContentType::PROTOBUF,
      resourceProvider);

  AWAIT_READY(connected);

  // Form and send a SUBSCRIBE call to register the resource provider.
  Future<Event::Subscribed> subscribed;
  EXPECT_CALL(*resourceProvider, subscribed(_))
    .WillOnce(FutureArg<0>(&subscribed));

  Call call;
  call.set_type(Call::SUBSCRIBE);

  Call::Subscribe* subscribe = call.mutable_subscribe();

  const v1::Resources resources = v1::Resources::parse("disk:4").get();
  subscribe->mutable_resources()->CopyFrom(resources);

  mesos::v1::ResourceProviderInfo* resourceProviderInfo =
    subscribe->mutable_resource_provider_info();

  resourceProviderInfo->set_name("test");
  resourceProviderInfo->set_type("org.apache.resource_provider.test");

  driver.send(call);

  // The response contains the assigned resource provider id.
  AWAIT_READY(subscribed);

  const mesos::v1::ResourceProviderID& resourceProviderId =
    subscribed->provider_id();

  // The manager will send out a message informing its subscriber
  // about the newly added resources.
  Future<ResourceProviderMessage> message =
    resourceProviderManager.messages().get();

  AWAIT_READY(message);

  EXPECT_EQ(
      ResourceProviderMessage::Type::UPDATE_TOTAL_RESOURCES,
      message->type);

  // We expect `ResourceProviderID`s to be set for all subscribed resources.
  // Inject them into the test expectation.
  Resources expectedResources;
  foreach (v1::Resource resource, resources) {
    resource.mutable_provider_id()->CopyFrom(resourceProviderId);
    expectedResources += devolve(resource);
  }

  EXPECT_EQ(devolve(resourceProviderId), message->updateTotalResources->id);
  EXPECT_EQ(expectedResources, message->updateTotalResources->total);
}

} // namespace tests {
} // namespace internal {
} // namespace mesos {


namespace mesos {
namespace internal {

class AdmitResourceProvider : public master::Operation {
public:
  explicit AdmitResourceProvider(const ResourceProviderID& id) : id_(id) {}
  AdmitResourceProvider(const AdmitResourceProvider&) = default;

private:
  Try<bool> perform(Registry* registry, hashset<SlaveID>*) override
  {
    if (std::find_if(
            registry->resource_providers().begin(),
            registry->resource_providers().end(),
            [this](const Registry::ResourceProvider& resourceProvider) {
              // FIXME(bbannier): use proper op.
              return resourceProvider.id() == this->id_;
            }) != registry->resource_providers().end()) {
      return Error("Resource provider already admitted");
    }

    Registry::ResourceProvider resourceProvider;
    resourceProvider.mutable_id()->CopyFrom(id_);
    registry->add_resource_providers()->CopyFrom(resourceProvider);


    return true; // Mutation.
  }

  const ResourceProviderID id_;
};

namespace slave {
namespace paths {
constexpr char RESOURCE_PROVIDER_INFO_FILE[] = "resource_providers.info";

string getResourcesProviderInfoPath(const string& rootDir)
{
  return path::join(rootDir, "resource_providers", RESOURCE_PROVIDER_INFO_FILE);
}
} // namespace paths

namespace state {
// FIXME(bbannier): Add this to slave::state::State.
struct ResourceProviderState
{
  hashset<ResourceProviderID> ids;

  // FIXME(bbannier): Add this to slave::state::recover.
  static Try<ResourceProviderState> recover(const string& rootDir, bool strict)
  {
    ResourceProviderState state;
    // Process the committed resources.
    const string& infoPath = paths::getResourcesProviderInfoPath(rootDir);
    if (!os::exists(infoPath)) {
      LOG(INFO) << "No committed checkpointed resources found at '" << infoPath
                << "'";
      return state;
  }

  ////////////////////////////////////////////////////////////////////////

  Try<int_fd> fd = os::open(infoPath, O_RDWR | O_CLOEXEC);
  if (fd.isError()) {
    string message =
      "Failed to open resource provider file '" + infoPath + "': " + fd.error();

    if (strict) {
      return Error(message);
    } else {
      LOG(WARNING) << message;
      return state;
    }
    }


    Result<ResourceProviderID> resourceProviderId = None();
    while (true) {
      // Ignore errors due to partial protobuf read and enable undoing
      // failed reads by reverting to the previous seek position.
      resourceProviderId =
        ::protobuf::read<ResourceProviderID>(fd.get(), true, true);
      if (!resourceProviderId.isSome()) {
        break;
      }

      CHECK_SOME(resourceProviderId);

      state.ids.insert(resourceProviderId.get());
    }

    if (resourceProviderId.isError()) {
      // FIXME(bbannier): error handling.
    }

    os::close(fd.get());

    return state;
  }
};

inline Try<Nothing> checkpoint(
    const std::string& path,
    const hashset<ResourceProviderID>& resourceProviders)
{
  google::protobuf::RepeatedPtrField<ResourceProviderID> resourceProviders_{
    resourceProviders.begin(), resourceProviders.end()};

  return checkpoint(path, resourceProviders_);
}

} // namespace state {
} // namespace slave {


class NOPE: public tests::MesosTest {};

TEST_F(NOPE, Master)
{
  std::unique_ptr<mesos::state::Storage> storage{
    new mesos::state::InMemoryStorage{}};
  auto state = mesos::state::protobuf::State(storage.get());
  auto registrar = master::Registrar{
    CreateMasterFlags(), &state, master::READONLY_HTTP_AUTHENTICATION_REALM};

  MasterInfo masterInfo;
  masterInfo.set_id("master_id");
  masterInfo.set_ip(0);
  masterInfo.set_port(0);

  registrar.recover(masterInfo);

  ResourceProviderID resourceProviderId;
  resourceProviderId.set_value("foo");

  {
    auto apply = registrar.apply(Owned<master::Operation>{
      new AdmitResourceProvider{resourceProviderId}});

    AWAIT_ASSERT_READY(apply);
    EXPECT_TRUE(apply.get());
  }

  {
    auto apply = registrar.apply(Owned<master::Operation>{
      new AdmitResourceProvider{resourceProviderId}});

    AWAIT_READY(apply);
    EXPECT_FALSE(apply.get());
  }
}


TEST_F(NOPE, Agent)
{
  const auto rootDir = os::getcwd();

  const auto path = slave::paths::getMetaRootDir(rootDir);

  {
    Try<slave::state::ResourceProviderState> state =
      slave::state::ResourceProviderState::recover(path, true);

    ASSERT_SOME(state);
    EXPECT_TRUE(state->ids.empty());
  }

  hashset<ResourceProviderID> resourceProviderIds;
  for (int i = 0; i < 100; ++i) {
    ResourceProviderID resourceProviderId;
    resourceProviderId.set_value(stringify(i));
    resourceProviderIds.insert(resourceProviderId);
  }

  slave::state::checkpoint(path, resourceProviderIds);

  {
    Try<slave::state::ResourceProviderState> state =
      slave::state::ResourceProviderState::recover(path, true);

    ASSERT_SOME(state);
    EXPECT_EQ(resourceProviderIds, state->ids);
  }
}

} // namespace internal {
} // namespace mesos {
