You are the long-lived Vulkan Backend worker. Keep this session alive after finishing.

Refresh safely:
- `git fetch origin`
- Start a fresh branch `codex/vulkan-descriptor-write-payload-handoff-20260517`
  from `origin/codex/quiz-vulkan-remake-baseline`.
- Do not stack this on previous ahead worker branches.

Read:
- `/mnt/c/aa/AGENTS.md`
- `apps/quiz/quiz-vulkan/src/render/vulkan/AGENTS.md`
- `apps/quiz/quiz-vulkan/src/render/vulkan/vulkan_backend_adapter.h`
- `apps/quiz/quiz-vulkan/src/render/vulkan/vulkan_backend_adapter.cpp`
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
- Continue after `bd934a2` and `ef822d5`.
- Current fake descriptor allocation can allocate descriptor set handles only after image texture materialization evidence is ready and sampler bindings match materialized sampler handoff records.
- Add the next narrow Vulkan-owned bridge for descriptor write payload handoff.
- Prefer data/fake-testable types that model a descriptor write payload for each packet binding:
  descriptor set handle, set/binding, descriptor kind, resource id, optional image-view handle, sampler handle, image layout, and blocker diagnostics.
- The API should consume completed command packet bridge data, completed descriptor set allocation data, and explicit native image descriptor resource evidence supplied to the API. It must not fabricate image view or sampler handles from image materialization alone.
- Prove it blocks missing descriptor set allocation, missing image descriptor evidence, mismatched texture resource id, mismatched sampler id, duplicate payloads, and incomplete native image handles.
- Prove it completes for matching fake image-view/sampler/layout evidence and carries stable payload records that a later `vkUpdateDescriptorSets` step can consume.
- Keep the default frame/executor path honest: do not silently attach descriptor write payloads unless the new API receives explicit completed payload evidence.
- Do not pull image decoding, texture upload, asset lookup, UI, scene, or domain semantics into Vulkan. Resource ids, sampler keys, descriptor binding metadata, and opaque native handles are enough.
- If real `VkWriteDescriptorSet` calls need broader renderer/resource upload contracts, report that as the next integrator follow-up instead of editing other engines.

Implementation style:
- Prefer C++23 designated initialization.
- Add interface lock assertions only for Vulkan-owned public structs/functions you add.
- Split helpers only when it improves cohesion/reviewability, not solely by line count.
- Do not use or duplicate external dependencies for this task.

Verification:
- Build `quiz_vulkan_interface_contract_compile_tests`.
- Build and run focused Vulkan tests covering descriptor write payload handoff:
  `quiz_vulkan_vulkan_command_packet_execution_tests`
  and any new/changed Vulkan test.
- Run `quiz_vulkan_renderer_tests` and `quiz_vulkan_architecture_boundary_tests` if includes or boundaries are touched.
- Run `git diff --check`.

Commit:
- Commit only scoped files.
- Report changed files, verification, risks, and commit hash.
