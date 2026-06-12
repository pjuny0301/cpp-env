You are the long-lived Vulkan Backend worker. Keep this tmux session alive after finishing.

Refresh safely:
- `git fetch origin`
- Start a fresh branch `codex/vulkan-descriptor-update-execution-evidence-20260531`
  from `origin/codex/quiz-vulkan-remake-baseline`.
- Do not stack this on old ahead worker branches.

Read:
- `/mnt/c/aa/AGENTS.md`
- `apps/quiz/quiz-vulkan/src/render/vulkan/AGENTS.md`
- existing Vulkan descriptor update/write/bind call evidence and command packet
  execution tests.

Scope only:
- `apps/quiz/quiz-vulkan/src/render/vulkan/*`
- `apps/quiz/quiz-vulkan/tests/render/vulkan/*`

Do not edit app, UI, layout, scene, domain, input, audio, text, image, asset,
top-level CMake, or aggregate contract registration.

Task:
- Move Vulkan one step closer to real descriptor update execution behind
  renderer-owned Vulkan contracts.
- Current baseline tracks descriptor payload binds, native write/bind call
  evidence, and descriptor update command path evidence. Add the next narrow
  Vulkan-owned execution evidence that consumes completed descriptor update
  command data and fake/native function table availability to produce a stable
  per-frame descriptor update execution summary.
- Prove these paths:
  - update command is executable when all handles/symbols/evidence are present;
  - blocks when descriptor set, image view, sampler, layout, or update symbol is
    missing/invalid;
  - blocks on stale/duplicated/wrong packet evidence;
  - preserves packet ids and descriptor set ids without image/text/app semantics.
- Keep CPU fallback intact. Do not fabricate real Vulkan success unless the new
  API explicitly receives complete fake/native evidence.
- If real allocator/update integration needs broader renderer contracts, report
  the exact integrator-owned follow-up.

Implementation style:
- Prefer C++23 designated initialization.
- Add interface lock assertions only for Vulkan-owned public structs/functions
  you add.
- Do not add external dependencies.

Verification:
- Build `quiz_vulkan_interface_contract_compile_tests`.
- Run focused Vulkan descriptor/update/command packet tests and
  `quiz_vulkan_renderer_tests` if renderer boundary includes changed.
- Run `git diff --check`.

Commit scoped files and report changed files, verification, risks, and commit
hash.
