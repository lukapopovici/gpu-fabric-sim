#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace fabric::topology {

using NodeId = std::uint32_t;
using LinkId = std::uint32_t;

enum class NodeKind { kGpu, kLeafSwitch, kSpineSwitch, kCoreSwitch };

struct Node {
  NodeId id;
  NodeKind kind;
  std::string label;  // e.g. "gpu-3", "leaf-1", "spine-0"
};

struct Link {
  LinkId id;
  NodeId a;
  NodeId b;
  double bandwidth_gbps;
  double latency_us;
  std::uint64_t queue_capacity_bytes = 0;  // 0 = caller should use a default
};

// Abstract fabric topology: a set of nodes and links plus adjacency lookup.
// ClosTopology is the first (and JD-required) implementation; the
// interface is kept generic so a ring/torus topology could be dropped in
// later without touching the router or traffic generator.
class Topology {
 public:
  virtual ~Topology() = default;

  virtual const std::vector<Node>& Nodes() const = 0;
  virtual const std::vector<Link>& Links() const = 0;

  // All links incident to `node`.
  virtual std::vector<LinkId> LinksAt(NodeId node) const = 0;

  // All GPU (i.e. traffic-endpoint) nodes -- what the traffic generator
  // and bench harness iterate over.
  virtual std::vector<NodeId> GpuNodes() const = 0;

  // All equal-cost shortest paths between two GPU nodes, expressed as
  // ordered lists of LinkIds. Used by both the static ECMP router (pick
  // uniformly/hash-based) and the adaptive router (pick congestion-aware).
  virtual std::vector<std::vector<LinkId>> EqualCostPaths(
      NodeId src, NodeId dst) const = 0;
};

}  
