#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat >&2 <<'USAGE'
usage: preflight-worker-env.sh [--report PATH] [worktree] [preset]

Checks the local worker build environment without configuring the project.

Environment overrides:
  CMAKE_EXE                 cmake/cmake.exe path.
  NINJA_EXE                 ninja/ninja.exe path.
  CC, CXX                   compiler paths reported in the preflight.
  QUIZ_CODEX_EXTERNAL_DIR   external dependency root.
  QUIZ_CODEX_PATH_STYLE     auto, windows, or posix.
USAGE
}

report_path=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --report)
      report_path="${2:-}"
      if [[ -z "${report_path}" ]]; then
        echo "--report requires a value" >&2
        exit 64
      fi
      shift 2
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

worktree="${1:-$(pwd)}"
preset="${2:-windows-mingw-ascii}"
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(git -C "${script_dir}/.." rev-parse --show-toplevel)"
external_dir="${QUIZ_CODEX_EXTERNAL_DIR:-${repo_root}/build/external/lib/cpp/desktop}"
path_style="${QUIZ_CODEX_PATH_STYLE:-auto}"

case "${preset}" in
  windows-*) default_cmake="/mnt/c/Program Files/CMake/bin/cmake.exe" ;;
  *) default_cmake="cmake" ;;
esac
cmake_exe="${CMAKE_EXE:-${default_cmake}}"
ninja_exe="${NINJA_EXE:-ninja}"

if [[ -f "${worktree}/CMakePresets.json" ]]; then
  source_dir="${worktree}"
else
  source_dir="${worktree}/apps/quiz/quiz-vulkan"
fi

status=0
checks=()
add_check() {
  local name="$1"
  local result="$2"
  local detail="$3"
  checks+=("| ${name} | ${result} | ${detail} |")
  [[ "${result}" == "PASS" || "${result}" == "WARN" ]] || status=1
}

command_or_file_exists() {
  local tool="$1"
  command -v "${tool}" >/dev/null 2>&1 || [[ -x "${tool}" || -f "${tool}" ]]
}

if [[ -d "${source_dir}" ]]; then
  add_check "source_dir" "PASS" "\`${source_dir}\`"
else
  add_check "source_dir" "FAIL" "missing \`${source_dir}\`"
fi

if command_or_file_exists "${cmake_exe}"; then
  cmake_version="$("${cmake_exe}" --version 2>/dev/null | head -n 1 || true)"
  add_check "cmake" "PASS" "\`${cmake_exe}\` ${cmake_version}"
else
  add_check "cmake" "FAIL" "missing \`${cmake_exe}\`"
fi

if command_or_file_exists "${ninja_exe}"; then
  ninja_version="$("${ninja_exe}" --version 2>/dev/null | head -n 1 || true)"
  add_check "ninja" "PASS" "\`${ninja_exe}\` ${ninja_version}"
else
  add_check "ninja" "WARN" "missing \`${ninja_exe}\`; preset may use another generator"
fi

if [[ -n "${CC:-}" || -n "${CXX:-}" ]]; then
  add_check "compiler_override" "PASS" "CC=\`${CC:-}\`, CXX=\`${CXX:-}\`"
else
  compiler_detail="$(command -v c++ 2>/dev/null || command -v g++ 2>/dev/null || true)"
  if [[ -n "${compiler_detail}" ]]; then
    add_check "compiler" "PASS" "\`${compiler_detail}\`"
  else
    add_check "compiler" "WARN" "no POSIX compiler found in PATH"
  fi
fi

if [[ -f "${source_dir}/CMakePresets.json" ]] && grep -Fq "\"name\": \"${preset}\"" "${source_dir}/CMakePresets.json"; then
  add_check "cmake_preset" "PASS" "\`${preset}\`"
else
  add_check "cmake_preset" "FAIL" "\`${preset}\` not found in ${source_dir}/CMakePresets.json"
fi

if [[ -d "${external_dir}" ]]; then
  manifest="${external_dir}/native-deps-manifest.md"
  if [[ -f "${manifest}" ]]; then
    manifest_sha="$(sha256sum "${manifest}" | awk '{print $1}')"
    add_check "external_snapshot" "PASS" "\`${external_dir}\`, manifest sha256=${manifest_sha}"
  else
    add_check "external_snapshot" "FAIL" "missing native-deps-manifest.md under \`${external_dir}\`"
  fi
else
  add_check "external_snapshot" "FAIL" "missing \`${external_dir}\`"
fi

if [[ "${path_style}" == "auto" ]]; then
  if [[ "${cmake_exe}" == *.exe || "${preset}" == windows-* ]]; then
    resolved_path_style="windows"
  else
    resolved_path_style="posix"
  fi
else
  resolved_path_style="${path_style}"
fi

if [[ "${resolved_path_style}" == "windows" ]]; then
  if command -v wslpath >/dev/null 2>&1; then
    converted="$(wslpath -m "${external_dir}")"
    add_check "windows_path_conversion" "PASS" "\`${external_dir}\` -> \`${converted}\`"
  else
    add_check "windows_path_conversion" "WARN" "wslpath unavailable; paths will not be converted"
  fi
else
  add_check "path_style" "PASS" "\`${resolved_path_style}\`"
fi

build_dir="$("${script_dir}/quiz-vulkan-worker-build-dir.sh" "${worktree}" "${preset}")"
add_check "build_dir" "PASS" "\`${build_dir}\`"

tmp_report="$(mktemp)"
{
  echo "# Worker Environment Preflight"
  echo
  echo "- generated_utc: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "- worktree: \`${worktree}\`"
  echo "- source_dir: \`${source_dir}\`"
  echo "- preset: \`${preset}\`"
  echo "- path_style: \`${resolved_path_style}\`"
  echo
  echo "| Check | Result | Detail |"
  echo "| --- | --- | --- |"
  printf '%s\n' "${checks[@]}"
} > "${tmp_report}"

cat "${tmp_report}"
if [[ -n "${report_path}" ]]; then
  mkdir -p "$(dirname "${report_path}")"
  cp "${tmp_report}" "${report_path}"
fi
rm -f "${tmp_report}"

exit "${status}"
