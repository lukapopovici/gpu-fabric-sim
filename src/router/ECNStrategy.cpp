#include "router/ECNStrategy.h"

#include <algorithm>

namespace fabric::router {

double ECNStrategy::CongestionScore(const LinkState& state) const {
  if (state.queue_capacity_bytes == 0) return 0.0;
  const double occupancy =
      static_cast<double>(state.queue_depth_bytes) /
      static_cast<double>(state.queue_capacity_bytes);

  if (occupancy <= min_threshold_fraction_) return 0.0;
  if (occupancy >= max_threshold_fraction_) return 1.0;

  // Linear ramp between the two thresholds (textbook RED marking curve).
  const double span = max_threshold_fraction_ - min_threshold_fraction_;
  return (occupancy - min_threshold_fraction_) / span;
}

bool ECNStrategy::IsCongested(const LinkState& state) const {
  // Treat "marking probability > 0" as the congestion boolean; callers
  // that want a graded decision (e.g. probabilistic marking per-packet)
  // should use CongestionScore() directly instead.
  return CongestionScore(state) > 0.0;
}

}  // namespace fabric::router
