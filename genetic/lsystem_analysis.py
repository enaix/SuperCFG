"""Analyze SuperGGD lsystem run: fitness curves, top-3 genome heatmaps, best-per-generation L-system GIF"""
import os
import json
import ast
import argparse
from io import BytesIO

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import imageio.v2 as imageio
from tqdm import tqdm


# Load results / lsystem.json
# ===========================

def load_results(output_folder: str) -> pd.DataFrame:
    df = pd.read_csv(os.path.join(output_folder, "results.csv"))
    df["solution_arr"] = df["solution"].apply(lambda s: np.array(ast.literal_eval(s)) if isinstance(s, str) and s else np.array([]))
    return df


def load_lsystem(output_folder: str):
    with open(os.path.join(output_folder, "lsystem.json")) as f:
        data = json.load(f)
    # Flattened: list[(group_id, [substrings])]
    groups_flat = [(g[0], list(g[1].keys())) for g in data["gene_groups"]]
    return groups_flat, data["symbols"], data["axioms"]


# Plots
# =====

def plot_top3_fitness(df: pd.DataFrame, save_to: str) -> pd.DataFrame:
    """Plot best, 2nd and 3rd best fitness per generation. Returns the top-3 frame"""
    top3 = (df.sort_values(["generation", "fitness"], ascending=[True, False])
              .groupby("generation").head(3).reset_index(drop=True))
    top3["rank"] = top3.groupby("generation").cumcount()

    fig, ax = plt.subplots(figsize=(10, 5))
    for r, label, style in [(0, "best", "-"), (1, "2nd", "--"), (2, "3rd", ":")]:
        sub = top3[top3["rank"] == r].sort_values("generation")
        ax.plot(sub["generation"], sub["fitness"], style, label=label)
    ax.set_xlabel("generation")
    ax.set_ylabel("fitness")
    ax.legend()
    ax.grid(True)
    fig.tight_layout()
    fig.savefig(save_to, dpi=100)
    plt.close(fig)
    return top3


def plot_top3_genomes(top3: pd.DataFrame, save_to: str) -> None:
    """One heatmap per rank (3 stacked): rows = gene index, cols = generation"""
    # Drop empty / mismatched-length gene vectors (e.g. failed generations), keep the modal length
    lengths = top3["solution_arr"].apply(len)
    modal_len = lengths[lengths > 0].mode().iloc[0] if (lengths > 0).any() else 0

    fig, axes = plt.subplots(3, 1, figsize=(10, 9), sharex=True)
    for r, ax in enumerate(axes):
        sub = top3[(top3["rank"] == r) & (top3["solution_arr"].apply(len) == modal_len)].sort_values("generation")
        if len(sub) == 0 or modal_len == 0:
            continue
        arrs = np.stack(sub["solution_arr"].values)
        gens = sub["generation"].values
        im = ax.imshow(arrs.T, aspect="auto", origin="lower",
                       extent=[gens.min(), gens.max(), 0, arrs.shape[1]])
        ax.set_ylabel(f"rank {r} gene")
        fig.colorbar(im, ax=ax)
    axes[-1].set_xlabel("generation")
    fig.tight_layout()
    fig.savefig(save_to, dpi=100)
    plt.close(fig)


# Genome -> rules (copied from superggd/examples/lsystem/__init__.py)
# ==================================================================

def gene_to_substr(groups_flat, g0, g1):
    group = groups_flat[int(g0)][1]
    return group[int(g1) % len(group)]


def solution_to_rules(solution, groups_flat, symbols):
    """Gene layout: [g0_0, g1_0, ..., g0_{n-1}, g1_{n-1}, sym_0, ..., sym_{n-1}, axiom]"""
    solution = list(map(int, solution))
    n_rules = (len(solution) - 1) // 3
    rhs, lhs = [], []
    for i in range(n_rules):
        rhs.append(gene_to_substr(groups_flat, solution[2*i], solution[2*i + 1]))
    for i in range(n_rules):
        val = solution[2*n_rules + i]
        lhs.append("" if val >= len(symbols) else symbols[val])
    rules = {}
    for l, r in zip(lhs, rhs):
        if l != "" and l not in rules:
            rules[l] = r  # first wins on duplicates
    return rules, lhs, rhs


# L-system string + renderer
# ==========================

def expand_lsystem(rules: dict, axiom: str, depth: int) -> str:
    s = axiom
    for _ in range(depth):
        s = "".join(rules.get(ch, ch) for ch in s)
    return s


def choose_axiom(rules: dict):
    """'0' preferred, else LHS with longest RHS. None if no rules"""
    if not rules:
        return None
    if "0" in rules:
        return "0"
    return max(rules.keys(), key=lambda k: len(rules[k]))


DEFAULT_COMMANDS = {
    "0": ("line_leaf",),
    "1": ("line",),
    "[": ("push_turn", +45),
    "]": ("pop_turn", -45),
}


def trace_lsystem(s: str, commands=None, step: float = 1.0, start_angle: float = 90.0):
    """Walk the turtle without plotting. Returns (segments, leaves) where segments is list[((x0,y0),(x1,y1))] and leaves is list[(x,y)]"""
    cmds = DEFAULT_COMMANDS if commands is None else commands
    x, y, theta = 0.0, 0.0, start_angle
    stack = []
    segments, leaves = [], []
    for ch in s:
        cmd = cmds.get(ch)
        if cmd is None:
            continue
        op = cmd[0]
        if op in ("line", "line_leaf"):
            rad = np.deg2rad(theta)
            nx, ny = x + step*np.cos(rad), y + step*np.sin(rad)
            segments.append(((x, y), (nx, ny)))
            if op == "line_leaf":
                leaves.append((nx, ny))
            x, y = nx, ny
        elif op == "push_turn":
            stack.append((x, y, theta))
            theta += cmd[1]
        elif op == "pop_turn":
            if stack:
                x, y, theta = stack.pop()
            theta += cmd[1]
    return segments, leaves


def render_lsystem(s: str, ax, commands=None, step: float = 1.0, start_angle: float = 90.0, leaf_color: str = "g", line_color: str = "k") -> None:
    """Render an lsystem string. ``commands`` maps char -> (op, *params), see DEFAULT_COMMANDS"""
    segments, leaves = trace_lsystem(s, commands=commands, step=step, start_angle=start_angle)
    for (x0, y0), (x1, y1) in segments:
        ax.plot([x0, x1], [y0, y1], color=line_color, linewidth=2)
    if leaf_color is None:
        for (lx, ly) in leaves:
            ax.plot(lx, ly, marker="o", color=leaf_color, markersize=1)
    ax.set_aspect("equal")
    ax.axis("off")


def render_best_gif(df: pd.DataFrame, groups_flat, symbols, save_to: str, depth: int = 5, fps: float = 3.0, commands=None, draw_leaves: bool = True) -> None:
    """Render one frame per generation using that generation's best solution"""
    best = (df.sort_values(["generation", "fitness"], ascending=[True, False])
              .groupby("generation").head(1).reset_index(drop=True))

    # First pass: trace every frame to compute a common bounding box so all rendered
    # frames share identical pixel dimensions (imageio requires uniform shape)
    traces = []
    xmin = ymin = +np.inf
    xmax = ymax = -np.inf
    for _, row in best.iterrows():
        rules, _, _ = solution_to_rules(row["solution_arr"], groups_flat, symbols)
        axiom = choose_axiom(rules)
        if axiom is None:
            traces.append(None)
            continue
        s = expand_lsystem(rules, axiom, depth)
        segs, leaves = trace_lsystem(s, commands=commands)
        traces.append((row, axiom, segs, leaves))
        pts = [p for seg in segs for p in seg] + list(leaves) + [(0.0, 0.0)]
        xs = [p[0] for p in pts]; ys = [p[1] for p in pts]
        xmin = min(xmin, min(xs)); xmax = max(xmax, max(xs))
        ymin = min(ymin, min(ys)); ymax = max(ymax, max(ys))

    if not np.isfinite(xmin):
        print("No renderable frames")
        return

    global_extent = max(xmax - xmin, ymax - ymin, 1.0)

    frames = []
    for entry in tqdm(traces):
        if entry is None:
            continue
        row, axiom, segs, leaves = entry

        # Per-frame zoom: small lsystems scaled up to SCALE_MAX, large ones kept at 1x
        SCALE_MAX = 6.0
        pts = [p for seg in segs for p in seg] + list(leaves) + [(0.0, 0.0)]
        xs = [p[0] for p in pts]; ys = [p[1] for p in pts]
        fx0, fx1 = min(xs), max(xs); fy0, fy1 = min(ys), max(ys)
        frame_extent = max(fx1 - fx0, fy1 - fy0, 1e-6)
        scale = 1.0 + (SCALE_MAX - 1.0) * (1.0 - frame_extent / global_extent)
        scale = max(1.0, min(SCALE_MAX, scale))

        window = global_extent / scale
        pad = 0.05 * window
        cx = 0.5 * (fx0 + fx1); cy = 0.5 * (fy0 + fy1)
        xlim = (cx - window/2 - pad, cx + window/2 + pad)
        ylim = (cy - window/2 - pad, cy + window/2 + pad)

        fig, ax = plt.subplots(figsize=(8, 8))
        for (x0, y0), (x1, y1) in segs:
            ax.plot([x0, x1], [y0, y1], color="k", linewidth=1)
        if draw_leaves:
            for (lx, ly) in leaves:
                ax.plot(lx, ly, marker="o", color="g", markersize=2)
        ax.set_xlim(xlim); ax.set_ylim(ylim)
        ax.set_aspect("equal")
        ax.axis("off")
        ax.set_title(f"gen {int(row['generation'])}  fit {row['fitness']:.3f}  axiom={axiom}  match={row['match']}  scale={scale:.2f}x")
        buf = BytesIO()
        fig.savefig(buf, format="png", dpi=150)
        plt.close(fig)
        buf.seek(0)
        frames.append(imageio.imread(buf))

    imageio.mimsave(save_to, frames, duration=1.0/fps)
    print(f"Saved {len(frames)} frames to {save_to}")


# Entry point
# ===========

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("output_folder", help="Path to the SuperGGD run output folder")
    ap.add_argument("--depth", type=int, default=5, help="L-system rewrite depth")
    ap.add_argument("--fps", type=float, default=3.0, help="GIF framerate")
    args = ap.parse_args()

    df = load_results(args.output_folder)
    groups_flat, symbols, _ = load_lsystem(args.output_folder)

    top3 = plot_top3_fitness(df, os.path.join(args.output_folder, "fitness_top3.png"))
    plot_top3_genomes(top3, os.path.join(args.output_folder, "genomes_top3.png"))
    print("Rendering lsystem for each generation...")
    render_best_gif(df, groups_flat, symbols, os.path.join(args.output_folder, "best_per_gen.gif"), depth=args.depth, fps=args.fps)


if __name__ == "__main__":
    main()
