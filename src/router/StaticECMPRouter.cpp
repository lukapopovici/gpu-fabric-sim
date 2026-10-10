#include "router/StaticECMPRouter.h"

namespace fabric::router {

RoutingDecision StaticECMPRouter::RouteFlow(const topology::Topology& topo,
                                              const traffic::Flow& flow) {
  const auto paths = topo.EqualCostPaths(flow.src, flow.dst);
  RoutingDecision decision{flow.flow_id, {}};

  if (paths.empty()) {
    return decision;
  }

  const std::uint64_t idx = HashFlow(flow) % paths.size();
  decision.path = paths[idx];
  return decision;
}

}  // namespace fabric::router
