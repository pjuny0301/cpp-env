# Worker Status Output Schema

`codex-workers/worker-status.sh` emits coordinator status in `table`, `tsv`,
or `json` format. The formats carry the same logical rows:

| Field | Meaning |
| --- | --- |
| `kind` | `main` for the inspected repo, `worker` for a Codex tmux pane. |
| `session` | Tmux session name for workers, `-` for the main repo row. |
| `state` | Worker state: `busy`, `idle`, `unknown`, or `-` for the main repo row. |
| `queued` | Count of queued prompt files for the worker, or `-` for main. |
| `command` | Current pane command, or `-` for main. |
| `path` | Repo or pane working directory inspected for git status. |
| `branch` | Current branch name, `detached`, or `-` when the path is not a git worktree. |
| `ahead` | Commit count that `HEAD` is ahead of `QUIZ_CODEX_BASE_REF`, or `-`. |
| `behind` | Commit count that `HEAD` is behind `QUIZ_CODEX_BASE_REF`, or `-`. |
| `dirty` | Count of dirty paths from `git status --porcelain` using the selected untracked mode, or `-`. |
| `head` | Short `HEAD` SHA, or `-`. |

## TSV

TSV output starts with this exact header:

```text
kind	session	state	queued	command	path	branch	ahead	behind	dirty	head
```

The first data row is always `kind=main`. Worker rows only include tmux
sessions whose names start with `codex-`.

## JSON

JSON output is an object with stable top-level keys:

```json
{
  "base_ref": "HEAD",
  "status_untracked": "no",
  "main": {
    "kind": "main",
    "session": "-",
    "state": "-",
    "queued": "-",
    "command": "-",
    "path": "/repo",
    "branch": "main",
    "ahead": "0",
    "behind": "0",
    "dirty": "0",
    "head": "0000000"
  },
  "workers": []
}
```

All JSON row values are strings so shell and jq callers can consume table, TSV,
and JSON fields without type-specific branching. `workers` is an array and may
be empty when no matching tmux panes are running.
