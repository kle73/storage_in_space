#!/usr/bin/env python3
"""
Analyse broadcast dissemination logs.

Input CSV columns:  is_sender,node,origin,id,time

A "packet" is identified by (origin, id). Every row with is_sender == 0 is a
first reception of that packet at `node`, at simulation time `time`.

ORIGIN TIMESTAMP
----------------
The injection instant t0 of a packet is currently NOT logged (the origin
satellite writes no row for its own packet), so by default t0 is taken as the
earliest reception. That understates every broadcast time by roughly one
transmission + queueing delay.

Once the origin row is logged (is_sender == 1, node == origin), run with
    --origin-time sender
or leave the default `auto`, which uses the sender row whenever one exists for
that packet and silently falls back to the first reception otherwise. Nothing
else in the script needs to change.
"""

import argparse
import os
import sys

import numpy as np
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


# ─────────────────────────────────────────────────────────────────────────────
# Loading
# ─────────────────────────────────────────────────────────────────────────────
def load(path):
    df = pd.read_csv(
        path,
        dtype={"is_sender": "int8", "node": "int32",
               "origin": "int32", "id": "int64", "time": "float64"},
    )
    missing = {"is_sender", "node", "origin", "id", "time"} - set(df.columns)
    if missing:
        sys.exit(f"ERROR: input is missing column(s): {sorted(missing)}")

    # A node may in principle log a packet more than once; keep the earliest.
    df = (df.sort_values("time")
            .drop_duplicates(subset=["is_sender", "node", "origin", "id"],
                             keep="first"))
    return df


# ─────────────────────────────────────────────────────────────────────────────
# Per-packet statistics
# ─────────────────────────────────────────────────────────────────────────────
def packet_table(df, origin_time_mode, count_origin):
    """One row per (origin, id) with t0, span, coverage and reception times."""
    senders = df[df.is_sender == 1]
    recv    = df[df.is_sender == 0]

    if recv.empty:
        sys.exit("ERROR: no reception rows (is_sender == 0) in the input.")

    # t0 candidates from the sender rows, if any
    sender_t0 = (senders.groupby(["origin", "id"])["time"].min()
                 if not senders.empty else pd.Series(dtype="float64"))

    recs = []
    for (origin, pid), g in recv.groupby(["origin", "id"], sort=False):
        times = np.sort(g["time"].to_numpy())

        have_sender = (origin, pid) in sender_t0.index
        if origin_time_mode == "sender":
            if not have_sender:
                continue                       # strict: skip packets with no origin row
            t0 = float(sender_t0.loc[(origin, pid)])
        elif origin_time_mode == "auto" and have_sender:
            t0 = float(sender_t0.loc[(origin, pid)])
        else:
            t0 = float(times[0])               # first-reception fallback

        rel = times - t0
        rel = rel[rel >= 0.0]                  # guard against clock/order oddities
        recs.append({
            "origin": origin,
            "id": pid,
            "t0": t0,
            "n_receivers": len(times),
            # the origin holds the packet at t0 by definition
            "n_reached": len(times) + (1 if count_origin else 0),
            "span": float(rel[-1]) if len(rel) else 0.0,
            "rel": rel,
            "used_sender_row": have_sender,
        })

    pk = pd.DataFrame(recs)
    if pk.empty:
        sys.exit("ERROR: no packets left after applying --origin-time.")
    return pk.sort_values("t0").reset_index(drop=True)


def coverage_matrix(pk, grid, count_origin):
    """rows = packets, cols = grid times, value = nodes reached by that time."""
    base = 1 if count_origin else 0
    m = np.empty((len(pk), len(grid)), dtype=np.float64)
    for i, rel in enumerate(pk["rel"].to_numpy()):
        m[i] = np.searchsorted(rel, grid, side="right") + base
    return m


# ─────────────────────────────────────────────────────────────────────────────
# Plots
# ─────────────────────────────────────────────────────────────────────────────
def plot_span_over_time(pk, out, window):
    fig, ax = plt.subplots(figsize=(11, 5))
    ax.scatter(pk.t0, pk.span * 1e3, s=4, alpha=0.15,
               color="#4C72B0", edgecolors="none", label="individual packets")

    r = pk.set_index("t0")["span"].rolling(window, min_periods=max(5, window // 10))
    # ax.plot(pk.t0, r.mean().to_numpy() * 1e3, lw=2.0, color="#C44E52",
            # label=f"rolling mean (w={window})")
    # ax.plot(pk.t0, r.median().to_numpy() * 1e3, lw=1.4, color="#DD8452",
            # ls="--", label="rolling median")
    # ax.plot(pk.t0, r.quantile(0.95).to_numpy() * 1e3, lw=1.0, color="#8172B3",
            # ls=":", label="rolling p95")

    ax.set_xlabel("injection time  t0  [s]")
    ax.set_ylabel("broadcast time (last − first) [ms]")
    ax.set_title("Broadcast completion time over the run")
    ax.grid(alpha=0.3)
    ax.legend(loc="upper right", framealpha=0.9)
    fig.tight_layout()
    fig.savefig(out, dpi=140)
    plt.close(fig)


def plot_coverage(pk, grid, cov, n_nodes, out, n_examples):
    frac = cov / n_nodes * 100.0
    g_ms = grid * 1e3

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 5.2))

    # left: absolute node count, sample of individual packets + aggregate
    rng = np.random.default_rng(0)
    for i in rng.choice(len(pk), size=min(n_examples, len(pk)), replace=False):
        ax1.step(g_ms, cov[i], where="post", color="#999999", alpha=0.12, lw=0.8)
    ax1.plot(g_ms, cov.mean(0), lw=2.4, color="#C44E52", label="mean")
    ax1.plot(g_ms, np.median(cov, 0), lw=1.6, color="#4C72B0", ls="--", label="median")
    ax1.axhline(n_nodes, color="k", ls=":", lw=1, label=f"all {n_nodes} nodes")
    ax1.set_xlabel("time since injection [ms]")
    ax1.set_ylabel("nodes reached")
    ax1.set_title(f"Dissemination curve ({min(n_examples, len(pk))} packets shown in grey)")
    ax1.grid(alpha=0.3); ax1.legend(loc="lower right")

    # right: percentage with percentile band
    p10, p50, p90 = np.percentile(frac, [10, 50, 90], axis=0)
    ax2.fill_between(g_ms, p10, p90, alpha=0.25, color="#4C72B0", label="p10–p90")
    ax2.plot(g_ms, frac.mean(0), lw=2.4, color="#C44E52", label="mean")
    ax2.plot(g_ms, p50, lw=1.6, color="#4C72B0", ls="--", label="median")
    for lvl in (50, 90, 99):
        ax2.axhline(lvl, color="grey", ls=":", lw=0.8)
    ax2.set_xlabel("time since injection [ms]")
    ax2.set_ylabel("constellation reached [%]")
    ax2.set_ylim(0, 105)
    ax2.set_title("Coverage fraction vs time since injection")
    ax2.grid(alpha=0.3); ax2.legend(loc="lower right")

    fig.tight_layout()
    fig.savefig(out, dpi=140)
    plt.close(fig)


def plot_time_to_coverage(pk, grid, cov, n_nodes, out, window, levels):
    """When does a packet cross X% coverage, as a function of injection time?"""
    fig, ax = plt.subplots(figsize=(11, 5))
    colors = plt.cm.viridis(np.linspace(0.15, 0.85, len(levels)))

    for lvl, c in zip(levels, colors):
        need = lvl / 100.0 * n_nodes
        idx = np.argmax(cov >= need, axis=1)
        reached = (cov >= need).any(axis=1)
        t = np.where(reached, grid[idx], np.nan) * 1e3
        s = pd.Series(t, index=pk.t0.to_numpy())
        ax.plot(pk.t0, s.rolling(window, min_periods=max(5, window // 10)).mean(),
                lw=1.8, color=c, label=f"t{lvl}  ({100*reached.mean():.0f}% of pkts reach it)")

    ax.set_xlabel("injection time  t0  [s]")
    ax.set_ylabel("time to reach coverage level [ms]")
    ax.set_title(f"Time to X% coverage over the run (rolling mean, w={window})")
    ax.grid(alpha=0.3); ax.legend(loc="upper right", framealpha=0.9)
    fig.tight_layout()
    fig.savefig(out, dpi=140)
    plt.close(fig)


def plot_final_coverage(pk, n_nodes, out):
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(13, 4.6))

    ax1.hist(pk.n_reached / n_nodes * 100.0, bins=60, color="#4C72B0")
    ax1.set_xlabel("final coverage [%]"); ax1.set_ylabel("packets")
    ax1.set_title("How much of the constellation each packet reached")
    ax1.grid(alpha=0.3)

    s = np.sort(pk.span.to_numpy()) * 1e3
    ax2.plot(s, np.arange(1, len(s) + 1) / len(s) * 100.0, lw=2, color="#C44E52")
    ax2.set_xlabel("broadcast time [ms]"); ax2.set_ylabel("packets ≤ x  [%]")
    ax2.set_title("CDF of broadcast completion time")
    ax2.grid(alpha=0.3)

    fig.tight_layout()
    fig.savefig(out, dpi=140)
    plt.close(fig)


# ─────────────────────────────────────────────────────────────────────────────
def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("csv")
    ap.add_argument("--outdir", default="broadcast_plots")
    ap.add_argument("--num-nodes", type=int, default=None,
                    help="constellation size; inferred from the data if omitted")
    ap.add_argument("--origin-time", choices=["auto", "sender", "first-reception"],
                    default="auto",
                    help="how to determine t0 (see module docstring)")
    ap.add_argument("--no-count-origin", action="store_true",
                    help="do NOT count the origin itself as reached at t0")
    ap.add_argument("--min-receivers", type=int, default=2,
                    help="ignore packets logged by fewer than this many nodes")
    ap.add_argument("--grid-points", type=int, default=600)
    ap.add_argument("--grid-quantile", type=float, default=0.99,
                    help="x-axis extent of the coverage plots, as a span quantile")
    ap.add_argument("--window", type=int, default=200,
                    help="rolling window, in packets, for the over-time plots")
    ap.add_argument("--examples", type=int, default=300)
    ap.add_argument("--levels", type=int, nargs="+", default=[50, 90, 99, 100])
    a = ap.parse_args()

    os.makedirs(a.outdir, exist_ok=True)
    count_origin = not a.no_count_origin

    df = load(a.csv)
    n_nodes = a.num_nodes or int(max(df.node.max(), df.origin.max())) + 1

    pk = packet_table(df, a.origin_time, count_origin)
    n_all = len(pk)
    pk = pk[pk.n_receivers >= a.min_receivers].reset_index(drop=True)
    if pk.empty:
        sys.exit("ERROR: no packets left after --min-receivers filter.")

    tmax = float(np.quantile(pk.span, a.grid_quantile)) or float(pk.span.max())
    grid = np.linspace(0.0, max(tmax, 1e-9), a.grid_points)
    cov = coverage_matrix(pk, grid, count_origin)

    # ── summary ──────────────────────────────────────────────────────────────
    used_sender = int(pk.used_sender_row.sum())
    print(f"rows                       : {len(df):,}")
    print(f"nodes (constellation size) : {n_nodes}")
    print(f"packets                    : {n_all:,}  "
          f"({n_all - len(pk):,} dropped by --min-receivers={a.min_receivers})")
    print(f"t0 source                  : mode='{a.origin_time}', "
          f"{used_sender:,}/{len(pk):,} packets have an origin row")
    if used_sender == 0:
        print("  NOTE: no origin rows present -> every broadcast time is short by")
        print("        roughly one transmission + queueing delay at the origin.")
    print(f"injection times            : {pk.t0.min():.2f} .. {pk.t0.max():.2f} s")
    print()
    print("broadcast time (last reception - t0), ms:")
    q = np.percentile(pk.span, [50, 90, 95, 99]) * 1e3
    print(f"  mean {pk.span.mean()*1e3:8.3f}   p50 {q[0]:8.3f}   p90 {q[1]:8.3f}"
          f"   p95 {q[2]:8.3f}   p99 {q[3]:8.3f}   max {pk.span.max()*1e3:8.3f}")
    print()
    print("final coverage:")
    fc = pk.n_reached / n_nodes * 100.0
    print(f"  mean {fc.mean():6.2f}%   median {fc.median():6.2f}%   "
          f"min {fc.min():6.2f}%   full-coverage packets: "
          f"{(pk.n_reached >= n_nodes).mean()*100:.1f}%")
    print()
    for lvl in a.levels:
        need = lvl / 100.0 * n_nodes
        ok = (cov >= need).any(axis=1)
        if ok.any():
            t = grid[np.argmax(cov >= need, axis=1)][ok] * 1e3
            print(f"  t{lvl:<3d}: reached by {ok.mean()*100:5.1f}% of packets, "
                  f"mean {t.mean():8.3f} ms, p90 {np.percentile(t, 90):8.3f} ms")
        else:
            print(f"  t{lvl:<3d}: never reached")

    # ── plots ────────────────────────────────────────────────────────────────
    plot_span_over_time(pk, f"{a.outdir}/01_broadcast_time_over_run.png", a.window)
    plot_coverage(pk, grid, cov, n_nodes, f"{a.outdir}/02_dissemination_curve.png",
                  a.examples)
    plot_time_to_coverage(pk, grid, cov, n_nodes,
                          f"{a.outdir}/03_time_to_coverage.png", a.window, a.levels)
    plot_final_coverage(pk, n_nodes, f"{a.outdir}/04_final_coverage_and_cdf.png")

    per_packet = pk.drop(columns=["rel"])
    per_packet.to_csv(f"{a.outdir}/per_packet.csv", index=False)
    print(f"\nwrote 4 figures + per_packet.csv to {a.outdir}/")


if __name__ == "__main__":
    main()