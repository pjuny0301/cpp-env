You are the long-lived Vulkan Backend worker. Keep this session alive after finishing.

First update your worktree safely:
- `git fetch origin`
- Start a fresh branch from `origin/codex/quiz-vulkan-remake-baseline`, for example `codex/vulkan-framebuffer-command-recording-20260515`.
- Your previous `cd9c732` work was integrated on main as `39a3692`; do not re-cherry-pick it.

Read:
- `/mnt/c/aa/AGENTS.md`
- `apps/quiz/quiz-vulkan/src/render/vulkan/AGENTS.md`

Scope:
- Only `apps/quiz/quiz-vulkan/src/render/vulkan/*`
- Only `apps/quiz/quiz-vulkan/tests/render/vulkan/*`
- Do not edit app, CMake, scene, UI, layout, domain, input, image, text, assets, platform, audio, or aggregate contract registration.

Task:
- Add the next narrow Vulkan backend step after framebuffer target readiness.
- Prefer command-recording/render-pass-begin readiness that consumes ready framebuffer target evidence and existing command recording / render pass contracts.
- Good evidence: selected framebuffer target index/handle, render pass handle, framebuffer extent, command buffer readiness, begin-render-pass/end-render-pass dispatch or fake lifecycle records, deterministic blockers for missing command buffer, missing render pass, missing framebuffer, extent mismatch, or command recording dispatch unready.
- Keep it backend-owned and data/fake-testable. Default tests must not require a real window, surface, GPU, or native smoke env var.
- Do not include scene/domain/app/UI/input/audio headers.
- Do not route quiz semantics or renderer draw-list ownership into this layer.
- If actual `vkCmdBeginRenderPass` wiring would require a broader public contract change, stop and report a narrow proposal instead of editing outside Vulkan.

Verification:
- Build `quiz_vulkan_interface_contract_compile_tests`.
- Build and run focused Vulkan tests you touch, plus `quiz_vulkan_renderer_tests`.
- Include render-pass/framebuffer tests if the new path consumes framebuffer records.
- Run `git diff --check`.
- Commit only scoped files and report commit hash, changed files, verification, external dependency use, and risks.
