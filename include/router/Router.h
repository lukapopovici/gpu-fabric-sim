#pragma once

#include <vector>

#include "topology/Topology.h"
#include "traffic/TrafficGenerator.h"

namespace fabric::router {

// A concrete forwarding decision for one flow: the ordered list of links
// it will traverse.
struct RoutingDecision {
  std::uint64_t flow_id;
  std::vector<topology::LinkId> path;
};

// Common interface for anything that decides how a flow's bytes cross the
// fabric. StaticECMPRouter and AdaptiveRouter are the two implementations
// benchmarked against each other; both consume the same Topology and Flow
// types so the bench harness can run identical traffic through each.
class Router {
 public:
  virtual ~Router() = default;

  virtual RoutingDecision RouteFlow(const topology::Topology& topo,
                                      const traffic::Flow& flow) = 0;

  virtual const char* Name() const = 0;
};

}  // namespace fabric::router
