#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat >&2 <<'USAGE'
usage: quiz-vulkan-worker-build-dir.sh [worktree] [preset]

Prints the Windows-style quiz-vulkan build directory for a worker worktree.

Defaults:
  worktree: current directory
  preset:   windows-mingw-ascii
USAGE
}

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
  usage
  exit 0
fi

worktree="${1:-$(pwd)}"
preset="${2:-windows-mingw-ascii}"

if [[ -f "${worktree}/CMakePresets.json" ]]; then
  worktree="$(cd "${worktree}/../../.." && pwd)"
fi

case "${preset}" in
  windows-mingw-ascii)
    build_dir="${worktree}/build/out/quiz/quiz-vulkan/windows-mingw-ascii"
    ;;
  windows-msvc)
    build_dir="${worktree}/build/out/quiz/quiz-vulkan/windows-msvc"
    ;;
  windows-msvc-vcpkg)
    build_dir="${worktree}/build/out/quiz/quiz-vulkan/windows-msvc-vcpkg"
    ;;
  linux-ninja)
    build_dir="${worktree}/build/out/quiz/quiz-vulkan/linux-ninja"
    ;;
  linux-ninja-vcpkg)
    build_dir="${worktree}/build/out/quiz/quiz-vulkan/linux-ninja-vcpkg"
    ;;
  *)
    echo "unsupported preset: ${preset}" >&2
    exit 1
    ;;
esac

if command -v wslpath >/dev/null 2>&1; then
  wslpath -m "${build_dir}"
else
  printf '%s\n' "${build_dir}"
fi
