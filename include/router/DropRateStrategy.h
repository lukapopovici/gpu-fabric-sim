#pragma once

#include "router/CongestionStrategy.h"

namespace fabric::router {

// Lagging signal: trigger on a rolling window's packet drop rate rather
// than instantaneous queue occupancy. Catches sustained congestion that
// queue-depth/ECN might miss if telemetry sampling is coarse, but reacts
// slower since drops have to actually happen first. Included primarily
// as the "worst case, what does a dumb/lossy signal look like" baseline
// in the strategy comparison.
class DropRateStrategy : public CongestionStrategy {
 public:
  explicit DropRateStrategy(double drop_rate_threshold = 0.01)
      : drop_rate_threshold_(drop_rate_threshold) {}

  bool IsCongested(const LinkState& state) const override;
  double CongestionScore(const LinkState& state) const override;
  const char* Name() const override { return "drop_rate"; }

 private:
  double drop_rate_threshold_;  // e.g. 0.01 => congested at >1% window drop rate
};

}  
