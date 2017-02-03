#!/usr/bin/env bash

# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at

#     http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -e
set -o pipefail

git clone --depth 1 file:///SRC "${1}"
(cd "${1}" && ./bootstrap)

# Configure sources
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
      ${CMAKE_ARGS} \
      "${1}"

export MAKEOPTS="-j $(nproc)"

# Build the external dependencies.
# TODO(mpark): Use an external dependencies target once MESOS-6924 is resolved.
cmake --build 3rdparty --target boost-1.53.0
cmake --build 3rdparty --target elfio-3.2
cmake --build 3rdparty --target glog-0.3.3
cmake --build 3rdparty --target gmock-1.7.0
cmake --build 3rdparty --target http_parser-2.6.2

# TODO(mpark): The `|| true` is a hack to try both `libev` and `libevent` and
#              use whichever one happens to be configured. This would also go
#              away with MESOS-6924.
cmake --build 3rdparty --target libev-4.22 || true
cmake --build 3rdparty --target libevent-2.1.5-beta || true

cmake --build 3rdparty --target leveldb-1.4
cmake --build 3rdparty --target nvml-352.79
cmake --build 3rdparty --target picojson-1.3.0
cmake --build 3rdparty --target protobuf-2.6.1
cmake --build 3rdparty --target zookeeper-3.4.8

# Generate the protobuf definitions.
# TODO(mpark): Use a protobuf generation target once MESOS-6925 is resolved.
cmake --build . --target mesos-protobufs
