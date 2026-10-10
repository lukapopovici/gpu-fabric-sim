#pragma once

#include <memory>

#include "bench/FlowStats.h"
#include "router/Router.h"
#include "sim/Simulator.h"
#include "telemetry/TelemetryStream.h"
#include "topology/Topology.h"
#include "traffic/TrafficGenerator.h"

namespace fabric::bench {

// Runs one router implementation against one traffic pattern on a fixed
// topology + seed, driving the discrete-event Simulator, and returns
// p50/p99/max/mean flow completion time. This is what wires together
// "run static ECMP vs run AdaptiveRouter with strategy X" into a single
// comparable BenchSummary each -- the actual comparison in main.cpp is
// just calling this twice with different Router instances.
class BenchHarness {
 public:
  BenchHarness(const topology::Topology& topo,
               const traffic::TrafficGenerator& traffic_gen,
               std::uint64_t seed);


  BenchSummary Run(router::Router& router, const std::string& strategy_label,
                    telemetry::TelemetryStream* telemetry = nullptr,
                    sim::SimTime telemetry_tick_interval_us = 50,
                    sim::SimTime telemetry_horizon_margin_us = 3000);

 private:
  const topology::Topology& topo_;
  const traffic::TrafficGenerator& traffic_gen_;
  std::uint64_t seed_;
};

} 
