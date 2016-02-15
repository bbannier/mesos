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
        "allocator/event_queue_dispatches",
        process::defer(
            allocator->self(),
            &HierarchicalAllocatorProcess::_event_queue_dispatches)),
    allocation_runs("allocator/allocation_runs")
{
  process::metrics::add(event_queue_dispatches);
  process::metrics::add(allocation_runs);

  // Create and install gauges for the total and allocated amount of
  // standard resources.
  // TODO(bbannier) Add support for more than just scalar resources.
  // TODO(bbannier) Simplify this once MESOS-3214 is fixed.
  string resourceKinds[] = {"cpus", "mem", "disk"};
  foreach (const string& resourceName, resourceKinds) {
    total.put(
        resourceName,
        Gauge(
            strings::join("/", "allocator/total", resourceName),
            process::defer(allocator->self(), [this, resourceName]() {
              return this->allocator->_total(resourceName);
            })));

    process::metrics::add(total.get(resourceName).get());

    allocated.put(
        resourceName,
        Gauge(
            strings::join("/", "allocator/allocated", resourceName),
            process::defer(allocator->self(), [this, resourceName]() {
              return this->allocator->_allocated(resourceName);
            })));

    process::metrics::add(allocated.get(resourceName).get());
  }
}


Metrics::~Metrics()
{
  process::metrics::remove(event_queue_dispatches);
  process::metrics::remove(allocation_runs);

  foreachvalue (const Gauge& gauge, total) {
    process::metrics::remove(gauge);
  }

  foreachvalue (const Gauge& gauge, allocated) {
    process::metrics::remove(gauge);
  }

  typedef hashmap<string, Gauge> RoleQuotaGauges;
  foreachvalue (const RoleQuotaGauges& roleQuotaGauges, quota_allocated) {
    foreachvalue (const Gauge& gauge, roleQuotaGauges) {
      process::metrics::remove(gauge);
    }
  }

  foreachvalue (const Gauge& gauge, total) {
    process::metrics::remove(gauge);
  }

  foreachvalue (const Gauge& gauge, allocated) {
    process::metrics::remove(gauge);
  }
}


void Metrics::setQuota(const std::string& role, const Quota& quota)
{
  hashmap<string, Gauge> quotaedResources;

  foreach (const Resource& resource, quota.info.guarantee()) {
    string resourceName = resource.name();

    quotaedResources.put(
        resourceName,
        Gauge(
            strings::join(
                "/", "allocator/quota", role, "allocated", resourceName),
            process::defer(allocator->self(), [this, role, resourceName]() {
              return this->allocator->_quota_allocated(role, resourceName);
            })));

    process::metrics::add(quotaedResources.get(resourceName).get());
  }

  quota_allocated.put(role, quotaedResources);
}


void Metrics::removeQuota(const std::string& role)
{
  Option<hashmap<string, Gauge>> roleQuotaGauges = quota_allocated.get(role);
  CHECK_SOME(roleQuotaGauges);

  foreachvalue (const Gauge& gauge, roleQuotaGauges.get()) {
    process::metrics::remove(gauge);
  }

  quota_allocated.erase(role);
}

} // namespace internal {
} // namespace allocator {
} // namespace master {
} // namespace internal {
} // namespace mesos {
