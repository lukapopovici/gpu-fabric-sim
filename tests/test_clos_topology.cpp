#include "topology/ClosTopology.h"
#include "test_main.h"

using fabric::topology::ClosConfig;
using fabric::topology::ClosTopology;

void RunTests() {
  ClosConfig config;
  config.pods = 2;
  config.leaves_per_pod = 2;
  config.spines_per_pod = 2;
  config.gpus_per_leaf = 4;
  config.core_count = 2;

  ClosTopology topo(config);

  const std::uint32_t expected_gpus =
      config.pods * config.leaves_per_pod * config.gpus_per_leaf;
  CHECK(topo.GpuNodes().size() == expected_gpus);

  // node count = gpus + leaves + spines (per pod) + core switches
  const std::uint32_t expected_leaves = config.pods * config.leaves_per_pod;
  const std::uint32_t expected_spines = config.pods * config.spines_per_pod;
  const std::uint32_t expected_cores = config.core_count;
  CHECK(topo.Nodes().size() == expected_gpus + expected_leaves +
                                    expected_spines + expected_cores);

  // Every GPU should have exactly one link (to its leaf).
  for (auto gpu : topo.GpuNodes()) {
    CHECK(topo.LinksAt(gpu).size() == 1);
  }

  // Every leaf should have gpus_per_leaf + spines_per_pod links.
  for (const auto& node : topo.Nodes()) {
    if (node.kind == fabric::topology::NodeKind::kLeafSwitch) {
      CHECK(topo.LinksAt(node.id).size() ==
            config.gpus_per_leaf + config.spines_per_pod);
    }
  }

  // Every spine should have leaves_per_pod (down to its pod's leaves)
  // plus core_count (up to every core switch) links.
  for (const auto& node : topo.Nodes()) {
    if (node.kind == fabric::topology::NodeKind::kSpineSwitch) {
      CHECK(topo.LinksAt(node.id).size() ==
            config.leaves_per_pod + config.core_count);
    }
  }

  // Every core switch should connect to every spine across every pod.
  for (const auto& node : topo.Nodes()) {
    if (node.kind == fabric::topology::NodeKind::kCoreSwitch) {
      CHECK(topo.LinksAt(node.id).size() ==
            config.pods * config.spines_per_pod);
    }
  }

  // Cross-pod GPU pair should now be routable with real ECMP diversity.
  // A cross-pod path is leaf -> spine (src pod) -> core -> spine (dst
  // pod) -> leaf, and the spine choice is independent on each end, so
  // total equal-cost paths = spines_per_pod^2 * core_count.
  const auto gpus = topo.GpuNodes();
  const auto cross_pod_paths = topo.EqualCostPaths(gpus.front(), gpus.back());
  CHECK(cross_pod_paths.size() ==
        config.spines_per_pod * config.spines_per_pod * config.core_count);
}

TEST_MAIN(RunTests)
