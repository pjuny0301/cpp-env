Continue in the same long-lived Vulkan backend session, but start from the
latest pushed integration baseline before editing.

Task: advance the real Vulkan frame path one step past the current swapchain
image acquire handoff evidence.

Fresh branch:
- Fetch origin.
- Create/switch to a new branch from `origin/codex/quiz-vulkan-remake-baseline`.
- Suggested branch: `codex/vulkan-present-image-execution-20260518`.

Ownership:
- You may edit only:
  - `apps/quiz/quiz-vulkan/src/render/vulkan/*`
  - `apps/quiz/quiz-vulkan/tests/render/vulkan/*`
- Do not edit `src/app/*`, `app.cpp`, `main.cpp`, top-level `CMakeLists.txt`,
  aggregate interface registration, or non-Vulkan engine folders.
- If a CMake or app wiring change is required, report the exact proposal instead
  of editing it.

Goal:
- Model and implement the next Vulkan-owned operation after swapchain acquire:
  present/finish evidence that connects acquired image index, queue family,
  command/submit readiness, present wait/signal intent, and recoverable failure
  state.
- Keep this behind Vulkan-owned structs/functions. Do not make Vulkan depend on
  scene/UI/app/domain/input/audio.
- Prefer implementing real operation planning and testable packet/evidence
  helpers over adding broad diagnostics with no execution consequence.
- Use existing Vulkan loader/device/swapchain/native-frame structures where
  possible. Do not duplicate approved external Vulkan headers or SDK files.

Verification:
- Build `quiz_vulkan_interface_contract_compile_tests`.
- Build and run focused Vulkan tests, including any new test and at least:
  `quiz_vulkan_vulkan_native_frame_operation_tests`,
  `quiz_vulkan_vulkan_present_completion_plan_tests`,
  `quiz_vulkan_vulkan_queue_submit_adapter_tests`,
  `quiz_vulkan_vulkan_swapchain_create_plan_tests`.
- Run `git diff --check`.

Commit:
- Commit scoped changes on your branch.
- Final report: changed files, exact verification commands/results, risks,
  whether integrator-owned CMake/app wiring is needed, and commit hash.
