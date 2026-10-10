#pragma once

#include <cstdint>
#include <deque>
#include <unordered_map>

#include "sim/Clock.h"
#include "topology/Topology.h"

namespace fabric::incast {

// Detects many-to-one traffic bursts ("incast"): N distinct senders'
// flows converging on the same destination GPU within a short time
// window. This is deliberately a *separate* signal from per-link
// congestion (QueueDepthStrategy/ECNStrategy/DropRateStrategy in
// router/): a link-level signal sees "this link's queue is filling up"
// but not "why" -- it can't distinguish one elephant flow from a dozen
// small flows all landing on the same receiver at once, which is exactly
// the collective-communication pattern (all-reduce, all-gather) most
// likely to actually happen in a training fabric.
//
// Knowing it's specifically incast (rather than a generic hot link)
// matters for the router's response: the right move for incast is often
// to *spread* new flows across more distinct paths preemptively, not to
// wait for a single path's queue to cross a threshold and then react.
class IncastDetector {
 public:
  struct Config {
    sim::SimTime window_us = 50;     // sliding window width
    std::uint32_t sender_threshold = 4;  // >= this many distinct senders
                                          // within window_us triggers incast
  };

  explicit IncastDetector(Config config) : config_(config) {}

  // Called by the data-plane sim every time a flow's first packet arrives
  // destined for `dst` from `src`, at simulated time `now`. Maintains a
  // per-destination sliding window of (time, src) arrivals.
  void RecordArrival(topology::NodeId dst, topology::NodeId src,
                      sim::SimTime now);

  // Returns true if `dst` currently has >= config_.sender_threshold
  // distinct senders with arrivals inside the last config_.window_us,
  // as of `now`.
  bool IsIncastTarget(topology::NodeId dst, sim::SimTime now) const;

  // Number of distinct senders currently in `dst`'s window -- exposed for
  // telemetry/heatmap and for tests, separately from the boolean trigger.
  std::size_t ActiveSenderCount(topology::NodeId dst, sim::SimTime now) const;

 private:
  struct Arrival {
    sim::SimTime time;
    topology::NodeId src;
  };

  // Drops entries older than window_us relative to `now`. Mutable window
  // state per destination; called from both RecordArrival and the const
  // query methods (hence mutable, since pruning on read is an
  // implementation detail, not an observable state change).
  void Prune(topology::NodeId dst, sim::SimTime now) const;

  Config config_;
  mutable std::unordered_map<topology::NodeId, std::deque<Arrival>>
      windows_;
};

}  
