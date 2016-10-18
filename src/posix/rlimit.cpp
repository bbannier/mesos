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

#include "posix/rlimit.hpp"

#include <sys/time.h>
#include <sys/types.h>

// Intentionally include `sys/resource.h` after `sys/time.h` and
// `sys/types.h`. This is the suggested include pattern under e.g.,
// BSD, https://www.freebsd.org/cgi/man.cgi?query=setrlimit.
#include <sys/resource.h>

#include <stout/os/strerror.hpp>

#include <stout/unreachable.hpp>

#ifndef __WINDOWS__

namespace mesos {
namespace rlimit {

Try<int> convert(RLimitInfo::RLimit::Type type)
{
  switch (type) {
    case RLimitInfo::RLimit::UNKNOWN:
      return Error(
          "Resource type 'UNKNOWN' does not correspond to a system resource");
    // Resource types defined in XSI.
    case RLimitInfo::RLimit::RLMT_AS:
      return RLIMIT_AS;
    case RLimitInfo::RLimit::RLMT_CORE:
      return RLIMIT_CORE;
    case RLimitInfo::RLimit::RLMT_FSIZE:
      return RLIMIT_FSIZE;
    case RLimitInfo::RLimit::RLMT_NOFILE:
      return RLIMIT_NOFILE;
    case RLimitInfo::RLimit::RLMT_CPU:
      return RLIMIT_CPU;
    case RLimitInfo::RLimit::RLMT_DATA:
      return RLIMIT_DATA;

    // Resource types also defined on BSDs like e.g., OS X.
    case RLimitInfo::RLimit::RLMT_MEMLOCK:
      return RLIMIT_MEMLOCK;
    case RLimitInfo::RLimit::RLMT_NPROC:
      return RLIMIT_NPROC;
    case RLimitInfo::RLimit::RLMT_RSS:
      return RLIMIT_RSS;
    case RLimitInfo::RLimit::RLMT_STACK:
      return RLIMIT_STACK;

#ifdef __linux__
    // Resource types defined in >=Linux 2.6.36.
    // NOTE: The resource limits defined for Linux are currently the
    // maximal possible set of understood types. Here we explicitly
    // list all types and in particular do not use a `default` case,
    // see MESOS-3754.
    case RLimitInfo::RLimit::RLMT_LOCKS:
      return RLIMIT_LOCKS;
    case RLimitInfo::RLimit::RLMT_MSGQUEUE:
      return RLIMIT_MSGQUEUE;
    case RLimitInfo::RLimit::RLMT_NICE:
      return RLIMIT_NICE;
    case RLimitInfo::RLimit::RLMT_RTPRIO:
      return RLIMIT_RTPRIO;
    case RLimitInfo::RLimit::RLMT_RTTIME:
      return RLIMIT_RTTIME;
    case RLimitInfo::RLimit::RLMT_SIGPENDING:
      return RLIMIT_SIGPENDING;
#else
    // Provide a `default` case for platforms not supporting all
    // resource limit types.
    default:
      return Error(
          "Resource type '" + RLimitInfo_RLimit_Type_Name(type) +
          "' not understood");
#endif // __linux__
  }

  UNREACHABLE();
}


Try<Nothing> set(const RLimitInfo::RLimit& limit)
{
  const Try<int> resource = convert(limit.type());

  if (resource.isError()) {
    return Error("Could not convert rlimit: " + resource.error());
  }

  ::rlimit resourceLimit;
  if (getrlimit(resource.get(), &resourceLimit) != 0) {
    return Error("Failed to initialize rlimit: " + os::strerror(errno));
  }

  resourceLimit.rlim_cur = limit.soft();
  resourceLimit.rlim_max = limit.hard();

  if (setrlimit(resource.get(), &resourceLimit) != 0) {
    return Error("Failed to set rlimit: " + os::strerror(errno));
  }

  return Nothing();
}

} // namespace rlimit {
} // namespace mesos {

#endif // __WINDOWS__
