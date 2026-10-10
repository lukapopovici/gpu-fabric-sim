#include "traffic/AllReducePattern.h"

#include <algorithm>
#include <random>

namespace fabric::traffic {

AllReducePattern::AllReducePattern(AllReduceConfig config)
    : config_(config) {}

std::vector<Flow> AllReducePattern::GenerateFlows(
    const topology::Topology& topo, std::uint64_t seed) const {
  const std::vector<topology::NodeId> ordered_gpus = topo.GpuNodes();
  std::vector<Flow> flows;
  if (ordered_gpus.size() < 2) return flows;

  // Deterministic PRNG seeded per-call so the *same* seed always produces
  // the same flow set for a given topology -- required for reproducible
  // static-vs-adaptive comparisons.
  std::mt19937_64 rng(seed);

  // Shuffle before forming ring pairs. GpuNodes() is returned in
  // build order (all GPUs under leaf 0 first, then leaf 1, ...), so a
  // ring over the *unshuffled* list mostly pairs GPUs that share a leaf
  // switch -- a single-hop, single-path link with zero ECMP diversity
  // for the router to exploit. Real all-reduce participants aren't
  // physically clustered like that, and without the shuffle the
  // adaptive router and static ECMP are mathematically forced to make
  // the same decision on most flows, since there's only one path to
  // choose from.
  std::vector<topology::NodeId> gpus = ordered_gpus;
  std::shuffle(gpus.begin(), gpus.end(), rng);

  const std::uint64_t bytes_per_chunk =
      config_.chunks > 0 ? config_.message_bytes / config_.chunks : 0;

  std::uint64_t flow_id = 0;
  // Small per-flow jitter within a chunk launch -- GPUs in the same
  // chunk don't fire at the exact same nanosecond in reality either, but
  // they're still close together. The real pacing comes from
  // chunk_stagger_us below, spacing successive chunks apart.
  std::uniform_int_distribution<sim::SimTime> jitter_us(0, 5);

  for (std::uint32_t chunk = 0; chunk < config_.chunks; ++chunk) {
    const sim::SimTime chunk_start =
        config_.round_start_time +
        static_cast<sim::SimTime>(chunk) * config_.chunk_stagger_us;

    for (std::size_t i = 0; i < gpus.size(); ++i) {
      const topology::NodeId src = gpus[i];
      const topology::NodeId dst = gpus[(i + 1) % gpus.size()];
      flows.push_back(Flow{
          flow_id++,
          src,
          dst,
          bytes_per_chunk,
          chunk_start + jitter_us(rng),
      });
    }
  }

  return flows;
}

}  // namespace fabric::traffic
