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

using mesos::state::protobuf::State;
using mesos::state::protobuf::Variable;

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
  explicit RegistrarProcess(mesos::state::Storage* storage)
    : state_(storage) {}

  Future<Registry> recover();

  Future<Nothing> store(const Registry& registry);

private:
  State state_;

  Option<Variable<Registry>> variable_;

  bool updating_ = false;
};


Future<Registry> RegistrarProcess::recover()
{
  if (updating_) {
    return Failure("'get' calling while updating");
  }

  return state_.fetch<Registry>(RESOURCE_PROVIDER_MANAGER_STATE)
    .then(defer(
        self(),
        [this](const Future<Variable<Registry>>& variable) -> Future<Registry> {
          variable_ = variable.get();

          return variable->get();
        }));
}


Future<Nothing> RegistrarProcess::store(const Registry& registry)
{
  const Variable<Registry> variable = variable_->mutate(registry);

  if (updating_) {
    return Failure("'set' called while updating");
  }

  updating_ = true;

  return state_.store(variable)
    .onAny(defer(
        self(),
        [this](const Future<Option<Variable<Registry>>>&) {
          updating_ = false;
        }))
    .then(defer(
        self(),
        [this](const Future<Option<Variable<Registry>>>& variable)
          -> Future<Nothing> {
          CHECK_SOME(variable.get());

          variable_ = variable->get();

          updating_ = false;

          return Nothing();
        }));
}


Registrar::Registrar(mesos::state::Storage* storage)
  : registrarProcess_(new RegistrarProcess{storage})
{
  process::spawn(*registrarProcess_, false);
}


Registrar::~Registrar()
{
  const PID<RegistrarProcess> pid = registrarProcess_->self();

  process::terminate(pid);
  process::wait(pid);
}


Future<Registry> Registrar::recover()
{
  return dispatch(*registrarProcess_, &RegistrarProcess::recover);
}


Future<Nothing> Registrar::store(const Registry& registry)
{
  return dispatch(*registrarProcess_, &RegistrarProcess::store, registry);
}

} // namespace resource_provider {
} // namespace mesos {
