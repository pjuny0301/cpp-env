#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
workspace_root="$(cd "$repo_root/../../.." && pwd)"
cd "$repo_root"

compiler="${CXX:-}"
if [[ -z "$compiler" ]]; then
    for candidate in "/mnt/c/MinGW/bin/g++.exe" "C:/MinGW/bin/g++.exe" "g++.exe" "g++"; do
        if [[ -x "$candidate" ]] || command -v "$candidate" >/dev/null 2>&1; then
            compiler="$candidate"
            break
        fi
    done
fi

if [[ -z "$compiler" ]]; then
    echo "MinGW g++ was not found. Set CXX=/mnt/c/MinGW/bin/g++.exe or put g++ on PATH." >&2
    exit 1
fi

build_root="${BUILD_ROOT:-$workspace_root/build/out/quiz/quiz-vulkan/manual}"
mkdir -p "$build_root"
build_dir="$(mktemp -d "$build_root/mingw-scene-layout.XXXXXX")"

cleanup() {
    if [[ "${KEEP_TEST_BINARIES:-0}" != "1" ]]; then
        rm -rf "$build_dir"
        rmdir "$build_root" 2>/dev/null || true
    else
        printf 'kept binaries in %s\n' "$build_dir"
    fi
}
trap cleanup EXIT

run_smoke_test() {
    local source="$1"
    local name="$2"
    local exe="$build_dir/$name.exe"

    "$compiler" -std=c++1z -Wall -Wextra -pedantic -Isrc "$source" -o "$exe"
    "$exe"
    printf 'passed %s\n' "$name"
}

printf 'compiler %s\n' "$compiler"
run_smoke_test "tests/scene/scene_layout_data_tests.cpp" "scene_layout_data_tests"
run_smoke_test "tests/layout/layout_placer_tests.cpp" "layout_placer_tests"
