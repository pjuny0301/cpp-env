You are the long-lived Vulkan Backend worker. Keep this session alive after finishing.

Refresh safely:
- `git fetch origin`
- Start a fresh branch `codex/vulkan-native-descriptor-set-evidence-20260516`
  from `origin/codex/quiz-vulkan-remake-baseline`.
- Do not stack this on previous ahead worker branches.

Read:
- `/mnt/c/aa/AGENTS.md`
- `apps/quiz/quiz-vulkan/src/render/vulkan/AGENTS.md`
- `apps/quiz/quiz-vulkan/src/render/vulkan/vulkan_backend_adapter.h`
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
- Continue from `build_vulkan_native_command_packet_executor_evidence`.
- Current baseline intentionally exposes the descriptor-handle gap: frame evidence can provide command buffer, graphics pipeline, pipeline layout, and viewport, but descriptor sets remain unavailable unless a Vulkan-owned descriptor allocation/upload path supplies native handles.
- Add the next narrow Vulkan-owned data/fake-testable step that supplies descriptor set handle evidence to native command packet execution.
- Prefer a small descriptor set allocation/handle evidence result and a merge/build function that consumes completed `vulkan_command_packet_bridge_result` / resource binding evidence and produces `vulkan_native_command_packet_descriptor_set` records with stable handles from a fake allocator.
- Keep the default `build_vulkan_native_command_packet_executor_evidence(frame, native_functions)` honest: it must not fabricate descriptor handles by itself unless your new API explicitly receives completed allocation evidence.
- Prove the native executor completes when descriptor evidence is provided, and still blocks when descriptor evidence is missing, incomplete, duplicated, or tied to the wrong packet.
- Keep it backend-owned. Do not pull image/text/asset semantics into Vulkan; resource ids and binding/set metadata are enough.
- If a real `VkDescriptorSet` allocator requires broader renderer/resource upload contracts, implement the narrow fake/data evidence and report the exact integrator-owned follow-up.

Implementation style:
- Prefer C++23 designated initialization.
- Add interface lock assertions only for Vulkan-owned public structs/functions you add.
- Split helpers only when it improves cohesion/reviewability, not solely by line count.
- Do not use or duplicate external dependencies for this task.

Verification:
- Build `quiz_vulkan_interface_contract_compile_tests`.
- Build and run focused Vulkan tests covering the descriptor evidence path:
  `quiz_vulkan_vulkan_command_packet_execution_tests`
  and any new/changed Vulkan test.
- Run `quiz_vulkan_renderer_tests` and `quiz_vulkan_architecture_boundary_tests` if includes or boundaries are touched.
- Run `git diff --check`.

Commit:
- Commit only scoped files.
- Report changed files, verification, risks, and commit hash.
