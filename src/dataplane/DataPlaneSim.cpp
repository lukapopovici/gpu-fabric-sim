#include "dataplane/DataPlaneSim.h"

#include <algorithm>

namespace fabric::dataplane {

DataPlaneSim::Result DataPlaneSim::SendOnLink(
    topology::LinkId link, double bandwidth_gbps,
    std::uint64_t queue_capacity_bytes, std::uint64_t bytes,
    sim::SimTime arrival_time, telemetry::TelemetryStream& telemetry) {
  telemetry.SetQueueCapacity(link, queue_capacity_bytes);

  auto it = busy_until_.find(link);
  const sim::SimTime link_free_at = (it == busy_until_.end()) ? arrival_time : it->second;

  const sim::SimTime start_time = std::max(arrival_time, link_free_at);
  const sim::SimTime queueing_delay_us = start_time - arrival_time;

  // Bytes effectively sitting in queue ahead of this send, approximated
  // as bandwidth * queueing_delay -- i.e. "how much data would have to
  // drain at this link's rate to explain the wait". This is the
  // FCFS-single-server approximation; a real switch has a discrete
  // buffer, but this is enough to produce a queue-depth signal that
  // trends with contention.
  const double bytes_per_us = (bandwidth_gbps * 1e9 / 8.0) / 1e6;
  const std::uint64_t implied_queue_bytes =
      static_cast<std::uint64_t>(bytes_per_us * static_cast<double>(queueing_delay_us));

  telemetry.SetQueueDepth(link, implied_queue_bytes, bytes_per_us,
                          arrival_time);

  if (queue_capacity_bytes > 0 && implied_queue_bytes + bytes > queue_capacity_bytes) {
    // Queue would overflow -- drop this send's bytes entirely rather than
    // enqueue them. Link's busy_until is left untouched since nothing
    // was actually transmitted.
    telemetry.RecordDrop(link, arrival_time);
    return Result{arrival_time, /*dropped=*/true};
  }

  const double service_time_us =
      static_cast<double>(bytes) / std::max(bytes_per_us, 1e-9);
  const sim::SimTime finish_time =
      start_time + static_cast<sim::SimTime>(service_time_us);

  busy_until_[link] = finish_time;
  telemetry.RecordSend(link, bytes, arrival_time);

  return Result{finish_time, /*dropped=*/false};
}

}  // namespace fabric::dataplane
