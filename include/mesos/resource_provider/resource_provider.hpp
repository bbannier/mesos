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

#ifndef __MESOS_RESOURCE_PROVIDER_PROTO_HPP__
#define __MESOS_RESOURCE_PROVIDER_PROTO_HPP__

// ONLY USEFUL AFTER RUNNING PROTOC.
#include <mesos/resource_provider/resource_provider.pb.h>

// FIXME(bbannier): move this where it belongs.

#include <string>

#include <mesos/type_utils.hpp>

#include <stout/strings.hpp>

namespace mesos {
namespace resource_provider {

inline bool operator==(
    const Registry::ResourceProvider& lhs,
    const Registry::ResourceProvider& rhs)
{
  if (lhs.id() != rhs.id()) {
    return false;
  }

  return true;
}


inline bool operator!=(
    const Registry::ResourceProvider& lhs,
    const Registry::ResourceProvider& rhs)
{
  return !(lhs == rhs);
}


inline bool operator==(const Registry& lhs, const Registry& rhs)
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


inline bool operator!=(const Registry& lhs, const Registry& rhs)
{
  return !(lhs == rhs);
}


inline std::ostream& operator<<(
    std::ostream& stream,
    const Registry::ResourceProvider& resourceProvider)
{
  return stream << resourceProvider.id();
}


inline std::ostream& operator<<(std::ostream& stream, const Registry& registry)
{
  std::string join = strings::join(", ", registry.resource_providers());

  return stream << "{" << join << "}";
}

} // namespace resource_provider {
} // namespace mesos {

#endif // __MESOS_RESOURCE_PROVIDER_PROTO_HPP__
