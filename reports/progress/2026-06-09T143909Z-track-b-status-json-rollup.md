# Track B Progress - 2026-06-09T14:39:09Z

## Completed

- Added `worker-status.sh --json` and `QUIZ_CODEX_STATUS_FORMAT=json` for structured coordinator status output.
- Kept table output as the default and preserved TSV behavior.
- Added shell-side JSON string escaping for paths, commands, branches, and session fields.
- Documented the JSON shape: `base_ref`, `status_untracked`, `main`, and `workers`.

## Verification

- `bash -n codex-workers/worker-status.sh`
- `shellcheck codex-workers/worker-status.sh`
- `codex-workers/worker-status.sh --help`
- `QUIZ_CODEX_BASE_REF=HEAD codex-workers/worker-status.sh "$(pwd)"`
- `QUIZ_CODEX_BASE_REF=HEAD codex-workers/worker-status.sh --tsv "$(pwd)"`
- `QUIZ_CODEX_BASE_REF=HEAD codex-workers/worker-status.sh --json "$(pwd)"` emitted valid JSON.
- `QUIZ_CODEX_STATUS_FORMAT=bad codex-workers/worker-status.sh "$(pwd)"` exited 64 with the expected validation error.
- `codex-workers/verify-worker-ledger.sh codex-workers/worker-ledger.tsv`
- `codex-workers/verify-source-manifest.sh "$(pwd)"`
- `codex-workers/verify-external-artifacts.sh "$(pwd)"`
- `git diff --check`

## Commits

- `be701d3` - Add JSON worker status output
