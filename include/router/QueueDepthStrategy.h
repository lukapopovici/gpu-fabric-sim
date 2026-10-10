#pragma once

#include "router/CongestionStrategy.h"

namespace fabric::router {

// Simplest signal: trigger when queue occupancy crosses a fixed fraction
// of capacity. Closest analogue to a switch without ECN support -- reacts
// only once a queue is actually building up, so it's a lagging-but-simple
// baseline to compare the other two strategies against.
class QueueDepthStrategy : public CongestionStrategy {
 public:
  explicit QueueDepthStrategy(double threshold_fraction = 0.7)
      : threshold_fraction_(threshold_fraction) {}

  bool IsCongested(const LinkState& state) const override;
  double CongestionScore(const LinkState& state) const override;
  const char* Name() const override { return "queue_depth"; }

 private:
  double threshold_fraction_;  // e.g. 0.7 => congested at 70% queue full
};

}  