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

#include <process/owned.hpp>

#include <stout/elf.hpp>
#include <stout/nothing.hpp>

#include "linux/ldd.hpp"

using std::string;
using std::vector;

using process::Owned;

namespace mesos {
namespace internal {

namespace {

Try<Nothing> collectDependencies(
    const string& path,
    const vector<ldcache::Entry>& cache,
    hashset<string>& needed)
{
  // We are doing a depth-first search, so if we already have this path,
  // then we already gathered all its dependencies.
  if (needed.contains(path)) {
    return Nothing();
  }

  Try<elf::File*> load = elf::File::load(path);
  if (!load.isSome()) {
    return Error(load.error());
  }

  Owned<elf::File> elf(load.get());

  Try<vector<string>> dependencies =
    elf->get_dynamic_strings(elf::DynamicTag::NEEDED);
  if (!dependencies.isSome()) {
    return Error(dependencies.error());
  }

  foreach(const string& dependency, dependencies.get()) {
    // Check the ld.so cache to find the actual path of the needed library.
    auto entry = std::find_if(
        cache.begin(),
        cache.end(),
        [&dependency](const ldcache::Entry& e) {
          return e.name == dependency;
        });

    if (entry == cache.end()) {
      return Error("'" + dependency + "' was not in the ld.so cache.");
    }

    // If we don't already have this entry, recursively add its dependencies.
    if (!needed.contains(entry->path)) {
      Try<Nothing> result = collectDependencies(entry->path, cache, needed);
      if (result.isError()) {
        return Error(result.error());
      }
    }

    needed.insert(entry->path);
  }

  // Finally add this path, since we have now collected its dependencies.
  needed.insert(path);
  return Nothing();
}

} // namespace {

Try<hashset<std::string>>
ldd(const std::string& path, const std::vector<ldcache::Entry>& cache)
{
  hashset<std::string> dependencies;

  Try<Nothing> result = collectDependencies(path, cache, dependencies);
  if (result.isError()) {
    return Error(result.error());
  }

  return dependencies;
}

} // namespace internal {
} // namespace mesos {
