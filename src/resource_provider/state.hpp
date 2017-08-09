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

#include <mesos/resource_provider/resource_provider.hpp>

#include <mesos/state/state.hpp>

#include <process/future.hpp>

#include <stout/nothing.hpp>

namespace mesos {
namespace resource_provider {
namespace state {

class RegistrarProcess;

class Registrar
{
public:
  explicit Registrar(std::unique_ptr<mesos::state::State> registry);

  Registrar(Registrar&&) = default;

  ~Registrar();

  process::Future<mesos::resource_provider::Registry> get();

  process::Future<Nothing> set(const Registry& registry);

private:
  std::unique_ptr<RegistrarProcess> registrarProcess_;
};

} // namespace state {
} // namespace resource_provider {
} // namespace mesos {

#endif // __RESOURCE_PROVIDER_REGISTRAR_HPP__
