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

#include <process/metrics/metrics.hpp>

#include <stout/strings.hpp>

#include <mesos/quota/quota.hpp>

#include "master/allocator/mesos/hierarchical.hpp"
#include "master/allocator/mesos/metrics.hpp"

using std::string;

using process::metrics::Gauge;

namespace mesos {
namespace internal {
namespace master {
namespace allocator {
namespace internal {

Metrics::Metrics(const HierarchicalAllocatorProcess& _allocator)
  : allocator(&_allocator),
    event_queue_dispatches(
        "allocator/mesos/event_queue_dispatches",
        process::defer(
            allocator, &HierarchicalAllocatorProcess::_event_queue_dispatches)),
    event_queue_dispatches_(
        "allocator/event_queue_dispatches",
        process::defer(
            allocator, &HierarchicalAllocatorProcess::_event_queue_dispatches)),
    allocation_runs("allocator/mesos/allocation_runs")
{
  process::metrics::add(event_queue_dispatches);
  process::metrics::add(event_queue_dispatches_);
  process::metrics::add(allocation_runs);

  // Create and install gauges for the total and allocated amount of
  // standard resources.
  // TODO(bbannier) Add support for more than just scalar resources.
  // TODO(bbannier) Simplify this once MESOS-3214 is fixed.
  string resourceKinds[] = {"cpus", "mem", "disk"};
  foreach (const string& resource, resourceKinds) {
    total.put(
        resource,
        Gauge(
            "allocator/mesos/" + resource + "/total",
            process::defer(
                allocator, &HierarchicalAllocatorProcess::_total, resource)));

    process::metrics::add(total.get(resource).get());

    allocated.put(
        resource,
        Gauge(
            "allocator/mesos/" + resource + "/offered_or_allocated",
            process::defer(
                allocator,
                &HierarchicalAllocatorProcess::_allocated,
                resource)));

    process::metrics::add(allocated.get(resource).get());
  }
}


Metrics::~Metrics()
{
  process::metrics::remove(event_queue_dispatches);
  process::metrics::remove(event_queue_dispatches_);
  process::metrics::remove(allocation_runs);

  foreachvalue (const Gauge& gauge, total) {
    process::metrics::remove(gauge);
  }

  foreachvalue (const Gauge& gauge, allocated) {
    process::metrics::remove(gauge);
  }

  foreachkey(const string& role, quota_allocated) {
    foreachvalue(const Gauge& gauge, quota_allocated[role]) {
      process::metrics::remove(gauge);
    }
  }

  foreachkey(const string& role, quota_guarantee) {
    foreachvalue(const Gauge& gauge, quota_guarantee[role]) {
      process::metrics::remove(gauge);
    }
  }
}


void Metrics::setQuota(const std::string& role, const Quota& quota)
{
  CHECK(!quota_allocated.contains(role));

  hashmap<string, Gauge> allocated;
  hashmap<string, Gauge> guarantee;

  foreach (const Resource& resource, quota.info.guarantee()) {
    // And gauges for the currently offered or allocated resources under quota.
    {
      Gauge gauge = Gauge(
          "allocator/mesos/quota/roles/" + role + "/" + resource.name() +
          "/offered_or_allocated",
          process::defer(
            allocator,
            &HierarchicalAllocatorProcess::_quota_allocated,
            role,
            resource.name()));

      allocated.put(resource.name(), gauge);

      process::metrics::add(allocated.get(resource.name()).get());
    }

    // Add gauges for the quota resource guarantees.
    {
      CHECK_EQ(Value::SCALAR, resource.type());
      double value = resource.scalar().value();

      Gauge gauge = Gauge(
          "allocator/mesos/quota/roles/" + role + "/" + resource.name() +
              "/guarantee",
          process::defer(allocator, [value]() { return value; }));

      guarantee.put(resource.name(), gauge);

      process::metrics::add(guarantee.get(resource.name()).get());
    }
  }

  quota_allocated[role] = allocated;
  quota_guarantee[role] = guarantee;
}


void Metrics::removeQuota(const std::string& role)
{
  CHECK(quota_allocated.contains(role));

  foreachvalue (const Gauge& gauge, quota_allocated[role]) {
    process::metrics::remove(gauge);
  }

  quota_allocated.erase(role);
}

} // namespace internal {
} // namespace allocator {
} // namespace master {
} // namespace internal {
} // namespace mesos {
