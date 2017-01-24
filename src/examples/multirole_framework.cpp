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

#include <mesos/resources.hpp>
#include <mesos/scheduler.hpp>
#include <mesos/type_utils.hpp>

#include <process/dispatch.hpp>
#include <process/process.hpp>

#include <stout/exit.hpp>
#include <stout/flags.hpp>
#include <stout/option.hpp>
#include <stout/os.hpp>
#include <stout/stringify.hpp>
#include <stout/try.hpp>

#include <glog/logging.h>

#include <cstdlib>
#include <memory>
#include <string>

#include "logging/logging.hpp"

namespace mesos {
std::ostream& operator<<(std::ostream& stream, const Offer& offer)
{
  return stream << offer.DebugString();
}

std::ostream& operator<<(std::ostream& stream, const TaskStatus& status)
{
  return stream << status.DebugString();
}
} // namespace mesos {

struct Flags : public virtual flags::FlagsBase
{
  Flags() { add(&Flags::master, "master", "Master to connect to"); }
  std::string master;
};

class MultiRoleSchedulerProcess
    : public process::Process<MultiRoleSchedulerProcess>
{
public:
  MultiRoleSchedulerProcess(
      const Flags& _flags, const mesos::FrameworkInfo& _frameworkInfo)
    : flags(_flags), frameworkInfo(_frameworkInfo) {}

  void registered() { isRegistered = true; }
private:
  const Flags flags;
  const mesos::FrameworkInfo frameworkInfo;

  bool isRegistered = false;
};

class MultiRoleScheduler : public mesos::Scheduler
{
public:
  MultiRoleScheduler(
      const Flags& flags, const mesos::FrameworkInfo& frameworkInfo)
    : process(flags, frameworkInfo)
  {
  }

  virtual ~MultiRoleScheduler()
  {
    process::terminate(process);
    process::wait(process);
  }

private:
  MultiRoleSchedulerProcess process;

  void registered(
      mesos::SchedulerDriver*,
      const mesos::FrameworkID& frameworkId,
      const mesos::MasterInfo&) override
  {
    LOG(INFO) << "Registered with framework ID: " << frameworkId;
    process::dispatch(process, &MultiRoleSchedulerProcess::registered);
  }

  void reregistered(
      mesos::SchedulerDriver* driver,
      const mesos::MasterInfo& masterInfo) override
  {
    LOG(ERROR) << "MultiRoleScheduler::reregistered: " << stringify(masterInfo);
  }

  void disconnected(mesos::SchedulerDriver* driver) override
  {
    LOG(ERROR) << "MultiRoleScheduler::disconnected";
  }

  void resourceOffers(
      mesos::SchedulerDriver* driver,
      const std::vector<mesos::Offer>& offers) override
  {
    LOG(ERROR) << "MultiRoleScheduler::resourceOffers: " << stringify(offers);
  }

  void offerRescinded(
      mesos::SchedulerDriver* driver, const mesos::OfferID& offerId) override
  {
    LOG(ERROR) << "MultiRoleScheduler::offerRescinded: " << stringify(offerId);
  }

  void statusUpdate(
      mesos::SchedulerDriver* driver, const mesos::TaskStatus& status) override
  {
    LOG(ERROR) << "MultiRoleScheduler::statusUpdate: " << stringify(status);
  }

  void frameworkMessage(
      mesos::SchedulerDriver* driver,
      const mesos::ExecutorID& executorId,
      const mesos::SlaveID& slaveId,
      const std::string& data) override
  {
    LOG(ERROR) << "MultiRoleScheduler::frameworkMessage: "
               << stringify(executorId) << " " << stringify(slaveId) << " "
               << stringify(data);
  }

  void slaveLost(
      mesos::SchedulerDriver* driver, const mesos::SlaveID& slaveId) override
  {
    LOG(ERROR) << "MultiRoleScheduler::slaveLost: " << stringify(slaveId);
  }

  void executorLost(
      mesos::SchedulerDriver* driver,
      const mesos::ExecutorID& executorId,
      const mesos::SlaveID& slaveId,
      int status) override
  {
    LOG(ERROR) << "MultiRoleScheduler::executorLost: " << stringify(executorId)
               << " " << stringify(slaveId);
  }

  void error(
      mesos::SchedulerDriver* driver, const std::string& message) override
  {
    LOG(ERROR) << "MultiRoleScheduler::error: " << message;
  }
};

int main(int argc, char** argv)
{
  Flags flags;
  Try<flags::Warnings> load = flags.load("MESOS_", argc, argv);

  if (load.isError()) {
    EXIT(EXIT_FAILURE) << flags.usage(load.error());
  }

  mesos::FrameworkInfo framework;
  framework.set_user(""); // Have Mesos fill the current user.
  framework.set_name("Multi-role framework (C++)");
  framework.set_checkpoint(true);

  MultiRoleScheduler scheduler(flags, framework);

  std::unique_ptr<mesos::MesosSchedulerDriver> driver;

  // TODO(hartem): Refactor these into a common set of flags.
  Option<std::string> value = os::getenv("MESOS_AUTHENTICATE_FRAMEWORKS");
  if (value.isSome()) {
    LOG(INFO) << "Enabling authentication for the framework";

    value = os::getenv("DEFAULT_PRINCIPAL");
    if (value.isNone()) {
      EXIT(EXIT_FAILURE)
          << "Expecting authentication principal in the environment";
    }

    mesos::Credential credential;
    credential.set_principal(value.get());

    framework.set_principal(value.get());

    value = os::getenv("DEFAULT_SECRET");
    if (value.isNone()) {
      EXIT(EXIT_FAILURE)
          << "Expecting authentication secret in the environment";
    }

    credential.set_secret(value.get());

    driver.reset(
        new mesos::MesosSchedulerDriver(
            &scheduler, framework, flags.master, credential));
  } else {
    framework.set_principal("multirole-framework-cpp");

    driver.reset(
        new mesos::MesosSchedulerDriver(&scheduler, framework, flags.master));
  }

  int status = driver->run() != mesos::DRIVER_STOPPED;

  // Ensure that the driver process terminates.
  driver->stop();

  return status;
}
