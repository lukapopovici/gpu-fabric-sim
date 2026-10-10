#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "sim/Clock.h"

namespace fabric::bench {

struct FlowCompletion {
  std::uint64_t flow_id;
  sim::SimTime start_time;
  sim::SimTime completion_time;
  bool failed = false;  // true if the flow exhausted its retry budget

  sim::SimTime CompletionDuration() const {
    return completion_time - start_time;
  }
};

// Summary stats over a set of flow completions: what the bench harness
// reports and what the README's "beat static routing on measured job
// completion time" claim is actually measured by.
struct BenchSummary {
  std::string router_name;
  std::string strategy_name;  // empty for static ECMP
  sim::SimTime p50_us = 0;
  sim::SimTime p99_us = 0;
  sim::SimTime max_us = 0;
  double mean_us = 0.0;
  std::size_t flow_count = 0;
  std::size_t failed_count = 0;  // flows that exhausted retries
};

// Computes p50/p99/max/mean over `completions`. Sorts a local copy, so
// safe to call with the harness's raw completion vector directly.
BenchSummary Summarize(const std::string& router_name,
                        const std::string& strategy_name,
                        std::vector<FlowCompletion> completions);

}  // namespace fabric::bench
