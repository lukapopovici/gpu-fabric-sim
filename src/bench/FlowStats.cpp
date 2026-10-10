#include "bench/FlowStats.h"

#include <algorithm>
#include <numeric>

namespace fabric::bench {

namespace {
sim::SimTime Percentile(const std::vector<sim::SimTime>& sorted_durations,
                         double p) {
  if (sorted_durations.empty()) return 0;
  const double idx_f = p * static_cast<double>(sorted_durations.size() - 1);
  const std::size_t idx = static_cast<std::size_t>(idx_f);
  return sorted_durations[std::min(idx, sorted_durations.size() - 1)];
}
}  // namespace

BenchSummary Summarize(const std::string& router_name,
                        const std::string& strategy_name,
                        std::vector<FlowCompletion> completions) {
  BenchSummary summary;
  summary.router_name = router_name;
  summary.strategy_name = strategy_name;
  summary.flow_count = completions.size();
  for (const auto& c : completions) {
    if (c.failed) ++summary.failed_count;
  }

  if (completions.empty()) return summary;

  std::vector<sim::SimTime> durations;
  durations.reserve(completions.size());
  for (const auto& c : completions) durations.push_back(c.CompletionDuration());
  std::sort(durations.begin(), durations.end());

  summary.p50_us = Percentile(durations, 0.50);
  summary.p99_us = Percentile(durations, 0.99);
  summary.max_us = durations.back();
  summary.mean_us =
      std::accumulate(durations.begin(), durations.end(), 0.0) /
      static_cast<double>(durations.size());

  return summary;
}

}  // namespace fabric::bench
