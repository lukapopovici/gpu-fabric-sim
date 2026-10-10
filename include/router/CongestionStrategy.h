#pragma once

#include <cstdint>

#include "sim/Clock.h"
#include "topology/Topology.h"

namespace fabric::router {

// Point-in-time state of a link that congestion strategies read from.
// Populated by the data-plane sim / telemetry stream every time a packet
// enters or leaves a link's queue -- strategies never touch the queue
// directly, they only see this snapshot. That separation is what lets
// QueueDepthStrategy, ECNStrategy, and DropRateStrategy all implement the
// same interface without knowing about each other or about the sim
// internals.
struct LinkState {
  topology::LinkId link;
  std::uint64_t queue_depth_bytes;
  std::uint64_t queue_capacity_bytes;
  std::uint64_t drops_in_window;
  std::uint64_t packets_in_window;
  sim::SimTime window_start;
  sim::SimTime now;
};

// Strategy interface (GoF Strategy pattern): each concrete strategy
// implements one way of deciding "is this link congested right now?" from
// the same LinkState snapshot. AdaptiveRouter holds a
// std::unique_ptr<CongestionStrategy> and calls IsCongested() on the
// candidate egress links for a flow -- swapping the concrete strategy is
// the entire mechanism for changing detection behavior, no changes to the
// router itself.
class CongestionStrategy {
 public:
  virtual ~CongestionStrategy() = default;

  // Returns true if `state` indicates the link should be treated as
  // congested (i.e. the adaptive router should prefer a different
  // equal-cost path for new/rerouted flows).
  virtual bool IsCongested(const LinkState& state) const = 0;

  // A normalized [0.0, 1.0] congestion score, for strategies/benchmarks
  // that want a graded signal rather than a boolean (e.g. to rank
  // multiple candidate paths instead of just filtering congested ones).
  virtual double CongestionScore(const LinkState& state) const = 0;

  // Short identifier used in bench harness output / CLI selection, e.g.
  // "queue_depth", "ecn", "drop_rate".
  virtual const char* Name() const = 0;
};

}  // namespace fabric::router
