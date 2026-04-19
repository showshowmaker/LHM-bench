#!/usr/bin/env python3

import argparse
import csv
import math
import os
import sys
from collections import defaultdict

try:
    import matplotlib.pyplot as plt
except ImportError as exc:
    print("matplotlib is required: pip install matplotlib", file=sys.stderr)
    raise


NUMERIC_FIELDS = [
    "avg_ns",
    "p50_ns",
    "p95_ns",
    "p99_ns",
    "throughput_qps",
    "avg_depth",
    "avg_component_steps",
    "avg_index_steps",
    "avg_steps",
]

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(SCRIPT_DIR)
DEFAULT_OUTPUT_DIR = os.path.join(PROJECT_ROOT, "results")


def parse_args():
    parser = argparse.ArgumentParser(
        description="Analyze nsbench depth-sweep CSV results and generate figures."
    )
    parser.add_argument(
        "--suite-dir",
        default=os.path.join(".", "root", "VSIterate"),
        help="Directory containing masstree_depth_sweep.csv and rocksdb_depth_sweep.csv",
    )
    parser.add_argument(
        "--query-kind",
        default="positive",
        help="Filter query_kind before aggregating, default: positive",
    )
    parser.add_argument(
        "--output-dir",
        default="",
        help="Directory for generated summaries and figures, default: <project-root>/results",
    )
    parser.add_argument(
        "--show",
        action="store_true",
        help="Show figures interactively after saving",
    )
    return parser.parse_args()


def ensure_dir(path):
    os.makedirs(path, exist_ok=True)


def read_rows(csv_path, query_kind):
    rows = []
    with open(csv_path, "r", newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            if query_kind and row.get("query_kind") != query_kind:
                continue
            parsed = {
                "backend": row.get("backend", ""),
                "dataset_name": row.get("dataset_name", ""),
                "manifest_path": row.get("manifest_path", ""),
                "depth": int(float(row["depth"])),
                "query_kind": row.get("query_kind", ""),
            }
            for field in NUMERIC_FIELDS:
                value = row.get(field, "")
                parsed[field] = float(value) if value != "" else math.nan
            rows.append(parsed)
    return rows


def aggregate_by_depth(rows):
    grouped = defaultdict(list)
    for row in rows:
        grouped[row["depth"]].append(row)

    summary = []
    for depth in sorted(grouped.keys()):
        bucket = grouped[depth]
        item = {"depth": depth, "samples": len(bucket)}
        for field in NUMERIC_FIELDS:
            values = [r[field] for r in bucket if not math.isnan(r[field])]
            item[field] = sum(values) / len(values) if values else math.nan
        summary.append(item)
    return summary


def write_summary_csv(path, backend, query_kind, summary):
    fieldnames = ["backend", "query_kind", "depth", "samples"] + NUMERIC_FIELDS
    with open(path, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        for row in summary:
            out = {
                "backend": backend,
                "query_kind": query_kind,
                "depth": row["depth"],
                "samples": row["samples"],
            }
            for field in NUMERIC_FIELDS:
                out[field] = row[field]
            writer.writerow(out)


def fit_slope(summary, field):
    xs = [row["depth"] for row in summary]
    ys = [row[field] for row in summary if not math.isnan(row[field])]
    xs = [row["depth"] for row in summary if not math.isnan(row[field])]
    if len(xs) < 2:
        return math.nan, math.nan
    x_mean = sum(xs) / len(xs)
    y_mean = sum(ys) / len(ys)
    denom = sum((x - x_mean) ** 2 for x in xs)
    if denom == 0:
        return math.nan, math.nan
    slope = sum((x - x_mean) * (y - y_mean) for x, y in zip(xs, ys)) / denom
    intercept = y_mean - slope * x_mean
    return slope, intercept


def write_slope_csv(path, rows):
    fieldnames = ["backend", "query_kind", "metric", "slope_per_depth", "intercept"]
    with open(path, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        for row in rows:
            writer.writerow(row)


def plot_latency(output_path, query_kind, rocks, lhm):
    plt.figure(figsize=(9, 5.5))
    plt.plot(
        [r["depth"] for r in rocks],
        [r["p50_ns"] for r in rocks],
        marker="o",
        linewidth=2,
        label="Iterative p50",
    )
    plt.plot(
        [r["depth"] for r in lhm],
        [r["p50_ns"] for r in lhm],
        marker="o",
        linewidth=2,
        label="LHM p50",
    )
    plt.plot(
        [r["depth"] for r in rocks],
        [r["p99_ns"] for r in rocks],
        marker="s",
        linewidth=2,
        label="Iterative p99",
    )
    plt.plot(
        [r["depth"] for r in lhm],
        [r["p99_ns"] for r in lhm],
        marker="s",
        linewidth=2,
        label="LHM p99",
    )
    plt.xlabel("Depth")
    plt.ylabel("Latency (ns)")
    plt.title(f"Path Resolution Latency vs Depth ({query_kind})")
    plt.grid(alpha=0.25)
    plt.legend()
    plt.tight_layout()
    plt.savefig(output_path, dpi=220)
    plt.close()


def plot_steps(output_path, query_kind, rocks, lhm):
    fig, axes = plt.subplots(1, 2, figsize=(12, 4.8))

    axes[0].plot(
        [r["depth"] for r in rocks],
        [r["avg_component_steps"] for r in rocks],
        marker="o",
        linewidth=2,
        label="Iterative",
    )
    axes[0].plot(
        [r["depth"] for r in lhm],
        [r["avg_component_steps"] for r in lhm],
        marker="o",
        linewidth=2,
        label="LHM",
    )
    axes[0].set_xlabel("Depth")
    axes[0].set_ylabel("Average Component Steps")
    axes[0].set_title(f"Component Steps ({query_kind})")
    axes[0].grid(alpha=0.25)
    axes[0].legend()

    axes[1].plot(
        [r["depth"] for r in rocks],
        [r["avg_index_steps"] for r in rocks],
        marker="o",
        linewidth=2,
        label="Iterative",
    )
    axes[1].plot(
        [r["depth"] for r in lhm],
        [r["avg_index_steps"] for r in lhm],
        marker="o",
        linewidth=2,
        label="LHM",
    )
    axes[1].set_xlabel("Depth")
    axes[1].set_ylabel("Average Index Steps")
    axes[1].set_title(f"Index Steps ({query_kind})")
    axes[1].grid(alpha=0.25)
    axes[1].legend()

    plt.tight_layout()
    plt.savefig(output_path, dpi=220)
    plt.close(fig)


def plot_avg_latency(output_path, query_kind, rocks, lhm):
    plt.figure(figsize=(9, 5.5))
    plt.plot(
        [r["depth"] for r in rocks],
        [r["avg_ns"] for r in rocks],
        marker="o",
        linewidth=2,
        label="Iterative avg",
    )
    plt.plot(
        [r["depth"] for r in lhm],
        [r["avg_ns"] for r in lhm],
        marker="o",
        linewidth=2,
        label="LHM avg",
    )
    plt.xlabel("Depth")
    plt.ylabel("Average Latency (ns)")
    plt.title(f"Average Path Resolution Latency vs Depth ({query_kind})")
    plt.grid(alpha=0.25)
    plt.legend()
    plt.tight_layout()
    plt.savefig(output_path, dpi=220)
    plt.close()


def print_summary(label, summary):
    print(f"=== {label} ===")
    print("depth\tavg_ns\tp50_ns\tp99_ns\tavg_component_steps\tavg_index_steps")
    for row in summary:
        print(
            f"{row['depth']}\t"
            f"{row['avg_ns']:.2f}\t"
            f"{row['p50_ns']:.2f}\t"
            f"{row['p99_ns']:.2f}\t"
            f"{row['avg_component_steps']:.2f}\t"
            f"{row['avg_index_steps']:.2f}"
        )
    print()


def main():
    args = parse_args()
    suite_dir = os.path.abspath(args.suite_dir)
    output_dir = os.path.abspath(args.output_dir) if args.output_dir else DEFAULT_OUTPUT_DIR
    ensure_dir(output_dir)

    rocks_csv = os.path.join(suite_dir, "rocksdb_depth_sweep.csv")
    lhm_csv = os.path.join(suite_dir, "masstree_depth_sweep.csv")

    if not os.path.exists(rocks_csv):
        raise SystemExit(f"missing file: {rocks_csv}")
    if not os.path.exists(lhm_csv):
        raise SystemExit(f"missing file: {lhm_csv}")

    rocks_rows = read_rows(rocks_csv, args.query_kind)
    lhm_rows = read_rows(lhm_csv, args.query_kind)
    if not rocks_rows:
        raise SystemExit(f"no rows matched query_kind={args.query_kind} in {rocks_csv}")
    if not lhm_rows:
        raise SystemExit(f"no rows matched query_kind={args.query_kind} in {lhm_csv}")

    rocks_summary = aggregate_by_depth(rocks_rows)
    lhm_summary = aggregate_by_depth(lhm_rows)

    write_summary_csv(
        os.path.join(output_dir, f"rocksdb_{args.query_kind}_summary.csv"),
        "rocksdb",
        args.query_kind,
        rocks_summary,
    )
    write_summary_csv(
        os.path.join(output_dir, f"masstree_{args.query_kind}_summary.csv"),
        "masstree",
        args.query_kind,
        lhm_summary,
    )

    slope_rows = []
    for backend, summary in (("rocksdb", rocks_summary), ("masstree", lhm_summary)):
        for metric in ("avg_ns", "p50_ns", "p99_ns", "avg_index_steps"):
            slope, intercept = fit_slope(summary, metric)
            slope_rows.append(
                {
                    "backend": backend,
                    "query_kind": args.query_kind,
                    "metric": metric,
                    "slope_per_depth": slope,
                    "intercept": intercept,
                }
            )
    write_slope_csv(os.path.join(output_dir, f"{args.query_kind}_slopes.csv"), slope_rows)

    plot_latency(
        os.path.join(output_dir, f"{args.query_kind}_depth_latency.png"),
        args.query_kind,
        rocks_summary,
        lhm_summary,
    )
    plot_avg_latency(
        os.path.join(output_dir, f"{args.query_kind}_depth_avg_latency.png"),
        args.query_kind,
        rocks_summary,
        lhm_summary,
    )
    plot_steps(
        os.path.join(output_dir, f"{args.query_kind}_depth_steps.png"),
        args.query_kind,
        rocks_summary,
        lhm_summary,
    )

    print_summary("Iterative (RocksDB)", rocks_summary)
    print_summary("LHM (Masstree)", lhm_summary)
    print(f"analysis outputs written to: {output_dir}")

    if args.show:
        plt.show()


if __name__ == "__main__":
    main()
