Start a fresh Vulkan Backend task from the latest pushed baseline.

Baseline setup:
- `git fetch origin`
- create/switch to a fresh branch from `origin/codex/quiz-vulkan-remake-baseline`, suggested name `codex/vulkan-swapchain-image-view-targets-20260515`
- Do not reuse a previous ahead branch for new work.

Scope:
- Only `apps/quiz/quiz-vulkan/src/render/vulkan/*` and `apps/quiz/quiz-vulkan/tests/render/vulkan/*`.
- Do not edit app/main/top-level CMake/aggregate contract registration.
- Do not include scene/UI/app/domain/input/audio headers.
- If a file is becoming too broad, split only cohesive private Vulkan backend helpers; do not move stable public interfaces without integrator approval.

Task:
- Continue from native swapchain create, image enumeration, and acquire execution boundaries.
- Add the next narrow native/fake boundary for swapchain image-view / render-target attachment readiness.
- Preserve evidence for swapchain image handle, image index/id, selected image format, extent, usage/aspect intent, image-view handle, lifecycle status, and per-image readiness.
- If Vulkan headers/symbols are available, model `vkCreateImageView` / `vkDestroyImageView` through the Vulkan backend dispatch layer; otherwise keep a deterministic fake/data-only path that makes missing symbol/readiness reasons explicit.
- Do not create real windows, real surfaces, or GPU-required default tests.
- Do not couple this to render draw-list semantics beyond prepared renderer-owned image/render-target data.

Verification:
- Build `quiz_vulkan_interface_contract_compile_tests`, `quiz_vulkan_vulkan_swapchain_create_plan_tests`, `quiz_vulkan_vulkan_native_frame_operation_tests`, `quiz_vulkan_renderer_tests`, and any new/changed focused Vulkan tests.
- Run the relevant focused Vulkan tests and `git diff --check`.
- Commit scoped files and report hash, changed files, verification, external dependency use, and risks.
