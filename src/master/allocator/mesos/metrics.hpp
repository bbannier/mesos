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

#ifndef __MASTER_ALLOCATOR_MESOS_METRICS_HPP__
#define __MASTER_ALLOCATOR_MESOS_METRICS_HPP__

#include <string>

#include <process/metrics/counter.hpp>
#include <process/metrics/gauge.hpp>

#include <process/pid.hpp>

#include <stout/hashmap.hpp>

namespace mesos {
namespace internal {
namespace master {
namespace allocator {
namespace internal {

// Forward declarations.
class HierarchicalAllocatorProcess;

// Collection of metrics for the allocator; these begin
// with the following prefix: `allocator/mesos/`.
struct Metrics
{
  explicit Metrics(const HierarchicalAllocatorProcess& allocator);

  ~Metrics();

  void setQuota(const std::string& role, const Quota& quota);

  void removeQuota(const std::string& role);

  const process::PID<HierarchicalAllocatorProcess> allocator;

  // Number of dispatch events currently waiting in the allocator process.
  process::metrics::Gauge event_queue_dispatches;

  // TODO(bbannier) This metric is identical to `event_queue_dispatches`, but
  // uses a name deprecated in 0.29. This metric should be removed with 0.30.
  process::metrics::Gauge event_queue_dispatches_;

  // Number of times the allocation loop was triggered.
  process::metrics::Counter allocation_runs;

  // Gauges for the total amount of each resource kind in the cluster.
  hashmap<std::string, process::metrics::Gauge> total;

  // Gauges for the allocated amount of each resource kind in the cluster.
  hashmap<std::string, process::metrics::Gauge> allocated;

  // Gauges for the per-role quota allocation for each resource kind.
  hashmap<std::string, hashmap<std::string, process::metrics::Gauge>>
      quota_allocated;

  // Gauges for the per-role quota guarantee for each resource kind.
  hashmap<std::string, hashmap<std::string, process::metrics::Gauge>>
      quota_guarantee;
};

} // namespace internal {
} // namespace allocator {
} // namespace master {
} // namespace internal {
} // namespace mesos {

#endif // __MASTER_ALLOCATOR_MESOS_METRICS_HPP__
