#include "topology/ClosTopology.h"

#include <algorithm>
#include <functional>
#include <stdexcept>

namespace fabric::topology {

ClosTopology::ClosTopology(ClosConfig config) : config_(config) { Build(); }

void ClosTopology::Build() {
  NodeId next_id = 0;

  const auto make_node = [&](NodeKind kind, const std::string& label) {
    Node n{next_id++, kind, label};
    nodes_.push_back(n);
    return n.id;
  };

  const auto add_link = [&](NodeId a, NodeId b, double bw,
                              std::uint64_t queue_capacity_bytes) {
    LinkId id = static_cast<LinkId>(links_.size());
    links_.push_back(
        Link{id, a, b, bw, config_.link_latency_us, queue_capacity_bytes});
    return id;
  };

  std::vector<std::vector<NodeId>> leaves(config_.pods);
  std::vector<std::vector<NodeId>> spines(config_.pods);

  for (std::uint32_t pod = 0; pod < config_.pods; ++pod) {
    for (std::uint32_t s = 0; s < config_.spines_per_pod; ++s) {
      spines[pod].push_back(
          make_node(NodeKind::kSpineSwitch,
                    "spine-p" + std::to_string(pod) + "-" + std::to_string(s)));
    }
    for (std::uint32_t l = 0; l < config_.leaves_per_pod; ++l) {
      NodeId leaf = make_node(
          NodeKind::kLeafSwitch,
          "leaf-p" + std::to_string(pod) + "-" + std::to_string(l));
      leaves[pod].push_back(leaf);

      for (std::uint32_t g = 0; g < config_.gpus_per_leaf; ++g) {
        NodeId gpu = make_node(
            NodeKind::kGpu,
            "gpu-p" + std::to_string(pod) + "-l" + std::to_string(l) + "-" +
                std::to_string(g));
        gpu_nodes_.push_back(gpu);
        add_link(gpu, leaf, config_.gpu_leaf_bandwidth_gbps,
                  config_.gpu_leaf_queue_capacity_bytes);
      }
      // Full mesh leaf<->spine within the pod (classic folded Clos).
      for (NodeId spine : spines[pod]) {
        add_link(leaf, spine, config_.leaf_spine_bandwidth_gbps,
                  config_.leaf_spine_queue_capacity_bytes);
      }
    }
  }

  // Core tier: every spine in every pod connects to every core switch
  // (full bipartite spine<->core mesh). This is what makes cross-pod
  // traffic routable at all, and gives it spines_per_pod * core_count
  // equal-cost paths -- the same ECMP fan-out benefit intra-pod traffic
  // gets from multiple spines, just one tier up.
  if (config_.core_count > 0) {
    std::vector<NodeId> cores;
    for (std::uint32_t c = 0; c < config_.core_count; ++c) {
      cores.push_back(make_node(NodeKind::kCoreSwitch, "core-" + std::to_string(c)));
    }
    for (std::uint32_t pod = 0; pod < config_.pods; ++pod) {
      for (NodeId spine : spines[pod]) {
        for (NodeId core : cores) {
          add_link(spine, core, config_.spine_core_bandwidth_gbps,
                    config_.spine_core_queue_capacity_bytes);
        }
      }
    }
  }

  adjacency_.assign(nodes_.size(), {});
  for (const Link& link : links_) {
    adjacency_[link.a].push_back(link.id);
    adjacency_[link.b].push_back(link.id);
  }
}

std::vector<LinkId> ClosTopology::LinksAt(NodeId node) const {
  if (node >= adjacency_.size()) {
    throw std::out_of_range("ClosTopology::LinksAt: invalid NodeId");
  }
  return adjacency_[node];
}

std::vector<std::vector<LinkId>> ClosTopology::EqualCostPaths(
    NodeId src, NodeId dst) const {
  // BFS from src to compute shortest hop-distance to every node, then DFS
  // back from dst along strictly-decreasing-distance neighbors to
  // enumerate every min-hop path. This naturally gives ECMP fan-out in a
  // Clos fabric: e.g. GPU->leaf->spine->leaf->GPU has multiple equal-cost
  // spine choices, all found by this walk.
  if (src == dst || src >= nodes_.size() || dst >= nodes_.size()) return {};

  constexpr int kUnvisited = -1;
  std::vector<int> dist(nodes_.size(), kUnvisited);
  dist[src] = 0;
  std::vector<NodeId> queue{src};
  std::size_t head = 0;
  while (head < queue.size()) {
    NodeId u = queue[head++];
    for (LinkId lid : adjacency_[u]) {
      const Link& link = links_[lid];
      NodeId v = (link.a == u) ? link.b : link.a;
      if (dist[v] == kUnvisited) {
        dist[v] = dist[u] + 1;
        queue.push_back(v);
      }
    }
  }

  if (dist[dst] == kUnvisited) return {};  // unreachable (e.g. pods=N, core_count=0)

  std::vector<std::vector<LinkId>> paths;
  std::vector<LinkId> current;

  // DFS from dst back toward src, only stepping to neighbors whose BFS
  // distance is exactly one less than the current node's -- guarantees
  // every discovered root-to-dst walk is a shortest path, and exploring
  // all such neighbors enumerates every equal-cost shortest path.
  std::function<void(NodeId)> dfs = [&](NodeId u) {
    if (u == src) {
      std::vector<LinkId> path(current.rbegin(), current.rend());
      paths.push_back(std::move(path));
      return;
    }
    for (LinkId lid : adjacency_[u]) {
      const Link& link = links_[lid];
      NodeId v = (link.a == u) ? link.b : link.a;
      if (dist[v] == dist[u] - 1) {
        current.push_back(lid);
        dfs(v);
        current.pop_back();
      }
    }
  };
  dfs(dst);

  return paths;
}

}  // namespace fabric::topology
