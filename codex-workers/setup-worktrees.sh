#!/usr/bin/env bash
set -euo pipefail

repo_root="${1:-/mnt/c/aa}"
worker_root="${QUIZ_CODEX_WORKER_ROOT:-/mnt/c/aa-workers}"
base_branch="${QUIZ_CODEX_BASE_BRANCH:-codex/quiz-vulkan-remake-baseline}"

roles=(
  text-engine
  image-texture
  audio-engine
  input-ime
  vulkan-backend
  asset-system
)

mkdir -p "${worker_root}/logs"

for role in "${roles[@]}"; do
  branch="codex/${role}"
  path="${worker_root}/${role}"

  if [[ -d "${path}/.git" || -f "${path}/.git" ]]; then
    echo "exists: ${path}"
    continue
  fi

  if git -C "${repo_root}" show-ref --verify --quiet "refs/heads/${branch}"; then
    git -C "${repo_root}" worktree add "${path}" "${branch}"
  else
    git -C "${repo_root}" worktree add "${path}" -b "${branch}" "${base_branch}"
  fi
done

git -C "${repo_root}" worktree list
