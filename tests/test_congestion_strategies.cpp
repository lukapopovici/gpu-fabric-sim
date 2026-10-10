#include <string>
#include <vector>

#include "router/DropRateStrategy.h"
#include "router/ECNStrategy.h"
#include "router/QueueDepthStrategy.h"
#include "test_main.h"

using namespace fabric::router;

namespace {
LinkState MakeState(std::uint64_t depth, std::uint64_t capacity,
                     std::uint64_t drops = 0, std::uint64_t packets = 0) {
  LinkState s{};
  s.link = 0;
  s.queue_depth_bytes = depth;
  s.queue_capacity_bytes = capacity;
  s.drops_in_window = drops;
  s.packets_in_window = packets;
  s.window_start = 0;
  s.now = 100;
  return s;
}
}  // namespace

void RunTests() {
  // QueueDepthStrategy: hard threshold at 70% by default.
  QueueDepthStrategy qd(0.7);
  CHECK(!qd.IsCongested(MakeState(500, 1000)));   // 50% < 70%
  CHECK(qd.IsCongested(MakeState(800, 1000)));    // 80% >= 70%
  CHECK(qd.CongestionScore(MakeState(1000, 1000)) == 1.0);

  // ECNStrategy: soft ramp between min/max thresholds.
  ECNStrategy ecn(0.2, 0.8);
  CHECK(ecn.CongestionScore(MakeState(100, 1000)) == 0.0);   // 10% < 20% floor
  CHECK(ecn.CongestionScore(MakeState(900, 1000)) == 1.0);   // 90% > 80% ceiling
  const double mid_score = ecn.CongestionScore(MakeState(500, 1000));  // 50%
  CHECK(mid_score > 0.0 && mid_score < 1.0);
  CHECK(!ecn.IsCongested(MakeState(100, 1000)));
  CHECK(ecn.IsCongested(MakeState(500, 1000)));

  // DropRateStrategy: threshold on rolling-window drop rate.
  DropRateStrategy dr(0.01);  // 1%
  CHECK(!dr.IsCongested(MakeState(0, 0, 0, 1000)));    // 0% drops
  CHECK(dr.IsCongested(MakeState(0, 0, 20, 1000)));    // 2% drops >= 1%
  CHECK(!dr.IsCongested(MakeState(0, 0, 0, 0)));       // no packets -> no signal

  // Strategy pattern sanity: all three implement the same interface and
  // can be stored/queried polymorphically.
  std::vector<const CongestionStrategy*> strategies = {&qd, &ecn, &dr};
  CHECK(strategies.size() == 3);
  CHECK(std::string(strategies[0]->Name()) == "queue_depth");
  CHECK(std::string(strategies[1]->Name()) == "ecn");
  CHECK(std::string(strategies[2]->Name()) == "drop_rate");
}

TEST_MAIN(RunTests)
