# Codex Workers

This project stores the prompts and shell scripts used to run role-specific Codex workers from separate git worktrees.

The workers are meant to implement behind existing quiz-vulkan interfaces, not redesign or move those interfaces. The main integration branch remains responsible for review and merge.

## Layout

- `prompts/common.md`: shared rules for all workers.
- `prompts/<role>.md`: role-specific task scope.
- `setup-worktrees.sh`: creates role worktrees under `/mnt/c/aa-workers` by default.
- `run-codex-tmux.sh`: starts one role in a persistent tmux session.
- `send-worker-prompt.sh`: pastes and submits a prompt file into an existing
  persistent tmux worker. When the worker pane looks busy, it queues the prompt
  under `codex-workers/queued/<session>/` instead of pasting into an active
  Codex input box; use `--force` only when you have inspected the pane and want
  immediate submission.
- `configure-quiz-vulkan-worker-build.sh`: configures a worker worktree build
  while pointing CMake at the central approved dependency checkout under
  `/mnt/c/aa/build/external/lib/cpp/desktop`.
- `quiz-vulkan-worker-build-dir.sh`: prints the worker-local build directory in
  a path format accepted by Windows CTest.
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

Send follow-up work to a long-lived worker without retyping a large prompt:

```bash
cat > /tmp/text-next.md <<'EOF'
Continue in this same long-lived session, but start a fresh branch from the
latest pushed baseline before editing. Keep edits inside the assigned engine
folder and focused tests. Commit scoped files and report the hash.
EOF
/mnt/c/aa/codex-workers/send-worker-prompt.sh codex-text-engine /tmp/text-next.md
```

If the Codex terminal UI leaves a long pasted prompt in the input box, tune
`QUIZ_CODEX_WORKER_SUBMIT_ENTER_COUNT` rather than manually editing the script.
The default sends four Enter keystrokes after paste because the UI sometimes
keeps the first one or two as multiline input.

If the worker is still processing a previous task, the command writes a queued
prompt file and leaves the tmux session untouched. After the worker reports idle,
send the queued file explicitly:

```bash
/mnt/c/aa/codex-workers/send-worker-prompt.sh \
  codex-text-engine \
  /mnt/c/aa/codex-workers/queued/codex-text-engine/<queued-prompt>.md
```

Configure a worker-local quiz-vulkan build with the central external
dependencies:

```bash
/mnt/c/aa/codex-workers/configure-quiz-vulkan-worker-build.sh \
  /mnt/c/aa-workers/text-engine \
  windows-mingw-ascii
```

This keeps build output inside the worker worktree's `build/out` while avoiding
duplicate or missing FreeType, HarfBuzz, Vulkan, stb, utf8proc, and miniaudio
source snapshots.

Build and test the worker-local tree:

```bash
cd /mnt/c/aa-workers/text-engine/apps/quiz/quiz-vulkan
build_dir="$(/mnt/c/aa/codex-workers/quiz-vulkan-worker-build-dir.sh \
  /mnt/c/aa-workers/text-engine \
  windows-mingw-ascii)"
/mnt/c/aa/codex-workers/with-build-lock.sh \
  "/mnt/c/Program Files/CMake/bin/cmake.exe" \
  --build "$build_dir" \
  --target quiz_vulkan_interface_contract_compile_tests

/mnt/c/aa/codex-workers/with-build-lock.sh \
  "/mnt/c/Program Files/CMake/bin/ctest.exe" \
  --test-dir "$build_dir" \
  -R "<focused_test_regex>" \
  --output-on-failure
```

For a compact coordinator view:

```bash
/mnt/c/aa/codex-workers/worker-status.sh /mnt/c/aa
```

Read `dirty`, `ahead`, and `behind` before assigning more work. A long-lived
session is useful when it keeps engine-specific context, but new tasks should
start from the latest pushed baseline when the old worker branch is far behind.

## Current Pipeline Limits

- Long-lived sessions preserve subsystem context, but their checked-out branch
  can drift behind the integration baseline. For each new task, keep the tmux
  session alive while switching the worktree to a fresh branch from
  `origin/codex/quiz-vulkan-remake-baseline`.
- Worker commits are still review inputs, not merge authority. The integrator
  verifies them on Windows MinGW, handles top-level CMake and FILE_SET changes,
  then pushes the baseline before assigning the next batch.
- If a worker only needs engine-local context, reuse the existing session. If a
  task changes ownership boundaries or needs a clean comparison, create a fresh
  branch/worktree and leave the old session available for inspection.
- Do not let workers duplicate external libraries that already exist under
  `build/external`. They should inspect approved external dependencies first,
  consume them behind existing interfaces when possible, and report any
  integrator-owned CMake wiring that remains.

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
