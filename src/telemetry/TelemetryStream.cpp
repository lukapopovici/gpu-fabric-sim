#include "telemetry/TelemetryStream.h"

#include <algorithm>

namespace fabric::telemetry {

TelemetryStream::TelemetryStream(std::string output_path, sim::SimTime window_us)
    : window_us_(window_us) {
  if (!output_path.empty()) {
    out_.open(output_path);
    writing_ = out_.is_open();
    if (writing_) {
      out_ << "time_us,link_id,queue_depth_bytes,queue_capacity_bytes,"
              "bytes_sent,packets_sent,packets_dropped\n";
    }
  }
}

void TelemetryStream::SetQueueCapacity(topology::LinkId link,
                                        std::uint64_t capacity_bytes) {
  stats_[link].link = link;
  stats_[link].queue_capacity_bytes = capacity_bytes;
}

void TelemetryStream::SetQueueDepth(topology::LinkId link,
                                     std::uint64_t depth_bytes,
                                     double drain_rate_bytes_per_us,
                                     sim::SimTime now) {
  auto& s = stats_[link];
  s.link = link;
  s.queue_depth_bytes = depth_bytes;
  queue_depth_states_[link] =
      QueueDepthState{now, std::max(drain_rate_bytes_per_us, 0.0)};
}

std::uint64_t TelemetryStream::CurrentQueueDepth(topology::LinkId link,
                                                 sim::SimTime now) const {
  const auto stats_it = stats_.find(link);
  if (stats_it == stats_.end()) return 0;

  const auto state_it = queue_depth_states_.find(link);
  if (state_it == queue_depth_states_.end() ||
      now <= state_it->second.last_update_time) {
    return stats_it->second.queue_depth_bytes;
  }

  const sim::SimTime elapsed = now - state_it->second.last_update_time;
  const double drained_bytes =
      state_it->second.drain_rate_bytes_per_us * static_cast<double>(elapsed);
  if (drained_bytes >=
      static_cast<double>(stats_it->second.queue_depth_bytes)) {
    return 0;
  }
  return static_cast<std::uint64_t>(
      static_cast<double>(stats_it->second.queue_depth_bytes) - drained_bytes);
}

void TelemetryStream::PruneWindow(topology::LinkId link,
                                   sim::SimTime now) const {
  auto it = windows_.find(link);
  if (it == windows_.end()) return;
  auto& dq = it->second;
  const sim::SimTime cutoff = now - window_us_;
  while (!dq.empty() && dq.front().time < cutoff) {
    dq.pop_front();
  }
}

void TelemetryStream::RecordSend(topology::LinkId link, std::uint64_t bytes,
                                  sim::SimTime now) {
  auto& s = stats_[link];
  s.link = link;
  s.bytes_sent += bytes;
  s.packets_sent += 1;

  PruneWindow(link, now);
  windows_[link].push_back(WindowEntry{now, /*dropped=*/false});
}

void TelemetryStream::RecordDrop(topology::LinkId link, sim::SimTime now) {
  auto& s = stats_[link];
  s.link = link;
  s.packets_dropped += 1;

  PruneWindow(link, now);
  windows_[link].push_back(WindowEntry{now, /*dropped=*/true});
}

router::LinkState TelemetryStream::Snapshot(topology::LinkId link,
                                              sim::SimTime now) const {
  router::LinkState state{};
  state.link = link;
  state.now = now;

  auto it = stats_.find(link);
  if (it == stats_.end()) return state;

  const LinkStats& s = it->second;
  state.queue_depth_bytes = CurrentQueueDepth(link, now);
  state.queue_capacity_bytes = s.queue_capacity_bytes;

  // Windowed drop rate -- a real sliding window over the last window_us_
  // microseconds, rather than a run-cumulative ratio. This matters: a
  // cumulative drop rate can never "recover" once a link has a bad patch
  // early in a run, even if it's been clean for the last 10ms. A rolling
  // window lets DropRateStrategy react to *current* behavior instead of
  // a smeared-out historical average.
  PruneWindow(link, now);
  auto win_it = windows_.find(link);
  std::uint64_t packets_in_window = 0;
  std::uint64_t drops_in_window = 0;
  if (win_it != windows_.end()) {
    for (const auto& entry : win_it->second) {
      ++packets_in_window;
      if (entry.dropped) ++drops_in_window;
    }
  }
  state.packets_in_window = packets_in_window;
  state.drops_in_window = drops_in_window;
  state.window_start = now - window_us_;

  return state;
}

void TelemetryStream::FlushTick(sim::SimTime now) {
  if (!writing_) return;
  for (const auto& [link_id, s] : stats_) {
    out_ << now << ',' << link_id << ',' << CurrentQueueDepth(link_id, now)
         << ','
         << s.queue_capacity_bytes << ',' << s.bytes_sent << ','
         << s.packets_sent << ',' << s.packets_dropped << '\n';
  }
}

}  // namespace fabric::telemetry
