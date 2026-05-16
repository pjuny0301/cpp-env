#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat >&2 <<'USAGE'
usage: send-worker-prompt.sh [--force] <tmux-session> <prompt-file>

Pastes a prompt file into an existing Codex tmux worker and submits it.
If the pane looks busy, the prompt is queued on disk instead of being pasted
unless --force is provided. The worker session is left running.
USAGE
}

force=false
if [[ "${1:-}" == "--force" ]]; then
  force=true
  shift
fi

session="${1:-}"
prompt_file="${2:-}"

if [[ -z "${session}" || -z "${prompt_file}" ]]; then
  usage
  exit 1
fi
if ! tmux has-session -t "${session}" 2>/dev/null; then
  echo "missing tmux session: ${session}" >&2
  exit 1
fi
if [[ ! -f "${prompt_file}" ]]; then
  echo "missing prompt file: ${prompt_file}" >&2
  exit 1
fi
if [[ ! -s "${prompt_file}" ]]; then
  echo "empty prompt file: ${prompt_file}" >&2
  exit 1
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
queue_root="${QUIZ_CODEX_WORKER_QUEUE_ROOT:-${script_dir}/queued}"

session_busy() {
  local pane
  pane="$(tmux capture-pane -pt "${session}:0" -S -30 2>/dev/null || true)"
  grep -Eq 'Working \(|Waiting for background terminal|esc to interrupt|background terminal running' <<<"${pane}"
}

queue_prompt() {
  local session_queue="${queue_root}/${session}"
  mkdir -p "${session_queue}"
  local stamp
  stamp="$(date +%Y%m%d-%H%M%S)"
  local queued="${session_queue}/${stamp}-$(basename "${prompt_file}")"
  cp "${prompt_file}" "${queued}"
  echo "worker is busy; queued prompt at ${queued}"
}

if [[ "${force}" != true ]] && session_busy; then
  queue_prompt
  exit 0
fi

tmux load-buffer "${prompt_file}"
tmux paste-buffer -t "${session}:0"
sleep 0.2
tmux send-keys -t "${session}:0" Enter
sleep 0.2
tmux send-keys -t "${session}:0" Enter

echo "sent prompt to ${session}"
