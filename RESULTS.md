# Benchmark results

These runs compare static ECMP with the adaptive router on the default Clos
topology (2 pods, 2 leaves per pod, 2 spines per pod, 4 GPUs per leaf, 2
cores), using seed `1234`. Links run at 400 Gbps with 8 MiB queue capacities.
Times are simulated microseconds, not wall-clock measurements.

## Ring all-reduce

| Workload | Router | Flows | Failed | p50 | p99 | Max |
|---|---|---:|---:|---:|---:|---:|
| Default: 16 MiB/GPU, 16 chunks | Static ECMP | 256 | 0 | 142 | 259 | 281 |
| | Adaptive, queue-depth | 256 | 0 | 140 | 239 | 257 |
| | Adaptive, ECN | 256 | 0 | 140 | 242 | 262 |
| | Adaptive, drop-rate | 256 | 0 | 139 | 239 | 257 |
| Higher load: 16 MiB/GPU, 8 chunks | Static ECMP | 128 | 16 | 425 | 816 | 916 |
| | Adaptive, queue-depth | 128 | 16 | 406 | 710 | 871 |
| | Adaptive, ECN | 128 | 16 | 415 | 789 | 890 |
| | Adaptive, drop-rate | 128 | 18 | 401 | 688 | 830 |

The higher-load configuration raises the per-flow chunk size from 1 MiB to
2 MiB. In this single seeded run, adaptive queue-depth and drop-rate routing
reduced p99 by about 13% and 16%, respectively, versus static ECMP. Drop-rate
had two additional failed flows, so p99 should not be read without the failure
count.

## Many-to-one incast

Command:

```sh
./build/gpu_fabric_sim --traffic many_to_one --message-bytes 1048576 --chunks 4 --strategy all
```

This creates 60 flows: 15 senders per chunk converge on one receiver, with
four bursts spaced 100 us apart. The run had zero failed flows. Static ECMP
reported p99 of 75 us; all three adaptive strategies also reported 75 us.
This moderate-load case exercises the incast detector and path-diversification
logic, but it does not show a latency improvement. The unit test verifies that
the detector activates and that the adaptive router selects both available
spine paths during an incast burst.

## Three-seed baseline sweep

The default ring all-reduce workload was also run with `--seed-sweep 3`
(seeds 1234, 1235, and 1236). The reported values are the mean and population
standard deviation across those three per-seed summaries:

| Router | p50 (mean +/- stddev) | p99 (mean +/- stddev) | Failed flows (mean +/- stddev) |
|---|---:|---:|---:|
| Static ECMP | 125.3 +/- 20.1 us | 226.3 +/- 24.5 us | 0 +/- 0 |
| Adaptive, queue-depth | 115.0 +/- 22.7 us | 211.7 +/- 19.3 us | 0 +/- 0 |
| Adaptive, ECN | 115.0 +/- 22.7 us | 212.7 +/- 20.7 us | 0 +/- 0 |
| Adaptive, drop-rate | 114.3 +/- 22.3 us | 211.7 +/- 19.3 us | 0 +/- 0 |

Three seeds validate the sweep mode and illustrate run-to-run variation, but
are still too few to support a general performance claim.

## Buffer-capacity stress sweep

To evaluate a more congested regime, the 16 MiB/GPU, 8-chunk all-reduce
workload was run over five seeds (1234-1238) with uniform capacity overrides
on all link tiers. The table reports p99 and failed flows as mean +/- standard
deviation:

| Buffer per link | Router | p99 | Failed flows |
|---:|---|---:|---:|
| 8 MiB | Static ECMP | 731.8 +/- 93.0 us | 14.0 +/- 6.3 |
| | Adaptive, queue-depth | 685.2 +/- 41.5 us | 11.2 +/- 6.3 |
| | Adaptive, ECN | 712.2 +/- 132.0 us | 11.4 +/- 5.1 |
| | Adaptive, drop-rate | 733.0 +/- 105.4 us | 10.6 +/- 5.7 |
| 4 MiB | Static ECMP | 653.8 +/- 74.0 us | 21.8 +/- 8.7 |
| | Adaptive, queue-depth | 633.4 +/- 56.2 us | 20.4 +/- 9.2 |
| | Adaptive, ECN | 670.6 +/- 43.3 us | 20.2 +/- 8.6 |
| | Adaptive, drop-rate | 648.8 +/- 47.5 us | 20.2 +/- 8.9 |
| 2 MiB | Static ECMP | 731.6 +/- 61.8 us | 35.0 +/- 10.2 |
| | Adaptive, queue-depth | 671.6 +/- 55.4 us | 37.6 +/- 12.3 |
| | Adaptive, ECN | 665.6 +/- 54.0 us | 37.0 +/- 11.8 |
| | Adaptive, drop-rate | 703.6 +/- 36.0 us | 37.8 +/- 10.6 |

At 4 MiB, adaptive queue-depth routing lowered mean p99 by about 3% and had
slightly fewer failed flows than static ECMP in this five-seed sample. At
2 MiB, adaptive strategies had more failed flows, despite lower p99 values.
Reducing capacity to 1 MiB caused all 128 flows per run to fail for every
router; the resulting low p99 is a failure artifact, not an improvement.
These measurements illustrate why latency must be interpreted alongside
failure rate, and do not establish a universal adaptive-routing advantage.

Reproduce an individual stress point with:

```sh
./build/gpu_fabric_sim --traffic all_reduce --message-bytes 16777216 --chunks 8 --queue-capacity-bytes 4194304 --strategy all --seed-sweep 5
```

## Caveats

The fixed-seed examples are reproducible but are not statistical comparisons.
The simulator's p50/p99 include terminal durations for failed flows; use the
`failed` count alongside latency percentiles. The higher-load result shows a
potential trade-off, not proof that adaptive routing generally outperforms
static ECMP. A multi-seed sweep is needed before making a broader performance
claim.

Reproduce the all-reduce runs with:

```sh
./build/gpu_fabric_sim --strategy all
./build/gpu_fabric_sim --traffic all_reduce --message-bytes 16777216 --chunks 8 --strategy all
```
