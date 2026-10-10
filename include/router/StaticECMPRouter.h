#pragma once

#include "router/FlowHash.h"
#include "router/Router.h"

namespace fabric::router {

// Baseline router: picks among a flow's equal-cost paths purely by
// hashing the flow's identity (classic 5-tuple-style ECMP hashing, here
// approximated with flow_id/src/dst -- see FlowHash.h). Never looks at
// link state -- this is the "dumb but standard" baseline the
// AdaptiveRouter has to beat on p50/p99 flow completion time.
class StaticECMPRouter : public Router {
 public:
  RoutingDecision RouteFlow(const topology::Topology& topo,
                              const traffic::Flow& flow) override;

  const char* Name() const override { return "static_ecmp"; }
};

} 
