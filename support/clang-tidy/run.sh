#!/bin/bash

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

# TODO(bbannier): Enable more upstream checks, e.g., from the Google set.
CHECKS='-*,mesos-*'

CONFIGURE_FLAGS=''

OUTPUT_DIRECTORY=$(mktemp -d -t mesos-tidy.XXXXXX) || exit 1
trap 'rm -rf "$OUTPUT_DIRECTORY"' EXIT

MESOS_DIRECTORY=$(cd "$(dirname "$0")/../.." && pwd)

# Execute the container.
docker run \
    --rm \
    -v "${MESOS_DIRECTORY}":/SRC \
    -v "${OUTPUT_DIRECTORY}":/OUT \
    -e CHECKS="${CHECKS}" \
    -e CONFIGURE_FLAGS="${CONFIGURE_FLAGS}" \
    mesosphere/mesos-tidy || exit 1

# Propagate any errors.
REPORT_FILE="${OUTPUT_DIRECTORY}"/clang-tidy.log
if test -s "${REPORT_FILE}"; then
  cat "${REPORT_FILE}"
  exit 1
fi
