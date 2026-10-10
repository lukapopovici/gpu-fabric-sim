#pragma once

#include <cstdint>

#include "traffic/TrafficGenerator.h"

namespace fabric::traffic {

// Generates bursty many-to-one traffic: every GPU except the receiver sends
// each chunk to the same receiver. The per-flow launch jitter keeps the
// arrivals close enough to exercise IncastDetector's sliding window.
struct ManyToOneConfig {
  std::uint64_t message_bytes = 64ull * 1024 * 1024;
  std::uint32_t chunks = 8;
  sim::SimTime round_start_time = 0;
  sim::SimTime chunk_stagger_us = 100;
};

class ManyToOnePattern : public TrafficGenerator {
 public:
  explicit ManyToOnePattern(ManyToOneConfig config);

  std::vector<Flow> GenerateFlows(const topology::Topology& topo,
                                 std::uint64_t seed) const override;

 private:
  ManyToOneConfig config_;
};

}  
