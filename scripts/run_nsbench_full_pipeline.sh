#!/usr/bin/env bash
set -euo pipefail

build_dataset_exe="${1:-}"
run_exe="${2:-}"
root_dir="${3:-./root}"
suite_dir="${SUITE_DIR:-VSIterate}"

depths="${DEPTHS:-1,2,4,8,16,32,64}"
siblings_per_dir="${SIBLINGS_PER_DIR:-256}"
files_per_leaf="${FILES_PER_LEAF:-256}"
positive_queries="${POSITIVE_QUERIES:-50000}"
negative_queries="${NEGATIVE_QUERIES:-50000}"
warmup="${WARMUP:-10000}"
repeats="${REPEATS:-10}"
backends="${BACKENDS:-masstree rocksdb}"
include_negative="${INCLUDE_NEGATIVE:-0}"
no_verify="${NO_VERIFY:-0}"

if [[ -z "${build_dataset_exe}" || -z "${run_exe}" ]]; then
  echo "usage: $0 <nsbench_build_dataset> <nsbench_run> [root_dir]" >&2
  exit 1
fi

mkdir -p "${root_dir}"

"${build_dataset_exe}" \
  --root "${root_dir}" \
  --depths "${depths}" \
  --siblings-per-dir "${siblings_per_dir}" \
  --files-per-leaf "${files_per_leaf}" \
  --positive-queries "${positive_queries}" \
  --negative-queries "${negative_queries}"

WARMUP="${warmup}" \
REPEATS="${repeats}" \
BACKENDS="${backends}" \
INCLUDE_NEGATIVE="${include_negative}" \
NO_VERIFY="${no_verify}" \
SUITE_DIR="${suite_dir}" \
bash "$(dirname "$0")/run_nsbench_depth_sweep.sh" "${run_exe}" "${root_dir}/${suite_dir}/datasets" "${root_dir}"
