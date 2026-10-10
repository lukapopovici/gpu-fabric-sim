#include "sim/EventQueue.h"
#include "test_main.h"

using fabric::sim::EventQueue;

void RunTests() {
  EventQueue q;
  CHECK(q.Empty());

  std::vector<int> fired_order;
  q.Schedule(100, "third", [&] { fired_order.push_back(3); });
  q.Schedule(10, "first", [&] { fired_order.push_back(1); });
  q.Schedule(50, "second", [&] { fired_order.push_back(2); });

  CHECK(!q.Empty());
  CHECK(q.Size() == 3);
  CHECK(q.PeekTime() == 10);

  while (auto ev = q.PopNext()) {
    if (ev->action) ev->action();
  }

  CHECK(q.Empty());
  CHECK((fired_order == std::vector<int>{1, 2, 3}));

  // Same-timestamp events fire in schedule order (FIFO tiebreak on id).
  EventQueue tie_q;
  std::vector<int> tie_order;
  tie_q.Schedule(5, "a", [&] { tie_order.push_back(1); });
  tie_q.Schedule(5, "b", [&] { tie_order.push_back(2); });
  while (auto ev = tie_q.PopNext()) {
    if (ev->action) ev->action();
  }
  CHECK((tie_order == std::vector<int>{1, 2}));
}

TEST_MAIN(RunTests)
