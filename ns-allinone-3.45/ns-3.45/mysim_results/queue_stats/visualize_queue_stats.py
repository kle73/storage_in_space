import matplotlib.pyplot as plt
import pandas as pd
import numpy as np
import os


def main():
    print("enter")
    df = pd.read_csv("experiment4/queue_statisticsS30.csv")
    timestamps = sorted(df["time"].unique())
    nodes = sorted(df["node"].unique())

    time_idx = {t: i for i, t in enumerate(timestamps)}
    node_idx = {n: i for i, n in enumerate(nodes)}
    # Build 2D array: shape (num_timestamps, 66)
    result = np.empty((len(timestamps), len(nodes)))

    # Map rows directly using vectorized index lookup
    t_indices = df["time"].map(time_idx).to_numpy()
    n_indices = df["node"].map(node_idx).to_numpy()
    result[t_indices, n_indices] = df["right"].to_numpy()
    # Transpose so rows=nodes (y-axis), cols=time (x-axis)
    fig, ax = plt.subplots(figsize=(14, 8))

    im = ax.imshow(
        result.T,
        aspect="auto",          # stretch to fill the axes
        origin="lower",         # node 0 at bottom
        interpolation="nearest",
        cmap="hot",          
    )

    cbar = fig.colorbar(im, ax=ax)
    cbar.set_label("up value")
    # Label x-axis ticks with actual timestamps (sample every N to avoid crowding)
    n_xticks = 10
    x_tick_pos = np.linspace(0, len(timestamps) - 1, n_xticks, dtype=int)
    ax.set_xticks(x_tick_pos)
    ax.set_xticklabels([timestamps[i] for i in x_tick_pos], rotation=45, ha="right")

    # Label y-axis with node IDs
    ax.set_yticks(range(len(nodes)))
    ax.set_yticklabels(nodes, fontsize=7)

    ax.set_xlabel("Time")
    ax.set_ylabel("Node")
    ax.set_title("Node Uptime Over Time")

    plt.tight_layout()
    plt.savefig("queueus.png")
    print("save")


# if __name__ == "__main__":
print("cwd: ", os.getcwd())
print("Writable:", os.access(".", os.W_OK))
main()
