# Worker Ledger Schema

`codex-workers/worker-ledger.tsv` is the machine-readable worker session ledger.
Rows are append-only checkpoints; later rows with the same `session` supersede
earlier rows for status dashboards.

Required tab-separated columns:

| Column | Required value |
| --- | --- |
| `session` | Stable worker session id. |
| `prompt_id` | Prompt or task id assigned to the worker. |
| `role` | Worker role such as `track-b`, `text-engine`, or `asset-system`. |
| `worktree` | Absolute worktree path used by the worker. |
| `branch` | Git branch checked out in that worktree. |
| `base_sha` | Full or short SHA resolved from the selected base before branch creation. |
| `status` | One of `planned`, `active`, `resumed`, `blocked`, `done`, `abandoned`. |
| `current_task` | Current work item, for example `B2 start-worker-task`. |
| `blocker` | `none` when clear, otherwise a concise blocking condition. |
| `last_heartbeat_utc` | UTC timestamp in `YYYY-MM-DDTHH:MM:SSZ` format. |

Scripts must preserve the header exactly and append a full row whenever they
start, resume, block, or complete a worker task.
