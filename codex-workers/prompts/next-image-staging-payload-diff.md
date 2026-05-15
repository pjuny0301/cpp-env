Start a fresh Image Engine task from the latest pushed baseline.

Baseline setup:
- `git fetch origin`
- create/switch to a fresh branch from `origin/codex/quiz-vulkan-remake-baseline`, suggested name `codex/image-staging-payload-diff-20260515`
- Do not reuse a previous ahead branch for new work.

Scope:
- Only `apps/quiz/quiz-vulkan/src/render/image/*` and `apps/quiz/quiz-vulkan/tests/render/image/*`.
- Do not edit app/main/top-level CMake/aggregate contract registration.
- Do not include Vulkan/scene/UI/app/domain/input/audio headers.

Task:
- Continue from image staging payload plans.
- Add compact diff/regression evidence for staging payload plans between frames or upload results: row-copy count changes, alignment/padding changes, total staging byte deltas, cache/sampler identity changes, mip-level readiness changes, and blocker transitions.
- Keep this image-owned and data-only; do not create Vulkan buffers or call Vulkan.

Verification:
- Build `quiz_vulkan_interface_contract_compile_tests`, `quiz_vulkan_fake_image_texture_uploader_tests`, `quiz_vulkan_image_texture_pipeline_tests`, and any new/changed focused image tests.
- Run the relevant focused image tests and `git diff --check`.
- Commit scoped files and report hash, changed files, verification, and risks.
