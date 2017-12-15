# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# THE CSI LIBRARY.
##################
add_library(csi
  client.cpp
  paths.cpp
  utils.cpp
  state.pb.cc)

add_custom_command(
  OUTPUT state.pb.h state.pb.cc
  COMMAND protoc
    -I ${CMAKE_CURRENT_SOURCE_DIR}
    -I ${CSI_PATH}
    --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/state.proto
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/state.proto)

target_link_libraries(csi PUBLIC
  mesos-protobufs
  stout
  process
  csi-interface)

# FIXME(bbannier): csi headers should probably be included with a relative path.
target_include_directories(csi PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/.. ${MESOS_PUBLIC_INCLUDE_DIR})
target_include_directories(csi PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
