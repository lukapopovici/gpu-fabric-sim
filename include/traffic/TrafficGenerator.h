#pragma once

#include <cstdint>
#include <vector>

#include "sim/Clock.h"
#include "topology/Topology.h"

namespace fabric::traffic {

// A single flow injected into the fabric: one GPU sending `bytes` to
// another GPU, starting at `start_time`. This is the unit the router
// routes and the bench harness measures completion time for.
struct Flow {
  std::uint64_t flow_id;
  topology::NodeId src;
  topology::NodeId dst;
  std::uint64_t bytes;
  sim::SimTime start_time;
};


class TrafficGenerator {
 public:
  virtual ~TrafficGenerator() = default;
  virtual std::vector<Flow> GenerateFlows(const topology::Topology& topo,
                                            std::uint64_t seed) const = 0;
};

} 
