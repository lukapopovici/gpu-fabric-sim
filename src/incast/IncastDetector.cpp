#include "incast/IncastDetector.h"

#include <unordered_set>

namespace fabric::incast {

void IncastDetector::Prune(topology::NodeId dst, sim::SimTime now) const {
  auto it = windows_.find(dst);
  if (it == windows_.end()) return;
  auto& deque = it->second;
  const sim::SimTime cutoff = now - config_.window_us;
  while (!deque.empty() && deque.front().time < cutoff) {
    deque.pop_front();
  }
}

void IncastDetector::RecordArrival(topology::NodeId dst, topology::NodeId src,
                                    sim::SimTime now) {
  Prune(dst, now);
  windows_[dst].push_back(Arrival{now, src});
}

std::size_t IncastDetector::ActiveSenderCount(topology::NodeId dst,
                                                sim::SimTime now) const {
  Prune(dst, now);
  auto it = windows_.find(dst);
  if (it == windows_.end()) return 0;

  // Distinct sender count within the window -- a burst from the same
  // sender doesn't count as incast, it has to be many-to-one.
  std::unordered_set<topology::NodeId> distinct_senders;
  for (const auto& arrival : it->second) {
    distinct_senders.insert(arrival.src);
  }
  return distinct_senders.size();
}

bool IncastDetector::IsIncastTarget(topology::NodeId dst,
                                     sim::SimTime now) const {
  return ActiveSenderCount(dst, now) >= config_.sender_threshold;
}

}  // namespace fabric::incast
