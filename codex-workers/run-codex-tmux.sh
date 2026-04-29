#!/usr/bin/env bash
set -euo pipefail

role="${1:?usage: run-codex-tmux.sh <text-engine|image-texture|audio-engine|input-ime|vulkan-backend|asset-system|feedback-sync>}"
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="${QUIZ_CODEX_WORKER_ROOT:-/mnt/c/aa-workers}"
worktree="${root}/${role}"
prompt="${script_dir}/prompts/${role}.md"
common="${script_dir}/prompts/common.md"
session="codex-${role}"
prompt_run="${root}/logs/${role}.prompt.md"

if [[ ! -d "${worktree}" ]]; then
  echo "missing worktree: ${worktree}" >&2
  exit 1
fi
if [[ ! -f "${prompt}" ]]; then
  echo "missing prompt: ${prompt}" >&2
  exit 1
fi
if tmux has-session -t "${session}" 2>/dev/null; then
  echo "tmux session already exists: ${session}" >&2
  exit 1
fi

{
  cat "${common}"
  printf "\n\n"
  cat "${prompt}"
} > "${prompt_run}"

tmux new-session -d -s "${session}" -c "${worktree}" \
  "prompt=\$(cat '${prompt_run}'); codex --cd '${worktree}' --no-alt-screen --dangerously-bypass-approvals-and-sandbox \"\$prompt\""

echo "started tmux session ${session} for ${role}"
echo "attach: tmux attach -t ${session}"
echo "capture: tmux capture-pane -pt ${session} -S -120"
