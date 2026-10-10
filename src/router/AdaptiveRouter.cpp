#include "router/AdaptiveRouter.h"

#include <algorithm>

#include "router/FlowHash.h"

namespace fabric::router {

AdaptiveRouter::AdaptiveRouter(
    std::unique_ptr<CongestionStrategy> strategy,
    std::unique_ptr<incast::IncastDetector> incast_detector)
    : strategy_(std::move(strategy)),
      incast_detector_(std::move(incast_detector)) {}

double AdaptiveRouter::ScorePath(
    const std::vector<topology::LinkId>& path) const {
  double total = 0.0;
  for (topology::LinkId link : path) {
    auto it = link_state_.find(link);
    if (it == link_state_.end()) continue;  // no telemetry yet -> assume clear
    total += strategy_->CongestionScore(it->second);
  }
  return total;
}

RoutingDecision AdaptiveRouter::RouteFlow(const topology::Topology& topo,
                                            const traffic::Flow& flow) {
  const auto paths = topo.EqualCostPaths(flow.src, flow.dst);
  RoutingDecision decision{flow.flow_id, {}};

  if (paths.empty()) {
    // No route found (e.g. cross-pod traffic before a core tier exists).
    return decision;
  }

  // flow.start_time doubles as "now" here: RouteFlow is always invoked
  // from a simulator event scheduled at the flow's start time, so this
  // is the correct simulated instant to query incast state at.
  if (incast_detector_->IsIncastTarget(flow.dst, flow.start_time)) {
    // Under incast, actively spread across all equal-cost paths in
    // round-robin order rather than greedily picking the single
    // lowest-congestion-score path. A many-to-one burst needs its
    // senders fanned out across distinct spine links *before* any one
    // path's queue fills up -- waiting for per-link congestion scores to
    // diverge (the greedy branch below) reacts one link too late for
    // incast specifically. That's the whole reason this is a separate
    // code path instead of just folding an incast penalty into
    // ScorePath: the fix for incast is diversification, not "avoid the
    // currently-worst link".
    std::size_t& idx = incast_round_robin_[flow.dst];
    decision.path = paths[idx % paths.size()];
    ++idx;
    return decision;
  }

  // Score every candidate path and convert to a selection weight instead
  // of deterministically picking the single lowest-scored path. A
  // winner-take-all pick causes herding oscillation in practice: once
  // path A looks even marginally less congested than B, *every*
  // subsequent flow switches fully onto A -- which then becomes the new
  // hotspot, so the next batch of flows all pile onto B instead. That's
  // worse than either static ECMP's fixed hash spread or a smoothly
  // weighted choice. Weighting proportionally to (1 - congestion) means
  // a lightly-congested path gets most, but not all, of the new flow
  // share, which damps the oscillation instead of amplifying it.
  std::vector<double> weights;
  weights.reserve(paths.size());
  double total_weight = 0.0;
  for (const auto& path : paths) {
    const double raw_score = ScorePath(path);
    const double avg_score =
        path.empty() ? 0.0 : raw_score / static_cast<double>(path.size());
    // Floor so a congested path still gets *some* share rather than
    // being starved to zero -- starving it entirely just recreates the
    // winner-take-all oscillation problem one level down, since it would
    // then look artificially clear the moment traffic stops.
    constexpr double kWeightFloor = 0.05;
    const double weight = std::max(kWeightFloor, 1.0 - std::min(avg_score, 1.0));
    weights.push_back(weight);
    total_weight += weight;
  }

  // Deterministic weighted pick via the flow's hash -- same flow always
  // lands on the same path for its lifetime (flow_id doesn't change
  // mid-flow, matching real ECMP/flow-affinity behavior), while new
  // flows overall distribute proportionally to path health.
  const std::uint64_t hash = HashFlow(flow);
  const double target =
      (static_cast<double>(hash % 1'000'000) / 1'000'000.0) * total_weight;
  double cumulative = 0.0;
  const std::vector<topology::LinkId>* chosen = &paths.front();
  for (std::size_t i = 0; i < paths.size(); ++i) {
    cumulative += weights[i];
    if (target <= cumulative) {
      chosen = &paths[i];
      break;
    }
  }

  decision.path = *chosen;
  return decision;
}

}  // namespace fabric::router
