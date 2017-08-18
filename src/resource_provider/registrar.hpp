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

#ifndef __RESOURCE_PROVIDER_REGISTRAR_HPP__
#define __RESOURCE_PROVIDER_REGISTRAR_HPP__

#include <memory>

#include <process/future.hpp>
#include <process/owned.hpp>

#include <stout/try.hpp>

#include "master/registrar.hpp"

#include "resource_provider/registry.hpp"

#include "slave/flags.hpp"


namespace mesos {
namespace resource_provider {

class Registrar
{
public:
  // Defines an abstraction for operations that can be applied on the
  // Registry.
  // TODO(xujyan): Make Operation generic so that we can apply them
  // against a generic "batch operation applier" abstraction, see TODO
  // in master/registrar.hpp for more details.
  class Operation : public process::Promise<bool>
  {
  public:
    virtual ~Operation() = default;

    // Attempts to invoke the operation on the registry object.
    // Aided by accumulator(s):
    //   slaveIDs - is the set of registered slaves.
    //
    // Returns whether the operation mutates 'registry', or an error if
    // the operation cannot be applied successfully.
    Try<bool> operator()(registry::Registry* registry);

    // Sets the promise based on whether the operation was successful.
    bool set();

  protected:
    virtual Try<bool> perform(registry::Registry* registry) = 0;

  private:
    bool success = false;
  };

  static Try<process::Owned<Registrar>> create(
      mesos::internal::master::Registrar* masterRegistrar);

  static Try<process::Owned<Registrar>> create(
      const mesos::internal::slave::Flags& slaveFlags);

  virtual ~Registrar() = default;

  virtual process::Future<registry::Registry> recover() = 0;
  virtual process::Future<bool> apply(process::Owned<Operation> operation) = 0;
};


class AdmitResourceProvider : public Registrar::Operation
{
public:
  explicit AdmitResourceProvider(const ResourceProviderID& id);

private:
  Try<bool> perform(registry::Registry* registry) override;

  ResourceProviderID id_;
};


class RemoveResourceProvider : public Registrar::Operation
{
public:
  explicit RemoveResourceProvider(const ResourceProviderID& id);

private:
  Try<bool> perform(registry::Registry* registry) override;

  ResourceProviderID id_;
};


class AgentRegistrarProcess;


class AgentRegistrar : public Registrar
{
public:
  explicit AgentRegistrar(const mesos::internal::slave::Flags& slaveFlags);

  ~AgentRegistrar() override;

  process::Future<registry::Registry> recover() override;

  process::Future<bool> apply(process::Owned<Operation> operation) override;

private:
  std::unique_ptr<AgentRegistrarProcess> process_;
};


class MasterRegistrarProcess;


class MasterRegistrar : public Registrar {
public:
  explicit MasterRegistrar(internal::master::Registrar* masterRegistrar);

  ~MasterRegistrar() override;

  process::Future<registry::Registry> recover() override;

  process::Future<bool> apply(process::Owned<Operation> operation) override;

private:
  std::unique_ptr<MasterRegistrarProcess> process_;
};

} // namespace resource_provider {
} // namespace mesos {

#endif // __RESOURCE_PROVIDER_REGISTRAR_HPP__
