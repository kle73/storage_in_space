#!/usr/bin/env python3
import sys
from collections import defaultdict
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import numpy as np


def parse_storage_file(filepath):
    """Parse the storage file: id, time, packets, _, rtt, dropped"""
    time_packets = defaultdict(int)
    time_rtts    = defaultdict(list)
    time_dropped = defaultdict(int)

    with open(filepath, "r") as f:
        for line in f:
            line = line.split("//")[0].strip()
            if not line:
                continue
            parts = line.split(",")
            if len(parts) < 5:
                continue
            try:
                time    = float(parts[1])
                packets = int(parts[2])
                rtt     = float(parts[4])
                dropped = int(parts[5]) if len(parts) >= 6 else 0
            except ValueError:
                continue

            time_packets[time] += packets
            time_dropped[time] += dropped
            if rtt != 10.0:
                time_rtts[time].append(rtt)

    return time_packets, time_rtts, time_dropped


def parse_queue_file(filepath):
    """Parse the queue file: node, time, up, down, left, right, _, retransmit"""
    time_isl         = defaultdict(int)
    time_retransmit  = defaultdict(int)

    with open(filepath, "r") as f:
        for line in f:
            line = line.split("//")[0].strip()
            if not line or line.startswith("node") or line.startswith("#"):
                continue
            parts = line.split(",")
            if len(parts) < 8:
                continue
            try:
                time        = float(parts[1])
                up          = int(parts[2])
                down        = int(parts[3])
                left        = int(parts[4])
                right       = int(parts[5])
                retransmit  = int(parts[7])
            except ValueError:
                continue

            time_isl[time]        += up + down + left + right
            time_retransmit[time] += retransmit

    return time_isl, time_retransmit


def main():

    time_packets, time_rtts, time_dropped = parse_storage_file("experiment4/packet_statisticsS30.txt")
    time_isl, time_retransmit = parse_queue_file("../queue_stats/experiment4/queue_statisticsS30.csv")

    times = sorted(time_packets.keys())

    total_packets = []
    avg_rtts      = []
    isl_queues    = []
    retransmit_q  = []
    in_flight     = []
    dropped_pkts  = []

    for t in times:
        total  = time_packets[t]
        isl    = time_isl.get(t, 0)
        retr   = time_retransmit.get(t, 0)
        flight = max(0, total - isl - retr)

        total_packets.append(total)
        isl_queues.append(isl)
        retransmit_q.append(retr)
        in_flight.append(flight)
        dropped_pkts.append(time_dropped.get(t, 0))

        rtts = time_rtts.get(t, [])
        avg_rtts.append(np.mean(rtts) if rtts else None)

    # ── Plot ──────────────────────────────────────────────────────────────────
    fig, ax1 = plt.subplots(figsize=(14, 7))

    colors = {
        "total":      "#2563eb",  # blue
        "isl":        "#16a34a",  # green
        "retransmit": "#d97706",  # amber
        "in_flight":  "#7c3aed",  # purple
        "dropped":    "#e11d48",  # rose/red
        "rtt":        "#dc2626",  # red
    }
    lw = 2.0

    l1, = ax1.plot(times, total_packets, color=colors["total"],
                   linewidth=lw, label="Total Packets")
    l2, = ax1.plot(times, isl_queues,    color=colors["isl"],
                   linewidth=lw, label="ISL Queue Packets")
    l3, = ax1.plot(times, retransmit_q,  color=colors["retransmit"],
                   linewidth=lw, label="Retransmit Queue Packets")
    l4, = ax1.plot(times, in_flight,     color=colors["in_flight"],
                   linewidth=lw, label="In-Flight Packets")
    l6, = ax1.plot(times, dropped_pkts,  color=colors["dropped"],
                    linewidth=lw, linestyle=":", label="Dropped Packets (cumulative)")

    ax1.set_xlabel("Time (s)", fontsize=12)
    ax1.set_ylabel("Packet Count", fontsize=12)
    ax1.yaxis.set_major_locator(ticker.MaxNLocator(integer=True))
    ax1.set_xlim(times[0], times[-1])

    # Right y-axis: RTT
    ax2 = ax1.twinx()

    rtt_times = [t for t, r in zip(times, avg_rtts) if r is not None]
    rtt_vals  = [r for r in avg_rtts if r is not None]

    l5 = None
    # rtt_times = None
    if rtt_times:
        l5, = ax2.plot(rtt_times, rtt_vals,
                       color=colors["rtt"], linewidth=lw,
                       linestyle="--", marker="o", markersize=3,
                       label="Avg RTT")
    ax2.set_ylabel("Average RTT (s)", color=colors["rtt"], fontsize=12)
    ax2.tick_params(axis="y", labelcolor=colors["rtt"])
                           #, l6
    handles = [l1, l2, l3, l4, l6] + ([l5] if l5 else [])
    ax1.legend(handles, [h.get_label() for h in handles],
               loc="upper left", fontsize=10)

    plt.title("Satellite Network — Packets & RTT over Time", fontsize=14)
    fig.tight_layout()

    output = "storage_plotS30_dummy.png"
    plt.savefig(output, dpi=150)
    print(f"Plot saved to {output}")


if __name__ == "__main__":
    main()