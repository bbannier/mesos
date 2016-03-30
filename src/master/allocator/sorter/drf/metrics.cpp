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

#include "master/allocator/sorter/drf/metrics.hpp"

#include <process/defer.hpp>

#include <process/metrics/metrics.hpp>

#include <stout/foreach.hpp>

#include "master/allocator/sorter/drf/sorter.hpp"

using std::string;

using process::UPID;
using process::defer;

using process::metrics::Gauge;

namespace mesos {
namespace internal {
namespace master {
namespace allocator {

Metrics::Metrics(
    const UPID& _context,
    DRFSorter& _sorter,
    const std::function<string(const string&)>& _keyFactory)
  : context(_context),
    sorter(&_sorter),
    keyFactory(_keyFactory) {}


Metrics::~Metrics() {
  foreachvalue (const Gauge& gauge, share) {
    process::metrics::remove(gauge);
  }
}


void Metrics::add(const string& client)
{
  CHECK(!share.contains(client));

  Gauge gauge(
      keyFactory(client),
      defer(context, [this, client]() {
        return sorter->calculateShare(client);
      }));

  share.put(client, gauge);

  process::metrics::add(gauge);
}


void Metrics::remove(const string& client)
{
  Option<Gauge> gauge = share.get(client);

  CHECK_SOME(gauge);

  share.erase(client);

  process::metrics::remove(gauge.get());
}

} // namespace allocator {
} // namespace master {
} // namespace internal {
} // namespace mesos {
