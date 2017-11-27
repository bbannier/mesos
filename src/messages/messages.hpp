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

#ifndef __MESSAGES_HPP__
#define __MESSAGES_HPP__

#include <google/protobuf/message.h>

#include <google/protobuf/io/zero_copy_stream_impl.h> // For ArrayInputStream.

#include <iosfwd>
#include <string>

#include <stout/error.hpp>
#include <stout/try.hpp>

#include "messages/messages.pb.h"

// Definitions for operators on protobuf classes that don't have these
// operators generated by the protobuf compiler.

namespace mesos {
namespace internal {

bool operator==(
    const ResourceVersionUUID& left,
    const ResourceVersionUUID& right);


bool operator!=(
    const ResourceVersionUUID& left,
    const ResourceVersionUUID& right);


std::ostream& operator<<(
    std::ostream& stream,
    const OfferOperationStatusUpdate& update);


std::ostream& operator<<(std::ostream& stream, const StatusUpdate& update);


std::ostream& operator<<(
    std::ostream& stream,
    const StatusUpdateRecord::Type& type);

} // namespace internal {
} // namespace mesos {

#endif // __MESSAGES_HPP__
