#include "bench/BenchHarness.h"

// GCC's inliner can mis-analyze the internal small-buffer layout of
// std::shared_ptr<std::function<...>> in the recursive per-hop lambda
// pattern used below and emit a spurious -Warray-bounds warning deep
// inside libstdc++'s std::function destructor. This is a known false
// positive with this pattern on recent GCC versions, not a real
// out-of-bounds access -- suppressed locally rather than globally so it
// doesn't hide a genuine array-bounds bug elsewhere in the project.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif

#include <algorithm>
#include <memory>

#include "dataplane/DataPlaneSim.h"
#include "router/AdaptiveRouter.h"

namespace fabric::bench {

namespace {
// Retransmission model for a dropped hop: a lossy UDP-style transport
// has to detect the drop (via timeout, since there's no ACK stream
// modeled here) before it can retry. Uses exponential backoff --
// kRetransmitBaseTimeoutUs * 2^retry_count, capped at
// kRetransmitMaxTimeoutUs -- rather than a fixed timeout, since hammering
// an already-congested link with fixed-interval retries just adds more
// load to the exact link that's struggling, and real transports (TCP,
// QUIC, etc.) universally back off instead.
constexpr int kMaxRetries = 5;
constexpr sim::SimTime kRetransmitBaseTimeoutUs = 10;
constexpr sim::SimTime kRetransmitMaxTimeoutUs = 200;

sim::SimTime RetransmitTimeout(int retry_count) {
  // retry_count is 0 on the first retry, so this gives base, 2*base,
  // 4*base, ... capped at the max. Shift amount is clamped so a large
  // retry_count can't undefined-behavior a left-shift.
  const int shift = std::min(retry_count, 20);
  const sim::SimTime backoff = kRetransmitBaseTimeoutUs * (sim::SimTime{1} << shift);
  return std::min(backoff, kRetransmitMaxTimeoutUs);
}
}  // namespace

BenchHarness::BenchHarness(const topology::Topology& topo,
                            const traffic::TrafficGenerator& traffic_gen,
                            std::uint64_t seed)
    : topo_(topo), traffic_gen_(traffic_gen), seed_(seed) {}

BenchSummary BenchHarness::Run(router::Router& router,
                                const std::string& strategy_label,
                                telemetry::TelemetryStream* telemetry_in,
                                sim::SimTime telemetry_tick_interval_us,
                                sim::SimTime telemetry_horizon_margin_us) {
  const std::vector<traffic::Flow> flows =
      traffic_gen_.GenerateFlows(topo_, seed_);

  sim::Simulator simulator;
  dataplane::DataPlaneSim data_plane;
  telemetry::TelemetryStream local_telemetry;
  telemetry::TelemetryStream& telemetry =
      telemetry_in != nullptr ? *telemetry_in : local_telemetry;

  auto* adaptive = dynamic_cast<router::AdaptiveRouter*>(&router);

  std::vector<FlowCompletion> completions;
  completions.reserve(flows.size());

  // Per-link bandwidth AND queue capacity, read straight off the
  // topology's Link records instead of a single hardcoded constant --
  // different tiers (GPU-facing vs. spine-core uplinks) can legitimately
  // have different buffer budgets, and ClosConfig now exposes that.
  struct LinkParams {
    double bandwidth_gbps;
    std::uint64_t queue_capacity_bytes;
  };
  std::unordered_map<topology::LinkId, LinkParams> link_params;
  for (const auto& link : topo_.Links()) {
    link_params[link.id] = LinkParams{link.bandwidth_gbps,
                                        link.queue_capacity_bytes};
  }
  // Fallback for any link that somehow wasn't populated with a capacity
  // (e.g. a future Topology implementation that leaves it at the
  // struct's default of 0) -- 0 would make DataPlaneSim drop everything
  // instantly, which is never useful behavior, so fall back to a sane
  // default instead of silently breaking every flow through that link.
  constexpr std::uint64_t kFallbackQueueCapacityBytes = 8ull * 1024 * 1024;
  constexpr double kFallbackBandwidthGbps = 100.0;

  // Periodically sample and flush link telemetry, independent of any
  // flow's own hop-by-hop updates -- this is what produces the CSV
  // time series tools/viz/heatmap.py reads (one row per tracked link
  // per tick). Scheduled as a fixed, non-recursive batch of events up
  // front (rather than a self-rescheduling recurring event) so there's
  // no risk of the simulator looping forever if some other bug left
  // events perpetually pending: the horizon is bounded by the last
  // flow's start time plus a fixed margin, computed once, here.
  //
  // If `telemetry` was constructed without an output path,
  // TelemetryStream::FlushTick() is a no-op, so these ticks cost
  // essentially nothing when nobody asked for a CSV.
  if (telemetry_tick_interval_us > 0) {
    sim::SimTime horizon = 0;
    for (const auto& flow : flows) {
      horizon = std::max(horizon, flow.start_time);
    }
    horizon += telemetry_horizon_margin_us;

    for (sim::SimTime t = 0; t <= horizon; t += telemetry_tick_interval_us) {
      simulator.Events().ScheduleAfter(
          0, t, "telemetry_tick",
          [&telemetry, t]() { telemetry.FlushTick(t); });
    }
  }

  // Each flow's journey is a chain of discrete events -- one per hop
  // (and one per retry) -- rather than a single synchronous loop that
  // resolves the whole path inside one event callback. This matters for
  // correctness, not just style: the simulator processes top-level
  // events in start_time order, so if an entire multi-hop flow (which
  // might take 100+us across several hops and retries) were resolved
  // synchronously, its later hops would call DataPlaneSim::SendOnLink
  // *before* a different flow's earlier-in-real-time hop on the same
  // link, even though that other flow started later but its hop
  // logically occurs sooner. DataPlaneSim's FCFS queueing model assumes
  // SendOnLink is called in true chronological order of arrival time
  // per link -- violating that silently corrupts the congestion signal
  // the adaptive router is scoring paths against. Scheduling each hop as
  // its own event lets the simulator's event queue do what it's for:
  // interleave everything in true simulated-time order automatically.

  for (const auto& flow : flows) {
    simulator.Events().ScheduleAfter(
        flow.start_time, 0, "route_flow", [&, flow]() {
          if (adaptive != nullptr) {
            adaptive->RecordArrival(flow.dst, flow.src, simulator.Now());
          }

          router::RoutingDecision decision = router.RouteFlow(topo_, flow);

          if (decision.path.empty()) {
            completions.push_back(FlowCompletion{flow.flow_id,
                                                    flow.start_time,
                                                    flow.start_time,
                                                    /*failed=*/true});
            return;
          }

          auto path = std::make_shared<std::vector<topology::LinkId>>(
              std::move(decision.path));

          // Per-flow recursive hop function: closes over flow (by value,
          // so flow_id/start_time/bytes are stable for this flow's
          // entire journey) plus the shared harness state by reference.
          auto hop_fn = std::make_shared<
              std::function<void(std::size_t, int, sim::SimTime)>>();
          *hop_fn = [&simulator, &data_plane, &telemetry, &link_params,
                     adaptive, &completions, flow, path,
                     hop_fn](std::size_t hop_index, int retry_count,
                              sim::SimTime hop_time) {
            if (hop_index >= path->size()) {
              completions.push_back(FlowCompletion{
                  flow.flow_id, flow.start_time, hop_time, /*failed=*/false});
              return;
            }

            const topology::LinkId link = (*path)[hop_index];
            const auto it = link_params.find(link);
            const double bw =
                it != link_params.end() ? it->second.bandwidth_gbps
                                          : kFallbackBandwidthGbps;
            const std::uint64_t queue_capacity =
                (it != link_params.end() && it->second.queue_capacity_bytes > 0)
                    ? it->second.queue_capacity_bytes
                    : kFallbackQueueCapacityBytes;

            auto result = data_plane.SendOnLink(link, bw, queue_capacity,
                                                  flow.bytes, hop_time,
                                                  telemetry);

            if (adaptive != nullptr) {
              adaptive->UpdateLinkState(adaptive->Strategy(),
                                          telemetry.Snapshot(link, hop_time));
            }

            if (result.dropped) {
              if (retry_count >= kMaxRetries) {
                completions.push_back(FlowCompletion{flow.flow_id,
                                                        flow.start_time,
                                                        hop_time,
                                                        /*failed=*/true});
                return;
              }
              // Schedule the retry as its own future event, after an
              // exponentially-growing timeout, rather than looping
              // synchronously or retrying at a fixed interval -- see the
              // RetransmitTimeout() comment above for why backoff
              // matters here.
              const sim::SimTime timeout = RetransmitTimeout(retry_count);
              simulator.Events().ScheduleAfter(
                  simulator.Now(), timeout, "retry_hop",
                  [hop_fn, hop_index, retry_count, hop_time, timeout]() {
                    (*hop_fn)(hop_index, retry_count + 1, hop_time + timeout);
                  });
              return;
            }

            // Schedule the next hop to fire at its true finish time,
            // rather than continuing the loop synchronously -- this is
            // what lets other flows' hops interleave correctly in
            // between.
            const sim::SimTime delay = result.finish_time - simulator.Now();
            simulator.Events().ScheduleAfter(
                simulator.Now(), delay, "next_hop",
                [hop_fn, hop_index, finish = result.finish_time]() {
                  (*hop_fn)(hop_index + 1, 0, finish);
                });
          };

          (*hop_fn)(0, 0, simulator.Now());
        });
  }

  simulator.Run();

  return Summarize(router.Name(), strategy_label, std::move(completions));
}

}  // namespace fabric::bench

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
