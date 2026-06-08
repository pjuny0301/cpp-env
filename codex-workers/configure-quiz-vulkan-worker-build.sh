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
  CMAKE_EXE                 Path to cmake/cmake.exe.
  QUIZ_CODEX_EXTERNAL_DIR   External desktop dependency root.
  QUIZ_CODEX_PATH_STYLE     auto, windows, or posix for CMake path args.
USAGE
}

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
  usage
  exit 0
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
worktree="${1:-$(pwd)}"
preset="${2:-windows-mingw-ascii}"
case "${preset}" in
  windows-*) default_cmake="/mnt/c/Program Files/CMake/bin/cmake.exe" ;;
  *) default_cmake="cmake" ;;
esac
cmake_exe="${CMAKE_EXE:-${default_cmake}}"
external_dir="${QUIZ_CODEX_EXTERNAL_DIR:-/mnt/c/aa/build/external/lib/cpp/desktop}"
if [[ -f "${worktree}/CMakePresets.json" ]]; then
  source_dir="${worktree}"
  repo_root="$(cd "${worktree}/../../.." && pwd)"
else
  source_dir="${worktree}/apps/quiz/quiz-vulkan"
  repo_root="${worktree}"
fi

if [[ ! -d "${source_dir}" ]]; then
  echo "missing quiz-vulkan source dir: ${source_dir}" >&2
  exit 1
fi
if ! command -v "${cmake_exe}" >/dev/null 2>&1 && [[ ! -x "${cmake_exe}" && ! -f "${cmake_exe}" ]]; then
  echo "missing CMake executable: ${cmake_exe}" >&2
  exit 1
fi
if [[ ! -d "${external_dir}" ]]; then
  echo "missing external dependency dir: ${external_dir}" >&2
  exit 1
fi

path_style="${QUIZ_CODEX_PATH_STYLE:-auto}"
if [[ "${path_style}" == "auto" ]]; then
  if [[ "${cmake_exe}" == *.exe || "${preset}" == windows-* ]]; then
    path_style="windows"
  else
    path_style="posix"
  fi
fi

case "${path_style}" in
  windows)
    if command -v wslpath >/dev/null 2>&1; then
      external_dir_for_cmake="$(wslpath -m "${external_dir}")"
    else
      external_dir_for_cmake="${external_dir}"
    fi
    ;;
  posix)
    external_dir_for_cmake="${external_dir}"
    ;;
  *)
    echo "unsupported QUIZ_CODEX_PATH_STYLE: ${path_style}" >&2
    exit 1
    ;;
esac

cd "${source_dir}"
QUIZ_CODEX_REPO_ROOT="${repo_root}" "${script_dir}/with-build-lock.sh" --deps-lock "${cmake_exe}" \
  --preset "${preset}" \
  -DQUIZ_VULKAN_DESKTOP_EXTERNAL_DIR="${external_dir_for_cmake}"
