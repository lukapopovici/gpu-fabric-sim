#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "bench/BenchHarness.h"
#include "incast/IncastDetector.h"
#include "router/AdaptiveRouter.h"
#include "router/DropRateStrategy.h"
#include "router/ECNStrategy.h"
#include "router/QueueDepthStrategy.h"
#include "router/StaticECMPRouter.h"
#include "telemetry/TelemetryStream.h"
#include "topology/ClosTopology.h"
#include "traffic/AllReducePattern.h"
#include "traffic/ManyToOnePattern.h"

namespace {

void PrintSummary(const fabric::bench::BenchSummary& s) {
  std::cout << s.router_name;
  if (!s.strategy_name.empty()) std::cout << " (" << s.strategy_name << ")";
  std::cout << ": flows=" << s.flow_count << " failed=" << s.failed_count
            << " p50=" << s.p50_us << "us" << " p99=" << s.p99_us << "us"
            << " max=" << s.max_us << "us" << " mean=" << s.mean_us << "us\n";
}

void PrintSweepSummary(const std::vector<fabric::bench::BenchSummary>& runs) {
  if (runs.size() == 1) {
    PrintSummary(runs.front());
    return;
  }

  const auto mean_and_stddev = [&runs](auto value) {
    double mean = 0.0;
    for (const auto& run : runs) mean += value(run);
    mean /= static_cast<double>(runs.size());

    double squared_deviation_sum = 0.0;
    for (const auto& run : runs) {
      const double deviation = value(run) - mean;
      squared_deviation_sum += deviation * deviation;
    }
    const double stddev =
        std::sqrt(squared_deviation_sum / static_cast<double>(runs.size()));
    return std::pair<double, double>{mean, stddev};
  };

  const auto [p50_mean, p50_stddev] =
      mean_and_stddev([](const auto& run) { return run.p50_us; });
  const auto [p99_mean, p99_stddev] =
      mean_and_stddev([](const auto& run) { return run.p99_us; });
  const auto [failed_mean, failed_stddev] =
      mean_and_stddev([](const auto& run) { return run.failed_count; });

  const auto& first = runs.front();
  std::cout << first.router_name;
  if (!first.strategy_name.empty()) {
    std::cout << " (" << first.strategy_name << ")";
  }
  std::cout << ": seeds=" << runs.size() << " p50=" << p50_mean << "+/-"
            << p50_stddev << "us p99=" << p99_mean << "+/-" << p99_stddev
            << "us failed=" << failed_mean << "+/-" << failed_stddev << '\n';
}

std::unique_ptr<fabric::router::CongestionStrategy> MakeStrategy(
    const std::string& name) {
  using namespace fabric::router;
  if (name == "ecn") return std::make_unique<ECNStrategy>();
  if (name == "drop_rate") return std::make_unique<DropRateStrategy>();
  return std::make_unique<QueueDepthStrategy>();  // default / "queue_depth"
}

// Everything the simulator run needs, with defaults matching the
// previous hardcoded demo setup -- so running with no flags at all
// reproduces exactly what main() used to do unconditionally.
struct Args {
  std::uint32_t pods = 2;
  std::uint32_t leaves_per_pod = 2;
  std::uint32_t spines_per_pod = 2;
  std::uint32_t gpus_per_leaf = 4;
  std::uint32_t core_count = 2;

  std::uint64_t message_bytes = 16ull * 1024 * 1024;
  std::uint32_t chunks = 16;
  std::string traffic_pattern = "all_reduce";
  std::uint64_t queue_capacity_bytes = 8ull * 1024 * 1024;

  std::uint64_t seed = 1234;
  std::uint32_t seed_sweep = 1;

  // "all" runs static ECMP plus every adaptive strategy (the original
  // demo behavior). Otherwise one of "queue_depth", "ecn", "drop_rate",
  // or "static" to run just that single router.
  std::string strategy = "all";

  std::string telemetry_out;  // empty = in-memory only, no CSV written
  std::uint64_t telemetry_interval_us = 50;
  std::uint64_t telemetry_horizon_margin_us = 3000;
};

void PrintUsage(const char* prog) {
  std::cout <<
      "Usage: " << prog << " [options]\n"
      "\n"
      "Topology:\n"
      "  --pods N             number of pods                  (default 2)\n"
      "  --leaves N           leaf switches per pod            (default 2)\n"
      "  --spines N           spine switches per pod           (default 2)\n"
      "  --gpus-per-leaf N    GPUs hanging off each leaf       (default 4)\n"
      "  --cores N            core switches (0 = no core tier) (default 2)\n"
      "\n"
      "Traffic:\n"
      "  --traffic NAME       all_reduce or many_to_one       (default all_reduce)\n"
      "  --message-bytes N    total payload per GPU            (default 16777216)\n"
      "  --chunks N           traffic pipeline depth            (default 16)\n"
      "  --queue-capacity-bytes N\n"
      "                       buffer capacity on every link tier (default 8388608)\n"
      "  --seed N             RNG seed, for reproducibility    (default 1234)\n"
      "  --seed-sweep N       run N consecutive seeds and report mean/stddev\n"
      "                                                        (default 1)\n"
      "\n"
      "Run selection:\n"
      "  --strategy NAME      one of: all, static, queue_depth, ecn,\n"
      "                       drop_rate                        (default all)\n"
      "\n"
      "Output:\n"
      "  --telemetry-out PATH write per-tick link stats CSV here (unset = none)\n"
      "  --telemetry-interval-us N   sampling interval for the CSV (default 50)\n"
      "  --telemetry-horizon-margin-us N\n"
      "                       how far past the last flow's start time to keep\n"
      "                       sampling (default 3000)\n"
      "\n"
      "  -h, --help           show this message\n";
}

// Minimal hand-rolled `--flag value` parser -- no external dependency,
// which matches the rest of this project's "no network access needed to
// build" stance (see tests/test_main.h for the same reasoning applied to
// unit tests instead of a fetched framework).
bool ParseArgs(int argc, char** argv, Args* args) {
  const auto next_value = [&](int& i) -> std::string {
    if (i + 1 >= argc) {
      std::cerr << "error: " << argv[i] << " requires a value\n";
      std::exit(1);
    }
    return argv[++i];
  };

  for (int i = 1; i < argc; ++i) {
    const std::string flag = argv[i];
    try {
      if (flag == "-h" || flag == "--help") {
        PrintUsage(argv[0]);
        return false;
      } else if (flag == "--pods") {
        args->pods = static_cast<std::uint32_t>(std::stoul(next_value(i)));
      } else if (flag == "--leaves") {
        args->leaves_per_pod =
            static_cast<std::uint32_t>(std::stoul(next_value(i)));
      } else if (flag == "--spines") {
        args->spines_per_pod =
            static_cast<std::uint32_t>(std::stoul(next_value(i)));
      } else if (flag == "--gpus-per-leaf") {
        args->gpus_per_leaf =
            static_cast<std::uint32_t>(std::stoul(next_value(i)));
      } else if (flag == "--cores") {
        args->core_count =
            static_cast<std::uint32_t>(std::stoul(next_value(i)));
      } else if (flag == "--message-bytes") {
        args->message_bytes = std::stoull(next_value(i));
      } else if (flag == "--chunks") {
        args->chunks = static_cast<std::uint32_t>(std::stoul(next_value(i)));
      } else if (flag == "--traffic") {
        args->traffic_pattern = next_value(i);
      } else if (flag == "--queue-capacity-bytes") {
        args->queue_capacity_bytes = std::stoull(next_value(i));
      } else if (flag == "--seed") {
        args->seed = std::stoull(next_value(i));
      } else if (flag == "--seed-sweep") {
        const std::uint64_t count = std::stoull(next_value(i));
        if (count > std::numeric_limits<std::uint32_t>::max()) {
          std::cerr << "error: invalid value for " << flag << "\n";
          std::exit(1);
        }
        args->seed_sweep = static_cast<std::uint32_t>(count);
      } else if (flag == "--strategy") {
        args->strategy = next_value(i);
      } else if (flag == "--telemetry-out") {
        args->telemetry_out = next_value(i);
      } else if (flag == "--telemetry-interval-us") {
        args->telemetry_interval_us = std::stoull(next_value(i));
      } else if (flag == "--telemetry-horizon-margin-us") {
        args->telemetry_horizon_margin_us = std::stoull(next_value(i));
      } else {
        std::cerr << "error: unknown option '" << flag << "'\n";
        PrintUsage(argv[0]);
        std::exit(1);
      }
    } catch (const std::exception&) {
      std::cerr << "error: invalid value for " << flag << "\n";
      std::exit(1);
    }
  }

  static const std::vector<std::string> kValidStrategies = {
      "all", "static", "queue_depth", "ecn", "drop_rate"};
  bool valid_strategy = false;
  for (const auto& s : kValidStrategies) {
    if (args->strategy == s) valid_strategy = true;
  }
  if (!valid_strategy) {
    std::cerr << "error: --strategy must be one of: all, static, "
                  "queue_depth, ecn, drop_rate\n";
    std::exit(1);
  }

  if (args->traffic_pattern != "all_reduce" &&
      args->traffic_pattern != "many_to_one") {
    std::cerr << "error: --traffic must be one of: all_reduce, many_to_one\n";
    std::exit(1);
  }
  if (args->seed_sweep == 0) {
    std::cerr << "error: --seed-sweep must be greater than zero\n";
    std::exit(1);
  }
  if (args->queue_capacity_bytes == 0) {
    std::cerr << "error: --queue-capacity-bytes must be greater than zero\n";
    std::exit(1);
  }
  if (args->seed > std::numeric_limits<std::uint64_t>::max() -
                       (args->seed_sweep - 1)) {
    std::cerr << "error: seed range overflows for --seed-sweep\n";
    std::exit(1);
  }

  return true;
}

// Derives a per-run telemetry output path from the user's base path by
// inserting a label before the extension (e.g. "telemetry.csv" +
// "queue_depth" -> "telemetry_queue_depth.csv"). Each router run gets
// its own TelemetryStream and CSV -- sharing one across multiple runs
// would interleave rows from different routers at overlapping
// timestamps (every run's Simulator starts its own clock at time 0),
// which would make the CSV meaningless to plot. Returns an empty string
// if `base` is empty, so "no telemetry requested" stays "no telemetry
// requested" for every run.
std::string PerRunTelemetryPath(const std::string& base,
                                  const std::string& label) {
  if (base.empty()) return "";
  const auto dot = base.find_last_of('.');
  if (dot == std::string::npos) return base + "_" + label;
  return base.substr(0, dot) + "_" + label + base.substr(dot);
}

}  // namespace

int main(int argc, char** argv) {
  using namespace fabric;

  Args args;
  if (!ParseArgs(argc, argv, &args)) {
    return 0;  // --help was requested
  }

  topology::ClosConfig clos_config;
  clos_config.pods = args.pods;
  clos_config.leaves_per_pod = args.leaves_per_pod;
  clos_config.spines_per_pod = args.spines_per_pod;
  clos_config.gpus_per_leaf = args.gpus_per_leaf;
  clos_config.core_count = args.core_count;
  clos_config.gpu_leaf_queue_capacity_bytes = args.queue_capacity_bytes;
  clos_config.leaf_spine_queue_capacity_bytes = args.queue_capacity_bytes;
  clos_config.spine_core_queue_capacity_bytes = args.queue_capacity_bytes;
  topology::ClosTopology topo(clos_config);

  std::cout << "Built Clos topology: " << topo.Nodes().size() << " nodes, "
            << topo.Links().size() << " links, " << topo.GpuNodes().size()
            << " GPUs\n";
  std::cout << "Queue capacity per link: " << args.queue_capacity_bytes
            << " bytes\n";

  std::unique_ptr<traffic::TrafficGenerator> traffic_gen;
  if (args.traffic_pattern == "many_to_one") {
    traffic::ManyToOneConfig traffic_config;
    traffic_config.message_bytes = args.message_bytes;
    traffic_config.chunks = args.chunks;
    traffic_gen =
        std::make_unique<traffic::ManyToOnePattern>(traffic_config);
  } else {
    traffic::AllReduceConfig traffic_config;
    traffic_config.message_bytes = args.message_bytes;
    traffic_config.chunks = args.chunks;
    traffic_gen = std::make_unique<traffic::AllReducePattern>(traffic_config);
  }
  std::cout << "Traffic pattern: " << args.traffic_pattern << '\n';
  const bool run_static = (args.strategy == "all" || args.strategy == "static");
  const bool run_all_adaptive = (args.strategy == "all");

  if (run_static) {
    std::vector<bench::BenchSummary> runs;
    for (std::uint32_t i = 0; i < args.seed_sweep; ++i) {
      const std::uint64_t seed = args.seed + i;
      bench::BenchHarness harness(topo, *traffic_gen, seed);
      router::StaticECMPRouter static_router;
      const std::string label =
          args.seed_sweep == 1
              ? "static"
              : "static_seed_" + std::to_string(seed);
      telemetry::TelemetryStream telemetry(
          PerRunTelemetryPath(args.telemetry_out, label));
      runs.push_back(harness.Run(
          static_router, "", &telemetry,
          static_cast<sim::SimTime>(args.telemetry_interval_us),
          static_cast<sim::SimTime>(args.telemetry_horizon_margin_us)));
    }
    PrintSweepSummary(runs);
  }

  std::vector<std::string> adaptive_strategies;
  if (run_all_adaptive) {
    adaptive_strategies = {"queue_depth", "ecn", "drop_rate"};
  } else if (args.strategy != "static") {
    adaptive_strategies = {args.strategy};
  }

  for (const std::string& strategy_name : adaptive_strategies) {
    std::vector<bench::BenchSummary> runs;
    for (std::uint32_t i = 0; i < args.seed_sweep; ++i) {
      const std::uint64_t seed = args.seed + i;
      bench::BenchHarness harness(topo, *traffic_gen, seed);
      incast::IncastDetector::Config incast_config;
      auto incast_detector =
          std::make_unique<incast::IncastDetector>(incast_config);
      auto strategy = MakeStrategy(strategy_name);

      router::AdaptiveRouter adaptive_router(std::move(strategy),
                                               std::move(incast_detector));
      const std::string label =
          args.seed_sweep == 1
              ? strategy_name
              : strategy_name + "_seed_" + std::to_string(seed);
      telemetry::TelemetryStream telemetry(
          PerRunTelemetryPath(args.telemetry_out, label));
      runs.push_back(harness.Run(
          adaptive_router, strategy_name, &telemetry,
          static_cast<sim::SimTime>(args.telemetry_interval_us),
          static_cast<sim::SimTime>(args.telemetry_horizon_margin_us)));
    }
    PrintSweepSummary(runs);
  }

  return 0;
}
