You are the long-lived Vulkan Backend worker. Keep this session alive after finishing.

First update your worktree safely:
- `git fetch origin`
- Start a fresh branch from `origin/codex/quiz-vulkan-remake-baseline`, for example `codex/vulkan-scoped-command-packet-execution-20260516`.
- Your previous `9e8ec95` work was integrated on main as `348dfb0`; do not re-cherry-pick it.

Read:
- `/mnt/c/aa/AGENTS.md`
- `apps/quiz/quiz-vulkan/src/render/vulkan/AGENTS.md`

Scope:
- Only `apps/quiz/quiz-vulkan/src/render/vulkan/*`
- Only `apps/quiz/quiz-vulkan/tests/render/vulkan/*`
- Do not edit app, CMake, scene, UI, layout, domain, input, image, text, assets, platform, audio, or aggregate contract registration.

Task:
- Add the next narrow Vulkan backend step after render-pass scope recording readiness.
- Prefer scoped command-packet execution evidence that composes the existing render-pass begin/end scope with the existing command packet execution/recorder path.
- Good evidence: selected render-pass scope id, framebuffer target id/handle, command buffer handle, packet bridge readiness, per-batch packet counts by category, first packet failure inside scope, end-render-pass skipped after begin failure, and successful empty-scope vs scoped-packet execution summaries.
- Keep this backend-owned and data/fake-testable. Default tests must not require a real window, surface, GPU, or native smoke env var.
- Do not include scene/domain/app/UI/input/audio headers.
- Do not route quiz semantics or renderer draw-list ownership into this layer; consume only existing Vulkan command packet/recording data contracts.
- If a broader renderer contract change is required, stop and report a narrow proposal instead of editing outside Vulkan.

Verification:
- Build `quiz_vulkan_interface_contract_compile_tests`.
- Build and run focused Vulkan tests you touch, plus `quiz_vulkan_renderer_tests`.
- Include command-buffer recording, command packet execution, and render-pass readiness tests if the new path composes those records.
- Run `git diff --check`.
- Commit only scoped files and report commit hash, changed files, verification, external dependency use, and risks.
