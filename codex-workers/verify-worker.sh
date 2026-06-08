#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat >&2 <<'USAGE'
usage: verify-worker.sh <role> <ctest-regex> [worktree] [preset]

Runs the standard worker verification sequence:
  preflight -> configure -> contract compile target -> focused CTest ->
  worker ledger check -> source manifest check -> external manifest check ->
  git diff --check.

Environment:
  CMAKE_EXE, CTEST_EXE, QUIZ_CODEX_EXTERNAL_DIR, QUIZ_CODEX_PATH_STYLE
USAGE
}

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
  usage
  exit 0
fi
if [[ $# -lt 2 ]]; then
  usage
  exit 64
fi

role="$1"
ctest_regex="$2"
worktree="${3:-$(pwd)}"
preset="${4:-${QUIZ_CODEX_PRESET:-windows-mingw-ascii}}"

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(git -C "${worktree}" rev-parse --show-toplevel)"
case "${preset}" in
  windows-*)
    default_cmake="/mnt/c/Program Files/CMake/bin/cmake.exe"
    default_ctest="/mnt/c/Program Files/CMake/bin/ctest.exe"
    ;;
  *)
    default_cmake="cmake"
    default_ctest="ctest"
    ;;
esac
cmake_exe="${CMAKE_EXE:-${default_cmake}}"
ctest_exe="${CTEST_EXE:-${default_ctest}}"

echo "verify-worker: role=${role}"
echo "verify-worker: worktree=${repo_root}"
echo "verify-worker: preset=${preset}"
echo "verify-worker: ctest_regex=${ctest_regex}"

"${script_dir}/preflight-worker-env.sh" "${repo_root}" "${preset}"
"${script_dir}/configure-quiz-vulkan-worker-build.sh" "${repo_root}" "${preset}"

build_dir="$("${script_dir}/quiz-vulkan-worker-build-dir.sh" "${repo_root}" "${preset}")"
QUIZ_CODEX_REPO_ROOT="${repo_root}" "${script_dir}/with-build-lock.sh" --build-dir "${build_dir}" "${cmake_exe}" \
  --build "${build_dir}" \
  --target quiz_vulkan_interface_contract_compile_tests

QUIZ_CODEX_REPO_ROOT="${repo_root}" "${script_dir}/with-build-lock.sh" --build-dir "${build_dir}" "${ctest_exe}" \
  --test-dir "${build_dir}" \
  -R "${ctest_regex}" \
  --output-on-failure

"${script_dir}/verify-worker-ledger.sh" "${repo_root}/codex-workers/worker-ledger.tsv"
"${script_dir}/verify-source-manifest.sh" "${repo_root}"
"${script_dir}/verify-external-artifacts.sh" "${repo_root}"

git -C "${repo_root}" diff --check

echo "verify-worker: OK"
