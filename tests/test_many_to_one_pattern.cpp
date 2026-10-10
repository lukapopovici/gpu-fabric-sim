#include <algorithm>
#include <unordered_set>

#include "incast/IncastDetector.h"
#include "router/AdaptiveRouter.h"
#include "router/QueueDepthStrategy.h"
#include "topology/ClosTopology.h"
#include "traffic/ManyToOnePattern.h"
#include "test_main.h"

using namespace fabric;

void RunTests() {
  topology::ClosConfig topology_config;
  topology_config.pods = 1;
  topology_config.leaves_per_pod = 2;
  topology_config.spines_per_pod = 2;
  topology_config.gpus_per_leaf = 8;
  topology_config.core_count = 0;
  topology::ClosTopology topo(topology_config);

  traffic::ManyToOneConfig traffic_config;
  traffic_config.message_bytes = 1200;
  traffic_config.chunks = 2;
  traffic_config.round_start_time = 10;
  traffic_config.chunk_stagger_us = 100;
  traffic::ManyToOnePattern pattern(traffic_config);

  const auto flows = pattern.GenerateFlows(topo, 42);
  const auto repeated_flows = pattern.GenerateFlows(topo, 42);
  CHECK(flows.size() == 30);
  CHECK(repeated_flows.size() == flows.size());

  const topology::NodeId dst = topo.GpuNodes().front();
  std::unordered_set<topology::NodeId> first_chunk_senders;
  for (std::size_t i = 0; i < flows.size(); ++i) {
    CHECK(flows[i].flow_id == repeated_flows[i].flow_id);
    CHECK(flows[i].src == repeated_flows[i].src);
    CHECK(flows[i].dst == repeated_flows[i].dst);
    CHECK(flows[i].start_time == repeated_flows[i].start_time);
    CHECK(flows[i].bytes == 600);
    CHECK(flows[i].dst == dst);
    CHECK(flows[i].src != dst);
    CHECK(flows[i].start_time >= 10 +
              static_cast<sim::SimTime>(i / 15) * 100);
    CHECK(flows[i].start_time <= 15 +
              static_cast<sim::SimTime>(i / 15) * 100);
    if (i < 15) first_chunk_senders.insert(flows[i].src);
  }
  CHECK(first_chunk_senders.size() == 15);

  incast::IncastDetector::Config detector_config;
  detector_config.window_us = 50;
  detector_config.sender_threshold = 3;
  incast::IncastDetector detector(detector_config);
  auto router_detector =
      std::make_unique<incast::IncastDetector>(detector_config);
  router::AdaptiveRouter adaptive_router(
      std::make_unique<router::QueueDepthStrategy>(),
      std::move(router_detector));
  std::vector<traffic::Flow> ordered_flows = flows;
  std::sort(ordered_flows.begin(), ordered_flows.end(),
            [](const traffic::Flow& a, const traffic::Flow& b) {
              return a.start_time < b.start_time;
            });
  bool incast_detected = false;
  std::unordered_set<topology::LinkId> incast_spine_links;
  const auto gpus = topo.GpuNodes();
  const std::unordered_set<topology::NodeId> remote_leaf_gpus(
      gpus.begin() + 8, gpus.end());
  for (const auto& flow : ordered_flows) {
    detector.RecordArrival(flow.dst, flow.src, flow.start_time);
    incast_detected |= detector.IsIncastTarget(flow.dst, flow.start_time);

    adaptive_router.RecordArrival(flow.dst, flow.src, flow.start_time);
    const auto decision = adaptive_router.RouteFlow(topo, flow);
    if (flow.start_time < 60 && adaptive_router.IsIncastTarget(
                                    flow.dst, flow.start_time) &&
        remote_leaf_gpus.contains(flow.src) && decision.path.size() >= 4) {
      incast_spine_links.insert(decision.path[1]);
    }
  }
  CHECK(incast_detected);
  CHECK(incast_spine_links.size() == 2);
}

TEST_MAIN(RunTests)
