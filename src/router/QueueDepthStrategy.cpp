#include "router/QueueDepthStrategy.h"

#include <algorithm>

namespace fabric::router {

double QueueDepthStrategy::CongestionScore(const LinkState& state) const {
  if (state.queue_capacity_bytes == 0) return 0.0;
  const double occupancy =
      static_cast<double>(state.queue_depth_bytes) /
      static_cast<double>(state.queue_capacity_bytes);
  return std::clamp(occupancy, 0.0, 1.0);
}

bool QueueDepthStrategy::IsCongested(const LinkState& state) const {
  return CongestionScore(state) >= threshold_fraction_;
}

}  // namespace fabric::router
