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
#include <utility>

#include <mesos/resource_provider/resource_provider.hpp>

#include <process/defer.hpp>
#include <process/dispatch.hpp>
#include <process/process.hpp>

#include <stout/check.hpp>
#include <stout/option.hpp>
#include <stout/protobuf.hpp>
#include <stout/try.hpp>

#include "resource_provider/state.hpp"

using std::string;

using mesos::state::State;
using mesos::state::Variable;

using mesos::resource_provider::Registry;

using process::defer;
using process::dispatch;
using process::Failure;
using process::Future;
using process::PID;
using process::Process;

namespace mesos {
namespace resource_provider {

constexpr char RESOURCE_PROVIDER_MANAGER_STATE[] = "resource_provider_manager";


class RegistrarProcess : public Process<RegistrarProcess>
{
public:
  explicit RegistrarProcess(std::unique_ptr<State> state)
    : state_(std::move(state)) {}

  Future<Registry> get();

  Future<Nothing> set(const Registry& registry);

private:
  std::unique_ptr<State> state_;

  std::unique_ptr<Variable> variable_;

  bool updating_ = false;
};


Future<Registry> RegistrarProcess::get()
{
  if (updating_) {
    return Failure("'get' calling while updating");
  }

  return state_->fetch(RESOURCE_PROVIDER_MANAGER_STATE)
    .then(defer(
        self(),
        [this](const Future<Variable>& variable) -> Future<Registry> {
          Try<Registry> deserialize =
            protobuf::deserialize<Registry>(variable->value());

          if (deserialize.isError()) {
            return Failure(deserialize.error());
          }

          variable_.reset(new Variable(variable.get()));

          return deserialize.get();
        }));
}

Future<Nothing> RegistrarProcess::set(const Registry& registry)
{
  Try<string> serialize = protobuf::serialize(registry);

  if (serialize.isError()) {
    return Failure(serialize.error());
  }

  const Variable variable = variable_->mutate(serialize.get());

  if (updating_) {
    return Failure("'set' called while updating");
  }

  updating_ = true;

  return state_->store(variable)
    .onAny(defer(
        self(),
        [this](const Future<Option<Variable>>&) {
          updating_ = false;
        }))
    .then(defer(
        self(),
        [this](const Future<Option<Variable>>& variable) -> Future<Nothing> {
          CHECK_SOME(variable.get());

          variable_.reset(new Variable(variable->get()));

          updating_ = false;

          return Nothing();
        }));
}


Registrar::Registrar(std::unique_ptr<State> registry)
  : registrarProcess_(new RegistrarProcess{std::move(registry)})
{
  process::spawn(*registrarProcess_, false);
}


Registrar::~Registrar()
{
  const PID<RegistrarProcess> pid = registrarProcess_->self();

  process::terminate(pid);
  process::wait(pid);
}


Future<Registry> Registrar::get()
{
  return dispatch(*registrarProcess_, &RegistrarProcess::get);
}


Future<Nothing> Registrar::set(const Registry& registry)
{
  return dispatch(*registrarProcess_, &RegistrarProcess::set, registry);
}

} // namespace resource_provider {
} // namespace mesos {
