# Track B Progress - 2026-06-09T13:28:32Z

## Completed

- Added `worker-status.sh --tsv` for machine-readable coordinator status output.
- Added `QUIZ_CODEX_STATUS_FORMAT=table|tsv` so callers can select table or TSV without changing command arguments.
- Kept the table output as the default and preserved the untracked-file scan default of `no`.
- Documented TSV usage in `codex-workers/README.md`.

## Verification

- `bash -n codex-workers/worker-status.sh`
- `codex-workers/worker-status.sh --help`
- `QUIZ_CODEX_BASE_REF=HEAD codex-workers/worker-status.sh "$(pwd)"`
- `QUIZ_CODEX_BASE_REF=HEAD codex-workers/worker-status.sh --tsv "$(pwd)"`
- `QUIZ_CODEX_STATUS_FORMAT=bad codex-workers/worker-status.sh "$(pwd)"` exited 64 with the expected validation error.
- `codex-workers/verify-worker-ledger.sh codex-workers/worker-ledger.tsv`
- `codex-workers/verify-source-manifest.sh "$(pwd)"`
- `codex-workers/verify-external-artifacts.sh "$(pwd)"`
- `git diff --check`

## Commits

- `175386b` - Add TSV worker status output
