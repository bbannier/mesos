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

#include <linux/capability.h>

#include <sys/prctl.h>

#include <string>

#include <stout/numify.hpp>
#include <stout/os.hpp>
#include <stout/stringify.hpp>

#include "capabilities.hpp"

using std::hex;
using std::ostream;
using std::string;
using std::vector;

using process::Owned;

// Definition provided by libc.
extern "C" {
extern int capset(cap_user_header_t header, cap_user_data_t data);

extern int capget(cap_user_header_t header, const cap_user_data_t data);
}


namespace mesos {
namespace internal {
namespace capabilities {

static constexpr char MAX_CAPS_PROCFS_PATH[] = "/proc/sys/kernel/cap_last_cap";
static const int CAPABIILITY_PROTOBUF_OFFSET = 1000;
static const unsigned SYSCALL_CAPABILITY_SETS = 3;


// System call payload (for linux capability version 3).
struct SyscallPayload
{
  struct __user_cap_header_struct head;
  struct __user_cap_data_struct set[_LINUX_CAPABILITY_U32S_3];
};


std::ostream& operator<<(std::ostream& stream, const Capability& cap)
{
  switch (cap) {
    case (CHOWN): return stream << "CHOWN";
    case (DAC_OVERRIDE): return stream << "DAC_OVERRIDE";
    case (DAC_READ_SEARCH): return stream << "DAC_READ_SEARCH";
    case (FOWNER): return stream << "FOWNER";
    case (FSETID): return stream << "FSETID";
    case (KILL): return stream << "KILL";
    case (SETGID): return stream << "SETGID";
    case (SETUID): return stream << "SETUID";
    case (SETPCAP): return stream << "SETPCAP";
    case (LINUX_IMMUTABLE): return stream << "LINUX_IMMUTABLE";
    case (NET_BIND_SERVICE): return stream << "NET_BIND_SERVICE";
    case (NET_BROADCAST): return stream << "NET_BROADCAST";
    case (NET_ADMIN): return stream << "NET_ADMIN";
    case (NET_RAW): return stream << "NET_RAW";
    case (IPC_LOCK): return stream << "IPC_LOCK";
    case (IPC_OWNER): return stream << "IPC_OWNER";
    case (SYS_MODULE): return stream << "SYS_MODULE";
    case (SYS_RAWIO): return stream << "SYS_RAWIO";
    case (SYS_CHROOT): return stream << "SYS_CHROOT";
    case (SYS_PTRACE): return stream << "SYS_PTRACE";
    case (SYS_PACCT): return stream << "SYS_PACCT";
    case (SYS_ADMIN): return stream << "SYS_ADMIN";
    case (SYS_BOOT): return stream << "SYS_BOOT";
    case (SYS_NICE): return stream << "SYS_NICE";
    case (SYS_RESOURCE): return stream << "SYS_RESOURCE";
    case (SYS_TIME): return stream << "SYS_TIME";
    case (SYS_TTY_CONFIG): return stream << "SYS_TTY_CONFIG";
    case (MKNOD): return stream << "MKNOD";
    case (LEASE): return stream << "LEASE";
    case (AUDIT_WRITE): return stream << "AUDIT_WRITE";
    case (AUDIT_CONTROL): return stream << "AUDIT_CONTROL";
    case (SETFCAP): return stream << "SETFCAP";
    case (MAC_OVERRIDE): return stream << "MAC_OVERRIDE";
    case (MAC_ADMIN): return stream << "MAC_ADMIN";
    case (SYSLOG): return stream << "SYSLOG";
    case (WAKE_ALARM): return stream << "WAKE_ALARM";
    case (BLOCK_SUSPEND): return stream << "BLOCK_SUSPEND";
    default:
      UNREACHABLE();
  }
}


ostream& operator<<(ostream& stream, const Type& cap)
{
  switch (cap) {
    case EFFECTIVE: return stream << "eff";
    case INHERITABLE: return stream << "inh";
    case PERMITTED: return stream << "perm";
    case BOUNDED: return stream << "bnd";
    default:
      UNREACHABLE();
  }
}


Set<Capability> ProcessCapabilities::get(const Type& type) const
{
  uint64_t capsValue = sets[type];

  Set<Capability> capSet;

  for (unsigned i = 0; i < MAX_CAPABILITY; i++) {
    if (capsValue & (1U << i)) {
      capSet.insert(static_cast<Capability>(i));
    }
  }

  return capSet;
}


void ProcessCapabilities::set(const Type& type, const Set<Capability>& set)
{
  CHECK(type < MAX_CAPABILITY_TYPE);

  uint64_t value = 0;

  for (unsigned i = 0; i < MAX_CAPABILITY; i++) {
    if (set.find(static_cast<Capability>(i)) != set.end()) {
      value |= (1U << i);
    }
  }

  sets[type] = value;
}


ostream& operator<<(ostream& stream, const ProcessCapabilities& cap)
{
  stream << "{";

  for (unsigned i = EFFECTIVE; i < SYSCALL_CAPABILITY_SETS; i++)  {
    if (i != 0) {
      stream << ", ";
    }

    stream << static_cast<Type>(i);
    stream << ": ";
    stream << "0x" << hex << cap.sets[i];
  }

  // TODO(jojy): Add conversion for bounded set.

  stream << "}";

  return stream;
}


static Try<uint32_t> getVersion()
{
  SyscallPayload payload;

  memset(&payload, 0, sizeof(payload));

  if (capget(&payload.head, NULL)) {
    return ErrnoError("Failed to get linux capability version");
  }

  if (payload.head.version != _LINUX_CAPABILITY_VERSION_3) {
    return Error(
        "Unsupported linux capabilities version: " + payload.head.version);
  }

  return payload.head.version;
}


Try<Owned<Capabilities>> Capabilities::create()
{
  Try<uint32_t> version = getVersion();
  if (version.isError()) {
    return Error(
        "Failed to get kernel capabilities version: " + version.error());
  }

  Try<string> lastCap = os::read(MAX_CAPS_PROCFS_PATH);
  if (lastCap.isError()) {
    return Error(
        "Failed to read '" + string(MAX_CAPS_PROCFS_PATH) + "': " +
        lastCap.error());
  }

  Try<unsigned> lastCapBit = numify<unsigned>(strings::trim(
      lastCap.get(),
      strings::SUFFIX,
      "\n"));

  if (lastCapBit.isError()) {
    return Error(
        "Invalid last capability value: '" + lastCap.get() + "': " +
        lastCapBit.error());
  }

  if (lastCapBit.get() >= MAX_CAPABILITY) {
    return Error(
        "System last capability value '" + lastCap.get() +
        "' is greater than maximum supported number of capabilities '" +
        stringify(SYSCALL_CAPABILITY_SETS) + "'");
  }

  return new Capabilities(version.get(), lastCapBit.get());
}


Capabilities::Capabilities(uint32_t _version, unsigned _lastCap)
  : version(_version),
    maxSystemCaps(_lastCap) {}


static int64_t getAllSystemCapabilities(unsigned maxSystemCaps)
{
  return (((uint64_t)1 << (maxSystemCaps + 1)) - 1);
}


unsigned Capabilities::getMaxCapabilityValue() const
{
  return maxSystemCaps;
}


Try<ProcessCapabilities> Capabilities::get() const
{
  SyscallPayload payload;

  memset(&payload, 0, sizeof(payload));
  payload.head.version = version;
  payload.head.pid = 0;

  if (capget(&payload.head, &payload.set[0])) {
    return ErrnoError("Failed to get capabilites");
  }

  ProcessCapabilities processCaps;

  processCaps.sets[EFFECTIVE] =
    payload.set[0].effective | ((uint64_t)(payload.set[1].effective) << 32);

  processCaps.sets[PERMITTED] =
    payload.set[0].permitted | ((uint64_t)(payload.set[1].permitted) << 32);

  processCaps.sets[INHERITABLE] =
    payload.set[0].inheritable | ((uint64_t)(payload.set[1].inheritable) << 32);

  // TODO(jojy): Add support for BOUNDING capabilities.

  return processCaps;
}


Try<Nothing> Capabilities::doCapSet(const ProcessCapabilities& processCaps)
{
  SyscallPayload payload;

  memset(&payload, 0, sizeof(payload));

  payload.head.version = version;
  payload.head.pid = 0;

  payload.set[0].effective = processCaps.sets[EFFECTIVE];
  payload.set[1].effective = processCaps.sets[EFFECTIVE] >> 32;

  payload.set[0].permitted = processCaps.sets[PERMITTED];
  payload.set[1].permitted = processCaps.sets[PERMITTED] >> 32;

  payload.set[0].inheritable = processCaps.sets[INHERITABLE];
  payload.set[1].inheritable = processCaps.sets[INHERITABLE] >> 32;

  if (capset(&payload.head, &payload.set[0])) {
    return ErrnoError("Failed to set capability");
  }

  return Nothing();
}


// We do two separate operations:
//  1. Set the `bounded` capabilities for the process.
//  2. Set the `effective`, `permitted` and `inheritable` capabilities.
//
// TODO(jojy): Is there a way to make this atomic? Ideally, we would
// like to rollback any changes if any of the operation fails.
Try<Nothing> Capabilities::set(const ProcessCapabilities& processCaps)
{
  Try<Nothing> setBound = setBounded(processCaps.sets[BOUNDED]);
  if (setBound.isError()) {
    return Error("Failed to set bounded capabilities: " + setBound.error());
  }

  return doCapSet(processCaps);
}


Try<Nothing> Capabilities::setBounded(uint64_t capSet)
{
  VLOG(1) << "Dropping capabilities: 0x" << hex << ~capSet;

  for (unsigned i = 0; i <= maxSystemCaps; i++) {
    if (!(capSet & (1U << i))) {
      int rc = prctl(PR_CAPBSET_DROP, i, 1);
      if (rc < 0 && rc != EINVAL) {
        return ErrnoError(
            "Failed to drop capability:"
            "PR_CAPBSET_DROP failed for the process");
      }
    }
  }

  return Nothing();
}


Try<Nothing> Capabilities::keepCapabilitiesOnSetUid()
{
  int rc = prctl(PR_SET_KEEPCAPS, 1);
  if (rc < 0) {
    return ErrnoError("Failed to set PR_SET_KEEPCAPS for the process");
  }

  return Nothing();
}


Try<Nothing> Capabilities::setCapabilitiesOnExec(
    const CapabilityInfo& execCapsInfo)
{
  Try<ProcessCapabilities> _currCaps = get();
  if (_currCaps.isError()) {
    return Error("Failed to get current capabilities: " + _currCaps.error());
  }

  ProcessCapabilities &currCaps = _currCaps.get();

  // Since `SETPCAP` is required in `effective` set of a process
  // for changing the bounded set, we do the following if its missing:
  //    1. Add `SETPCAP` to the `effective` set.
  //    2. Update the process's capabilities.
  if (!(currCaps.sets[EFFECTIVE] & (1U << SETPCAP))) {
    currCaps.sets[EFFECTIVE] |= (1U << SETPCAP);

    Try<Nothing> adjustCaps = doCapSet(currCaps);
    if (adjustCaps.isError()) {
      return Error(
          "Failed to add `SETPCAP`: " + adjustCaps.error());
    }
  }

  uint64_t permCapsValue = currCaps.sets[PERMITTED];
  uint64_t newCapsValue = 0;

  // Calculate the new set of capabilities. If the new set contains a
  // capability thats not in the `permitted` set, we exit early.
  if (execCapsInfo.capabilities_size()) {
    foreach (const int& execCap, execCapsInfo.capabilities()) {
      Capability cap =
        convert(static_cast<CapabilityInfo::Capability>(execCap));

      if (permCapsValue & (1U << cap)) {
        newCapsValue |= (1U << cap);
      } else {
        return Error(
            "Requested capability '" + stringify(cap) +
            "' exceeds permitted capabilities");
      }
    }
  }

  ProcessCapabilities newCaps;

  newCaps.sets[EFFECTIVE]
    = newCaps.sets[PERMITTED]
    = newCaps.sets[INHERITABLE]
    = newCaps.sets[BOUNDED]
    = newCapsValue;

  Try<Nothing> setCaps = set(newCaps);
  if (setCaps.isError()) {
    return Error(setCaps.error());
  }

  VLOG(1) << "Process exec capabilties set as: " << newCaps;

  return Nothing();
}


Capability convert(const CapabilityInfo::Capability& cap)
{
  unsigned capValue = cap - CAPABIILITY_PROTOBUF_OFFSET;
  CHECK(capValue < MAX_CAPABILITY);

  return static_cast<Capability>(capValue);
}


CapabilityInfo toCapabilityInfo(const Set<Capability>& capSet)
{
  CapabilityInfo capsInfo;

  foreach (const Capability& cap, capSet) {
    capsInfo.add_capabilities(static_cast<CapabilityInfo::Capability>(
        cap + CAPABIILITY_PROTOBUF_OFFSET));
  }

  return capsInfo;
}


Try<CapabilityInfo> getAllSupportedCapabilities()
{
  Try<Owned<Capabilities>> capabilities = Capabilities::create();
  if (capabilities.isError()) {
    return Error("Failed to instantiate capabilities: " + capabilities.error());
  }

  uint64_t allCaps = getAllSystemCapabilities(
      capabilities.get()->getMaxCapabilityValue());

  CapabilityInfo capabilityInfo;

  for (unsigned i = 0; i < MAX_CAPABILITY; i++) {
    if (allCaps & (1U << i)) {
      capabilityInfo.add_capabilities(static_cast<CapabilityInfo::Capability>(
          CAPABIILITY_PROTOBUF_OFFSET + i));
    }
  }

  return capabilityInfo;
}

} // namespace capabilities {
} // namespace internal {
} // namespace mesos {
