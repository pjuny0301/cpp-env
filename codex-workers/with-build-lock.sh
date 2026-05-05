#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "usage: with-build-lock.sh <command> [args...]" >&2
  exit 64
fi

repo_root="${QUIZ_CODEX_REPO_ROOT:-/mnt/c/aa}"
lock_dir="${repo_root}/build/out/quiz/quiz-vulkan/windows-mingw-ascii"
mkdir -p "${lock_dir}"

exec flock "${lock_dir}/.codex-build.lock" "$@"
