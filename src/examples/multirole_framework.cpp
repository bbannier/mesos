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

#include <stout/check.hpp>
#include <stout/exit.hpp>
#include <stout/flags.hpp>
#include <stout/foreach.hpp>
#include <stout/json.hpp>
#include <stout/option.hpp>
#include <stout/os.hpp>
#include <stout/protobuf.hpp>
#include <stout/stringify.hpp>
#include <stout/try.hpp>

#include <glog/logging.h>

#include <algorithm>
#include <cstdlib>
#include <deque>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "common/parse.hpp"
#include "common/protobuf_utils.hpp"
#include "logging/logging.hpp"
#include "v1/parse.hpp"

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

struct TaskWithRole {
  mesos::TaskInfo taskInfo;
  std::string role;
};

bool operator==(const TaskWithRole& lhs, const TaskWithRole& rhs)
{
  return std::tie(lhs.taskInfo, lhs.role) == std::tie(rhs.taskInfo, rhs.role);
}

struct Flags : public virtual flags::FlagsBase
{
  Flags()
  {
    add(&Flags::master, "master", "Master to connect to");
    add(&Flags::roles,
        "roles",
        "Array of roles the framework should subscribe as");
    add(&Flags::tasks_, "tasks", "FIXME(bbannier)");
  }

  std::string master;

  Option<JSON::Array> roles;

  JSON::Object tasks_;
  std::deque<TaskWithRole> tasks;
};

class MultiRoleSchedulerProcess
    : public process::Process<MultiRoleSchedulerProcess>
{
public:
  MultiRoleSchedulerProcess(
      const Flags& _flags, const mesos::FrameworkInfo& _frameworkInfo)
    : flags(_flags), frameworkInfo(_frameworkInfo), waitingTasks(_flags.tasks)
  {}

  void resourceOffers(
      mesos::SchedulerDriver* driver, const std::vector<mesos::Offer>& offers)
  {
    for (auto&& offer : offers) {
      // Determine the role the offer was made for.
      Option<std::string> resourcesRole_ = None();
      for (auto&& resource : offer.resources()) {
        CHECK(resource.has_allocation_info());
        CHECK(resource.allocation_info().has_role());

        if (resourcesRole_.isNone()) {
          resourcesRole_ = resource.allocation_info().role();
        } else {
          CHECK_EQ(resourcesRole_.get(), resource.allocation_info().role());
        }
      }
      CHECK_SOME(resourcesRole_);

      const std::string resourcesRole = resourcesRole_.get();
      const mesos::Resources resources(offer.resources());

      // Find waiting tasks matching the role this allocation was made to.
      std::vector<TaskWithRole> candidateTasks;
      std::copy_if(
          waitingTasks.begin(),
          waitingTasks.end(),
          std::back_inserter(candidateTasks),
          [&resourcesRole](const TaskWithRole& task) {
            return task.role == resourcesRole;
          });

      // FIXME(bbannier): pick a task that actually fits on the offer
      // instead of just sending a potentially too big task.

      if (candidateTasks.empty()) {
        // Decline offer if there is no work to do.
        driver->declineOffer(offer.id());
      } else {
        // Launch the task and transition it from waiting to running.
        TaskWithRole candidateTask = candidateTasks[0];
        mesos::TaskInfo task = candidateTask.taskInfo;
        task.mutable_slave_id()->CopyFrom(offer.slave_id());
        driver->launchTasks(offer.id(), {task});

        CHECK(
            std::find(
                runningTasks.begin(),
                runningTasks.end(),
                candidateTask) == runningTasks.end());
        runningTasks.push_back(candidateTask);

        waitingTasks.erase(
            std::remove_if(
                waitingTasks.begin(),
                waitingTasks.end(),
                [&candidateTask](const TaskWithRole& task) {
                  return task == candidateTask;
                }));
      }
    }
  }

  void statusUpdate(
      mesos::SchedulerDriver* driver, const mesos::TaskStatus& status)
  {
    auto it = std::find_if(
        runningTasks.begin(),
        runningTasks.end(),
        [&status](const TaskWithRole& task) {
          return task.taskInfo.task_id() == status.task_id();
        });

    CHECK(it != runningTasks.end())
      << "Received status update for unknown task";

    if (mesos::internal::protobuf::isTerminalState(status.state())) {
      LOG(INFO) << "Task '" << status.task_id()
                << "' has reached terminal state '" << stringify(status.state())
                << "'";
      runningTasks.erase(it);
    } else {
      LOG(INFO) << "Task '" << status.task_id()
                << "' has transitioned to state '" << stringify(status.state())
                << "'";
    }

    if (waitingTasks.empty() && runningTasks.empty()) {
      driver->stop();
    }
  }

private:
  const Flags flags;
  const mesos::FrameworkInfo frameworkInfo;

  std::deque<TaskWithRole> waitingTasks;
  std::deque<TaskWithRole> runningTasks;
};

class MultiRoleScheduler : public mesos::Scheduler
{
public:
  MultiRoleScheduler(
      const Flags& flags, const mesos::FrameworkInfo& frameworkInfo)
    : process(flags, frameworkInfo)
  {
    process::spawn(process);
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
  }

  void reregistered(
      mesos::SchedulerDriver* driver,
      const mesos::MasterInfo& masterInfo) override
  {
    LOG(FATAL) << "MultiRoleScheduler::reregistered: " << stringify(masterInfo);
  }

  void disconnected(mesos::SchedulerDriver* driver) override
  {
    LOG(FATAL) << "MultiRoleScheduler::disconnected";
  }

  void resourceOffers(
      mesos::SchedulerDriver* driver,
      const std::vector<mesos::Offer>& offers) override
  {
    LOG(INFO) << "Resource offers received";

    process::dispatch(
        process, &MultiRoleSchedulerProcess::resourceOffers, driver, offers);
  }

  void offerRescinded(
      mesos::SchedulerDriver* driver, const mesos::OfferID& offerId) override
  {
    LOG(FATAL) << "MultiRoleScheduler::offerRescinded: " << stringify(offerId);
  }

  void statusUpdate(
      mesos::SchedulerDriver* driver, const mesos::TaskStatus& status) override
  {
    LOG(INFO) << "Received status update for task " << status.task_id();
    process::dispatch(
        process, &MultiRoleSchedulerProcess::statusUpdate, driver, status);
  }

  void frameworkMessage(
      mesos::SchedulerDriver* driver,
      const mesos::ExecutorID& executorId,
      const mesos::SlaveID& slaveId,
      const std::string& data) override
  {
    LOG(FATAL) << "MultiRoleScheduler::frameworkMessage: "
               << stringify(executorId) << " " << stringify(slaveId) << " "
               << stringify(data);
  }

  void slaveLost(
      mesos::SchedulerDriver* driver, const mesos::SlaveID& slaveId) override
  {
    LOG(FATAL) << "MultiRoleScheduler::slaveLost: " << stringify(slaveId);
  }

  void executorLost(
      mesos::SchedulerDriver* driver,
      const mesos::ExecutorID& executorId,
      const mesos::SlaveID& slaveId,
      int status) override
  {
    LOG(FATAL) << "MultiRoleScheduler::executorLost: " << stringify(executorId)
               << " " << stringify(slaveId);
  }

  void error(
      mesos::SchedulerDriver* driver, const std::string& message) override
  {
    LOG(FATAL) << "MultiRoleScheduler::error: " << message;
  }
};

int main(int argc, char** argv)
{
  process::initialize();

  Flags flags;
  Try<flags::Warnings> load = flags.load("MESOS_", argc, argv);

  if (load.isError()) {
    EXIT(EXIT_FAILURE) << flags.usage(load.error());
  }

  if (flags.help) {
    EXIT(EXIT_SUCCESS) << flags.usage();
  }

  // FIXME(bbannier): Make `role` just a field.
  foreachpair (auto&& role, auto&& task, flags.tasks_.values) {
    auto task_ = protobuf::parse<mesos::TaskInfo>(task);
    if (task_.isError()) {
      EXIT(EXIT_FAILURE) << "Invalid task definition: " << task_.error();
    }

    // FIXME(bbannier): why do we need to quote these separately as
    // opposed to Resource.role ?
    flags.tasks.push_back({task_.get(), '"' + role + '"'});
  }

  mesos::FrameworkInfo framework;
  framework.set_user(""); // Have Mesos fill the current user.
  framework.set_name("Multi-role framework (C++)");
  framework.set_checkpoint(true);

  framework.add_capabilities()->set_type(
      mesos::FrameworkInfo::Capability::MULTI_ROLE);

  if (flags.roles.isSome()) {
    for (auto&& value : flags.roles->values) {
      framework.add_roles(stringify(value));
    }
  }

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

  return status;
}
