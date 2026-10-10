#pragma once

#include "router/CongestionStrategy.h"

namespace fabric::router {

// RED/ECN-style signal: marking probability ramps linearly from 0 at
// `min_threshold_fraction` occupancy to 1.0 at `max_threshold_fraction`
// occupancy, rather than a hard cutoff. This gives an earlier, softer
// signal than QueueDepthStrategy -- the router can start nudging flows
// away from a link before its queue is actually full, at the cost of
// being more sensitive to transient bursts (tunable via the two
// thresholds).
class ECNStrategy : public CongestionStrategy {
 public:
  ECNStrategy(double min_threshold_fraction = 0.2,
              double max_threshold_fraction = 0.8)
      : min_threshold_fraction_(min_threshold_fraction),
        max_threshold_fraction_(max_threshold_fraction) {}

  bool IsCongested(const LinkState& state) const override;
  double CongestionScore(const LinkState& state) const override;
  const char* Name() const override { return "ecn"; }

 private:
  double min_threshold_fraction_;  // marking probability starts rising here
  double max_threshold_fraction_;  // marking probability saturates at 1.0 here
};

}  
