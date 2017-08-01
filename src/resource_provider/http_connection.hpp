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

#include <glog/logging.h>

#include <functional>
#include <ostream>
#include <string>
#include <tuple>
#include <queue>

#include <mesos/http.hpp>

#include <mesos/v1/mesos.hpp>

#include <process/async.hpp>
#include <process/collect.hpp>
#include <process/defer.hpp>
#include <process/future.hpp>
#include <process/http.hpp>
#include <process/mutex.hpp>
#include <process/process.hpp>

#include <process/ssl/flags.hpp>

#include <stout/recordio.hpp>
#include <stout/result.hpp>

#include "common/http.hpp"
#include "common/recordio.hpp"

namespace mesos {
namespace internal {

/**
 * HTTP connection handler.
 *
 * Manages the connection to a HTTP endpoint as provided by the V1 API.
 *
 */
template <typename Call, typename Event>
class HttpConnectionProcess
  : public process::Process<HttpConnectionProcess<Call, Event>>
{
public:
  /**
   * Construct a HTTP connection process.
   *
   * @param id ID of the actor.
   * @param _endpoint
   * @param _contenType the content type expected by this connection.
   * @param validate a callback which will be invoked when a call needs
   *     to be validated.
   * @param connected a callback which will be invoked when the connection
   *     is established.
   * @param disconnected a callback which will be invoked when the
   *     connection is disconnected.
   * @param received a callback which will be be invoked when events
   *     are received.
   * @param _authorizationToken the 'Authorization' header of HTTP
   *     requests will use this token if set.
   */
  HttpConnectionProcess(
      const std::string& id,
      const process::http::URL& _endpoint,
      ContentType _contentType,
      const std::function<Option<Error>(const Call&)>& validate,
      const std::function<void(void)>& connected,
      const std::function<void(void)>& disconnected,
      const std::function<void(const std::queue<Event>&)>& received,
      const Option<std::string>& _authorizationToken)
    : process::ProcessBase(id),
      state(State::DISCONNECTED),
      endpoint(_endpoint),
      contentType(_contentType),
      callbacks {validate, connected, disconnected, received},
      authorizationToken(_authorizationToken) {}

  typedef HttpConnectionProcess<Call, Event> Self;

  void send(const Call& call)
  {
    Option<Error> error = callbacks.validate(call);

    if (error.isSome()) {
      LOG(WARNING) << "Dropping " << call.type() << ": " << error->message;
      return;
    }

    if (call.type() == Call::SUBSCRIBE && state != State::CONNECTED) {
      // It might be possible that the scheduler is retrying. We drop the
      // request if we have an ongoing subscribe request in flight or if the
      // scheduler is already subscribed.
      LOG(WARNING) << "Dropping " << call.type()
                   << ": Resource provider is in state " << state;
      return;
    }

    if (call.type() != Call::SUBSCRIBE && state != State::SUBSCRIBED) {
      // We drop all non-subscribe calls if we are not currently subscribed.
      LOG(WARNING) << "Dropping " << call.type()
                   << ": Resource provider is in state " << state;
      return;
    }

    VLOG(1) << "Sending " << call.type() << " call to " << endpoint;

    process::http::Request request;
    request.method = "POST";
    request.url = endpoint;
    request.body = serialize(contentType, call);
    request.keepAlive = true;
    request.headers = {{"Accept", stringify(contentType)},
                       {"Content-Type", stringify(contentType)}};

    if (authorizationToken.isSome()) {
      request.headers["Authorization"] = authorizationToken.get();
    }

    CHECK_SOME(connections);

    process::Future<process::http::Response> response;
    if (call.type() == Call::SUBSCRIBE) {
      state = State::SUBSCRIBING;

      // Send a streaming request for Subscribe call.
      response = connections->subscribe.send(request, true);
    } else {
      response = connections->nonSubscribe.send(request);
    }

    response.onAny(defer(this->self(),
                         &Self::_send,
                         call,
                         lambda::_1));
  }

protected:
  void initialize() override
  {
    connect();
  }

  void finalize() override
  {
    disconnect();
  }

  void connect()
  {
    CHECK_EQ(State::DISCONNECTED, state);

    state = State::CONNECTING;

    process::http::URL endpoint_ = endpoint;

    auto connector =
      [endpoint_]() -> process::Future<process::http::Connection> {
        return process::http::connect(endpoint_);
      };

    // We create two persistent connections here, one for subscribe
    // call/streaming response and another for non-subscribe calls/responses.
    collect(connector(), connector())
      .onAny(defer(this->self(), &Self::connected, lambda::_1));
  }

  void connected(
      const process::Future<std::tuple<
        process::http::Connection, process::http::Connection>>& _connections)
  {
    CHECK_EQ(State::CONNECTING, state);

    if (!_connections.isReady()) {
      disconnected(_connections.isFailed()
                     ? _connections.failure()
                     : "Connection future discarded");
      return;
    }

    VLOG(1) << "Connected with the remote endpoint at " << endpoint;

    state = State::CONNECTED;

    connections = Connections {
        std::get<0>(_connections.get()),
        std::get<1>(_connections.get())};

    connections->subscribe.disconnected()
      .onAny(defer(this->self(),
                   &Self::disconnected, "Subscribe connection interrupted"));

    connections->nonSubscribe.disconnected()
      .onAny(defer(
          this->self(),
          &Self::disconnected, "Non-subscribe connection interrupted"));

    // Invoke the connected callback once we have established both subscribe
    // and non-subscribe connections with the master.
    mutex.lock()
      .then(defer(this->self(), [this]() {
        return process::async(callbacks.connected);
      }))
      .onAny(lambda::bind(&process::Mutex::unlock, mutex));
  }

  void disconnect()
  {
    if (connections.isSome()) {
      connections->subscribe.disconnect();
      connections->nonSubscribe.disconnect();
    }

    if (subscribed.isSome()) {
      subscribed->reader.close();
    }

    state = State::DISCONNECTED;

    connections = None();
    subscribed = None();
  }

  void disconnected(const std::string& failure)
  {
    CHECK_NE(State::DISCONNECTED, state);

    bool connected = (state == State::CONNECTED ||
                      state == State::SUBSCRIBING ||
                      state == State::SUBSCRIBED);

    if (connected) {
      mutex.lock()
        .then(defer(this->self(), [this]() {
          return process::async(callbacks.disconnected);
        }))
        .onAny(lambda::bind(&process::Mutex::unlock, mutex));
    }

    disconnect();
  }

  void _send(
      const Call& call,
      const process::Future<process::http::Response>& response)
  {
    CHECK(!response.isDiscarded());
    CHECK(state == State::SUBSCRIBING || state == State::SUBSCRIBED) << state;

    if (response.isFailed()) {
      LOG(ERROR) << "Request for call type " << call.type() << " failed: "
                 << response.failure();
      return;
    }

    if (response->code == process::http::Status::OK) {
      // Only SUBSCRIBE call should get a "200 OK" response.
      CHECK_EQ(Call::SUBSCRIBE, call.type());
      CHECK_EQ(process::http::Response::PIPE, response->type);
      CHECK_SOME(response->reader);

      state = State::SUBSCRIBED;

      process::http::Pipe::Reader reader = response->reader.get();

      auto deserializer =
        lambda::bind(deserialize<Event>, contentType, lambda::_1);

      process::Owned<recordio::Reader<Event>> decoder(
          new recordio::Reader<Event>(
              ::recordio::Decoder<Event>(deserializer),
              reader));

      subscribed = SubscribedResponse { reader, decoder };

      read();

      return;
    }

    if (response->code == process::http::Status::ACCEPTED) {
      // Only non SUBSCRIBE calls should get a "202 Accepted" response.
      CHECK_NE(Call::SUBSCRIBE, call.type());
      return;
    }

    // We reset the state to connected if the subscribe call did not
    // succceed. The scheduler can then retry the subscribe call.
    if (call.type() == Call::SUBSCRIBE) {
      state = State::CONNECTED;
    }

    if (response->code == process::http::Status::SERVICE_UNAVAILABLE ||
        response->code == process::http::Status::NOT_FOUND) {
      LOG(WARNING) << "Received '" << response->status << "' ("
                   << response->body << ") for " << call.type();
      return;
    }

    LOG(WARNING) << "Received unexpected '" + response->status + "' ("
                 << response->body << ") for " << call.type();
  }

  void read()
  {
    subscribed->decoder->read()
      .onAny(defer(this->self(),
                   &Self::_read,
                   subscribed->reader,
                   lambda::_1));
  }

  void _read(
      const process::http::Pipe::Reader& reader,
      const process::Future<Result<Event>>& event)
  {
    CHECK(!event.isDiscarded());

    CHECK_EQ(State::SUBSCRIBED, state);

    if (event.isFailed()) {
      LOG(ERROR) << "Failed to decode stream of events: "
                 << event.failure();
      disconnected(event.failure());
      return;
    }

    if (event->isNone()) {
      const std::string error = "End-Of-File received";
      LOG(ERROR) << error;

      disconnected(error);
      return;
    }

    if (event->isError()) {
      LOG(ERROR) << "Failed to de-serialize event: " << event->error();
    } else {
      receive(event.get().get());
    }

    read();
  }

  void receive(const Event& event)
  {
    // Check if we're are no longer subscribed but received an event.
    if (state != State::SUBSCRIBED) {
      LOG(WARNING) << "Ignoring " << stringify(event.type())
                   << " event because we're no longer subscribed";
      return;
    }

    // Queue up the event and invoke the 'received' callback if this
    // is the first event (between now and when the 'received'
    // callback actually gets invoked more events might get queued).
    events.push(event);

    if (events.size() == 1) {
      mutex.lock()
        .then(defer(this->self(), [this]() {
          process::Future<Nothing> future =
            process::async(callbacks.received, events);
          events = std::queue<Event>();
          return future;
        }))
        .onAny(lambda::bind(&process::Mutex::unlock, mutex));
    }
  }

private:
  struct Callbacks
  {
    std::function<Option<Error>(const Call&)> validate;
    std::function<void(void)> connected;
    std::function<void(void)> disconnected;
    std::function<void(const std::queue<Event>&)> received;
  };

  struct Connections
  {
    process::http::Connection subscribe;
    process::http::Connection nonSubscribe;
  };

  struct SubscribedResponse
  {
    process::http::Pipe::Reader reader;
    process::Owned<recordio::Reader<Event>> decoder;
  };

  enum class State
  {
    DISCONNECTED, // Either of subscribe/non-subscribe connection is broken.
    CONNECTING, // Trying to establish subscribe and non-subscribe connections.
    CONNECTED, // Established subscribe and non-subscribe connections.
    SUBSCRIBING, // Trying to subscribe with the agent.
    SUBSCRIBED // Subscribed with the agent.
  };

  friend std::ostream& operator<<(std::ostream& stream, State state)
  {
    switch (state) {
      case State::DISCONNECTED: return stream << "DISCONNECTED";
      case State::CONNECTING:   return stream << "CONNECTING";
      case State::CONNECTED:    return stream << "CONNECTED";
      case State::SUBSCRIBING:  return stream << "SUBSCRIBING";
      case State::SUBSCRIBED:   return stream << "SUBSCRIBED";
    }

    UNREACHABLE();
  }

  State state;
  Option<Connections> connections;
  Option<SubscribedResponse> subscribed;
  const process::http::URL endpoint;
  const mesos::ContentType contentType;
  const Callbacks callbacks;
  const Option<std::string> authorizationToken;
  process::Mutex mutex; // Used to serialize the callback invocations.
  std::queue<Event> events;
};

} // namespace internal {
} // namespace mesos {
