You are the long-lived Vulkan Backend worker. Keep this session alive after finishing.

First update your worktree safely:
- `git fetch origin`
- Start a fresh branch from `origin/codex/quiz-vulkan-remake-baseline`, for example `codex/vulkan-scoped-packet-frame-summary-20260516`.
- Your previous `2ff259e` work was integrated on main as `d5ffb54`; do not re-cherry-pick it.

Read:
- `/mnt/c/aa/AGENTS.md`
- `apps/quiz/quiz-vulkan/src/render/vulkan/AGENTS.md`

Scope:
- Only `apps/quiz/quiz-vulkan/src/render/vulkan/*`
- Only `apps/quiz/quiz-vulkan/tests/render/vulkan/*`
- Do not edit app, CMake, scene, UI, layout, domain, input, image, text, assets, platform, audio, or aggregate contract registration.

Task:
- Add the next narrow Vulkan backend step after scoped command-packet execution summaries.
- Thread `vulkan_scoped_command_packet_execution_result` into the backend frame result / frame handoff summaries so a submitted Vulkan frame reports whether command packets executed inside the selected render-pass scope.
- Good evidence: scoped execution checked/ready/status/fallback, render-pass scope id, selected framebuffer target/index/handle, command buffer handle, planned/attempted/executed packet counts, per-category packet counts, first failed packet evidence, empty-scope completion, and whether frame-level `commands_recorded` was gated by scoped execution.
- Keep this backend-owned and data/fake-testable. Default tests must not require a real window, surface, GPU, or native smoke env var.
- Do not include scene/domain/app/UI/input/audio headers.
- Do not route quiz semantics or renderer draw-list ownership into this layer; consume only existing Vulkan frame plan, command packet, render-pass scope, and frame-result data contracts.
- If this requires a breaking renderer summary contract change outside `src/render/vulkan`, stop and report a narrow proposal instead of editing outside Vulkan.

Verification:
- Build `quiz_vulkan_interface_contract_compile_tests`.
- Build and run focused Vulkan tests you touch, plus `quiz_vulkan_renderer_tests`.
- Include command packet execution, command-buffer recording, render-pass readiness, frame pipeline/handoff, and renderer tests if the new result threads through those records.
- Run `git diff --check`.
- Commit only scoped files and report commit hash, changed files, verification, external dependency use, and risks.
