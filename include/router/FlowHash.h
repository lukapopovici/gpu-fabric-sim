#pragma once

#include <cstdint>
#include <functional>

#include "traffic/TrafficGenerator.h"

namespace fabric::router {

// Deterministic hash of a flow's identity (src/dst/flow_id), run through
// a 64-bit avalanche mixer for good bit distribution. Shared by
// StaticECMPRouter (picks path = hash % num_paths) and AdaptiveRouter
// (picks path via a weighted fractional split of the hash range) so both
// routers start from the same quality of pseudo-random spread and any
// difference in their behavior comes from the congestion awareness
// itself, not from one having a better/worse hash function.
//
inline std::uint64_t HashFlow(const traffic::Flow& flow) {
  std::uint64_t x = std::hash<std::uint32_t>{}(flow.src) ^
                     (std::hash<std::uint32_t>{}(flow.dst) << 1) ^
                     (std::hash<std::uint64_t>{}(flow.flow_id) << 2);
  // MurmurHash3/SplitMix64-style finalizer.
  x ^= x >> 33;
  x *= 0xff51afd7ed558ccdULL;
  x ^= x >> 33;
  x *= 0xc4ceb9fe1a85ec53ULL;
  x ^= x >> 33;
  return x;
}

}
