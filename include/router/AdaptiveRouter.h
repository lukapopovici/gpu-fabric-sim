#pragma once

#include <memory>
#include <unordered_map>

#include "incast/IncastDetector.h"
#include "router/CongestionStrategy.h"
#include "router/Router.h"

namespace fabric::router {

// Congestion-aware router: among a flow's equal-cost paths, prefers the
// one whose links score lowest on the injected CongestionStrategy, and
// consults an IncastDetector to react to many-to-one bursts distinctly
// from generic per-link congestion (see IncastDetector.h for why that's
// a separate signal).
//
// The strategy is injected as a std::unique_ptr<CongestionStrategy> --
// this class never knows or cares whether it's holding a
// QueueDepthStrategy, ECNStrategy, or DropRateStrategy. That's the whole
// point of the Strategy pattern here: benchmarking three detection
// signals against static ECMP (and against each other) is a matter of
// constructing three AdaptiveRouters with different strategies, not three
// different router implementations.
class AdaptiveRouter : public Router {
 public:
  AdaptiveRouter(std::unique_ptr<CongestionStrategy> strategy,
                  std::unique_ptr<incast::IncastDetector> incast_detector);

  RoutingDecision RouteFlow(const topology::Topology& topo,
                              const traffic::Flow& flow) override;

  const char* Name() const override { return "adaptive"; }

  // Called by the data-plane sim whenever a link's queue/drop counters
  // change, so the router's view of link state stays current between
  // routing decisions.
  void UpdateLinkState(const CongestionStrategy&, const LinkState& state) {
    link_state_[state.link] = state;
  }

  const CongestionStrategy& Strategy() const { return *strategy_; }

  // Forwards to the internal IncastDetector -- called by the data-plane
  // sim whenever a flow's first packet arrives at its destination, so the
  // many-to-one burst signal stays current between routing decisions.
  void RecordArrival(topology::NodeId dst, topology::NodeId src,
                      sim::SimTime now) {
    incast_detector_->RecordArrival(dst, src, now);
  }

  bool IsIncastTarget(topology::NodeId dst, sim::SimTime now) const {
    return incast_detector_->IsIncastTarget(dst, now);
  }

 private:
  std::unique_ptr<CongestionStrategy> strategy_;
  std::unique_ptr<incast::IncastDetector> incast_detector_;

  // Most recent LinkState we've observed per link, keyed by LinkId. This
  // is what CongestionScore()/IsCongested() get evaluated against when
  // picking among a flow's equal-cost paths.
  std::unordered_map<topology::LinkId, LinkState> link_state_;

  // Sums CongestionScore() across a candidate path's links -- lower is
  // better. Used for the default (non-incast) greedy path selection.
  double ScorePath(const std::vector<topology::LinkId>& path) const;

  // Per-destination round-robin counter used only while that destination
  // is flagged as an incast target -- see RouteFlow() for why incast
  // gets a different selection strategy than the greedy-lowest-score
  // default.
  std::unordered_map<topology::NodeId, std::size_t> incast_round_robin_;
};

}  // namespace fabric::router
