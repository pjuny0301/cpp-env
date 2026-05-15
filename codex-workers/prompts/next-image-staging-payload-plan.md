Start a fresh Image Engine task from the latest pushed baseline.

Baseline setup:
- `git fetch origin`
- create/switch to a fresh branch from `origin/codex/quiz-vulkan-remake-baseline`, suggested name `codex/image-staging-payload-plan-20260515`
- Do not reuse the previous ahead commit branch for new work.

Scope:
- Only `apps/quiz/quiz-vulkan/src/render/image/*` and `apps/quiz/quiz-vulkan/tests/render/image/*`.
- Do not edit app/main/top-level CMake/aggregate contract registration.
- Do not include Vulkan/scene/UI/app/domain/input/audio headers.

Task:
- Continue from upload payload layout evidence.
- Add a narrow data-only staging payload plan for decoded texture bytes before Vulkan upload: row copies, row padding/alignment evidence, total staging bytes, mip-level references when present, sampler/cache identity, and blocker diagnostics.
- Keep this image-owned; do not create Vulkan buffers or call Vulkan.
- Preserve placeholder/fallback behavior when decoded bytes are missing or layout evidence is invalid.

Verification:
- Build `quiz_vulkan_interface_contract_compile_tests`, `quiz_vulkan_fake_image_texture_uploader_tests`, `quiz_vulkan_image_texture_pipeline_tests`, and any new/changed focused image tests.
- Run the relevant focused image tests and `git diff --check`.
- Commit scoped files and report hash, changed files, verification, and risks.
