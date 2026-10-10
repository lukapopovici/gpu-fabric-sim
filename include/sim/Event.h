#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "sim/Clock.h"

namespace fabric::sim {

using EventId = std::uint64_t;

// A single discrete-event callback scheduled to fire at `time`.
// Payload-free by design: whatever needs to happen is captured in `action`.
// Keeping Event itself dumb (no virtual dispatch) keeps the hot path
// (push/pop from the EventQueue) cheap, which matters once we're pushing
// millions of packet-level events per benchmark run.
struct Event {
  SimTime time;
  EventId id;          // monotonically increasing, used to break time ties
                        // (FIFO ordering for same-timestamp events)
  std::string label;    // human-readable tag for debugging/tracing, e.g.
                        // "flow_start", "link_send", "congestion_check"
  std::function<void()> action;
};

// Min-heap ordering: earlier time first; if tied, lower id (i.e. the one
// scheduled first) goes first. Comparator returns true when `a` should be
// considered "after" `b" in priority (std::priority_queue is a max-heap by
// default, so this comparator inverts the natural order to get a min-heap).
struct EventOrder {
  bool operator()(const Event& a, const Event& b) const {
    if (a.time != b.time) return a.time > b.time;
    return a.id > b.id;
  }
};

}  // namespace fabric::sim
