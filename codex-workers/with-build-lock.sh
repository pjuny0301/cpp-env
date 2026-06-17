#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat >&2 <<'USAGE'
usage: with-build-lock.sh [--build-dir DIR] [--deps-lock|--no-deps-lock] <command> [args...]

Serializes commands that share a CMake build directory. Configure commands also
take a repository-wide dependency lock so parallel workers do not race while
CMake inspects or generates shared external dependency build trees.
USAGE
}

build_dir_arg=""
deps_lock_mode="auto"
while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)
      build_dir_arg="${2:-}"
      if [[ -z "${build_dir_arg}" ]]; then
        echo "--build-dir requires a value" >&2
        exit 64
      fi
      shift 2
      ;;
    --deps-lock)
      deps_lock_mode="yes"
      shift
      ;;
    --no-deps-lock)
      deps_lock_mode="no"
      shift
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    --)
      shift
      break
      ;;
    *)
      break
      ;;
  esac
done

if [[ $# -lt 1 ]]; then
  usage
  exit 64
fi

repo_root="${QUIZ_CODEX_REPO_ROOT:-/mnt/c/aa}"
default_build_dir="${repo_root}/build/out/quiz/quiz-vulkan/windows-mingw-ascii"

normalize_lock_path() {
  local path="$1"
  if command -v wslpath >/dev/null 2>&1 && [[ "${path}" =~ ^[A-Za-z]:/ ]]; then
    wslpath -u "${path}"
  else
    printf '%s\n' "${path}"
  fi
}

infer_build_dir() {
  if [[ -n "${build_dir_arg}" ]]; then
    normalize_lock_path "${build_dir_arg}"
    return
  fi

  local previous=""
  for arg in "$@"; do
    case "${previous}" in
      --build|--test-dir|-B)
        normalize_lock_path "${arg}"
        return
        ;;
    esac
    previous="${arg}"
  done

  printf '%s\n' "${default_build_dir}"
}

needs_dependency_lock() {
  case "${deps_lock_mode}" in
    yes) return 0 ;;
    no) return 1 ;;
  esac

  local saw_build=0
  local saw_test=0
  local saw_preset=0
  local saw_external_override=0
  for arg in "$@"; do
    [[ "${arg}" == "--build" ]] && saw_build=1
    [[ "${arg}" == "--test-dir" ]] && saw_test=1
    [[ "${arg}" == "--preset" ]] && saw_preset=1
    [[ "${arg}" == -DQUIZ_VULKAN_DESKTOP_EXTERNAL_DIR=* ]] && saw_external_override=1
  done

  if [[ "${saw_build}" -eq 0 && "${saw_test}" -eq 0 && ( "${saw_preset}" -eq 1 || "${saw_external_override}" -eq 1 ) ]]; then
    return 0
  fi
  return 1
}

now_ms() {
  date +%s%3N
}

acquire_lock() {
  local name="$1"
  local lock_file="$2"
  local fd_var="$3"
  mkdir -p "$(dirname "${lock_file}")"
  local start
  start="$(now_ms)"
  eval "exec {${fd_var}}>\"\${lock_file}\""
  eval "flock \"\${${fd_var}}\""
  local end
  end="$(now_ms)"
  echo "with-build-lock: ${name}_lock=${lock_file} waited_ms=$((end - start))" >&2
}

build_dir="$(infer_build_dir "$@")"
build_lock="${build_dir}/.codex-build.lock"
deps_lock="${repo_root}/build/out/.codex-global-deps.lock"

if needs_dependency_lock "$@"; then
  acquire_lock "deps" "${deps_lock}" deps_lock_fd
fi
acquire_lock "build" "${build_lock}" build_lock_fd

"$@"
