You are the long-lived Vulkan Backend worker. Keep this session alive after finishing.

Refresh safely:
- `git fetch origin`
- Start a fresh branch `codex/vulkan-image-resource-descriptor-materialization-20260516`
  from `origin/codex/quiz-vulkan-remake-baseline`.
- Do not stack this on previous ahead worker branches.

Read:
- `/mnt/c/aa/AGENTS.md`
- `apps/quiz/quiz-vulkan/src/render/vulkan/AGENTS.md`
- `apps/quiz/quiz-vulkan/src/render/vulkan/vulkan_backend_adapter.h`
- `apps/quiz/quiz-vulkan/src/render/vulkan/vulkan_backend_adapter.cpp`
- `apps/quiz/quiz-vulkan/src/render/vulkan/vulkan_backend_resource_binding.cpp`
- `apps/quiz/quiz-vulkan/src/render/image/image_texture_frame_resource_packet_materialization.h`
- `apps/quiz/quiz-vulkan/tests/render/vulkan/vulkan_command_packet_execution_tests.cpp`

Scope only:
- `apps/quiz/quiz-vulkan/src/render/vulkan/*`
- `apps/quiz/quiz-vulkan/tests/render/vulkan/*`

Do not edit:
- `src/app/*`, `app.cpp`, `main.cpp`
- top-level `CMakeLists.txt`
- scene, domain, UI, input, audio, text, image, asset, platform files
- aggregate test registration or build system files

Task:
- Continue after the descriptor allocation evidence work now integrated on baseline.
- Current resource binding can still treat an image URI as available before the image texture pipeline has materialized cache/upload/sampler handoff records.
- Add the next narrow Vulkan-owned bridge that can consume image frame resource materialization evidence before native descriptor handle allocation.
- Keep the default Vulkan frame path honest: existing APIs must not silently fabricate image texture readiness.
- Prefer an overload/result/helper that proves descriptor allocation can be blocked by missing or blocked image materialization evidence, and can proceed when matching materialized image cache/upload/sampler records are present.
- Do not pull image decoding, texture upload, sampler creation, or asset lookup into Vulkan. Vulkan may consume renderer boundary evidence and stable resource ids only.
- Keep descriptor set allocation fake/data-testable. If real `VkWriteDescriptorSet` or sampler/image-view handles need broader contracts, report that as an integrator follow-up instead of editing other engines.

Implementation style:
- Prefer C++23 designated initialization.
- Add interface lock assertions only for Vulkan-owned public structs/functions you add.
- Split helpers only when it improves cohesion/reviewability, not solely by line count.
- Do not use or duplicate external dependencies for this task.

Verification:
- Build `quiz_vulkan_interface_contract_compile_tests`.
- Build and run focused Vulkan tests covering the image-materialization-to-descriptor path:
  `quiz_vulkan_vulkan_command_packet_execution_tests`
  and any new/changed Vulkan test.
- Run `quiz_vulkan_renderer_tests` and `quiz_vulkan_architecture_boundary_tests` if includes or boundaries are touched.
- Run `git diff --check`.

Commit:
- Commit only scoped files.
- Report changed files, verification, risks, and commit hash.
