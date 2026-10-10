#pragma once

#include <cstdint>

#include "topology/Topology.h"

namespace fabric::topology {

struct ClosConfig {
  std::uint32_t pods = 4;
  std::uint32_t leaves_per_pod = 2;
  std::uint32_t spines_per_pod = 2;
  std::uint32_t gpus_per_leaf = 4;
  std::uint32_t core_count = 2;

  double gpu_leaf_bandwidth_gbps = 400.0;
  double leaf_spine_bandwidth_gbps = 400.0;
  double spine_core_bandwidth_gbps = 400.0;
  double link_latency_us = 0.5;

  // Per-link queue (buffer) capacity, in bytes. Split by tier since real
  // switches typically give different buffer budgets to server-facing
  // ports vs. fabric-facing uplinks. BenchHarness reads these off
  // Topology::Link instead of using one hardcoded constant for every
  // link in the fabric.
  std::uint64_t gpu_leaf_queue_capacity_bytes = 8ull * 1024 * 1024;
  std::uint64_t leaf_spine_queue_capacity_bytes = 8ull * 1024 * 1024;
  std::uint64_t spine_core_queue_capacity_bytes = 8ull * 1024 * 1024;
};

// Builds and owns a folded-Clos (fat-tree) topology from a ClosConfig.
// Total GPU count = pods * leaves_per_pod * gpus_per_leaf.
class ClosTopology : public Topology {
 public:
  explicit ClosTopology(ClosConfig config);

  const std::vector<Node>& Nodes() const override { return nodes_; }
  const std::vector<Link>& Links() const override { return links_; }
  std::vector<LinkId> LinksAt(NodeId node) const override;
  std::vector<NodeId> GpuNodes() const override { return gpu_nodes_; }
  std::vector<std::vector<LinkId>> EqualCostPaths(NodeId src,
                                                    NodeId dst) const override;

  const ClosConfig& Config() const { return config_; }

 private:
  void Build();

  ClosConfig config_;
  std::vector<Node> nodes_;
  std::vector<Link> links_;
  std::vector<NodeId> gpu_nodes_;

  // adjacency_[node] -> incident LinkIds. Built once in Build() so
  // LinksAt() is O(1) instead of scanning all links every call.
  std::vector<std::vector<LinkId>> adjacency_;
};

} 
