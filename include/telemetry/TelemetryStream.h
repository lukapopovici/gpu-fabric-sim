#pragma once

#include <deque>
#include <fstream>
#include <string>
#include <unordered_map>

#include "router/CongestionStrategy.h"
#include "sim/Clock.h"
#include "telemetry/LinkStats.h"
#include "topology/Topology.h"

namespace fabric::telemetry {

// Collects per-link stats over the course of a simulation run and (a)
// exposes point-in-time LinkState snapshots for the router's congestion
// strategies, (b) serializes a time series to disk that
// tools/viz/heatmap.py reads to render the live congestion heatmap.
//
// This is the single place link-level ground truth lives -- the data
// plane sim updates it, the router reads from it (indirectly, via
// AdaptiveRouter::UpdateLinkState), and the Python viz layer reads its
// exported CSV/JSON after the run.
class TelemetryStream {
 public:
  // `output_path`: if empty, stats are tracked in-memory only (no file
  // written) -- useful for bench-only runs that don't need the heatmap.
  // `window_us`: width of the rolling window used for drops_in_window /
  // packets_in_window in Snapshot() -- this is what DropRateStrategy
  // actually reacts to. Defaults to 100us; pass the same order of
  // magnitude as your congestion-detection cadence.
  explicit TelemetryStream(std::string output_path = "",
                            sim::SimTime window_us = 100);

  void RecordSend(topology::LinkId link, std::uint64_t bytes,
                    sim::SimTime now);
  void RecordDrop(topology::LinkId link, sim::SimTime now);

  // Snapshot suitable for feeding a CongestionStrategy. drops_in_window /
  // packets_in_window are computed over a real sliding window ending at
  // `now` (see window_us above), not a run-cumulative ratio.
  router::LinkState Snapshot(topology::LinkId link, sim::SimTime now) const;

  void SetQueueCapacity(topology::LinkId link, std::uint64_t capacity_bytes);

  // Sets the queue-depth estimate and its line-rate drain rate at `now`.
  // DataPlaneSim computes the estimate from FCFS queueing delay; snapshots
  // and flushes decay it from this timestamp while the link is idle.
  void SetQueueDepth(topology::LinkId link, std::uint64_t depth_bytes,
                     double drain_rate_bytes_per_us, sim::SimTime now);

  // Appends the current cumulative state of all tracked links as one row
  // to the output stream, if configured. Called periodically by the sim
  // to build the time series the heatmap animates.
  void FlushTick(sim::SimTime now);

 private:
  struct WindowEntry {
    sim::SimTime time;
    bool dropped;
  };

  struct QueueDepthState {
    sim::SimTime last_update_time;
    double drain_rate_bytes_per_us;
  };

  std::uint64_t CurrentQueueDepth(topology::LinkId link,
                                  sim::SimTime now) const;

  // Drops window entries older than window_us_ relative to `now`. Mutable
  // window state per link; called from RecordSend/RecordDrop and from the
  // const Snapshot() query (hence mutable -- pruning on read is an
  // implementation detail, not an observable state change).
  void PruneWindow(topology::LinkId link, sim::SimTime now) const;

  std::unordered_map<topology::LinkId, LinkStats> stats_;
  std::unordered_map<topology::LinkId, QueueDepthState> queue_depth_states_;
  mutable std::unordered_map<topology::LinkId, std::deque<WindowEntry>>
      windows_;
  sim::SimTime window_us_;
  std::ofstream out_;
  bool writing_ = false;
};

}  
