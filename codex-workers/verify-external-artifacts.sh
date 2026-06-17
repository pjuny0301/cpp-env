#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat >&2 <<'USAGE'
usage: verify-external-artifacts.sh [repo-root] [manifest]

Enforces that external artifacts are recorded with URL, version/ref, license,
hash, local path, and reason. The default manifest is
build/external/lib/cpp/desktop/native-deps-manifest.md.
USAGE
}

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
  usage
  exit 0
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="${1:-$(git -C "${script_dir}/.." rev-parse --show-toplevel)}"
manifest="${2:-${repo_root}/build/external/lib/cpp/desktop/native-deps-manifest.md}"
manifest_root="$(dirname "${manifest}")"

if [[ ! -f "${manifest}" ]]; then
  echo "missing external artifact manifest: ${manifest}" >&2
  exit 1
fi

tmp_known="$(mktemp)"
trap 'rm -f "${tmp_known}"' EXIT

awk -F '|' '
  function trim(s) {
    gsub(/^[ \t]+|[ \t]+$/, "", s);
    return s;
  }
  /^## Downloaded/ { in_downloaded=1; next }
  /^## Deferred/ { in_downloaded=0 }
  !in_downloaded { next }
  /^\| ---/ { next }
  /^\| Name / { next }
  /^\|/ {
    name=trim($2); version=trim($3); url=trim($4); license=trim($5); local_path=trim($6); hash=trim($8); reason=trim($9);
    if (name == "" || version == "" || url == "" || license == "" || local_path == "" || hash == "" || reason == "") {
      printf("manifest row has required empty field: %s\n", $0) > "/dev/stderr";
      exit 2;
    }
    if (url !~ /^https?:\/\//) {
      printf("manifest row has non-http source URL: %s\n", $0) > "/dev/stderr";
      exit 2;
    }
    gsub(/^`|`$/, "", local_path);
    print local_path;
  }
' "${manifest}" > "${tmp_known}"

if [[ ! -s "${tmp_known}" ]]; then
  echo "no downloaded external artifact rows found in ${manifest}" >&2
  exit 1
fi

if [[ -d "${manifest_root}" ]]; then
  while IFS= read -r path; do
    name="$(basename "${path}")"
    case "${name}" in
      AGENTS.md|README.md|native-deps-manifest.md|_tmp) continue ;;
    esac
    rel="${path#"${manifest_root}"/}"
    if ! grep -Fxq "${rel}" "${tmp_known}"; then
      echo "external artifact path is not manifest-recorded: ${rel}" >&2
      exit 1
    fi
  done < <(find "${manifest_root}" -mindepth 1 -maxdepth 1 -print | LC_ALL=C sort)
fi

echo "external artifact manifest OK: ${manifest}"
