#!/usr/bin/env bash
set -euo pipefail

exe_path="${1:-}"
dataset_root="${2:-./datasets}"
artifact_root="${3:-./root}"
warmup="${WARMUP:-10000}"
repeats="${REPEATS:-10}"
backends="${BACKENDS:-masstree rocksdb}"
include_negative="${INCLUDE_NEGATIVE:-0}"
no_verify="${NO_VERIFY:-0}"
suite_dir="${SUITE_DIR:-VSIterate}"

if [[ -z "${exe_path}" ]]; then
  echo "usage: $0 <nsbench_run> [dataset_root] [artifact_root]" >&2
  exit 1
fi

run_root="${artifact_root}/${suite_dir}"
mkdir -p "${run_root}"
manifest_list="${run_root}/manifest_list.txt"
find "${dataset_root}" -mindepth 2 -maxdepth 2 -name manifest.txt | sort > "${manifest_list}"

for backend in ${backends}; do
  csv_path="${run_root}/${backend}_depth_sweep.csv"
  args=(
    --backend "${backend}"
    --manifest-list "${manifest_list}"
    --artifact-root "${artifact_root}"
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
