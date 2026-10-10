#pragma once

#include <cstdint>

#include "traffic/TrafficGenerator.h"

namespace fabric::traffic {


struct AllReduceConfig {
  std::uint64_t message_bytes = 64ull * 1024 * 1024;  // total payload per GPU
  std::uint32_t chunks = 8;         // ring all-reduce pipeline depth
  sim::SimTime round_start_time = 0;
  sim::SimTime chunk_stagger_us = 100;
};

class AllReducePattern : public TrafficGenerator {
 public:
  explicit AllReducePattern(AllReduceConfig config);

  std::vector<Flow> GenerateFlows(const topology::Topology& topo,
                                    std::uint64_t seed) const override;

 private:
  AllReduceConfig config_;
};

} 
