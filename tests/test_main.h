#pragma once

#include <cstdlib>
#include <iostream>
#include <string>

// Minimal assert-based test harness so the skeleton doesn't need
// Catch2/GTest fetched over the network to build offline. Each test file
// defines RUN_TESTS() as a sequence of CHECK(...) calls; main() at the
// bottom of each file calls it and reports pass/fail.

inline int g_checks_run = 0;
inline int g_checks_failed = 0;

#define CHECK(cond)                                                     \
  do {                                                                  \
    ++g_checks_run;                                                     \
    if (!(cond)) {                                                      \
      ++g_checks_failed;                                                \
      std::cerr << "FAIL: " << #cond << " at " << __FILE__ << ":"       \
                << __LINE__ << "\n";                                    \
    }                                                                   \
  } while (0)

#define TEST_MAIN(run_tests_fn)                                         \
  int main() {                                                          \
    run_tests_fn();                                                     \
    std::cout << (g_checks_run - g_checks_failed) << "/" << g_checks_run \
              << " checks passed\n";                                    \
    return g_checks_failed == 0 ? 0 : 1;                                \
  }
