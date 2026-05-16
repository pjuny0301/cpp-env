You are the long-lived Image/Texture worker. Keep this session alive after finishing.

Refresh safely:
- `git fetch origin`
- Start a fresh branch `codex/image-resource-packet-consumption-diff-20260517`
  from `origin/codex/quiz-vulkan-remake-baseline`.
- Do not stack this on previous ahead worker branches.

Read:
- `/mnt/c/aa/AGENTS.md`
- `apps/quiz/quiz-vulkan/src/render/image/AGENTS.md`
- `apps/quiz/quiz-vulkan/src/render/image/image_texture_frame_resource_packet_materialization.h`
- `apps/quiz/quiz-vulkan/src/render/image/image_texture_frame_binding_summary.h`
- `apps/quiz/quiz-vulkan/tests/render/image/image_texture_frame_resource_packet_materialization_tests.cpp`
- `apps/quiz/quiz-vulkan/tests/render/image/image_engine_interface_contract_tests.cpp`

Scope only:
- `apps/quiz/quiz-vulkan/src/render/image/*`
- `apps/quiz/quiz-vulkan/tests/render/image/*`

Do not edit:
- `src/app/*`, `app.cpp`, `main.cpp`
- top-level `CMakeLists.txt`
- scene, domain, UI, input, audio, asset, platform, text, or Vulkan files
- aggregate test registration or build system files

Task:
- Add an image-owned renderer-boundary consumption diff/summary over
  `render_image_texture_frame_resource_packet_materialization`.
- This is not a Vulkan implementation. Keep it data-only and image-owned.
- The goal is to make later renderer/Vulkan integration able to tell whether a frame's image resource packet identities changed without rereading the full cache/upload/sampler records.
- Include compact evidence for:
  - added/removed materialized texture packets;
  - changed stable texture cache key, texture id/revision, texture extent, sampler key, upload request/generation, uploaded byte count, placeholder flag, and readiness/blocker status;
  - regression/improvement counts when a packet moves ready -> blocked or blocked -> ready;
  - stable no-change frames;
  - duplicate or missing stable packet identity if this can occur at this layer.
- Prefer extending `image_texture_frame_resource_packet_materialization.h` and its focused tests rather than adding a new public header that would require integrator-owned FILE_SET registration. If a new public header is genuinely necessary, stop and report the exact integrator-owned registration instead of editing CMake.
- Do not introduce external dependencies or downloads for this task.

Implementation style:
- Prefer C++23 designated initialization.
- Add interface lock assertions only for image-owned public structs/functions you add.
- Split helpers only if it improves cohesion/reviewability, not solely by line count.

Verification:
- Build `quiz_vulkan_interface_contract_compile_tests`.
- Build and run focused image tests covering the new consumption diff/summary.
- Run `git diff --check`.

Commit:
- Commit only scoped files.
- Report changed files, verification, external dependency use, risks, and commit hash.
