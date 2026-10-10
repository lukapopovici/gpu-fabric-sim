#pragma once

#include <cstdint>

#include "topology/Topology.h"

namespace fabric::telemetry {

// Running counters for one link, updated by the data-plane sim as
// packets enter/leave its queue. TelemetryStream aggregates these into
// the LinkState snapshots that CongestionStrategy implementations
// consume, and into the per-link series the Python heatmap plots.
struct LinkStats {
  topology::LinkId link;
  std::uint64_t queue_depth_bytes = 0;
  std::uint64_t queue_capacity_bytes = 0;
  std::uint64_t bytes_sent = 0;
  std::uint64_t packets_sent = 0;
  std::uint64_t packets_dropped = 0;

  double UtilizationFraction() const {
    if (queue_capacity_bytes == 0) return 0.0;
    return static_cast<double>(queue_depth_bytes) /
           static_cast<double>(queue_capacity_bytes);
  }
};

}  // namespace fabric::telemetry
