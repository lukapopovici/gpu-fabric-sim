#pragma once

#include "sim/Clock.h"
#include "sim/EventQueue.h"

namespace fabric::sim {

// Drives the discrete-event loop: pop earliest event, advance sim time to
// its timestamp, execute it (which may itself schedule further events),
// repeat until the queue drains or an optional horizon is hit.
//
// This is deliberately the *only* place simulated time advances. Every
// other module (router, topology, telemetry) only ever reads Now() or
// schedules future events -- nothing else owns time.
class Simulator {
 public:
  EventQueue& Events() { return queue_; }
  SimTime Now() const { return now_; }

  // Runs until the event queue is empty.
  void Run();

  // Runs until either the queue is empty or sim time reaches `end_time`,
  // whichever comes first. Useful for bounding runaway benchmark runs.
  void RunUntil(SimTime end_time);

 private:
  EventQueue queue_;
  SimTime now_ = kSimTimeZero;
};

}  
