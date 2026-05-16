#!/usr/bin/env bash
set -euo pipefail

repo_root="${1:-/mnt/c/aa}"
base_ref="${QUIZ_CODEX_BASE_REF:-origin/codex/quiz-vulkan-remake-baseline}"

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
  dirty="$(git -C "${path}" status --porcelain | wc -l | tr -d ' ')"

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

echo "base_ref=${base_ref}"
echo "main ${repo_root} $(print_git_status "${repo_root}")"
echo
printf '%-52s %-8s %-10s %-70s %s\n' "session" "state" "command" "path" "git"

tmux list-panes -a -F '#{session_name}|#{pane_current_command}|#{pane_current_path}' |
while IFS='|' read -r session command path; do
  case "${session}" in
    codex-*) ;;
    *) continue ;;
  esac

  printf '%-52s %-8s %-10s %-70s %s\n' \
    "${session}" \
    "$(worker_state "${session}")" \
    "${command}" \
    "${path}" \
    "$(print_git_status "${path}")"
done
