Start a fresh Vulkan Backend task from the latest pushed baseline.

Baseline setup:
- `git fetch origin`
- create/switch to a fresh branch from `origin/codex/quiz-vulkan-remake-baseline`, suggested name `codex/vulkan-surface-capability-readiness-20260515`
- Do not reuse the previous ahead commit branch for new work.

Scope:
- Only `apps/quiz/quiz-vulkan/src/render/vulkan/*` and `apps/quiz/quiz-vulkan/tests/render/vulkan/*`.
- Do not edit app/main/top-level CMake/aggregate contract registration.
- Do not include scene/UI/app/domain/input/audio headers.

Task:
- Add the next narrow native Vulkan surface/swapchain readiness step after device extension readiness.
- Prefer data-only capability evidence for supplied opaque surface handles: surface support, surface capabilities, surface formats, and present modes.
- Use dispatch-table/fake-function style so default tests do not require a real window, real surface, or GPU.
- Gate future swapchain creation on required surface query readiness and preserve diagnostic reasons for missing symbols, missing surface handle, unsupported present queue, zero formats, and zero present modes.
- Keep renderer CPU fallback intact.

Verification:
- Build `quiz_vulkan_interface_contract_compile_tests`, `quiz_vulkan_vulkan_device_readiness_tests`, `quiz_vulkan_renderer_tests`, and any new/changed Vulkan focused tests.
- Run the relevant focused Vulkan tests and `git diff --check`.
- Commit scoped files and report hash, changed files, verification, and risks.
