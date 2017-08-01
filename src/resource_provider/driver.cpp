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

#include <mesos/v1/resource_provider.hpp>

#include <string>

#include <process/dispatch.hpp>
#include <process/http.hpp>
#include <process/process.hpp>

#include <stout/base64.hpp>

#include "internal/devolve.hpp"

#include "resource_provider/http_connection.hpp"
#include "resource_provider/validation.hpp"

using process::dispatch;
using process::spawn;
using process::terminate;
using process::UPID;
using process::wait;

using std::function;
using std::string;
using std::queue;

namespace {

process::http::URL endpointURL(const process::UPID& upid)
{
  string scheme = "http";

#ifdef USE_SSL_SOCKET
  if (process::network::openssl::flags().enabled) {
    scheme = "https";
  }
#endif // USE_SSL_SOCKET

  return process::http::URL(
      scheme,
      upid.address.ip,
      upid.address.port,
      upid.id + "/api/v1/resource_provider");
}


Option<Error> validate(const mesos::v1::resource_provider::Call& call)
{
  return mesos::internal::resource_provider::validation::call::validate(
      mesos::internal::devolve(call));
}


Option<string> authenticationToken(
    const Option<mesos::v1::Credential>& credential)
{
  if (credential.isSome()) {
    return "Basic " +
           base64::encode(credential->principal() + ":" + credential->secret());
  }

  return None();
}

} // namespace {

namespace mesos {
namespace v1 {
namespace resource_provider {

Driver::Driver(
    const UPID& upid,
    ContentType contentType,
    const function<void(void)>& connected,
    const function<void(void)>& disconnected,
    const function<void(const queue<Event>&)>& received,
    const Option<Credential>& credential)
  : process(new DriverProcess(
        process::ID::generate("resource-provider-driver"),
        endpointURL(upid),
        contentType,
        validate,
        connected,
        disconnected,
        received,
        authenticationToken(credential)))
{
  spawn(CHECK_NOTNULL(process.get()));
}


Driver::~Driver()
{
  terminate(process.get());
  wait(process.get());
}


void Driver::send(const Call& call)
{
  dispatch(process.get(), &DriverProcess::send, call);
}

} // namespace resource_provider {
} // namespace v1 {
} // namespace mesos {
