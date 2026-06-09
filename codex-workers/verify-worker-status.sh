#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat >&2 <<'USAGE'
usage: verify-worker-status.sh [repo-root]

Verifies worker-status.sh table, TSV, and JSON output contracts.

Environment:
  QUIZ_CODEX_BASE_REF           Integration baseline for ahead/behind counts.
  QUIZ_CODEX_STATUS_UNTRACKED   Git untracked scan mode: no, normal, or all.
USAGE
}

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
  usage
  exit 0
fi
if [[ $# -gt 1 ]]; then
  usage
  exit 64
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="${1:-$(git -C "${script_dir}/.." rev-parse --show-toplevel)}"
base_ref="${QUIZ_CODEX_BASE_REF:-HEAD}"
tmp_dir="$(mktemp -d "${TMPDIR:-/tmp}/quiz-worker-status.XXXXXX")"
trap 'rm -rf "${tmp_dir}"' EXIT

table_output="${tmp_dir}/status.table"
tsv_output="${tmp_dir}/status.tsv"
json_output="${tmp_dir}/status.json"

QUIZ_CODEX_BASE_REF="${base_ref}" "${script_dir}/worker-status.sh" "${repo_root}" >"${table_output}"
QUIZ_CODEX_BASE_REF="${base_ref}" "${script_dir}/worker-status.sh" --tsv "${repo_root}" >"${tsv_output}"
QUIZ_CODEX_BASE_REF="${base_ref}" "${script_dir}/worker-status.sh" --json "${repo_root}" >"${json_output}"

if ! grep -Fq "base_ref=${base_ref}" "${table_output}"; then
  echo "verify-worker-status: table output is missing base_ref=${base_ref}" >&2
  exit 1
fi
if ! grep -Fq "main ${repo_root}" "${table_output}"; then
  echo "verify-worker-status: table output is missing main repo row" >&2
  exit 1
fi

expected_header=$'kind\tsession\tstate\tqueued\tcommand\tpath\tbranch\tahead\tbehind\tdirty\thead'
actual_header="$(head -n 1 "${tsv_output}")"
if [[ "${actual_header}" != "${expected_header}" ]]; then
  echo "verify-worker-status: TSV header mismatch" >&2
  exit 1
fi
if ! grep -Fq $'main\t-\t-\t-\t-\t' "${tsv_output}"; then
  echo "verify-worker-status: TSV output is missing main row" >&2
  exit 1
fi

python3 - "${json_output}" "${base_ref}" "${repo_root}" <<'PY'
import json
import sys

json_path, expected_base_ref, expected_repo_root = sys.argv[1:4]
with open(json_path, "r", encoding="utf-8") as handle:
    payload = json.load(handle)

required_top_level = {"base_ref", "status_untracked", "main", "workers"}
missing_top_level = required_top_level - payload.keys()
if missing_top_level:
    raise SystemExit(f"missing top-level JSON keys: {sorted(missing_top_level)}")

if payload["base_ref"] != expected_base_ref:
    raise SystemExit("base_ref mismatch")
if not isinstance(payload["workers"], list):
    raise SystemExit("workers must be an array")

required_row_keys = {
    "kind",
    "session",
    "state",
    "queued",
    "command",
    "path",
    "branch",
    "ahead",
    "behind",
    "dirty",
    "head",
}

def validate_row(row, expected_kind=None):
    missing = required_row_keys - row.keys()
    if missing:
        raise SystemExit(f"missing row keys: {sorted(missing)}")
    for key in required_row_keys:
        if not isinstance(row[key], str):
            raise SystemExit(f"row field must be string: {key}")
    if expected_kind is not None and row["kind"] != expected_kind:
        raise SystemExit(f"row kind mismatch: {row['kind']}")

validate_row(payload["main"], "main")
if payload["main"]["path"] != expected_repo_root:
    raise SystemExit("main path mismatch")

for worker in payload["workers"]:
    validate_row(worker, "worker")
PY

echo "verify-worker-status: OK"
