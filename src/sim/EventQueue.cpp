#include "sim/EventQueue.h"

#include <cassert>

namespace fabric::sim {

EventId EventQueue::Schedule(SimTime time, std::string label,
                              std::function<void()> action) {
  const EventId id = next_id_++;
  heap_.push(Event{time, id, std::move(label), std::move(action)});
  return id;
}

EventId EventQueue::ScheduleAfter(SimTime now, SimTime delay,
                                   std::string label,
                                   std::function<void()> action) {
  assert(delay >= 0 && "negative delay would schedule an event in the past");
  return Schedule(now + delay, std::move(label), std::move(action));
}

std::optional<Event> EventQueue::PopNext() {
  if (heap_.empty()) return std::nullopt;
  Event next = std::move(const_cast<Event&>(heap_.top()));
  heap_.pop();
  return next;
}

SimTime EventQueue::PeekTime() const {
  assert(!heap_.empty() && "PeekTime() called on empty EventQueue");
  return heap_.top().time;
}

}  // namespace fabric::sim
