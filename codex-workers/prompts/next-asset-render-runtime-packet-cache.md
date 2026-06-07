You are the long-lived Asset System worker. Keep this tmux session alive after finishing.

Refresh safely:
- `git fetch origin`
- Start a fresh branch `codex/asset-render-runtime-packet-cache-20260531`
  from `origin/codex/quiz-vulkan-remake-baseline`.
- Do not stack this on old ahead worker branches.

Read:
- `/mnt/c/aa/AGENTS.md`
- `apps/quiz/quiz-vulkan/src/assets/AGENTS.md`
- existing runtime catalog, runtime resolver policy, materialized byte cache,
  shader payload, and generic runtime payload manifest tests.

Scope only:
- `apps/quiz/quiz-vulkan/src/assets/*`
- `apps/quiz/quiz-vulkan/tests/assets/*`

Do not edit app, render/text/image/Vulkan/input/audio, top-level CMake, or
aggregate contract registration.

Task:
- Move asset runtime one step closer to real renderer resource consumption.
- Current baseline can summarize shader runtime payloads, generic runtime
  payload manifests, and materialized byte cache invalidation. Add a narrow
  asset-owned runtime cache handoff for renderer resource packet inputs.
- The handoff should classify and preserve:
  - asset kind, logical id, revision, content hash;
  - host-path-free runtime identity;
  - materialized byte availability and byte count;
  - cache hit/replaced/invalidated state;
  - ready vs blocked state and blocker reason;
  - manifest compatibility/version evidence.
- Keep the API generic enough for font/image/sound/shader/deck assets, but do
  not build a large framework. One small data path with focused tests is enough.
- If render-owned consumers need CMake/app wiring, report the exact follow-up
  instead of editing outside `src/assets` and `tests/assets`.

Implementation style:
- Prefer C++23 designated initialization.
- Do not download or duplicate dependencies unless existing approved external
  artifacts cannot support the test fixture.

Verification:
- Build `quiz_vulkan_interface_contract_compile_tests`.
- Run focused asset tests covering runtime catalog/resolver/cache/manifest
  behavior.
- Run `git diff --check`.

Commit scoped files and report changed files, verification, risks, and commit
hash.
