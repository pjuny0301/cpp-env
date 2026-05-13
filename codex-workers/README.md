# Codex Workers

This project stores the prompts and shell scripts used to run role-specific Codex workers from separate git worktrees.

The workers are meant to implement behind existing quiz-vulkan interfaces, not redesign or move those interfaces. The main integration branch remains responsible for review and merge.

## Layout

- `prompts/common.md`: shared rules for all workers.
- `prompts/<role>.md`: role-specific task scope.
- `setup-worktrees.sh`: creates role worktrees under `/mnt/c/aa-workers` by default.
- `run-codex-tmux.sh`: starts one role in a persistent tmux session.
- `with-build-lock.sh`: serializes shared Windows CMake/CTest access so
  parallel workers do not race on the same build directory.
- `worker-status.sh`: summarizes live Codex tmux sessions, current paths, branch
  names, dirty-file counts, and ahead/behind counts versus the integration
  baseline.

## One-Time Setup

From the main repo:

```bash
/mnt/c/aa/codex-workers/setup-worktrees.sh /mnt/c/aa
```

Defaults:

- Worker root: `/mnt/c/aa-workers`
- Base branch: `codex/quiz-vulkan-remake-baseline`
- Role branches: `codex/text-engine`, `codex/image-texture`, `codex/audio-engine`, `codex/input-ime`, `codex/vulkan-backend`, `codex/asset-system`

Override paths if needed:

```bash
QUIZ_CODEX_WORKER_ROOT=/mnt/c/aa-workers \
QUIZ_CODEX_BASE_BRANCH=codex/quiz-vulkan-remake-baseline \
/mnt/c/aa/codex-workers/setup-worktrees.sh /mnt/c/aa
```

## Start A Worker

Use tmux. It leaves a real terminal session alive, so the worker is not killed if this coordinator stops.

```bash
/mnt/c/aa/codex-workers/run-codex-tmux.sh text-engine
/mnt/c/aa/codex-workers/run-codex-tmux.sh image-texture
/mnt/c/aa/codex-workers/run-codex-tmux.sh input-ime
/mnt/c/aa/codex-workers/run-codex-tmux.sh audio-engine
/mnt/c/aa/codex-workers/run-codex-tmux.sh feedback-sync
```

Start `vulkan-backend` and `asset-system` after the lower contracts settle, unless the integrator explicitly wants them in parallel.

## Inspect Workers

```bash
tmux list-sessions
tmux capture-pane -pt codex-text-engine -S -120
tmux attach -t codex-text-engine
```

For a compact coordinator view:

```bash
/mnt/c/aa/codex-workers/worker-status.sh /mnt/c/aa
```

Read `dirty`, `ahead`, and `behind` before assigning more work. A long-lived
session is useful when it keeps engine-specific context, but new tasks should
start from the latest pushed baseline when the old worker branch is far behind.

To detach from a visible tmux worker without stopping it, press:

```text
Ctrl-b d
```

If you want a normal Windows Terminal tab/window, open WSL and run:

```bash
tmux attach -t codex-text-engine
```

The worker continues after the terminal detaches. Do not close it with `exit` unless you intentionally want to stop that worker.

Do not kill worker sessions just because they run long. Let them continue, or leave the tmux session for later inspection.

## Worker Contract

Each worker should:

- stay in its own worktree and branch,
- read root `AGENTS.md` and the nearest subsystem `AGENTS.md`,
- implement behind existing interfaces,
- stop and propose before moving/renaming/changing public interface signatures,
- build `quiz_vulkan_interface_contract_compile_tests`,
- run role-specific tests,
- run `git diff --check`,
- run shared Windows CMake/CTest commands through `with-build-lock.sh`,
- reserve full CTest for app/CMake/public-contract changes, large integration batches, or explicit integrator requests,
- keep all downloaded dependencies, tools, fixtures, fonts, SDK/header drops, and datasets under `/mnt/c/aa/build/external` or the worker worktree's `build/external` equivalent,
- report source URL, version/commit, license, exact local path, and reason for each external item used,
- commit on its role branch,
- treat `src/app/*`, `app.cpp`, `main.cpp`, top-level `CMakeLists.txt`, and aggregate contract registration as integrator-owned unless the prompt explicitly grants that write set,
- propose app/runtime/CMake wiring instead of editing those files from an engine worker,
- report changed files, verification, risks, and commit hash.

## Current Role Names

- `text-engine`
- `image-texture`
- `audio-engine`
- `input-ime`
- `vulkan-backend`
- `asset-system`
- `feedback-sync`

## Feedback Sync Worker

Use `feedback-sync` for review feedback that crosses docs, traceability, and app-level UX routing. This role may edit the specific app router/test files named in its prompt, but engine workers still must not edit `src/app/*`.
