#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat >&2 <<'USAGE'
usage: start-worker-task.sh [--resume] [--dry-run] <role> <prompt_id> [worktree-root]

Creates a clean worker worktree from the configured base ref, records the base
SHA in codex-workers/worker-ledger.tsv, and refuses to reuse an existing branch
or worktree unless --resume is supplied.

Environment:
  QUIZ_CODEX_BASE_REF       Base ref. Default: origin/codex/ui-engine-phase12-secured-20260608T190736Z
  QUIZ_CODEX_FETCH_REMOTE   Remote to fetch before resolving base. Default: origin
  QUIZ_CODEX_WORKER_LEDGER  Ledger path. Default: codex-workers/worker-ledger.tsv
  QUIZ_CODEX_WORKTREE_ROOT  Worker worktree root when the third arg is omitted.
USAGE
}

resume=0
dry_run=0
while [[ $# -gt 0 ]]; do
  case "$1" in
    --resume)
      resume=1
      shift
      ;;
    --dry-run)
      dry_run=1
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
    -*)
      echo "unknown option: $1" >&2
      usage
      exit 64
      ;;
    *)
      break
      ;;
  esac
done

if [[ $# -lt 2 || $# -gt 3 ]]; then
  usage
  exit 64
fi

role="$1"
prompt_id="$2"
worktree_root_arg="${3:-}"

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(git -C "${script_dir}/.." rev-parse --show-toplevel)"
base_ref="${QUIZ_CODEX_BASE_REF:-origin/codex/ui-engine-phase12-secured-20260608T190736Z}"
fetch_remote="${QUIZ_CODEX_FETCH_REMOTE:-origin}"
ledger="${QUIZ_CODEX_WORKER_LEDGER:-${repo_root}/codex-workers/worker-ledger.tsv}"
worktree_root="${worktree_root_arg:-${QUIZ_CODEX_WORKTREE_ROOT:-${repo_root}/../worktrees}}"

sanitize_ref_part() {
  printf '%s' "$1" |
    tr '[:upper:]' '[:lower:]' |
    sed -E 's/[^a-z0-9._-]+/-/g; s/^-+//; s/-+$//; s/-+/-/g'
}

role_slug="$(sanitize_ref_part "${role}")"
prompt_slug="$(sanitize_ref_part "${prompt_id}")"
if [[ -z "${role_slug}" || -z "${prompt_slug}" ]]; then
  echo "role and prompt_id must contain at least one ref-safe character" >&2
  exit 64
fi

session="${role_slug}-${prompt_slug}"
worktree="${worktree_root}/${session}"
timestamp="$(date -u +%Y%m%dT%H%M%SZ)"
branch="codex/${session}-${timestamp}"

require_clean_git_tree() {
  local path="$1"
  if [[ -n "$(git -C "${path}" status --porcelain)" ]]; then
    echo "git tree must be clean before starting worker: ${path}" >&2
    git -C "${path}" status --short >&2
    exit 1
  fi
}

require_clean_git_tree "${repo_root}"

if [[ "${dry_run}" -eq 0 ]]; then
  git -C "${repo_root}" fetch --prune "${fetch_remote}"
fi
base_sha="$(git -C "${repo_root}" rev-parse "${base_ref}^{commit}")"
heartbeat="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
status="active"

if [[ -e "${worktree}" ]]; then
  if [[ "${resume}" -eq 0 ]]; then
    echo "worker worktree already exists: ${worktree}" >&2
    echo "use --resume to reuse it" >&2
    exit 1
  fi
  if [[ ! -d "${worktree}/.git" && ! -f "${worktree}/.git" ]]; then
    echo "existing path is not a git worktree: ${worktree}" >&2
    exit 1
  fi
  require_clean_git_tree "${worktree}"
  branch="$(git -C "${worktree}" branch --show-current)"
  if [[ -z "${branch}" ]]; then
    echo "resume requires a named branch in ${worktree}" >&2
    exit 1
  fi
  status="resumed"
elif git -C "${repo_root}" show-ref --verify --quiet "refs/heads/${branch}"; then
  echo "generated branch already exists: ${branch}" >&2
  echo "rerun later or use --resume with an existing worktree" >&2
  exit 1
elif [[ "${dry_run}" -eq 0 ]]; then
  mkdir -p "${worktree_root}"
  git -C "${repo_root}" worktree add -b "${branch}" "${worktree}" "${base_sha}"
fi

row="${session}"$'\t'"${prompt_id}"$'\t'"${role}"$'\t'"${worktree}"$'\t'"${branch}"$'\t'"${base_sha}"$'\t'"${status}"$'\t'"start-worker-task"$'\t'"none"$'\t'"${heartbeat}"
if [[ "${dry_run}" -eq 0 ]]; then
  printf '%s\n' "${row}" >> "${ledger}"
fi

echo "session=${session}"
echo "worktree=${worktree}"
echo "branch=${branch}"
echo "base_ref=${base_ref}"
echo "base_sha=${base_sha}"
echo "status=${status}"
if [[ "${dry_run}" -eq 1 ]]; then
  echo "dry_run=true"
fi
