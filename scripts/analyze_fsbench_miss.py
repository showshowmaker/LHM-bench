#!/usr/bin/env python3

import argparse
import csv
import glob
import os
from collections import defaultdict

try:
    import matplotlib.pyplot as plt
except ImportError as exc:
    raise SystemExit("matplotlib is required: python3 -m pip install matplotlib") from exc


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(SCRIPT_DIR)
DEFAULT_RESULTS_DIR = os.path.join(PROJECT_ROOT, "results")
DEFAULT_OUTPUT_DIR = os.path.join(DEFAULT_RESULTS_DIR, "fsbench_miss_analysis")


def parse_args():
    parser = argparse.ArgumentParser(
        description="Analyze fsbench_miss CSV output and generate latency figures."
    )
    parser.add_argument("--csv", action="append", default=[], help="Input CSV file, can be repeated")
    parser.add_argument("--glob", dest="csv_glob", default="", help="Glob pattern for input CSV files")
    parser.add_argument("--output-dir", default=DEFAULT_OUTPUT_DIR, help="Directory for analysis outputs")
    return parser.parse_args()


def resolve_inputs(args):
    paths = list(args.csv)
    if args.csv_glob:
        paths.extend(sorted(glob.glob(args.csv_glob)))
    if not paths:
        paths = [os.path.join(DEFAULT_RESULTS_DIR, "fsbench_miss.csv")]
    unique = []
    seen = set()
    for path in paths:
        path = os.path.abspath(path)
        if path not in seen:
            seen.add(path)
            unique.append(path)
    return unique


def read_rows(paths):
    rows = []
    for path in paths:
        with open(path, "r", newline="", encoding="utf-8") as f:
            reader = csv.DictReader(f)
            for row in reader:
                rows.append(
                    {
                        "source_csv": path,
                        "backend": row["backend"],
                        "mode": row["mode"],
                        "op": row["op"],
                        "query_kind": row["query_kind"],
                        "query_count": int(row["query_count"]),
                        "file_count": int(row["file_count"]),
                        "depth": int(row["depth"]),
                        "siblings_per_dir": int(row["siblings_per_dir"]),
                        "files_per_leaf": int(row["files_per_leaf"]),
                        "avg_ns": float(row["avg_ns"]),
                        "p50_ns": float(row["p50_ns"]),
                        "p95_ns": float(row["p95_ns"]),
                        "p99_ns": float(row["p99_ns"]),
                        "avg_bytes": float(row["avg_bytes"]),
                        "success_rate": float(row["success_rate"]),
                    }
                )
    return rows


def ensure_dir(path):
    os.makedirs(path, exist_ok=True)


def write_summary(path, rows):
    if not rows:
        return
    with open(path, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)


def choose_axis(rows):
    candidates = [
        ("file_count", "File Count"),
        ("depth", "Depth"),
        ("siblings_per_dir", "Siblings / Directory"),
        ("files_per_leaf", "Files / Leaf Directory"),
    ]
    for key, label in candidates:
        values = {row[key] for row in rows}
        if len(values) > 1:
            return key, label
    return "depth", "Depth"


def plot_by_mode_and_backend(rows, output_prefix):
    groups = defaultdict(list)
    for row in rows:
        groups[(row["op"], row["query_kind"])].append(row)

    for (op, query_kind), bucket in groups.items():
        axis_key, axis_label = choose_axis(bucket)

        fig, axes = plt.subplots(1, 2, figsize=(12, 5))
        for backend in sorted({row["backend"] for row in bucket}):
            for mode in sorted({row["mode"] for row in bucket}):
                subset = [row for row in bucket if row["backend"] == backend and row["mode"] == mode]
                if not subset:
                    continue
                subset.sort(key=lambda row: row[axis_key])
                x = [row[axis_key] for row in subset]
                label = f"{backend}-{mode}"
                axes[0].plot(x, [row["p50_ns"] for row in subset], marker="o", linewidth=2, label=label)
                axes[0].plot(x, [row["p99_ns"] for row in subset], marker="s", linewidth=2, linestyle="--", label=f"{label}-p99")
                axes[1].plot(x, [row["success_rate"] for row in subset], marker="o", linewidth=2, label=label)

        axes[0].set_xlabel(axis_label)
        axes[0].set_ylabel("Latency (ns)")
        axes[0].set_title(f"{op} / {query_kind}: p50 and p99")
        axes[0].grid(alpha=0.25)
        axes[0].legend(fontsize=8)

        axes[1].set_xlabel(axis_label)
        axes[1].set_ylabel("Success Rate")
        axes[1].set_title(f"{op} / {query_kind}: success rate")
        axes[1].set_ylim(0.0, 1.05)
        axes[1].grid(alpha=0.25)
        axes[1].legend(fontsize=8)

        plt.tight_layout()
        filename = f"{output_prefix}_{op}_{query_kind}.png"
        plt.savefig(filename, dpi=220)
        plt.close(fig)


def print_summary(rows):
    print("backend\tmode\top\tquery_kind\tavg_ns\tp50_ns\tp99_ns\tsuccess_rate")
    for row in rows:
        print(
            f"{row['backend']}\t{row['mode']}\t{row['op']}\t{row['query_kind']}\t"
            f"{row['avg_ns']:.2f}\t{row['p50_ns']:.2f}\t{row['p99_ns']:.2f}\t{row['success_rate']:.4f}"
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

    rows.sort(key=lambda row: (row["op"], row["query_kind"], row["backend"], row["mode"], row["file_count"], row["depth"]))
    ensure_dir(args.output_dir)
    write_summary(os.path.join(args.output_dir, "fsbench_miss_summary.csv"), rows)
    plot_by_mode_and_backend(rows, os.path.join(args.output_dir, "fsbench_miss"))
    print_summary(rows)
    print(f"analysis outputs written to: {args.output_dir}")


if __name__ == "__main__":
    main()
