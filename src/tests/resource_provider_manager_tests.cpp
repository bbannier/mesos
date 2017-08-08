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
namespace resource_provider {

struct NOPE : internal::tests::MesosTest {};

TEST_F(NOPE, NOPE)
{
  std::unique_ptr<mesos::state::Storage> storage{
    new mesos::state::InMemoryStorage{}};

  std::unique_ptr<mesos::state::State> state{
    new mesos::state::protobuf::State{storage.get()}};

  // state.fetch<state::protobuf::Variable<State>>("resource_provider_manager");

  const auto& fetch = state->fetch("resource_provider_manager");

  AWAIT_READY(fetch);
  Try<State> deserialize = protobuf::deserialize<State>(fetch->value());

  ASSERT_SOME(deserialize);
  EXPECT_TRUE(deserialize->resource_providers().empty());

  State deserialize_ = deserialize.get();
  deserialize_.add_resource_providers()->mutable_id()->set_value(
      "test_resource_provider");

  auto serialize = protobuf::serialize(deserialize_);
  ASSERT_SOME(serialize);

  auto fetch_ = fetch.get();
  fetch_.mutate(serialize.get());

  auto store = state->store(fetch_);

  AWAIT_READY(store);
  ASSERT_SOME(store.get());
  EXPECT_EQ(fetch_.value(), store.get()->value());
}

bool operator==(
    const State::ResourceProvider& lhs,
    const State::ResourceProvider& rhs)
{
  if (lhs.id() != rhs.id()) {
    return false;
  }

  return true;
}
bool operator!=(
    const State::ResourceProvider& lhs,
    const State::ResourceProvider& rhs)
{
  return !(lhs == rhs);
}

bool operator==(const State& lhs, const State& rhs)
{
  if (lhs.resource_providers_size() != rhs.resource_providers_size()) {
    return false;
  }

  for (int i = 0; i < lhs.resource_providers_size(); ++i) {
    if (lhs.resource_providers(i) != rhs.resource_providers(i)) {
      return false;
    }
  }

  return true;
}
bool operator!=(const State& lhs, const State& rhs) { return !(lhs == rhs); }

std::ostream& operator<<(
    std::ostream& stream,
    const State::ResourceProvider& resourceProvider)
{
  return stream << resourceProvider.id();
}

std::ostream& operator<<(std::ostream& stream, const State& state)
{
  std::string join = strings::join(", ", state.resource_providers());

  return stream << "{" << join << "}";
}

namespace state {

constexpr char RESOURCE_PROVIDER_MANAGER_STATE[] = "resource_provider_manager";

struct RegistrarProcess : process::Process<RegistrarProcess>
{
  std::unique_ptr<mesos::state::State> state_;

  std::unique_ptr<mesos::state::Variable> variable_;

  bool updating = false;

  explicit RegistrarProcess(std::unique_ptr<mesos::state::State> state)
    : state_(std::move(state)) {}

  Future<mesos::resource_provider::State> get()
  {
    if (updating) {
      return process::Failure("'get' calling while updating");
    }

    // FIXME(bbannier): handle `after`.
    return state_->fetch(RESOURCE_PROVIDER_MANAGER_STATE)
      .then(defer(
          self(),
          [this](const process::Future<mesos::state::Variable>& variable)
            -> Future<mesos::resource_provider::State> {
            Try<State> deserialize =
              protobuf::deserialize<State>(variable->value());

            if (deserialize.isError()) {
              return process::Failure(deserialize.error());
            }

            variable_.reset(new mesos::state::Variable(variable.get()));

            return deserialize.get();
          }));
  }

  Future<Nothing> set(const State& registry)
  {
    Try<string> serialize = protobuf::serialize(registry);

    if (serialize.isError()) {
      return process::Failure(serialize.error());
    }

    auto variable = variable_->mutate(serialize.get());

    if (updating) {
      return process::Failure("'set' called while updating");
    }

    updating = true;

    // FIXME(bbannier): handle `after`.
    return state_->store(variable)
      .onAny(defer(
          self(),
          [this](const Future<Option<mesos::state::Variable>>&) {
            updating = false;
          }))
      .then(defer(
          self(),
          [this](const Future<Option<mesos::state::Variable>>& variable)
            -> Future<Nothing> {
            CHECK_SOME(variable.get());

            variable_.reset(new mesos::state::Variable(variable->get()));

            updating = false;

            return Nothing();
          }));
  }
};

struct Registrar
{
  Registrar(std::unique_ptr<mesos::state::State> state)
    : registrarProcess_(new RegistrarProcess{std::move(state)})
  {
    process::spawn(*registrarProcess_, false);
  }

  Registrar(Registrar&&) = default;

  ~Registrar()
  {
    auto pid = registrarProcess_->self();

    process::terminate(pid);
    process::wait(pid);
  }

  Future<mesos::resource_provider::State> get()
  {
    return process::dispatch(*registrarProcess_, &RegistrarProcess::get);
  }

  Future<Nothing> set(const State& state_)
  {
    return process::dispatch(
        *registrarProcess_, &RegistrarProcess::set, state_);
  }

  std::unique_ptr<RegistrarProcess> registrarProcess_;
};

} // namespace state {


TEST_F(NOPE, NOPE2)
{
  std::unique_ptr<mesos::state::Storage> storage{
    new mesos::state::InMemoryStorage{}};

  std::unique_ptr<mesos::state::State> state_{
    new mesos::state::protobuf::State{storage.get()}};

  state::Registrar reg{std::move(state_)};

  {
    auto get = reg.get();
    AWAIT_READY(get);

    EXPECT_TRUE(get->resource_providers().empty());
  }

  mesos::resource_provider::State state;
  state.add_resource_providers()->mutable_id()->set_value("foo");
  ASSERT_FALSE(state.resource_providers().empty());

  {
    auto set = reg.set(state);
    AWAIT_READY(set);
  }

  {
    auto get = reg.get();
    AWAIT_READY(get);

    EXPECT_EQ(state, get.get());
  }
}

} // namespace resource_provider {
} // namespace mesos {
