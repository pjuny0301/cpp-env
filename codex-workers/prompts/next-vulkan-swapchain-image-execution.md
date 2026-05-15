Start a fresh Vulkan Backend task from the latest pushed baseline.

Baseline setup:
- `git fetch origin`
- create/switch to a fresh branch from `origin/codex/quiz-vulkan-remake-baseline`, suggested name `codex/vulkan-swapchain-image-execution-20260515`
- Do not reuse a previous ahead branch for new work.

Scope:
- Only `apps/quiz/quiz-vulkan/src/render/vulkan/*` and `apps/quiz/quiz-vulkan/tests/render/vulkan/*`.
- Do not edit app/main/top-level CMake/aggregate contract registration.
- Do not include scene/UI/app/domain/input/audio headers.

Task:
- Continue from native swapchain create/destroy adapter boundary.
- Add the next narrow native/fake execution boundary for swapchain image enumeration and acquire using a created swapchain handle.
- Preserve dispatch readiness, swapchain handle, image-count/image-handle evidence, acquire timeout/result/suboptimal/out-of-date diagnostics, and deterministic fake results.
- Default tests must not require a real window, real surface, or GPU.

Verification:
- Build `quiz_vulkan_interface_contract_compile_tests`, `quiz_vulkan_vulkan_swapchain_create_plan_tests`, `quiz_vulkan_vulkan_native_frame_operation_tests`, `quiz_vulkan_renderer_tests`, and any new/changed focused Vulkan tests.
- Run the relevant focused Vulkan tests and `git diff --check`.
- Commit scoped files and report hash, changed files, verification, and risks.
