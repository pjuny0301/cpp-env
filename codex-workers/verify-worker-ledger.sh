#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat >&2 <<'USAGE'
usage: verify-worker-ledger.sh [ledger.tsv]

Validates the Track B worker ledger schema and row values.
USAGE
}

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
  usage
  exit 0
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ledger="${1:-${script_dir}/worker-ledger.tsv}"
expected_header=$'session\tprompt_id\trole\tworktree\tbranch\tbase_sha\tstatus\tcurrent_task\tblocker\tlast_heartbeat_utc'

if [[ ! -f "${ledger}" ]]; then
  echo "missing worker ledger: ${ledger}" >&2
  exit 1
fi

header="$(head -n 1 "${ledger}")"
if [[ "${header}" != "${expected_header}" ]]; then
  echo "invalid worker ledger header in ${ledger}" >&2
  echo "expected: ${expected_header}" >&2
  echo "actual:   ${header}" >&2
  exit 1
fi

line_no=1
while IFS=$'\t' read -r session prompt_id role worktree branch base_sha status current_task blocker heartbeat extra; do
  line_no=$((line_no + 1))
  [[ -n "${session}${prompt_id}${role}${worktree}${branch}${base_sha}${status}${current_task}${blocker}${heartbeat}${extra:-}" ]] || continue

  if [[ -n "${extra:-}" ]]; then
    echo "line ${line_no}: too many columns" >&2
    exit 1
  fi

  for value_name in session prompt_id role worktree branch base_sha status current_task blocker heartbeat; do
    value="${!value_name}"
    if [[ -z "${value}" ]]; then
      echo "line ${line_no}: ${value_name} is required" >&2
      exit 1
    fi
  done

  case "${status}" in
    planned|active|resumed|blocked|done|abandoned) ;;
    *)
      echo "line ${line_no}: invalid status '${status}'" >&2
      exit 1
      ;;
  esac

  if [[ ! "${heartbeat}" =~ ^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z$ ]]; then
    echo "line ${line_no}: invalid UTC heartbeat '${heartbeat}'" >&2
    exit 1
  fi

  if [[ ! "${base_sha}" =~ ^[0-9a-fA-F]{7,40}$ ]]; then
    echo "line ${line_no}: invalid base_sha '${base_sha}'" >&2
    exit 1
  fi
done < <(tail -n +2 "${ledger}")

echo "worker ledger OK: ${ledger}"
