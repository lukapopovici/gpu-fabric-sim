#include "incast/IncastDetector.h"
#include "test_main.h"

using fabric::incast::IncastDetector;

void RunTests() {
  IncastDetector::Config config;
  config.window_us = 50;
  config.sender_threshold = 4;
  IncastDetector detector(config);

  const fabric::topology::NodeId dst = 99;

  // Three distinct senders within the window -- below threshold of 4.
  detector.RecordArrival(dst, /*src=*/1, /*now=*/10);
  detector.RecordArrival(dst, /*src=*/2, /*now=*/15);
  detector.RecordArrival(dst, /*src=*/3, /*now=*/20);
  CHECK(detector.ActiveSenderCount(dst, 20) == 3);
  CHECK(!detector.IsIncastTarget(dst, 20));

  // A 4th distinct sender crosses the threshold.
  detector.RecordArrival(dst, /*src=*/4, /*now=*/25);
  CHECK(detector.ActiveSenderCount(dst, 25) == 4);
  CHECK(detector.IsIncastTarget(dst, 25));

  // Repeated arrivals from the same sender don't inflate the count.
  detector.RecordArrival(dst, /*src=*/4, /*now=*/26);
  CHECK(detector.ActiveSenderCount(dst, 26) == 4);

  // Old arrivals fall out of the window as time advances.
  CHECK(detector.ActiveSenderCount(dst, 26 + config.window_us + 1) == 0);
  CHECK(!detector.IsIncastTarget(dst, 26 + config.window_us + 1));

  // A destination with no recorded arrivals is never an incast target.
  CHECK(!detector.IsIncastTarget(/*dst=*/12345, 0));
}

TEST_MAIN(RunTests)
