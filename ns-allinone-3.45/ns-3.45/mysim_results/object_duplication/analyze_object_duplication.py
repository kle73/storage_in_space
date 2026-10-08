"""
analyze_object_duplication.py
==============================
Analyzes object duplication quality for a single experiment run.

Takes two input files:
  object_duplication/experiment/obj_inject<N>.csv
  object_duplication/experiment/obj_dup<N>.csv

obj_inject columns : time_s, origin, obj_id, num_frags, mode
  mode=0  both copies created at origin (immediately duplicated)
  mode=1  up-only injected; relay satellites must create the down copy
  mode=2  down-only injected; relay satellites must create the up copy

obj_dup columns    : time_s, relay_sat, origin, obj_id, frag_id
  one row per fragment that successfully receives its deferred second copy

Usage:
  python3 analyze_object_duplication.py [fill_level]
  fill_level defaults to 10
"""

import sys
import os
import pandas as pd
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.gridspec import GridSpec

# ── Config ────────────────────────────────────────────────────────────────────
FILL_LEVEL   = int(sys.argv[1]) if len(sys.argv) > 1 else 100
BASE         = os.path.dirname(__file__)
DUP_DIR      = os.path.join(BASE, "object_duplication", "experiment1")
OUT_DIR      = os.path.join(BASE, "object_duplication")
os.makedirs(OUT_DIR, exist_ok=True)

INJECT_FILE  = os.path.join(DUP_DIR, f"obj_inject{FILL_LEVEL}.csv")
DUP_FILE     = os.path.join(DUP_DIR, f"obj_dup{FILL_LEVEL}.csv")

print(f"Fill level : {FILL_LEVEL}")
print(f"Inject file: {INJECT_FILE}")
print(f"Dup file   : {DUP_FILE}")

# ── Load ──────────────────────────────────────────────────────────────────────
inject = pd.read_csv(INJECT_FILE)
dup    = pd.read_csv(DUP_FILE)

print(f"\nInject events : {len(inject):,}")
print(f"Dup events    : {len(dup):,}")
print(f"Mode breakdown:\n{inject['mode'].value_counts().sort_index()}")

# ── Build per-object summary ──────────────────────────────────────────────────
# For mode=0 objects: both copies exist at inject_time, no dup events needed.
# For mode=1/2 objects: count how many of their fragments appear in obj_dup,
#   and when the LAST one was created (= time of full duplication).

# Deferred-copy objects (need relay duplication)
deferred = inject[inject["mode"] != 0].copy()
deferred = deferred.rename(columns={"time_s": "inject_time"})

if len(dup) > 0:
    # For each (origin, obj_id): count unique frag_ids duplicated and max dup time
    dup_agg = (dup.groupby(["origin", "obj_id"])
                  .agg(frags_duped=("frag_id", "nunique"),
                       last_dup_time=("time_s", "max"),
                       first_dup_time=("time_s", "min"))
                  .reset_index())

    objects = deferred.merge(dup_agg, on=["origin", "obj_id"], how="left")
else:
    objects = deferred.copy()
    objects["frags_duped"]    = 0
    objects["last_dup_time"]  = np.nan
    objects["first_dup_time"] = np.nan

objects["frags_duped"]   = objects["frags_duped"].fillna(0).astype(int)
objects["dup_fraction"]  = objects["frags_duped"] / objects["num_frags"]
objects["fully_duped"]   = objects["dup_fraction"] >= 1.0
objects["delay_ms"]      = np.where(
    objects["fully_duped"],
    (objects["last_dup_time"] - objects["inject_time"]) * 1000,
    np.nan
)

n_deferred       = len(objects)
n_fully_duped    = objects["fully_duped"].sum()
n_partial        = ((objects["frags_duped"] > 0) & ~objects["fully_duped"]).sum()
n_never_duped    = (objects["frags_duped"] == 0).sum()
n_mode0          = (inject["mode"] == 0).sum()

print(f"\nObject statistics:")
print(f"  Mode=0 (immediately duplicated at origin): {n_mode0:,}")
print(f"  Mode≠0 (deferred duplication needed)     : {n_deferred:,}")
print(f"    Fully duplicated                        : {n_fully_duped:,} "
      f"({100*n_fully_duped/max(n_deferred,1):.1f}%)")
print(f"    Partially duplicated                    : {n_partial:,} "
      f"({100*n_partial/max(n_deferred,1):.1f}%)")
print(f"    Never duplicated                        : {n_never_duped:,} "
      f"({100*n_never_duped/max(n_deferred,1):.1f}%)")
if objects["fully_duped"].any():
    print(f"  Time-to-full-duplication (ms):  "
          f"mean={objects['delay_ms'].mean():.1f}  "
          f"median={objects['delay_ms'].median():.1f}  "
          f"p95={objects['delay_ms'].quantile(0.95):.1f}  "
          f"max={objects['delay_ms'].max():.1f}")


# ── Network-wide copies over time ─────────────────────────────────────────────
# At time T, fragment (origin, obj_id, frag_id) has 2 copies if:
#   mode=0: from inject_time of its object onward
#   mode≠0: from its dup_time onward (or never if no dup event)
#
# We build a sorted event list and walk it to compute the running count.

events = []

# mode=0 objects: all num_frags copies created at inject_time
mode0 = inject[inject["mode"] == 0]
for _, row in mode0.iterrows():
    events.append((row["time_s"], int(row["num_frags"])))

# mode≠0 objects: each dup row adds 1 copy
for _, row in dup.iterrows():
    events.append((row["time_s"], 1))

if events:
    ev_df = pd.DataFrame(events, columns=["time", "delta"])
    ev_df = ev_df.sort_values("time")
    ev_df["cumulative_frags_with_2copies"] = ev_df["delta"].cumsum()

    # Total fragments in the system
    total_frags = (inject["num_frags"] * (inject["mode"] == 0)).sum() + \
                  n_fully_duped * inject.loc[inject["mode"]!=0, "num_frags"].mean()
    # Simpler: total injected frags
    total_frags_injected = inject["num_frags"].sum()
    ev_df["completeness"] = ev_df["cumulative_frags_with_2copies"] / total_frags_injected
else:
    ev_df = pd.DataFrame(columns=["time","delta","cumulative_frags_with_2copies","completeness"])


# ─────────────────────────────────────────────────────────────────────────────
# Figure 1: Overview — mode distribution + duplication outcome
# ─────────────────────────────────────────────────────────────────────────────
fig, axes = plt.subplots(1, 2, figsize=(12, 5))

# Left: injection mode pie
mode_counts = inject["mode"].value_counts().sort_index()
mode_labels = {0: "mode=0\n(both at origin)", 1: "mode=1\n(up only)", 2: "mode=2\n(down only)"}
labels = [mode_labels.get(m, str(m)) for m in mode_counts.index]

def pct_label(pct):
    return f"{pct:.1f}%" if pct >= 1 else ""

wedges, texts, autotexts = axes[0].pie(mode_counts.values, autopct=pct_label,
            colors=["#2ecc71", "#3498db", "#e74c3c"], startangle=90, pctdistance=0.7)
axes[0].legend(
    wedges,
    labels,
    title="Injection mode",
    loc="center left",
    bbox_to_anchor=(1.0, 0.5)
)
axes[0].set_title(f"Injection mode distribution\n({len(inject):,} objects total)")

# Right: duplication outcome for deferred objects
if n_deferred > 0:
    outcomes  = [n_fully_duped, n_partial, n_never_duped]
    out_labels = ["Fully duplicated", "Partially duplicated", "Never duplicated"]
    out_colors = ["#2ecc71", "#f39c12", "#e74c3c"]
    bars = axes[1].bar(out_labels, outcomes, color=out_colors, alpha=0.85, edgecolor="black")
    for bar, val in zip(bars, outcomes):
        axes[1].text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.5,
                     f"{val:,}\n({100*val/n_deferred:.1f}%)",
                     ha="center", va="bottom", fontsize=9)
    axes[1].set_ylabel("Number of objects")
    axes[1].set_title(f"Deferred duplication outcome\n({n_deferred:,} mode≠0 objects)")
    axes[1].grid(True, axis="y", alpha=0.3)
else:
    axes[1].text(0.5, 0.5, "No deferred-duplication objects",
                 transform=axes[1].transAxes, ha="center", fontsize=12)

fig.suptitle(f"Object Duplication Overview — Object Size {666 if FILL_LEVEL == 1000 else FILL_LEVEL} packets", fontsize=13)
plt.tight_layout()
out = os.path.join(OUT_DIR, f"dup_overview_{FILL_LEVEL}.png")
plt.savefig(out, dpi=150)
print(f"\nSaved {out}")
plt.close()


# ─────────────────────────────────────────────────────────────────────────────
# Figure 2: Duplication completeness per object
# ─────────────────────────────────────────────────────────────────────────────
if n_deferred > 0:
    fig, axes = plt.subplots(1, 2, figsize=(12, 5))

    # Left: histogram of dup_fraction
    axes[0].hist(objects["dup_fraction"], bins=np.linspace(0, 1, 22),
                 color="#3498db", edgecolor="black", alpha=0.8)
    axes[0].axvline(1.0, color="green", linestyle="--", linewidth=1.5,
                    label="fully duplicated")
    axes[0].set_xlabel("Fraction of fragments duplicated")
    axes[0].set_ylabel("Number of objects")
    axes[0].set_title("Per-object duplication completeness\n"
                      "(1.0 = all fragments have 2 copies)")
    axes[0].legend()
    axes[0].grid(True, axis="y", alpha=0.3)

    # Right: scatter of frags_duped vs num_frags (spot partial objects)
    jitter = np.random.default_rng(42).uniform(-0.3, 0.3, len(objects))
    color_map = objects["dup_fraction"].values
    sc = axes[1].scatter(objects["num_frags"] + jitter,
                         objects["frags_duped"],
                         c=color_map, cmap="RdYlGn", vmin=0, vmax=1,
                         alpha=0.4, s=8)
    axes[1].plot([0, objects["num_frags"].max()],
                 [0, objects["num_frags"].max()],
                 "g--", linewidth=1, label="fully duplicated")
    plt.colorbar(sc, ax=axes[1]).set_label("Duplication fraction")
    axes[1].set_xlabel("Fragments per object (num_frags)")
    axes[1].set_ylabel("Fragments successfully duplicated")
    axes[1].set_title("Fragments duplicated vs total per object\n"
                      "(on diagonal = complete; below = partial)")
    axes[1].legend(fontsize=8)
    axes[1].grid(True, alpha=0.3)

    fig.suptitle(f"Duplication Completeness — fill level {FILL_LEVEL}%", fontsize=13)
    plt.tight_layout()
    out = os.path.join(OUT_DIR, f"dup_completeness_{FILL_LEVEL}.png")
    plt.savefig(out, dpi=150)
    print(f"Saved {out}")
    plt.close()


# ─────────────────────────────────────────────────────────────────────────────
# Figure 3: Time-to-full-duplication distribution
# ─────────────────────────────────────────────────────────────────────────────
fully_duped_delay = objects.loc[objects["fully_duped"], "delay_ms"].dropna()

if len(fully_duped_delay) > 0:
    fig, axes = plt.subplots(1, 2, figsize=(12, 5))

    # Left: histogram
    axes[0].hist(fully_duped_delay, bins=50, color="#9b59b6", edgecolor="black", alpha=0.8)
    axes[0].axvline(fully_duped_delay.mean(),   color="red",   linestyle="--",
                    linewidth=1.5, label=f"mean={fully_duped_delay.mean():.0f} ms")
    axes[0].axvline(fully_duped_delay.median(), color="orange", linestyle="--",
                    linewidth=1.5, label=f"median={fully_duped_delay.median():.0f} ms")
    axes[0].set_xlabel("Time to full duplication (ms)")
    axes[0].set_ylabel("Number of objects")
    axes[0].set_title("Distribution of time-to-full-duplication\n"
                      "(for mode≠0 objects that were fully duplicated)")
    axes[0].legend(fontsize=9)
    axes[0].grid(True, axis="y", alpha=0.3)

    # Right: CDF
    sorted_delay = np.sort(fully_duped_delay)
    cdf = np.arange(1, len(sorted_delay) + 1) / len(sorted_delay)
    axes[1].plot(sorted_delay, cdf, color="#9b59b6", linewidth=2)
    for q, ls in [(0.50, "--"), (0.90, ":"), (0.95, "-.")]:
        val = np.quantile(sorted_delay, q)
        axes[1].axvline(val, color="gray", linestyle=ls, linewidth=1,
                        label=f"p{int(q*100)}={val:.0f} ms")
    axes[1].set_xlabel("Time to full duplication (ms)")
    axes[1].set_ylabel("CDF")
    axes[1].set_title("CDF of time-to-full-duplication")
    axes[1].legend(fontsize=9)
    axes[1].grid(True, alpha=0.3)

    fig.suptitle(f"Time-to-Full-Duplication — fill level {FILL_LEVEL}%  "
                 f"(n={len(fully_duped_delay):,} fully-duplicated objects)", fontsize=12)
    plt.tight_layout()
    out = os.path.join(OUT_DIR, f"dup_delay_{FILL_LEVEL}.png")
    plt.savefig(out, dpi=150)
    print(f"Saved {out}")
    plt.close()
else:
    print("No fully-duplicated deferred objects — skipping delay plot.")


# ─────────────────────────────────────────────────────────────────────────────
# Figure 4: Network-wide duplication completeness over simulation time
# ─────────────────────────────────────────────────────────────────────────────
if len(ev_df) > 0:
    fig, axes = plt.subplots(2, 1, figsize=(12, 8), sharex=True)

    axes[0].plot(ev_df["time"], ev_df["cumulative_frags_with_2copies"],
                 color="#2ecc71", linewidth=1.5)
    axes[0].set_ylabel("Fragments with 2 copies")
    axes[0].set_title("Cumulative fragments with 2 copies over time")
    axes[0].grid(True, alpha=0.3)

    axes[1].plot(ev_df["time"], ev_df["completeness"] * 100,
                 color="#3498db", linewidth=1.5)
    axes[1].axhline(100, color="green", linestyle="--", linewidth=1, label="100% complete")
    axes[1].set_ylabel("% of injected fragments with 2 copies")
    axes[1].set_xlabel("Simulation time (s)")
    axes[1].set_title("Network-wide duplication completeness over time")
    axes[1].set_ylim(0, 105)
    axes[1].legend(fontsize=9)
    axes[1].grid(True, alpha=0.3)

    fig.suptitle(f"Duplication Completeness Over Time — fill level {FILL_LEVEL}%", fontsize=13)
    plt.tight_layout()
    out = os.path.join(OUT_DIR, f"dup_completeness_over_time_{FILL_LEVEL}.png")
    plt.savefig(out, dpi=150)
    print(f"Saved {out}")
    plt.close()


# ─────────────────────────────────────────────────────────────────────────────
# Figure 5: Per-origin-satellite breakdown
# ─────────────────────────────────────────────────────────────────────────────
if n_deferred > 0:
    per_origin = (objects.groupby("origin")
                         .agg(total_objs=("obj_id", "count"),
                              fully_duped=("fully_duped", "sum"),
                              mean_delay_ms=("delay_ms", "mean"),
                              mean_dup_frac=("dup_fraction", "mean"))
                         .reset_index())
    per_origin["dup_rate"] = per_origin["fully_duped"] / per_origin["total_objs"]

    fig = plt.figure(figsize=(14, 9))
    gs  = GridSpec(2, 2, figure=fig, hspace=0.45, wspace=0.35)

    # Top-left: duplication rate per origin satellite
    ax1 = fig.add_subplot(gs[0, 0])
    ax1.bar(per_origin["origin"], per_origin["dup_rate"] * 100,
            color="#3498db", alpha=0.8, width=0.7)
    ax1.set_xlabel("Origin satellite ID")
    ax1.set_ylabel("Full duplication rate (%)")
    ax1.set_title("Full duplication rate per originating satellite")
    ax1.grid(True, axis="y", alpha=0.3)
    ax1.set_ylim(0, 110)

    # Top-right: mean dup fraction per origin
    ax2 = fig.add_subplot(gs[0, 1])
    ax2.bar(per_origin["origin"], per_origin["mean_dup_frac"] * 100,
            color="#9b59b6", alpha=0.8, width=0.7)
    ax2.set_xlabel("Origin satellite ID")
    ax2.set_ylabel("Mean fragment duplication (%)")
    ax2.set_title("Mean fragment duplication rate per satellite\n"
                  "(< 100% = some fragments never got 2nd copy)")
    ax2.grid(True, axis="y", alpha=0.3)
    ax2.set_ylim(0, 110)

    # Bottom-left: mean time-to-full-dup per origin
    ax3 = fig.add_subplot(gs[1, 0])
    valid = per_origin.dropna(subset=["mean_delay_ms"])
    ax3.bar(valid["origin"], valid["mean_delay_ms"],
            color="#e67e22", alpha=0.8, width=0.7)
    ax3.set_xlabel("Origin satellite ID")
    ax3.set_ylabel("Mean delay (ms)")
    ax3.set_title("Mean time-to-full-duplication per satellite")
    ax3.grid(True, axis="y", alpha=0.3)

    # Bottom-right: scatter delay vs time of injection (are later objects faster/slower?)
    ax4 = fig.add_subplot(gs[1, 1])
    fully = objects[objects["fully_duped"]].copy()
    sc = ax4.scatter(fully["inject_time"], fully["delay_ms"],
                     c=fully["origin"], cmap="tab20", alpha=0.4, s=8)
    ax4.set_xlabel("Object injection time (s)")
    ax4.set_ylabel("Time to full duplication (ms)")
    ax4.set_title("Duplication delay vs injection time\n"
                  "(color = origin satellite)")
    ax4.grid(True, alpha=0.3)

    fig.suptitle(f"Per-Satellite Duplication Analysis — fill level {FILL_LEVEL}%",
                 fontsize=12)
    out = os.path.join(OUT_DIR, f"dup_per_satellite_{FILL_LEVEL}.png")
    plt.savefig(out, dpi=150, bbox_inches="tight")
    print(f"Saved {out}")
    plt.close()


# ── Summary CSV ───────────────────────────────────────────────────────────────
summary = {
    "fill_level":              FILL_LEVEL,
    "total_objects":           len(inject),
    "mode0_objects":           n_mode0,
    "deferred_objects":        n_deferred,
    "fully_duplicated":        n_fully_duped,
    "partially_duplicated":    n_partial,
    "never_duplicated":        n_never_duped,
    "full_dup_rate_pct":       100 * n_fully_duped / max(n_deferred, 1),
    "mean_delay_ms":           objects["delay_ms"].mean(),
    "median_delay_ms":         objects["delay_ms"].median(),
    "p95_delay_ms":            objects["delay_ms"].quantile(0.95),
    "max_delay_ms":            objects["delay_ms"].max(),
}
pd.DataFrame([summary]).to_csv(
    os.path.join(OUT_DIR, f"dup_summary_{FILL_LEVEL}.csv"),
    index=False, float_format="%.3f")
print(f"Saved {os.path.join(OUT_DIR, f'dup_summary_{FILL_LEVEL}.csv')}")

print("\nDone.")
