#include "sim/Simulator.h"

namespace fabric::sim {

void Simulator::Run() {
  while (auto event = queue_.PopNext()) {
    now_ = event->time;
    if (event->action) event->action();
  }
}

void Simulator::RunUntil(SimTime end_time) {
  while (!queue_.Empty() && queue_.PeekTime() <= end_time) {
    auto event = queue_.PopNext();
    now_ = event->time;
    if (event->action) event->action();
  }
}

}  // namespace fabric::sim
