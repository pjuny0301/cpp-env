#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

usage() {
  cat >&2 <<'USAGE'
usage: worker-status.sh [repo-root]

Summarizes live Codex tmux sessions, queued prompt counts, and git status for
the main repo and worker pane paths.

Environment:
  QUIZ_CODEX_BASE_REF           Integration baseline for ahead/behind counts.
  QUIZ_CODEX_WORKER_QUEUE_ROOT  Queue root. Default: codex-workers/queued.
  QUIZ_CODEX_STATUS_UNTRACKED   Git untracked scan mode: no, normal, or all.
                                Default: no.
USAGE
}

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
  usage
  exit 0
fi

repo_root="${1:-$(git -C "${script_dir}/.." rev-parse --show-toplevel)}"
base_ref="${QUIZ_CODEX_BASE_REF:-origin/codex/ui-engine-phase12-secured-20260608T190736Z}"
queue_root="${QUIZ_CODEX_WORKER_QUEUE_ROOT:-${script_dir}/queued}"
status_untracked="${QUIZ_CODEX_STATUS_UNTRACKED:-no}"

case "${status_untracked}" in
  no|normal|all) ;;
  *)
    echo "QUIZ_CODEX_STATUS_UNTRACKED must be one of: no, normal, all" >&2
    exit 64
    ;;
esac

if ! command -v tmux >/dev/null 2>&1; then
  echo "tmux is required" >&2
  exit 1
fi

print_git_status() {
  local path="$1"

  if [[ ! -d "${path}/.git" && ! -f "${path}/.git" ]]; then
    echo "branch=- ahead=- behind=- dirty=- head=-"
    return
  fi

  local branch
  branch="$(git -C "${path}" branch --show-current 2>/dev/null || true)"
  if [[ -z "${branch}" ]]; then
    branch="detached"
  fi

  local ahead="-"
  local behind="-"
  if git -C "${path}" rev-parse --verify --quiet "${base_ref}" >/dev/null; then
    local counts
    counts="$(git -C "${path}" rev-list --left-right --count "HEAD...${base_ref}")"
    ahead="${counts%%$'\t'*}"
    behind="${counts##*$'\t'}"
  fi

  local dirty
  dirty="$(git -C "${path}" status --porcelain "--untracked-files=${status_untracked}" | wc -l | tr -d ' ')"

  local head
  head="$(git -C "${path}" rev-parse --short HEAD 2>/dev/null || true)"

  echo "branch=${branch} ahead=${ahead} behind=${behind} dirty=${dirty} head=${head}"
}

worker_state() {
  local session="$1"
  local pane
  pane="$(tmux capture-pane -pt "${session}:0" -S -30 2>/dev/null || true)"
  if grep -Eq 'Working \(|Waiting for background terminal|esc to interrupt|background terminal running' <<<"${pane}"; then
    echo "busy"
    return
  fi
  if grep -Eq '^› ' <<<"${pane}"; then
    echo "idle"
    return
  fi
  echo "unknown"
}

queued_prompt_count() {
  local session="$1"
  local session_queue="${queue_root}/${session}"
  if [[ ! -d "${session_queue}" ]]; then
    echo "0"
    return
  fi
  find "${session_queue}" -maxdepth 1 -type f | wc -l | tr -d ' '
}

echo "base_ref=${base_ref}"
echo "status_untracked=${status_untracked}"
echo "main ${repo_root} $(print_git_status "${repo_root}")"
echo
printf '%-52s %-8s %-6s %-10s %-70s %s\n' "session" "state" "queued" "command" "path" "git"

tmux_panes="$(tmux list-panes -a -F '#{session_name}|#{pane_current_command}|#{pane_current_path}' 2>/dev/null || true)"
if [[ -z "${tmux_panes}" ]]; then
  exit 0
fi

printf '%s\n' "${tmux_panes}" |
while IFS='|' read -r session command path; do
  case "${session}" in
    codex-*) ;;
    *) continue ;;
  esac

  printf '%-52s %-8s %-6s %-10s %-70s %s\n' \
    "${session}" \
    "$(worker_state "${session}")" \
    "$(queued_prompt_count "${session}")" \
    "${command}" \
    "${path}" \
    "$(print_git_status "${path}")"
done
