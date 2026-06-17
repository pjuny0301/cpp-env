# Track B Progress - 2026-06-09T15:25:31Z

## Completed

- Added `codex-workers/worker-status.schema.md` to document the stable table, TSV, and JSON fields emitted by `worker-status.sh`.
- Linked the schema from `codex-workers/README.md`.
- Confirmed JSON status output remains parseable and keeps row values as strings for downstream shell/jq consumers.

## Verification

- `bash -n codex-workers/worker-status.sh`
- `shellcheck codex-workers/worker-status.sh`
- `QUIZ_CODEX_BASE_REF=HEAD codex-workers/worker-status.sh "$(pwd)"`
- `QUIZ_CODEX_BASE_REF=HEAD codex-workers/worker-status.sh --tsv "$(pwd)"`
- `QUIZ_CODEX_BASE_REF=HEAD codex-workers/worker-status.sh --json "$(pwd)"`
- `python3 -m json.tool /tmp/track-b-worker-status.json`
- `git diff --check`

## Commits

- `47fbdba` - Document worker status output schema
