#!/usr/bin/env python3
"""Render the fabric congestion heatmap from a BenchHarness telemetry CSV.

Input is one of the per-run CSVs produced by `gpu_fabric_sim
--telemetry-out PATH` (see main.cpp / TelemetryStream) -- one file per
router/strategy, named e.g. telemetry_static.csv, telemetry_ecn.csv, etc.
Columns: time_us, link_id, queue_depth_bytes, queue_capacity_bytes,
bytes_sent, packets_sent, packets_dropped.

Usage:
    python heatmap.py telemetry_static.csv
    python heatmap.py telemetry_static.csv --out static_heatmap.png
    python heatmap.py telemetry_static.csv telemetry_ecn.csv --compare
    python heatmap.py telemetry_static.csv --animate --out static.gif

Requires: pandas, matplotlib, numpy (see requirements.txt).
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd


def load_utilization(csv_path: Path) -> tuple[pd.DataFrame, pd.DataFrame]:
    """Loads a telemetry CSV and returns (utilization, drops) pivot tables.

    Both are indexed by link_id (rows) x time_us (columns). utilization
    is queue_depth_bytes / queue_capacity_bytes, clamped to [0, 1] --
    that's what CongestionStrategy implementations actually react to, so
    it's the most meaningful thing to visualize. drops is the raw
    packets_dropped count per (link, tick), useful as an overlay to spot
    exactly where and when the fabric actually lost packets, not just
    where it got busy.
    """
    df = pd.read_csv(csv_path)
    if df.empty:
        raise ValueError(
            f"{csv_path} has no data rows -- was --telemetry-out set for "
            "this run, and did the run actually inject any flows?"
        )

    df["utilization"] = (
        df["queue_depth_bytes"] / df["queue_capacity_bytes"].replace(0, np.nan)
    ).clip(0, 1).fillna(0)

    util = df.pivot_table(
        index="link_id", columns="time_us", values="utilization", fill_value=0
    )
    drops = df.pivot_table(
        index="link_id",
        columns="time_us",
        values="packets_dropped",
        fill_value=0,
        aggfunc="max",
    )
    # Reindex drops onto the same link/time axes as util in case some
    # links never appear in a given tick's rows.
    drops = drops.reindex(index=util.index, columns=util.columns, fill_value=0)
    return util, drops


def plot_static_heatmap(util: pd.DataFrame, drops: pd.DataFrame, title: str,
                          out_path: Path) -> None:
    """Static PNG: link utilization over time, with a red tick for any
    (link, time) cell where a drop occurred. This is the default output
    -- fast, no extra dependencies, and the thing you actually want for
    "did adaptive routing avoid the hotspot ECMP hit" side-by-side
    comparisons.
    """
    fig, ax = plt.subplots(figsize=(12, max(4, 0.25 * len(util.index))))

    im = ax.imshow(
        util.values,
        aspect="auto",
        origin="lower",
        cmap="inferno",
        vmin=0,
        vmax=1,
        extent=[util.columns.min(), util.columns.max(), 0, len(util.index)],
    )

    # Mark drops with a red 'x' at the corresponding (time, link row) cell.
    drop_rows, drop_cols = np.nonzero(drops.values)
    if len(drop_rows) > 0:
        times = util.columns.values[drop_cols]
        ax.scatter(times, drop_rows + 0.5, marker="x", color="red", s=18,
                    linewidths=1.2, label="packet drop")
        ax.legend(loc="upper right", fontsize=8)

    ax.set_yticks(np.arange(len(util.index)) + 0.5)
    ax.set_yticklabels(util.index)
    ax.set_xlabel("simulated time (us)")
    ax.set_ylabel("link id")
    ax.set_title(title)

    cbar = fig.colorbar(im, ax=ax)
    cbar.set_label("queue utilization (0 = empty, 1 = full)")

    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    print(f"wrote {out_path}")


def plot_comparison(csvs: list[Path], out_path: Path) -> None:
    """Side-by-side small-multiples comparison across multiple runs
    (e.g. static vs. each adaptive strategy), sharing one color scale so
    the panels are directly comparable at a glance.
    """
    loaded = [(csv.stem, *load_utilization(csv)) for csv in csvs]

    fig, axes = plt.subplots(
        len(loaded), 1, figsize=(12, 3 * len(loaded)), sharex=True
    )
    if len(loaded) == 1:
        axes = [axes]

    im = None
    for ax, (label, util, drops) in zip(axes, loaded):
        im = ax.imshow(
            util.values,
            aspect="auto",
            origin="lower",
            cmap="inferno",
            vmin=0,
            vmax=1,
            extent=[util.columns.min(), util.columns.max(), 0, len(util.index)],
        )
        drop_rows, drop_cols = np.nonzero(drops.values)
        if len(drop_rows) > 0:
            times = util.columns.values[drop_cols]
            ax.scatter(times, drop_rows + 0.5, marker="x", color="red", s=14,
                        linewidths=1.0)
        ax.set_ylabel(label, fontsize=9)
        ax.set_yticks([])

    axes[-1].set_xlabel("simulated time (us)")
    fig.suptitle("Fabric congestion by router/strategy (shared 0-1 scale)")
    if im is not None:
        fig.colorbar(im, ax=axes, label="queue utilization", shrink=0.8)

    fig.savefig(out_path, dpi=150)
    print(f"wrote {out_path}")


def plot_animated(util: pd.DataFrame, drops: pd.DataFrame, title: str,
                    out_path: Path) -> None:
    """Animated GIF: one frame per tick, showing the heatmap "filling in"
    over simulated time, plus a running drop counter. Heavier than the
    static plot (renders one frame per timestep) so it's opt-in via
    --animate rather than the default.
    """
    from matplotlib.animation import PillowWriter

    times = util.columns.values
    fig, ax = plt.subplots(figsize=(10, max(4, 0.25 * len(util.index))))

    frame_data = np.zeros_like(util.values)
    im = ax.imshow(
        frame_data,
        aspect="auto",
        origin="lower",
        cmap="inferno",
        vmin=0,
        vmax=1,
    )
    ax.set_yticks(np.arange(len(util.index)))
    ax.set_yticklabels(util.index, fontsize=7)
    ax.set_xlabel("tick index (see title for simulated time)")
    ax.set_ylabel("link id")
    cbar = fig.colorbar(im, ax=ax)
    cbar.set_label("queue utilization")

    total_drops_so_far = np.cumsum(drops.values.sum(axis=0))

    writer = PillowWriter(fps=10)
    with writer.saving(fig, str(out_path), dpi=100):
        for i, t in enumerate(times):
            frame_data[:, i] = util.values[:, i]
            im.set_data(frame_data)
            ax.set_title(
                f"{title} -- t={t}us, cumulative drops={int(total_drops_so_far[i])}"
            )
            writer.grab_frame()

    print(f"wrote {out_path}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                       formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("csv", nargs="+", type=Path,
                         help="telemetry CSV(s) from --telemetry-out")
    parser.add_argument("--out", type=Path, default=None,
                         help="output image path (default derived from input name)")
    parser.add_argument("--compare", action="store_true",
                         help="render all given CSVs as stacked small-multiples "
                              "instead of one plot per file")
    parser.add_argument("--animate", action="store_true",
                         help="render an animated GIF instead of a static PNG "
                              "(single CSV only)")
    args = parser.parse_args()

    for csv in args.csv:
        if not csv.exists():
            print(f"error: {csv} does not exist", file=sys.stderr)
            return 1

    if args.compare:
        out_path = args.out or Path("comparison_heatmap.png")
        plot_comparison(args.csv, out_path)
        return 0

    if args.animate:
        if len(args.csv) != 1:
            print("error: --animate only supports a single CSV", file=sys.stderr)
            return 1
        csv = args.csv[0]
        util, drops = load_utilization(csv)
        out_path = args.out or csv.with_suffix(".gif")
        plot_animated(util, drops, title=csv.stem, out_path=out_path)
        return 0

    for csv in args.csv:
        util, drops = load_utilization(csv)
        out_path = args.out if (args.out and len(args.csv) == 1) else csv.with_suffix(".png")
        plot_static_heatmap(util, drops, title=csv.stem, out_path=out_path)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
