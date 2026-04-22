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


TARGET_COLOR = "#00ff00"


# Load results / lsystem.json / lsystems description
# ==================================================

def load_results(output_folder: str) -> pd.DataFrame:
    df = pd.read_csv(os.path.join(output_folder, "results.csv"))
    df["solution_arr"] = df["solution"].apply(lambda s: np.fromstring(s[1:-1], sep=", ") if isinstance(s, str) and s else np.array([]))
    return df


def load_lsystem(output_folder: str):
    with open(os.path.join(output_folder, "lsystem.json")) as f:
        data = json.load(f)
    # Flattened: list[(group_id, [substrings])]
    groups_flat = [(g[0], list(g[1].keys())) for g in data["gene_groups"]]
    return groups_flat, data["symbols"], data["axioms"]


def load_description(path: str, name: str = None, depth: int = None):
    """Load the lsystem entry from lsystems*.json matching name/depth. None if no match"""
    with open(path) as f:
        data = json.load(f)
    candidates = data
    if name:
        candidates = [d for d in candidates if d.get("name") == name]
    if not candidates:
        return None
    if depth is not None:
        by_depth = [d for d in candidates if d.get("depth") == depth]
        if by_depth:
            candidates = by_depth
    return candidates[0]


def fill_blank_first_rows(df: pd.DataFrame) -> pd.DataFrame:
    """pygad emits an empty row at sol_idx=0 per generation. Fill with the previous gen's best"""
    df = df.sort_values(["generation", "sol_idx"]).reset_index(drop=True).copy()
    prev_best = None
    for gen in sorted(df["generation"].unique()):
        gen_mask = df["generation"] == gen
        non_blank = df[gen_mask & (df["solution_arr"].apply(len) > 0)]
        best_row = non_blank.loc[non_blank["fitness"].idxmax()] if len(non_blank) > 0 else None
        blank_first = df[gen_mask & (df["sol_idx"] == 0) & (df["solution_arr"].apply(len) == 0)]
        if prev_best is not None and len(blank_first) > 0:
            idx = blank_first.index[0]
            df.at[idx, "fitness"] = prev_best["fitness"]
            df.at[idx, "solution_arr"] = prev_best["solution_arr"]
            if "match" in df.columns:
                df.at[idx, "match"] = prev_best.get("match", 0)
        if best_row is not None:
            prev_best = best_row
    return df


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


# Plots
# =====

def plot_top3_fitness(df: pd.DataFrame, save_to: str, target_rules: dict = None,
                      groups_flat=None, symbols=None) -> pd.DataFrame:
    """Plot best, 2nd and 3rd best fitness per generation. Bright-green markers mark target-matching bests"""
    top3 = (df.sort_values(["generation", "fitness"], ascending=[True, False])
              .groupby("generation").head(3).reset_index(drop=True))
    top3["rank"] = top3.groupby("generation").cumcount()

    fig, ax = plt.subplots(figsize=(10, 5))
    for r, label, style in [(0, "best", "-"), (1, "2nd", "--"), (2, "3rd", ":")]:
        sub = top3[top3["rank"] == r].sort_values("generation")
        ax.plot(sub["generation"], sub["fitness"], style, label=label)

    if target_rules is not None and groups_flat is not None:
        hits = []
        best = top3[top3["rank"] == 0].sort_values("generation")
        for _, row in best.iterrows():
            if len(row["solution_arr"]) == 0:
                continue
            rules, _, _ = solution_to_rules(row["solution_arr"], groups_flat, symbols)
            if rules == target_rules:
                hits.append((row["generation"], row["fitness"]))
        if hits:
            gx, gy = zip(*hits)
            ax.scatter(gx, gy, color=TARGET_COLOR, s=60, zorder=5,
                       edgecolor="k", linewidth=0.5, label="target")

    ax.set_xlabel("generation")
    ax.set_ylabel("fitness")
    ax.legend()
    ax.grid(True)
    fig.tight_layout()
    fig.savefig(save_to, dpi=140)
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


# L-system string + renderer
# ==========================

def build_commands(angle: float = 45.0, inverse_pm: bool = False, turn_on_push_pop: bool = False):
    """Char -> (op, *params). F/G/A/B draw forward, 'b' moves without drawing, +/- turn by angle
    (swapped if inverse_pm), X is no-op. [/] push/pop position+angle unless turn_on_push_pop,
    in which case they turn left/right instead"""
    sign = -1.0 if inverse_pm else 1.0
    cmds = {
        "F": ("line",),
        "G": ("line",),
        "A": ("line",),
        "B": ("line",),
        "b": ("move",),
        "+": ("turn",  sign * angle),
        "-": ("turn", -sign * angle),
        "X": ("nop",),
    }
    if turn_on_push_pop:
        cmds["["] = ("turn", +angle)
        cmds["]"] = ("turn", -angle)
    else:
        cmds["["] = ("push",)
        cmds["]"] = ("pop",)
    return cmds


def expand_lsystem(rules: dict, axiom: str, depth: int) -> str:
    s = axiom
    for _ in range(depth):
        s = "".join(rules.get(ch, ch) for ch in s)
    return s


def choose_axiom(rules: dict):
    """LHS with longest RHS. None if no rules"""
    if not rules:
        return None
    return max(rules.keys(), key=lambda k: len(rules[k]))


def trace_lsystem(s: str, commands: dict, step: float = 1.0, start_angle: float = 90.0):
    """Walk the turtle without plotting. Returns list[((x0,y0),(x1,y1))]"""
    x, y, theta = 0.0, 0.0, start_angle
    stack = []
    segments = []
    for ch in s:
        cmd = commands.get(ch)
        if cmd is None:
            continue
        op = cmd[0]
        if op == "line":
            rad = np.deg2rad(theta)
            nx, ny = x + step*np.cos(rad), y + step*np.sin(rad)
            segments.append(((x, y), (nx, ny)))
            x, y = nx, ny
        elif op == "move":
            rad = np.deg2rad(theta)
            x += step*np.cos(rad); y += step*np.sin(rad)
        elif op == "turn":
            theta += cmd[1]
        elif op == "push":
            stack.append((x, y, theta))
        elif op == "pop":
            if stack:
                x, y, theta = stack.pop()
    return segments


def render_lsystem(s: str, ax, commands: dict, step: float = 1.0, start_angle: float = 90.0, line_color: str = "k") -> None:
    """Render an lsystem string onto an axis"""
    segments = trace_lsystem(s, commands=commands, step=step, start_angle=start_angle)
    for (x0, y0), (x1, y1) in segments:
        ax.plot([x0, x1], [y0, y1], color=line_color, linewidth=2)
    ax.set_aspect("equal")
    ax.axis("off")


def render_best_gif(df: pd.DataFrame, groups_flat, symbols, save_to: str, commands: dict,
                    depth: int = 5, fps: float = 3.0, base_color: str = "k",
                    target_rules: dict = None, axiom_override: str = None) -> None:
    """Render one frame per generation using that generation's best non-blank solution.
    Target-matching frames are drawn in bright green"""
    non_blank = df[df["solution_arr"].apply(len) > 0]
    best = (non_blank.sort_values(["generation", "fitness"], ascending=[True, False])
                    .groupby("generation").head(1).reset_index(drop=True))

    # First pass: trace every frame to compute a common bounding box so all rendered
    # frames share identical pixel dimensions (imageio requires uniform shape)
    traces = []
    xmin = ymin = +np.inf
    xmax = ymax = -np.inf
    for _, row in best.iterrows():
        rules, _, _ = solution_to_rules(row["solution_arr"], groups_flat, symbols)
        axiom = axiom_override if axiom_override else choose_axiom(rules)
        if axiom is None:
            traces.append(None)
            continue
        s = expand_lsystem(rules, axiom, depth)
        segs = trace_lsystem(s, commands=commands)
        is_target = (target_rules is not None) and (rules == target_rules)
        traces.append((row, axiom, segs, is_target, rules))
        pts = [p for seg in segs for p in seg] + [(0.0, 0.0)]
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
        row, axiom, segs, is_target, rules = entry

        # Per-frame zoom: small lsystems scaled up to SCALE_MAX, large ones kept at 1x
        SCALE_MAX = 6.0
        pts = [p for seg in segs for p in seg] + [(0.0, 0.0)]
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

        color = TARGET_COLOR if is_target else base_color
        fig, ax = plt.subplots(figsize=(8, 8))
        for (x0, y0), (x1, y1) in segs:
            ax.plot([x0, x1], [y0, y1], color=color, linewidth=1)
        ax.set_xlim(xlim); ax.set_ylim(ylim)
        ax.set_aspect("equal")
        ax.axis("off")
        tag = " [TARGET]" if is_target else ""
        ax.set_title(f"gen {int(row['generation'])}  fit {row['fitness']:.3f}  axiom={axiom}  match={row['match']}  {tag}")
        ax.text(xlim[0] + pad, ylim[1] - pad, "\n".join([f"{r[0]} -> {r[1]}" for r in rules.items()]))
        if target_rules is not None:
            ax.text(xlim[1] - pad, ylim[1] - pad, "\n".join([f"TGT: {r[0]} -> {r[1]}" for r in target_rules.items()]), ha="right")
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
    ap.add_argument("--lsystems-file", default="lsystems_small.json", help="Path to the lsystems description JSON")
    ap.add_argument("--lsystem-name", default=None, help="Name of the lsystem entry to match against")
    ap.add_argument("--base-color", default="k", help="Base line color for GIF rendering")
    ap.add_argument("--generations", type=int, default=None, help="Limit processing to the first N generations")
    ap.add_argument("--depth", type=int, default=5, help="L-system rewrite depth")
    ap.add_argument("--fps", type=float, default=3.0, help="GIF framerate")
    args = ap.parse_args()

    df = load_results(args.output_folder)
    groups_flat, symbols, _ = load_lsystem(args.output_folder)

    desc = load_description(args.lsystems_file, name=args.lsystem_name) if args.lsystem_name else None
    angle = float(desc["angle"]) if desc and "angle" in desc else 45.0
    inverse_pm = bool(desc.get("inverse_pm", False)) if desc else False
    turn_on_push_pop = bool(desc.get("turn_on_push_pop", False)) if desc else False
    commands = build_commands(angle=angle, inverse_pm=inverse_pm, turn_on_push_pop=turn_on_push_pop)
    target_rules = desc.get("rules") if desc else None
    axiom_override = desc.get("axiom") if desc else None

    df = fill_blank_first_rows(df)
    if args.generations is not None:
        df = df[df["generation"] <= args.generations].reset_index(drop=True)

    top3 = plot_top3_fitness(df, os.path.join(args.output_folder, "fitness_top3.png"),
                             target_rules=target_rules, groups_flat=groups_flat, symbols=symbols)
    plot_top3_genomes(top3, os.path.join(args.output_folder, "genomes_top3.png"))
    print("Rendering lsystem for each generation...")
    render_best_gif(df, groups_flat, symbols, os.path.join(args.output_folder, "best_per_gen.gif"),
                    commands=commands, depth=args.depth, fps=args.fps,
                    base_color=args.base_color, target_rules=target_rules, axiom_override=axiom_override)


if __name__ == "__main__":
    main()
