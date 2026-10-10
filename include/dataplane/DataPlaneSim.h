#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>

#include "sim/Clock.h"
#include "telemetry/TelemetryStream.h"
#include "topology/Topology.h"

namespace fabric::dataplane {

class DataPlaneSim {
 public:
  struct Result {
    sim::SimTime finish_time;
    bool dropped;
  };

  // Sends `bytes` over `link` (with the given bandwidth and queue
  // capacity) starting no earlier than `arrival_time`. Updates
  // `telemetry` with the send (or drop) and returns either the time the
  // last bit clears the link, or dropped=true if the queue would have
  // overflowed.
  Result SendOnLink(topology::LinkId link, double bandwidth_gbps,
                      std::uint64_t queue_capacity_bytes, std::uint64_t bytes,
                      sim::SimTime arrival_time,
                      telemetry::TelemetryStream& telemetry);

 private:
  // Time the link becomes free again, per link. Absence = link has never
  // been used, i.e. free since time zero.
  std::unordered_map<topology::LinkId, sim::SimTime> busy_until_;
};

}  // namespace fabric::dataplane
