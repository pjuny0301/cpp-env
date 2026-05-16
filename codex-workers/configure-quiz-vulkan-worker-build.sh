#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat >&2 <<'USAGE'
usage: configure-quiz-vulkan-worker-build.sh [worktree] [preset]

Configures quiz-vulkan from a worker worktree while reusing the approved
central external dependency checkout under /mnt/c/aa/build/external.

Defaults:
  worktree: current directory
  preset:   windows-mingw-ascii

Environment:
  CMAKE_EXE                 Path to cmake.exe.
  QUIZ_CODEX_EXTERNAL_DIR   External desktop dependency root.
USAGE
}

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
  usage
  exit 0
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
worktree="${1:-$(pwd)}"
preset="${2:-windows-mingw-ascii}"
cmake_exe="${CMAKE_EXE:-/mnt/c/Program Files/CMake/bin/cmake.exe}"
external_dir="${QUIZ_CODEX_EXTERNAL_DIR:-/mnt/c/aa/build/external/lib/cpp/desktop}"
source_dir="${worktree}/apps/quiz/quiz-vulkan"

if [[ ! -d "${source_dir}" ]]; then
  echo "missing quiz-vulkan source dir: ${source_dir}" >&2
  exit 1
fi
if [[ ! -x "${cmake_exe}" && ! -f "${cmake_exe}" ]]; then
  echo "missing CMake executable: ${cmake_exe}" >&2
  exit 1
fi
if [[ ! -d "${external_dir}" ]]; then
  echo "missing external dependency dir: ${external_dir}" >&2
  exit 1
fi

if command -v wslpath >/dev/null 2>&1; then
  external_dir_for_cmake="$(wslpath -m "${external_dir}")"
else
  external_dir_for_cmake="${external_dir}"
fi

cd "${source_dir}"
"${script_dir}/with-build-lock.sh" "${cmake_exe}" \
  --preset "${preset}" \
  -DQUIZ_VULKAN_DESKTOP_EXTERNAL_DIR="${external_dir_for_cmake}"
