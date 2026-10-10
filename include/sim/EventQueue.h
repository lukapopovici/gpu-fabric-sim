#pragma once

#include <optional>
#include <queue>
#include <vector>

#include "sim/Event.h"

namespace fabric::sim {

// Owns the min-heap of pending events. This is the beating heart of the
// discrete-event core: the Simulator just repeatedly pops the earliest
// event and executes it, advancing simulated time in jumps rather than
// ticking a wall clock.
class EventQueue {
 public:
  // Schedules `action` to run at `time`. Returns the assigned EventId
  // (currently informational only; cancellation is a future extension).
  EventId Schedule(SimTime time, std::string label,
                    std::function<void()> action);

  // Schedules `action` to run `delay` microseconds after `now`.
  EventId ScheduleAfter(SimTime now, SimTime delay, std::string label,
                        std::function<void()> action);

  bool Empty() const { return heap_.empty(); }
  std::size_t Size() const { return heap_.size(); }

  // Removes and returns the earliest-scheduled event, or std::nullopt if
  // the queue is empty.
  std::optional<Event> PopNext();

  // Time of the next event without removing it. Callers should check
  // Empty() first.
  SimTime PeekTime() const;

 private:
  std::priority_queue<Event, std::vector<Event>, EventOrder> heap_;
  EventId next_id_ = 0;
};

}  // namespace fabric::sim
