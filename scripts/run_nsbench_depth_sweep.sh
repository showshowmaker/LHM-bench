#!/usr/bin/env bash
set -euo pipefail

exe_path="${1:-}"
dataset_root="${2:-./datasets}"
output_dir="${3:-./results}"
warmup="${WARMUP:-10000}"
repeats="${REPEATS:-10}"
backends="${BACKENDS:-masstree rocksdb}"
include_negative="${INCLUDE_NEGATIVE:-0}"
no_verify="${NO_VERIFY:-0}"

if [[ -z "${exe_path}" ]]; then
  echo "usage: $0 <nsbench_run> [dataset_root] [output_dir]" >&2
  exit 1
fi

mkdir -p "${output_dir}"
manifest_list="${output_dir}/manifest_list.txt"
find "${dataset_root}" -mindepth 2 -maxdepth 2 -name manifest.txt | sort > "${manifest_list}"

for backend in ${backends}; do
  csv_path="${output_dir}/${backend}_depth_sweep.csv"
  args=(
    --backend "${backend}"
    --manifest-list "${manifest_list}"
    --warmup "${warmup}"
    --repeats "${repeats}"
    --output-csv "${csv_path}"
  )
  if [[ "${include_negative}" == "1" ]]; then
    args+=(--include-negative)
  fi
  if [[ "${no_verify}" == "1" ]]; then
    args+=(--no-verify)
  fi
  "${exe_path}" "${args[@]}"
done
