#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

usage() {
  cat >&2 <<'USAGE'
usage: worker-status.sh [--format table|tsv|json] [--tsv] [--json] [repo-root]

Summarizes live Codex tmux sessions, queued prompt counts, and git status for
the main repo and worker pane paths.

Environment:
  QUIZ_CODEX_BASE_REF           Integration baseline for ahead/behind counts.
  QUIZ_CODEX_WORKER_QUEUE_ROOT  Queue root. Default: codex-workers/queued.
  QUIZ_CODEX_STATUS_FORMAT      Output format: table, tsv, or json. Default: table.
  QUIZ_CODEX_STATUS_UNTRACKED   Git untracked scan mode: no, normal, or all.
                                Default: no.
USAGE
}

status_format="${QUIZ_CODEX_STATUS_FORMAT:-table}"
repo_root_arg=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --help|-h)
      usage
      exit 0
      ;;
    --format)
      if [[ $# -lt 2 ]]; then
        usage
        exit 64
      fi
      status_format="$2"
      shift 2
      ;;
    --format=*)
      status_format="${1#--format=}"
      shift
      ;;
    --tsv)
      status_format="tsv"
      shift
      ;;
    --json)
      status_format="json"
      shift
      ;;
    --*)
      usage
      exit 64
      ;;
    *)
      if [[ -n "${repo_root_arg}" ]]; then
        usage
        exit 64
      fi
      repo_root_arg="$1"
      shift
      ;;
  esac
done

repo_root="${repo_root_arg:-$(git -C "${script_dir}/.." rev-parse --show-toplevel)}"
base_ref="${QUIZ_CODEX_BASE_REF:-origin/codex/ui-engine-phase12-secured-20260608T190736Z}"
queue_root="${QUIZ_CODEX_WORKER_QUEUE_ROOT:-${script_dir}/queued}"
status_untracked="${QUIZ_CODEX_STATUS_UNTRACKED:-no}"

case "${status_format}" in
  table|tsv|json) ;;
  *)
    echo "QUIZ_CODEX_STATUS_FORMAT must be one of: table, tsv, json" >&2
    exit 64
    ;;
esac

case "${status_untracked}" in
  no|normal|all) ;;
  *)
    echo "QUIZ_CODEX_STATUS_UNTRACKED must be one of: no, normal, all" >&2
    exit 64
    ;;
esac

if ! command -v tmux >/dev/null 2>&1; then
  echo "tmux is required" >&2
  exit 1
fi

git_status_values() {
  local path="$1"

  if [[ ! -d "${path}/.git" && ! -f "${path}/.git" ]]; then
    printf -- '-\t-\t-\t-\t-\n'
    return
  fi

  local branch
  branch="$(git -C "${path}" branch --show-current 2>/dev/null || true)"
  if [[ -z "${branch}" ]]; then
    branch="detached"
  fi

  local ahead="-"
  local behind="-"
  if git -C "${path}" rev-parse --verify --quiet "${base_ref}" >/dev/null; then
    local counts
    counts="$(git -C "${path}" rev-list --left-right --count "HEAD...${base_ref}")"
    ahead="${counts%%$'\t'*}"
    behind="${counts##*$'\t'}"
  fi

  local dirty
  dirty="$(git -C "${path}" status --porcelain "--untracked-files=${status_untracked}" | wc -l | tr -d ' ')"

  local head
  head="$(git -C "${path}" rev-parse --short HEAD 2>/dev/null || true)"

  printf '%s\t%s\t%s\t%s\t%s\n' "${branch}" "${ahead}" "${behind}" "${dirty}" "${head}"
}

print_git_status() {
  local path="$1"
  local branch ahead behind dirty head
  IFS=$'\t' read -r branch ahead behind dirty head < <(git_status_values "${path}")
  echo "branch=${branch} ahead=${ahead} behind=${behind} dirty=${dirty} head=${head}"
}

json_escape() {
  local value="$1"
  value="${value//\\/\\\\}"
  value="${value//\"/\\\"}"
  value="${value//$'\n'/\\n}"
  value="${value//$'\r'/\\r}"
  value="${value//$'\t'/\\t}"
  printf '%s' "${value}"
}

json_string() {
  printf '"'
  json_escape "$1"
  printf '"'
}

json_status_object() {
  local indent="$1"
  local kind="$2"
  local session="$3"
  local state="$4"
  local queued="$5"
  local command="$6"
  local path="$7"
  local branch ahead behind dirty head
  IFS=$'\t' read -r branch ahead behind dirty head < <(git_status_values "${path}")

  printf '%s{' "${indent}"
  printf '"kind":'; json_string "${kind}"
  printf ',"session":'; json_string "${session}"
  printf ',"state":'; json_string "${state}"
  printf ',"queued":'; json_string "${queued}"
  printf ',"command":'; json_string "${command}"
  printf ',"path":'; json_string "${path}"
  printf ',"branch":'; json_string "${branch}"
  printf ',"ahead":'; json_string "${ahead}"
  printf ',"behind":'; json_string "${behind}"
  printf ',"dirty":'; json_string "${dirty}"
  printf ',"head":'; json_string "${head}"
  printf '}'
}

worker_state() {
  local session="$1"
  local pane
  pane="$(tmux capture-pane -pt "${session}:0" -S -30 2>/dev/null || true)"
  if grep -Eq 'Working \(|Waiting for background terminal|esc to interrupt|background terminal running' <<<"${pane}"; then
    echo "busy"
    return
  fi
  if grep -Eq '^› ' <<<"${pane}"; then
    echo "idle"
    return
  fi
  echo "unknown"
}

queued_prompt_count() {
  local session="$1"
  local session_queue="${queue_root}/${session}"
  if [[ ! -d "${session_queue}" ]]; then
    echo "0"
    return
  fi
  find "${session_queue}" -maxdepth 1 -type f | wc -l | tr -d ' '
}

tmux_panes="$(tmux list-panes -a -F '#{session_name}|#{pane_current_command}|#{pane_current_path}' 2>/dev/null || true)"

if [[ "${status_format}" == "tsv" ]]; then
  printf 'kind\tsession\tstate\tqueued\tcommand\tpath\tbranch\tahead\tbehind\tdirty\thead\n'
  IFS=$'\t' read -r main_branch main_ahead main_behind main_dirty main_head < <(git_status_values "${repo_root}")
  printf 'main\t-\t-\t-\t-\t%s\t%s\t%s\t%s\t%s\t%s\n' \
    "${repo_root}" \
    "${main_branch}" \
    "${main_ahead}" \
    "${main_behind}" \
    "${main_dirty}" \
    "${main_head}"
elif [[ "${status_format}" == "json" ]]; then
  printf '{\n'
  printf '  "base_ref": '; json_string "${base_ref}"; printf ',\n'
  printf '  "status_untracked": '; json_string "${status_untracked}"; printf ',\n'
  printf '  "main": '
  json_status_object "" "main" "-" "-" "-" "-" "${repo_root}"
  printf ',\n'
  printf '  "workers": ['
else
  echo "base_ref=${base_ref}"
  echo "status_untracked=${status_untracked}"
  echo "main ${repo_root} $(print_git_status "${repo_root}")"
  echo
  printf '%-52s %-8s %-6s %-10s %-70s %s\n' "session" "state" "queued" "command" "path" "git"
fi

first_json_worker=1
while IFS='|' read -r session command path; do
  if [[ -z "${session}" ]]; then
    continue
  fi
  case "${session}" in
    codex-*) ;;
    *) continue ;;
  esac

  state="$(worker_state "${session}")"
  queued="$(queued_prompt_count "${session}")"
  if [[ "${status_format}" == "tsv" ]]; then
    IFS=$'\t' read -r branch ahead behind dirty head < <(git_status_values "${path}")
    printf 'worker\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
      "${session}" \
      "${state}" \
      "${queued}" \
      "${command}" \
      "${path}" \
      "${branch}" \
      "${ahead}" \
      "${behind}" \
      "${dirty}" \
      "${head}"
  elif [[ "${status_format}" == "json" ]]; then
    if [[ "${first_json_worker}" -eq 1 ]]; then
      printf '\n'
      first_json_worker=0
    else
      printf ',\n'
    fi
    json_status_object "    " "worker" "${session}" "${state}" "${queued}" "${command}" "${path}"
  else
    printf '%-52s %-8s %-6s %-10s %-70s %s\n' \
      "${session}" \
      "${state}" \
      "${queued}" \
      "${command}" \
      "${path}" \
      "$(print_git_status "${path}")"
  fi
done <<< "${tmux_panes}"

if [[ "${status_format}" == "json" ]]; then
  if [[ "${first_json_worker}" -eq 1 ]]; then
    printf ']\n'
  else
    printf '\n  ]\n'
  fi
  printf '}\n'
fi
