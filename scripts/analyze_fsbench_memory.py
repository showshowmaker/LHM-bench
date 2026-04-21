#!/usr/bin/env python3

import argparse
import csv
import glob
import math
import os
from collections import defaultdict

try:
    import matplotlib.pyplot as plt
except ImportError as exc:
    raise SystemExit("matplotlib is required: python3 -m pip install matplotlib") from exc


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(SCRIPT_DIR)
DEFAULT_RESULTS_DIR = os.path.join(PROJECT_ROOT, "results")
DEFAULT_OUTPUT_DIR = os.path.join(DEFAULT_RESULTS_DIR, "fsbench_memory_analysis")


def parse_args():
    parser = argparse.ArgumentParser(
        description="Analyze fsbench_memory CSV output and generate summary plots."
    )
    parser.add_argument(
        "--csv",
        action="append",
        default=[],
        help="Input CSV file. Can be repeated. Default: <project-root>/results/fsbench_memory.csv",
    )
    parser.add_argument(
        "--glob",
        dest="csv_glob",
        default="",
        help="Glob pattern for input CSV files, for example 'results/fsbench_memory_*.csv'",
    )
    parser.add_argument(
        "--output-dir",
        default=DEFAULT_OUTPUT_DIR,
        help="Directory for generated summaries and figures",
    )
    parser.add_argument(
        "--trend-phase",
        default="warm",
        choices=["baseline", "populate", "warm"],
        help="Which phase to use for cross-run trend figures",
    )
    return parser.parse_args()


def resolve_inputs(args):
    paths = list(args.csv)
    if args.csv_glob:
        paths.extend(sorted(glob.glob(args.csv_glob)))
    if not paths:
        paths = [os.path.join(DEFAULT_RESULTS_DIR, "fsbench_memory.csv")]
    paths = [os.path.abspath(path) for path in paths]
    unique = []
    seen = set()
    for path in paths:
        if path not in seen:
            unique.append(path)
            seen.add(path)
    return unique


def read_rows(paths):
    rows = []
    for path in paths:
        with open(path, "r", newline="", encoding="utf-8") as f:
            reader = csv.DictReader(f)
            for row in reader:
                parsed = {
                    "source_csv": path,
                    "backend": row["backend"],
                    "file_count": int(row["file_count"]),
                    "depth": int(row["depth"]),
                    "siblings_per_dir": int(row["siblings_per_dir"]),
                    "files_per_leaf": int(row["files_per_leaf"]),
                    "phase": row["phase"],
                    "total_meta_bytes": int(row["total_meta_bytes"]),
                    "bytes_per_file": int(row["bytes_per_file"]),
                    "slab_dentry_bytes": int(row["slab_dentry_bytes"]),
                    "slab_inode_bytes": int(row["slab_inode_bytes"]),
                    "slab_ext4_inode_bytes": int(row["slab_ext4_inode_bytes"]),
                    "lhm_index_bytes": int(row["lhm_index_bytes"]),
                    "lhm_inode_bytes": int(row["lhm_inode_bytes"]),
                    "lhm_string_bytes": int(row["lhm_string_bytes"]),
                    "process_rss_bytes": int(row["process_rss_bytes"]),
                }
                rows.append(parsed)
    return rows


def ensure_dir(path):
    os.makedirs(path, exist_ok=True)


def config_key(row):
    return (
        row["file_count"],
        row["depth"],
        row["siblings_per_dir"],
        row["files_per_leaf"],
    )


def config_label(key):
    file_count, depth, siblings, files = key
    return f"files={file_count}, depth={depth}, siblings={siblings}, leaf_files={files}"


def write_combined_csv(path, rows):
    if not rows:
        return
    fieldnames = list(rows[0].keys())
    with open(path, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def write_summary_csv(path, rows):
    fieldnames = [
        "backend",
        "phase",
        "file_count",
        "depth",
        "siblings_per_dir",
        "files_per_leaf",
        "total_meta_bytes",
        "bytes_per_file",
        "slab_dentry_bytes",
        "slab_inode_bytes",
        "slab_ext4_inode_bytes",
        "lhm_index_bytes",
        "lhm_inode_bytes",
        "lhm_string_bytes",
        "process_rss_bytes",
    ]
    with open(path, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def plot_phase_bars(rows, output_path):
    configs = sorted({config_key(row) for row in rows})
    phases = ["baseline", "populate", "warm"]
    backends = sorted({row["backend"] for row in rows})

    fig, axes = plt.subplots(len(configs), 2, figsize=(12, max(4.5, 4.0 * len(configs))))
    if len(configs) == 1:
        axes = [axes]

    for idx, key in enumerate(configs):
        cfg_rows = [row for row in rows if config_key(row) == key]
        bytes_axis = axes[idx][0]
        total_axis = axes[idx][1]

        x = list(range(len(phases)))
        width = 0.35 if len(backends) == 2 else 0.25
        for backend_index, backend in enumerate(backends):
            backend_rows = {row["phase"]: row for row in cfg_rows if row["backend"] == backend}
            shift = (backend_index - (len(backends) - 1) / 2.0) * width
            bytes_axis.bar(
                [value + shift for value in x],
                [backend_rows.get(phase, {}).get("bytes_per_file", 0) for phase in phases],
                width=width,
                label=backend,
            )
            total_axis.bar(
                [value + shift for value in x],
                [backend_rows.get(phase, {}).get("total_meta_bytes", 0) for phase in phases],
                width=width,
                label=backend,
            )

        bytes_axis.set_xticks(x)
        bytes_axis.set_xticklabels(phases)
        bytes_axis.set_ylabel("Bytes / File")
        bytes_axis.set_title(f"Per-File Metadata Memory\n{config_label(key)}")
        bytes_axis.grid(axis="y", alpha=0.25)
        bytes_axis.legend()

        total_axis.set_xticks(x)
        total_axis.set_xticklabels(phases)
        total_axis.set_ylabel("Total Metadata Bytes")
        total_axis.set_title(f"Total Metadata Memory\n{config_label(key)}")
        total_axis.grid(axis="y", alpha=0.25)
        total_axis.legend()

    plt.tight_layout()
    plt.savefig(output_path, dpi=220)
    plt.close(fig)


def choose_trend_axis(keys):
    metrics = [
        ("file_count", 0, "File Count"),
        ("depth", 1, "Depth"),
        ("siblings_per_dir", 2, "Siblings / Directory"),
        ("files_per_leaf", 3, "Files / Leaf Directory"),
    ]
    for name, idx, label in metrics:
        values = {key[idx] for key in keys}
        if len(values) > 1:
            return idx, label
    return None, None


def plot_trends(rows, phase, output_prefix):
    phase_rows = [row for row in rows if row["phase"] == phase]
    if not phase_rows:
        return

    keys = sorted({config_key(row) for row in phase_rows})
    axis_index, axis_label = choose_trend_axis(keys)
    if axis_index is None:
        return

    backend_rows = defaultdict(list)
    for row in phase_rows:
        backend_rows[row["backend"]].append(row)

    fig1, ax1 = plt.subplots(figsize=(9, 5.5))
    fig2, ax2 = plt.subplots(figsize=(9, 5.5))

    for backend, bucket in sorted(backend_rows.items()):
        bucket.sort(key=lambda row: config_key(row)[axis_index])
        x = [config_key(row)[axis_index] for row in bucket]
        y_total = [row["total_meta_bytes"] for row in bucket]
        y_per_file = [row["bytes_per_file"] for row in bucket]

        ax1.plot(x, y_total, marker="o", linewidth=2, label=backend)
        ax2.plot(x, y_per_file, marker="o", linewidth=2, label=backend)

    ax1.set_xlabel(axis_label)
    ax1.set_ylabel("Total Metadata Bytes")
    ax1.set_title(f"Total Metadata Memory Trend ({phase})")
    ax1.grid(alpha=0.25)
    ax1.legend()
    fig1.tight_layout()
    fig1.savefig(output_prefix + "_total_bytes.png", dpi=220)
    plt.close(fig1)

    ax2.set_xlabel(axis_label)
    ax2.set_ylabel("Bytes / File")
    ax2.set_title(f"Per-File Metadata Memory Trend ({phase})")
    ax2.grid(alpha=0.25)
    ax2.legend()
    fig2.tight_layout()
    fig2.savefig(output_prefix + "_bytes_per_file.png", dpi=220)
    plt.close(fig2)


def plot_ext4_breakdown(rows, output_path):
    ext4_rows = [row for row in rows if row["backend"] == "ext4"]
    if not ext4_rows:
        return

    configs = sorted({config_key(row) for row in ext4_rows})
    phases = ["baseline", "populate", "warm"]
    fig, axes = plt.subplots(len(configs), 1, figsize=(11, max(4.0, 3.8 * len(configs))))
    if len(configs) == 1:
        axes = [axes]

    for idx, key in enumerate(configs):
        cfg_rows = {row["phase"]: row for row in ext4_rows if config_key(row) == key}
        dentry = [cfg_rows.get(phase, {}).get("slab_dentry_bytes", 0) for phase in phases]
        inode = [cfg_rows.get(phase, {}).get("slab_inode_bytes", 0) for phase in phases]
        ext4_inode = [cfg_rows.get(phase, {}).get("slab_ext4_inode_bytes", 0) for phase in phases]

        ax = axes[idx]
        ax.bar(phases, dentry, label="dentry")
        ax.bar(phases, inode, bottom=dentry, label="inode_cache")
        ax.bar(
            phases,
            ext4_inode,
            bottom=[a + b for a, b in zip(dentry, inode)],
            label="ext4_inode_cache",
        )
        ax.set_ylabel("Bytes")
        ax.set_title(f"Ext4 Slab Breakdown\n{config_label(key)}")
        ax.grid(axis="y", alpha=0.25)
        ax.legend()

    plt.tight_layout()
    plt.savefig(output_path, dpi=220)
    plt.close(fig)


def print_console_summary(rows):
    print("backend\tphase\tfile_count\tdepth\tbytes_per_file\ttotal_meta_bytes")
    for row in rows:
        print(
            f"{row['backend']}\t{row['phase']}\t{row['file_count']}\t{row['depth']}\t"
            f"{row['bytes_per_file']}\t{row['total_meta_bytes']}"
        )


def main():
    args = parse_args()
    input_paths = resolve_inputs(args)
    for path in input_paths:
        if not os.path.exists(path):
            raise SystemExit(f"missing csv: {path}")

    rows = read_rows(input_paths)
    if not rows:
        raise SystemExit("no rows loaded from input CSV files")

    rows.sort(key=lambda row: (row["backend"], row["phase"], row["file_count"], row["depth"]))

    ensure_dir(args.output_dir)
    write_combined_csv(os.path.join(args.output_dir, "fsbench_memory_combined.csv"), rows)
    write_summary_csv(os.path.join(args.output_dir, "fsbench_memory_summary.csv"), rows)

    plot_phase_bars(rows, os.path.join(args.output_dir, "fsbench_memory_by_phase.png"))
    plot_trends(
        rows,
        args.trend_phase,
        os.path.join(args.output_dir, f"fsbench_memory_{args.trend_phase}_trend"),
    )
    plot_ext4_breakdown(rows, os.path.join(args.output_dir, "fsbench_memory_ext4_slab_breakdown.png"))

    print_console_summary(rows)
    print(f"analysis outputs written to: {args.output_dir}")


if __name__ == "__main__":
    main()
