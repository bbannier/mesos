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

#include "resource_provider/registrar.hpp"

#include <deque>
#include <algorithm>
#include <string>
#include <utility>

#include <mesos/type_utils.hpp>

#include <mesos/state/leveldb.hpp>
#include <mesos/state/protobuf.hpp>

#include <process/defer.hpp>
#include <process/process.hpp>

#include <stout/none.hpp>
#include <stout/nothing.hpp>
#include <stout/option.hpp>
#include <stout/path.hpp>


using std::deque;
using std::string;
using std::unique_ptr;

using mesos::resource_provider::registry::Registry;
using mesos::resource_provider::registry::ResourceProvider;

using mesos::state::protobuf::Variable;
using mesos::state::LevelDBStorage;
using mesos::state::protobuf::State;

using process::defer;
using process::Failure;
using process::Future;
using process::Owned;
using process::Process;
using process::Promise;


namespace slave = mesos::internal::slave;

namespace mesos {
namespace resource_provider {

Try<bool> Registrar::Operation::operator()(Registry* registry)
{
  Try<bool> result = perform(registry);

  success = !result.isError();

  return result;
}


AdmitResourceProvider::AdmitResourceProvider(const ResourceProviderID& id)
  : id_(id) {}


Try<bool> AdmitResourceProvider::perform(Registry* registry)
{
  if (registry->has_resource_providers()) {
    const auto& resourceProviders =
      registry->resource_providers().resource_providers();

    if (std::find_if(
            resourceProviders.begin(),
            resourceProviders.end(),
            [this](const ResourceProvider& resourceProvider) {
              return resourceProvider.id() == this->id_;
            }) != resourceProviders.end()) {
      return Error("Resource provider already admitted");
    }
  }

  ResourceProvider resourceProvider;
  resourceProvider.mutable_id()->CopyFrom(id_);

  registry->mutable_resource_providers()->add_resource_providers()->CopyFrom(
      resourceProvider);

  return true; // Mutation.
}


RemoveResourceProvider::RemoveResourceProvider(const ResourceProviderID& id)
  : id_(id) {}


Try<bool> RemoveResourceProvider::perform(Registry* registry)
{
  if (!registry->has_resource_providers()) {
    return Error("Attempted to remove an unknown resource provider");
  }

  const auto& resourceProviders =
    registry->resource_providers().resource_providers();

  auto pos = std::find_if(
      resourceProviders.begin(),
      resourceProviders.end(),
      [this](const ResourceProvider& resourceProvider) {
        return resourceProvider.id() == this->id_;
      });

  if (pos == resourceProviders.end()) {
    return Error("Attempted to remove an unknown resource provider");
  }

  registry->mutable_resource_providers()->mutable_resource_providers()->erase(
      pos);

  // If this operation remove the laster resource provider also
  // clear the registry field instead of leaving an empty field
  // behind.
  if (registry->resource_providers().resource_providers().empty()) {
    registry->clear_resource_providers();
  }

  return true; // Mutation.
}


bool Registrar::Operation::set()
{
  return Promise<bool>::set(success);
}


Try<Owned<Registrar>> Registrar::create(const slave::Flags& slaveFlags)
{
  return new AgentRegistrar(slaveFlags);
}


class AgentRegistrarProcess : public Process<AgentRegistrarProcess>
{
public:
  AgentRegistrarProcess(const slave::Flags& flags);

  Future<Registry> recover();

  Future<bool> apply(Owned<Registrar::Operation> operation);

  Future<bool> _apply(Owned<Registrar::Operation> operation);

  void update();

  void _update(
      const Future<Option<Variable<Registry>>>& store,
      const Registry& updatedRegistry,
      deque<Owned<Registrar::Operation>> applied);

private:
  LevelDBStorage storage_;
  State state_;

  Option<Promise<Registry>> recovered_;
  Option<Registry> registry_;
  Option<Variable<Registry>> variable_;

  Option<Error> error_;

  deque<Owned<Registrar::Operation>> operations_;

  bool updating_ = false;
};


AgentRegistrarProcess::AgentRegistrarProcess(const slave::Flags& flags)
  : ProcessBase(process::ID::generate("resource-provider-agent-registrar")),
    storage_(path::join(flags.work_dir, "resource_provider_manager")),
    state_(&storage_) {}


Future<Registry> AgentRegistrarProcess::recover()
{
  constexpr char NAME[] = "RESOURCE_PROVIDER_REGISTRAR";

  if (recovered_.isNone()) {
    recovered_ = Promise<Registry>();
    state_.fetch<Registry>(NAME).then(defer(
        self(),
        [this](const Variable<Registry>& recovery) {
          recovered_->set(recovery.get());
          registry_ = recovery.get();
          variable_ = recovery;

          return Nothing();
        }));
  }

  return recovered_->future();
}


Future<bool> AgentRegistrarProcess::apply(Owned<Registrar::Operation> operation)
{
  if (recovered_.isNone()) {
    return Failure("Attempted to apply the operation before recovering");
  }

  return recovered_->future().then(defer(
      self(),
      &Self::_apply,
      std::move(operation)));
}


Future<bool> AgentRegistrarProcess::_apply(
    Owned<Registrar::Operation> operation)
{
  if (error_.isSome()) {
    return Failure(error_.get());
  }

  operations_.push_back(std::move(operation));

  Future<bool> future = operations_.back()->future();
  if (!updating_) {
    update();
  }

  return future;
}


void AgentRegistrarProcess::update()
{
  CHECK(!updating_);
  CHECK_NONE(error_);

  if (operations_.empty()) {
    return; // No-op.
  }

  updating_ = true;

  CHECK_SOME(registry_);
  Registry updatedRegistry = registry_.get();

  foreach (Owned<Registrar::Operation>& operation, operations_) {
    (*operation)(&updatedRegistry);
  }

  auto applied = std::move(operations_);

  // Serialize updated registry.
  CHECK_SOME(variable_);

  Future<Option<Variable<Registry>>> store =
    state_.store(variable_->mutate(updatedRegistry));

  store.onAny(defer(
      self(),
      &Self::_update,
      lambda::_1,
      updatedRegistry,
      std::move(applied)));

  operations_.clear();
}


void AgentRegistrarProcess::_update(
    const Future<Option<Variable<Registry>>>& store,
    const Registry& updatedRegistry,
    deque<Owned<Registrar::Operation>> applied)
{
  updating_ = false;
  // Abort if the storage operation did not succeed.
  if (!store.isReady() || store.get().isNone()) {
    string message = "Failed to update registry: ";

    if (store.isFailed()) {
      message += store.failure();
    } else if (store.isDiscarded()) {
      message += "discarded";
    } else {
      message += "version mismatch";
    }

    while (!applied.empty()) {
      applied.front()->fail(message);
      applied.pop_front();
    }

    error_ = Error(message);

    LOG(ERROR) << "Registrar abortng: " << message;
  }

  variable_ = store->get();
  registry_ = updatedRegistry;

  // Remove the operations.
  while (!applied.empty()) {
    Owned<Registrar::Operation> operation = applied.front();
    applied.pop_front();

    operation->set();
  }

  if (!operations_.empty()) {
    update();
  }
}


AgentRegistrar::AgentRegistrar(const slave::Flags& slaveFlags)
  : process_(new AgentRegistrarProcess(slaveFlags))
{
  process::spawn(process_.get(), false);
}


AgentRegistrar::~AgentRegistrar()
{
  process::terminate(*process_);
  process::wait(*process_);
}


Future<Registry> AgentRegistrar::recover()
{
  return dispatch(process_.get(), &AgentRegistrarProcess::recover);
}


Future<bool> AgentRegistrar::apply(Owned<Operation> operation)
{
  return dispatch(
      process_.get(),
      &AgentRegistrarProcess::apply,
      std::move(operation));
}

} // namespace resource_provider {
} // namespace mesos {
