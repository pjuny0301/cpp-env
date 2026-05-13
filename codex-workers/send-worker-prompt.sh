#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat >&2 <<'USAGE'
usage: send-worker-prompt.sh <tmux-session> <prompt-file>

Pastes a prompt file into an existing Codex tmux worker and submits it.
The worker session is left running.
USAGE
}

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

tmux load-buffer "${prompt_file}"
tmux paste-buffer -t "${session}:0"
tmux send-keys -t "${session}:0" Enter

echo "sent prompt to ${session}"
