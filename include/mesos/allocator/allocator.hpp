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

#ifndef __MESOS_ALLOCATOR_ALLOCATOR_HPP__
#define __MESOS_ALLOCATOR_ALLOCATOR_HPP__

#include <string>
#include <vector>
#include <tuple>

// ONLY USEFUL AFTER RUNNING PROTOC.
#include <mesos/allocator/allocator.pb.h>

#include <mesos/maintenance/maintenance.hpp>

#include <mesos/quota/quota.hpp>

#include <mesos/resources.hpp>

#include <process/future.hpp>

#include <stout/check.hpp>
#include <stout/duration.hpp>
#include <stout/hashmap.hpp>
#include <stout/hashset.hpp>
#include <stout/lambda.hpp>
#include <stout/option.hpp>
#include <stout/try.hpp>

namespace mesos {

enum class SourceType
{
  UNKNOWN,
  AGENT,
  RESOURCE_PROVIDER
};

namespace allocator {

class SourceID
{
public:
  SourceID() = default;

  SourceID(const SlaveID& agentId)
    : value(agentId.value()),
      type(SourceType::AGENT) {}

  SourceID(const ResourceProviderID& resourceProviderId)
    : value(resourceProviderId.value()),
      type(SourceType::RESOURCE_PROVIDER) {}

  std::string value;
  SourceType type = SourceType::UNKNOWN;

  friend bool operator==(const SourceID& left, const SourceID& right)
  {
    return std::tie(left.value, left.type) == std::tie(right.value, right.type);
  }

  friend std::ostream& operator<<(
      std::ostream& stream,
      const SourceID& sourceId)
  {
    return stream << sourceId.value;
  }

  // FIXME(bbannier): This is a transient helper function. Remove it
  // once the users it have been update.
  /*implicit*/ operator SlaveID() const
  {
    CHECK(type == SourceType::AGENT);
    SlaveID slaveId;
    slaveId.set_value(value);
    return slaveId;
  }
};


class SourceInfo {
public:
  SourceInfo() = default;

  SourceInfo(
      const SlaveInfo& agentInfo_,
      const std::vector<SlaveInfo::Capability>& capabilities_)
    : id(agentInfo_.id()),
      capabilities(capabilities_),
      hostname(agentInfo_.hostname()) {}

      SourceInfo(
          const ResourceProviderInfo& resourceProviderInfo_,
          const Option<std::string>& hostname_)
        : id(resourceProviderInfo_.id()),
          hostname(hostname_)
      {
        // We use a switch statement here so that adding unhandled capabilities
        // triggers a warning. New values should be added as separate cases
        // updating the value in the capabilities container with fallthrough to
        // the next value.
        std::vector<SlaveInfo::Capability> allCapabilities(1);
        switch (SlaveInfo::Capability::UNKNOWN) {
          case SlaveInfo::Capability::UNKNOWN:
          case SlaveInfo::Capability::MULTI_ROLE:
            allCapabilities[0].set_type(SlaveInfo::Capability::MULTI_ROLE);
        }

        capabilities = allCapabilities;
      }

  SourceID id;

  std::vector<SlaveInfo::Capability> capabilities;
  Option<std::string> hostname;

  friend bool operator==(const SourceInfo& left, const SourceInfo& right) {
    return std::tie(left.id, left.capabilities) ==
           std::tie(right.id, right.capabilities);
  }
};

} // namespace allocator {
} // namespace mesos {

namespace std {
template <>
struct hash<mesos::allocator::SourceID>
{
  std::size_t operator()(const mesos::allocator::SourceID& sourceId) const
  {
    return std::hash<std::string>()(sourceId.value);
  }
};
}


namespace mesos {
namespace allocator {

/**
 * Basic model of an allocator: resources are allocated to a framework
 * in the form of offers. A framework can refuse some resources in
 * offers and run tasks in others. Allocated resources can have offer
 * operations applied to them in order for frameworks to alter the
 * resource metadata (e.g. creating persistent volumes). Resources can
 * be recovered from a framework when tasks finish/fail (or are lost
 * due to an agent failure) or when an offer is rescinded.
 *
 * This is the public API for resource allocators.
 */
class Allocator
{
public:
  /**
   * Attempts either to create a built-in DRF allocator or to load an
   * allocator instance from a module using the given name. If `Try`
   * does not report an error, the wrapped `Allocator*` is not null.
   *
   * @param name Name of the allocator.
   */
  static Try<Allocator*> create(const std::string& name);

  Allocator() {}

  virtual ~Allocator() {}

  /**
   * Initializes the allocator when the master starts up. Any errors in
   * initialization should fail fast and result in an ABORT. The master expects
   * the allocator to be successfully initialized if this call returns.
   *
   * @param allocationInterval The allocate interval for the allocator, it
   *     determines how often the allocator should perform the batch
   *     allocation. An allocator may also perform allocation based on events
   *     (a framework is added and so on), this depends on the implementation.
   * @param offerCallback A callback the allocator uses to send allocations
   *     to the frameworks.
   * @param inverseOfferCallback A callback the allocator uses to send reclaim
   *     allocations from the frameworks.
   */
  virtual void initialize(
      const Duration& allocationInterval,
      const lambda::function<void(
          const FrameworkID&,
          const hashmap<std::string, hashmap<SourceID, Resources>>&)>&
        offerCallback,
      const lambda::function<void(
          const FrameworkID&, const hashmap<SourceID, UnavailableResources>&)>&
        inverseOfferCallback,
      const Option<std::set<std::string>>& fairnessExcludeResourceNames =
        None()) = 0;

  /**
   * Informs the allocator of the recovered state from the master.
   *
   * Because it is hard to define recovery for a running allocator, this
   * method should be called after `initialize()`, but before actual
   * allocation starts (i.e. `addSlave()` is called).
   *
   * TODO(alexr): Consider extending the signature with expected
   * frameworks count once it is available upon the master failover.
   *
   * @param quotas A (possibly empty) collection of quotas, keyed by
   *     their role, known to the master.
   */
  virtual void recover(
      const int expectedAgentCount,
      const hashmap<std::string, Quota>& quotas) = 0;

  /**
   * Adds a framework to the Mesos cluster. The allocator is invoked when
   * a new framework joins the Mesos cluster and is entitled to participate
   * in resource sharing.
   *
   * @param used Resources used by this framework. The allocator should
   *     account for these resources when updating the allocation of this
   *     framework. The allocator should avoid double accounting when yet
   *     unknown resource providers are added later in `addSlave()`.
   *
   * @param active Whether the framework is initially activated.
   */
  virtual void addFramework(
      const FrameworkID& frameworkId,
      const FrameworkInfo& frameworkInfo,
      const hashmap<SourceID, Resources>& used,
      bool active) = 0;

  /**
   * Removes a framework from the Mesos cluster. It is up to an allocator to
   * decide what to do with framework's resources. For example, they may be
   * released and added back to the shared pool of resources.
   */
  virtual void removeFramework(
      const FrameworkID& frameworkId) = 0;

  /**
   * Activates a framework in the Mesos cluster.
   * Offers are only sent to active frameworks.
   */
  virtual void activateFramework(
      const FrameworkID& frameworkId) = 0;

   /**
   * Deactivates a framework in the Mesos cluster.
   * Resource offers are not sent to deactivated frameworks.
   */
  virtual void deactivateFramework(
      const FrameworkID& frameworkId) = 0;

  /**
   * Updates capabilities of a framework in the Mesos cluster.
   *
   * This will be invoked when a framework is re-added. As some of the
   * framework's capabilities may be updated when re-added, this API should
   * update the capabilities of the newly added framework to Mesos cluster to
   * reflect the latest framework info. Please refer to the design document here
   * https://cwiki.apache.org/confluence/display/MESOS/Design+doc:+Updating+Framework+Info // NOLINT
   * for more details related to framework update.
   */
  virtual void updateFramework(
      const FrameworkID& frameworkId,
      const FrameworkInfo& frameworkInfo) = 0;

  /**
   * Adds or re-adds an agent to the Mesos cluster. It is invoked when a
   * new agent joins the cluster or in case of agent recovery.
   *
   * @param sourceID ID of the source to be added or re-added.
   * @param sourceInfo information on the source being added or re-added.
   * @param total The `total` resources are passed explicitly because it
   *     includes resources that are dynamically "checkpointed" on the agent
   *     (e.g. persistent volumes, dynamic reservations, etc).
   * @param used Resources that are allocated on the current agent. The
   *     allocator should avoid double accounting when yet unknown frameworks
   *     are added later in `addFramework()`.
   */
  // FIXME(bbannier): Rename to `addSource`.
  virtual void addSlave(
      const SourceID& sourceId,
      const SourceInfo& sourceInfo,
      const Option<Unavailability>& unavailability,
      const Resources& total,
      const hashmap<FrameworkID, Resources>& used) = 0;

  /**
   * Removes a resource provider from the Mesos cluster. All resources belonging
   * to this provider should be released by the allocator.
   */
  // FIXME(bbannier): Rename to `removeSource`.
  virtual void removeSlave(
      const SourceID& sourceID) = 0;

  /**
   * Updates a resource provider.
   *
   * Updates the latest oversubscribed resources or capabilities for a
   * resource provider.
   * TODO(vinod): Instead of just oversubscribed resources have this
   * method take total resources. We can then reuse this method to
   * update resource providers's total resources in the future.
   *
   * @param oversubscribed The new oversubscribed resources estimate from
   *     the resource provider. The oversubscribed resources include the total
   *     amount of oversubscribed resources that are allocated and available.
   * @param sourceInfo The new SourceInfo of a resource provider. This is
   *     only expected to be set when a resource provider agent is added.
   */
  // FIXME(bbannier): Rename to `updateSource`.
  virtual void updateSlave(
      const SourceID& sourceId,
      const Option<Resources>& oversubscribed = None(),
      const Option<SourceInfo>& sourceInfo = None()) = 0;
  // FIXME(bbannier): provide semantics of updateSourceTotal, either
  // here or in another function, at least for RPs.

  /**
   * Activates an agent. This is invoked when an agent reregisters. Offers
   * are only sent for activated agents.
   */
  // FIXME(bbannier): Rename to `activateSource`.
  virtual void activateSlave(
      const SourceID& sourceId) = 0;

  /**
   * Deactivates an agent.
   *
   * This is triggered if an agent disconnects from the master. The allocator
   * should treat all offers from the deactivated agent as rescinded. (There
   * is no separate call to the allocator to handle this). Resources aren't
   * "recovered" when an agent deactivates because the resources are lost.
   */
  // FIXME(bbannier): Rename to `deactivateSource`.
  virtual void deactivateSlave(const SourceID& sourceId) = 0;

  /**
   * Updates the list of trusted agents.
   *
   * This is invoked when the master starts up with the --whitelist flag.
   *
   * @param whitelist A set of agents that are allowed to contribute
   *     their resources to the resource pool.
   */
  virtual void updateWhitelist(
      const Option<hashset<std::string>>& whitelist) = 0;

  /**
   * Requests resources for a framework.
   *
   * A framework may request resources via this call. It is up to the allocator
   * how to react to this request. For example, a request may be ignored, or
   * may influence internal priorities the allocator may keep for frameworks.
   */
  virtual void requestResources(
      const FrameworkID& frameworkId,
      const std::vector<Request>& requests) = 0;

  /**
   * Updates allocation by applying offer operations.
   *
   * This call is mainly intended to support persistence-related features
   * (dynamic reservation and persistent volumes). The allocator may react
   * differently for certain offer operations. The allocator should use this
   * call to update bookkeeping information related to the framework. The
   * `offeredResources` are the resources that the operations are applied to
   * and must be allocated to a single role.
   */
  virtual void updateAllocation(
      const FrameworkID& frameworkId,
      const SourceID& sourceId,
      const Resources& offeredResources,
      const std::vector<Offer::Operation>& operations) = 0;

  /**
   * Updates available resources on a resource provider based on a
   * sequence of offer operations. Operations may include e.g., reserve,
   * unreserve, create or destroy.
   *
   * @param sourceID ID of the resource provider.
   * @param operations The offer operations to apply to this
   *     providers's resources.
   */
  virtual process::Future<Nothing> updateAvailable(
      const SourceID& sourceId,
      const std::vector<Offer::Operation>& operations) = 0;

  /**
   * Updates unavailability for a a resource provider.
   *
   * We currently support storing the next unavailability, if there is one,
   * per resource provider. If `unavailability` is not set then there is no
   * known upcoming unavailability. This might require the implementation of the
   * function to remove any inverse offers that are outstanding.
   */
  virtual void updateUnavailability(
      const SourceID& sourceId,
      const Option<Unavailability>& unavailability) = 0;

  /**
   * Updates inverse offer.
   *
   * Informs the allocator that the inverse offer has been responded to or
   * revoked.
   *
   * @param unavailableResources The `unavailableResources` can be used by the
   *     allocator to distinguish between different inverse offers sent to the
   *     same framework for the same slave.
   * @param status If `status` is not set then the inverse offer was not
   *     responded to, possibly because the offer timed out or was rescinded.
   *     This might require the implementation of the function to remove any
   *     inverse offers that are outstanding.
   * @param filters A filter attached to the inverse offer can be used by the
   *     framework to control when it wants to be contacted again with the
   *     inverse offer. The "filters" for InverseOffers are identical to the
   *     existing mechanism for re-offering Offers to frameworks.
   */
  virtual void updateInverseOffer(
      const SourceID& sourceId,
      const FrameworkID& frameworkId,
      const Option<UnavailableResources>& unavailableResources,
      const Option<InverseOfferStatus>& status,
      const Option<Filters>& filters = None()) = 0;

  /**
   * Retrieves the status of all inverse offers maintained by the allocator.
   */
  virtual process::Future<hashmap<
      SourceID,
      hashmap<FrameworkID, InverseOfferStatus>>>
  getInverseOfferStatuses() = 0;

  /**
   * Recovers resources.
   *
   * Used to update the set of available resources for a specific resource
   * provider. This method is invoked to inform the allocator about allocated
   * resources that have been refused or are no longer in use. Allocated
   * resources will have an `allocation_info.role` assigned and callers are
   * expected to only call this with resources allocated to a single role.
   *
   * TODO(bmahler): We could allow resources allocated to multiple roles
   * within a single call here, but filtering them in the same way does
   * not seem desirable.
   */
  virtual void recoverResources(
      const FrameworkID& frameworkId,
      const SourceID& sourceId,
      const Resources& resources,
      const Option<Filters>& filters) = 0;

  /**
   * Suppresses offers.
   *
   * Informs the allocator to stop sending offers to this framework for the
   * specified role. If the role is not specified, we will stop sending offers
   * to this framework for all of its roles.
   *
   * @param role The optional role parameter allows frameworks with multiple
   *     roles to do fine-grained suppression.
   */
  virtual void suppressOffers(
      const FrameworkID& frameworkId,
      const Option<std::string>& role) = 0;

  /**
   * Revives offers to this framework for the specified role. This is
   * invoked by a framework when it wishes to receive filtered resources
   * immediately or get itself out of a suppressed state.
   *
   * @param role The optional role parameter allows frameworks with multiple
   *     roles to do fine-grained revival.
   */
  virtual void reviveOffers(
      const FrameworkID& frameworkId,
      const Option<std::string>& role) = 0;

  /**
   * Informs the allocator to set quota for the given role.
   *
   * It is up to the allocator implementation how to satisfy quota. An
   * implementation may employ different strategies for roles with or
   * without quota. Hence an empty (or zero) quota is not necessarily the
   * same as an absence of quota. Logically, this method implies that the
   * given role should be transitioned to the group of roles with quota
   * set. An allocator implementation may assert quota for the given role
   * is not set prior to the call and react accordingly if this assumption
   * is violated (i.e. fail).
   *
   * TODO(alexr): Consider returning a future which an allocator can fail
   * in order to report failure.
   *
   * TODO(alexr): Consider adding an `updateQuota()` method which allows
   * updating existing quota.
   */
  virtual void setQuota(
      const std::string& role,
      const Quota& quota) = 0;

  /**
   * Informs the allocator to remove quota for the given role.
   *
   * An allocator implementation may employ different strategies for roles
   * with or without quota. Hence an empty (or zero) quota is not necessarily
   * the same as an absence of quota. Logically, this method implies that the
   * given role should be transitioned to the group of roles without quota
   * set (absence of quota). An allocator implementation may assert quota
   * for the given role is set prior to the call and react accordingly if
   * this assumption is violated (i.e. fail).
   *
   * TODO(alexr): Consider returning a future which an allocator can fail in
   * order to report failure.
   */
  virtual void removeQuota(
      const std::string& role) = 0;

  /**
   * Updates the weight associated with one or more roles. If a role
   * was previously configured to have a weight and that role is
   * omitted from this list, it keeps its old weight.
   */
  virtual void updateWeights(
      const std::vector<WeightInfo>& weightInfos) = 0;
};

} // namespace allocator {
} // namespace mesos {

#endif // __MESOS_MASTER_ALLOCATOR_HPP__
