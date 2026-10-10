#pragma once

#include <cstdint>

namespace fabric::sim {

// Simulated time, in microseconds, since the start of the run.
// Kept as a strong-ish typedef (not a raw double) so nobody accidentally
// mixes wall-clock time with sim time.
using SimTime = std::int64_t;

constexpr SimTime kSimTimeZero = 0;

}  // namespace fabric::sim
