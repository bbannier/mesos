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

#ifndef __LINUX_CAPABILITIES_HPP__
#define __LINUX_CAPABILITIES_HPP__

#include <string>
#include <vector>

#include <process/owned.hpp>

#include <stout/flags.hpp>
#include <stout/nothing.hpp>
#include <stout/protobuf.hpp>
#include <stout/set.hpp>
#include <stout/try.hpp>

#include <mesos/mesos.hpp>

namespace mesos {
namespace internal {
namespace capabilities {

// Superset of all capabilities. This is the set currently supported
// by linux (kernel 4.0).
enum Capability : unsigned
{
  CHOWN,
  DAC_OVERRIDE,
  DAC_READ_SEARCH,
  FOWNER,
  FSETID,
  KILL,
  SETGID,
  SETUID,
  SETPCAP,
  LINUX_IMMUTABLE,
  NET_BIND_SERVICE,
  NET_BROADCAST,
  NET_ADMIN,
  NET_RAW,
  IPC_LOCK,
  IPC_OWNER,
  SYS_MODULE,
  SYS_RAWIO,
  SYS_CHROOT,
  SYS_PTRACE,
  SYS_PACCT,
  SYS_ADMIN,
  SYS_BOOT,
  SYS_NICE,
  SYS_RESOURCE,
  SYS_TIME,
  SYS_TTY_CONFIG,
  MKNOD,
  LEASE,
  AUDIT_WRITE,
  AUDIT_CONTROL,
  SETFCAP,
  MAC_OVERRIDE,
  MAC_ADMIN,
  SYSLOG,
  WAKE_ALARM,
  BLOCK_SUSPEND,
  AUDIT_READ,
  MAX_CAPABILITY
};


std::ostream& operator<<(std::ostream& stream, const Capability& cap);


enum Type : unsigned
{
  EFFECTIVE,
  PERMITTED,
  INHERITABLE,
  BOUNDED,
  MAX_CAPABILITY_TYPE
};


std::ostream& operator<<(std::ostream& stream, const Type& type);


// Encapsulation of capability value sets, one for each of `effective`,
// `permitted` and `inheritable`.
class ProcessCapabilities
{
public:
  Set<Capability> get(const Type& type) const;

  void set(const Type& type, const Set<Capability>& set);

  friend class Capabilities;

  friend std::ostream& operator<<(
      std::ostream& stream,
      const ProcessCapabilities& set);

private:
  uint64_t sets[MAX_CAPABILITY_TYPE];
};


// Provides wrapper for the linux process capabilities interface.
// Note: This is a class instead of an interface because it has state
// associated with it.
//
// TODO(jojy): Currently we only support linux capabilities. Consider
// refactoring the interface so that we can support a generic
// interface which can be used for other OSes(BSD, Windows etc).
class Capabilities
{
public:
  // Factory method to create Capabilities object.
  // @return Owned `Capabilities` pointer on success;
  //          Error on falure. Failure conditions could be:
  //          - Error getting system information(e.g, version).
  //          - Unsupported linux kernel capabilities version.
  //          - Maximum capability supported by kernel exceeds the
  //          ones defined in the enum `Capabilities`.
  static Try<process::Owned<Capabilities>> create();

  // Get the maximum capability value supported by the system.
  unsigned getMaxCapabilityValue() const;

  // Gets capability set for the calling process.
  //
  // @return ProcessCapabilities on success.
  //         Error on failure.
  Try<ProcessCapabilities> get() const;

  // Sets capabilities for the calling process.
  //
  //  @param set `ProcessCapabilities` that needs to be set for the process.
  //  @return  Nothing on success.
  //           Error on failure.
  Try<Nothing> set(const ProcessCapabilities& set);

  // Process control interface to enforce keeping the parent process's
  // capabilities after a change in uid/gid.
  //
  // @return  Nothing on success.
  //          Error on failure.
  Try<Nothing> keepCapabilitiesOnSetUid();

  // Sets capabilities that will be effective for the process after an
  // `exec` operation.
  //
  // Error conditions:
  //  - If execCaps(@param) exceeds the current PERMITTED capabilities
  //    for the current user.
  //  - Getting current capability for the process.
  //
  // @param execCapsInfo Capabilities that will be set when the
  //                     process execs.
  // @return Nothing in case of success.
  //         Error otherwise.
  Try<Nothing> setCapabilitiesOnExec(const CapabilityInfo& execCapsInfo);

private:
  Capabilities(uint32_t version, unsigned lastCap);

  Try<Nothing> setBounded(uint64_t flags);
  Try<Nothing> doCapSet(const ProcessCapabilities& processCaps);

  // Kernel capabilities version.
  const uint32_t version;

  // Maximum count of capabilities supported by the system.
  const unsigned maxSystemCaps;
};


Capability convert(const CapabilityInfo::Capability& cap);


CapabilityInfo toCapabilityInfo(const Set<Capability>& capSet);


// Uses `Capabilities` object to get all capabilities supported by
// the system. It returns a `CapabilityInfo` object for ease of use
// at the call site.
Try<CapabilityInfo> getAllSupportedCapabilities();

} // namespace capabilities {
} // namespace internal {
} // namespace mesos {


namespace mesos {

inline std::ostream& operator<<(
    std::ostream& stream,
    const CapabilityInfo& capsInfo)
{
  return stream << stringify(JSON::protobuf(capsInfo));
}

} // namespace mesos {


namespace flags {

template <>
inline Try<mesos::CapabilityInfo> parse(const std::string& value)
{
  Try<JSON::Object> json = parse<JSON::Object>(value);
  if (json.isError()) {
    return Error(json.error());
  }

  return protobuf::parse<mesos::CapabilityInfo>(json.get());
}

} // namespace flags {

#endif // __LINUX_CAPABILITIES_HPP__
