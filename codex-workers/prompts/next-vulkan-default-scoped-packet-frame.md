Long-lived Vulkan Backend worker. Keep this session alive after finishing.

Refresh safely:
- `git fetch origin`
- Start fresh branch `codex/vulkan-default-scoped-packet-frame-20260516` from `origin/codex/quiz-vulkan-remake-baseline`.
- Previous `c90e84f` is integrated as `f783f6f`; do not reapply it.

Read `/mnt/c/aa/AGENTS.md` and `apps/quiz/quiz-vulkan/src/render/vulkan/AGENTS.md`.

Scope only:
- `apps/quiz/quiz-vulkan/src/render/vulkan/*`
- `apps/quiz/quiz-vulkan/tests/render/vulkan/*`

Task:
- Next narrow step: make backend frame submission attach scoped packet execution evidence by default when the existing frame data already has a ready render-pass scope/packet bridge path.
- Keep it fake/data-testable and backend-owned. No app, scene, UI, layout, domain, input, image, text, asset, platform, audio, CMake, or aggregate contract edits.
- Do not change CPU fallback rendering behavior or require a real GPU/window/smoke env var.
- If default attachment is not possible without a broader contract change, implement the narrow backend readiness/proposal evidence instead and explain the blocker.

Verify:
- Build `quiz_vulkan_interface_contract_compile_tests`.
- Run touched Vulkan tests plus renderer, command packet, command-buffer recording, render-pass readiness, frame handoff.
- `git diff --check`.
- Commit scoped files and report hash, files, verification, deps, risks.
