You are the long-lived Vulkan Backend worker. Keep this session alive after finishing.

First update your worktree safely:
- `git fetch origin`
- Start a fresh branch from `origin/codex/quiz-vulkan-remake-baseline`, for example `codex/vulkan-image-view-framebuffer-targets-20260515`.
- Your previous `2381371` work was integrated on main as `f7ac2e1`; do not re-cherry-pick it.

Read:
- `/mnt/c/aa/AGENTS.md`
- `apps/quiz/quiz-vulkan/src/render/vulkan/AGENTS.md`

Scope:
- Only `apps/quiz/quiz-vulkan/src/render/vulkan/*`
- Only `apps/quiz/quiz-vulkan/tests/render/vulkan/*`
- Do not edit app, CMake, scene, UI, layout, domain, input, image, text, assets, platform, audio, or aggregate contract registration.

Task:
- Add the next narrow real-backend-adjacent Vulkan step after swapchain image-view targets.
- Prefer framebuffer/render-target attachment readiness that consumes the existing swapchain image-view target result and existing render-pass/framebuffer readiness contracts.
- Good evidence: `vkCreateFramebuffer`/`vkDestroyFramebuffer` dispatch readiness, per-image framebuffer target lifecycle records, render pass handle readiness, image-view handle readiness, extent/layer/attachment intent, fake create/destroy handle execution, and deterministic blockers for missing symbols, invalid device, missing render pass, missing image view, or extent mismatch.
- Keep it backend-owned and data/fake-testable. Default tests must not require a real window, surface, GPU, or native Vulkan smoke env var.
- Do not include scene/domain/app/UI/input/audio headers.
- Do not change public renderer contracts outside the Vulkan backend surface unless you stop and report a proposal.

Verification:
- Build `quiz_vulkan_interface_contract_compile_tests`.
- Build and run focused Vulkan tests you touch, plus `quiz_vulkan_renderer_tests`.
- Include `quiz_vulkan_vulkan_swapchain_create_plan_tests` if the new path consumes swapchain image-view records.
- Run `git diff --check`.
- Commit only scoped files and report commit hash, changed files, verification, external dependency use, and risks.
