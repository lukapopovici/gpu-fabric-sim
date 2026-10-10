#include "traffic/ManyToOnePattern.h"

#include <random>

namespace fabric::traffic {

ManyToOnePattern::ManyToOnePattern(ManyToOneConfig config)
    : config_(config) {}

std::vector<Flow> ManyToOnePattern::GenerateFlows(
    const topology::Topology& topo, std::uint64_t seed) const {
  const std::vector<topology::NodeId> gpus = topo.GpuNodes();
  std::vector<Flow> flows;
  if (gpus.size() < 2) return flows;

  std::mt19937_64 rng(seed);
  std::uniform_int_distribution<sim::SimTime> jitter_us(0, 5);
  const std::uint64_t bytes_per_chunk =
      config_.chunks > 0 ? config_.message_bytes / config_.chunks : 0;
  const topology::NodeId dst = gpus.front();

  std::uint64_t flow_id = 0;
  for (std::uint32_t chunk = 0; chunk < config_.chunks; ++chunk) {
    const sim::SimTime chunk_start =
        config_.round_start_time +
        static_cast<sim::SimTime>(chunk) * config_.chunk_stagger_us;
    for (topology::NodeId src : gpus) {
      if (src == dst) continue;
      flows.push_back(Flow{flow_id++, src, dst, bytes_per_chunk,
                           chunk_start + jitter_us(rng)});
    }
  }
  return flows;
}

}  // namespace fabric::traffic
