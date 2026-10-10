#include "router/DropRateStrategy.h"

#include <algorithm>

namespace fabric::router {

double DropRateStrategy::CongestionScore(const LinkState& state) const {
  if (state.packets_in_window == 0) return 0.0;
  const double drop_rate =
      static_cast<double>(state.drops_in_window) /
      static_cast<double>(state.packets_in_window);
  if (drop_rate_threshold_ <= 0.0) return drop_rate > 0.0 ? 1.0 : 0.0;
  // Normalize relative to threshold so score hits 1.0 right at the
  // threshold rather than requiring 100% drop rate to saturate.
  return std::clamp(drop_rate / drop_rate_threshold_, 0.0, 1.0);
}

bool DropRateStrategy::IsCongested(const LinkState& state) const {
  if (state.packets_in_window == 0) return false;
  const double drop_rate =
      static_cast<double>(state.drops_in_window) /
      static_cast<double>(state.packets_in_window);
  return drop_rate >= drop_rate_threshold_;
}

}  // namespace fabric::router
