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
#include <vector>

#include <process/owned.hpp>

#include <stout/error.hpp>
#include <stout/foreach.hpp>
#include <stout/nothing.hpp>
#include <stout/os.hpp>
#include <stout/strings.hpp>
#include <stout/try.hpp>

#include "linux/ldd.hpp"

#include "rootfs.hpp"

using std::string;
using std::vector;

using process::Owned;

namespace mesos {
namespace internal {
namespace tests {

Rootfs::~Rootfs()
{
  if (os::exists(root)) {
    os::rmdir(root);
  }
}


Try<Nothing> Rootfs::add(const string& file)
{
  Result<std::string> realpath = os::realpath(file);
  if (realpath.isError()) {
      return Error("Failed to add '" + file +
                    "' to rootfs: " + realpath.error());
  }

  Try<Nothing> result = copyPath(realpath.get());
  if (result.isError()) {
    return Error("Failed to add '" + realpath.get() +
                  "' to rootfs: " + result.error());
  }

  if (file != realpath.get()) {
    result = copyPath(file);
    if (result.isError()) {
      return Error("Failed to add '" + file + "' to rootfs: " +
                    result.error());
    }
  }

  return Nothing();
}


Try<Nothing> Rootfs::copyPath(const std::string& path)
{
  if (!os::exists(path)) {
    return ErrnoError();
  }

  if (!strings::startsWith(path, "/")) {
    return Error("Not an absolute path");
  }

  std::string dirname = Path(path).dirname();
  std::string target = path::join(root, dirname);

  if (!os::exists(target)) {
    Try<Nothing> mkdir = os::mkdir(target);
    if (mkdir.isError()) {
      return Error("Failed to create directory '" + target + "'in rootfs: " +
                    mkdir.error());
    }
  }

  // TODO(jieyu): Make sure 'path' is not under 'root'.

  // Copy the files. We preserve all attributes so that e.g., `ping`
  // keeps its file-based capabilities.
  if (os::stat::isdir(path)) {
    if (os::system(strings::format(
            "cp -r --preserve=all '%s' '%s'",
            path, target).get()) != 0) {
      return ErrnoError("Failed to copy to rootfs");
    }
  } else {
    if (os::system(strings::format(
            "cp --preserve=all '%s' '%s'",
            path, target).get()) != 0) {
      return ErrnoError("Failed to copy to rootfs");
    }
  }

  return Nothing();
}


Try<process::Owned<Rootfs>> LinuxRootfs::create(const std::string& root)
{
  process::Owned<Rootfs> rootfs(new LinuxRootfs(root));

  if (!os::exists(root)) {
    Try<Nothing> mkdir = os::mkdir(root);
    if (mkdir.isError()) {
      return Error("Failed to create root directory: " + mkdir.error());
    }
  }

  Try<vector<ldcache::Entry>> cache = ldcache::parse();

  if (cache.isError()) {
    return Error("Failed to parse ld.so cache: " + cache.error());
  }

  const std::vector<std::string> programs = {
    "/bin/echo",
    "/bin/ls",
    "/bin/ping",
    "/bin/sh",
    "/bin/sleep",
  };

  const std::vector<std::string> files = {
    "/etc/passwd"
  };

  hashset<string> needed;

  foreach(const string& program, programs) {
    Try<hashset<string>> dependencies = ldd(program, cache.get());
    if (dependencies.isError()) {
      return Error("Failed to find dependencies for '" + program + "': " +
          dependencies.error());
    }

    needed = needed | dependencies.get();
  }

  foreach (const std::string& file, needed) {
    Try<Nothing> result = rootfs->add(file);
    if (result.isError()) {
      return Error(result.error());
    }
  }

  foreach (const std::string& file, files) {
    Try<Nothing> result = rootfs->add(file);
    if (result.isError()) {
      return Error(result.error());
    }
  }

  const std::vector<std::string> directories = {
    "/proc",
    "/sys",
    "/dev",
    "/tmp"
  };

  foreach (const std::string& directory, directories) {
    Try<Nothing> mkdir = os::mkdir(path::join(root, directory));
    if (mkdir.isError()) {
      return Error("Failed to create '" + directory +
                   "' in rootfs: " + mkdir.error());
    }
  }

  return rootfs;
}

} // namespace tests {
} // namespace internal {
} // namespace mesos {
