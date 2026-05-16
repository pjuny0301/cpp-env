You are the long-lived Text Engine worker. Keep this session alive after finishing.

Refresh safely:
- `git fetch origin`
- Start a fresh branch `codex/text-resource-packet-consumption-diff-20260517`
  from `origin/codex/quiz-vulkan-remake-baseline`.
- Do not stack this on previous ahead worker branches.

Read:
- `/mnt/c/aa/AGENTS.md`
- `apps/quiz/quiz-vulkan/src/render/text/AGENTS.md`
- `apps/quiz/quiz-vulkan/src/render/text/text_frame_upload_handoff.h`
- `apps/quiz/quiz-vulkan/tests/render/text/text_frame_resource_packet_materialization_tests.cpp`
- `apps/quiz/quiz-vulkan/tests/render/text/text_engine_interface_contract_tests.cpp`

Scope only:
- `apps/quiz/quiz-vulkan/src/render/text/*`
- `apps/quiz/quiz-vulkan/tests/render/text/*`

Do not edit:
- `src/app/*`, `app.cpp`, `main.cpp`
- top-level `CMakeLists.txt`
- scene, domain, UI, input, audio, image, asset, platform, or Vulkan files
- aggregate test registration or build system files

Task:
- Add a text-owned renderer-boundary consumption diff/summary over
  `render_text_frame_resource_packet_materialization`.
- This is not renderer or Vulkan implementation. Keep it data-only and text-owned.
- The goal is to let later renderer/Vulkan integration tell whether text atlas resource packet identities changed without rereading every full packet/page record.
- Include compact evidence for:
  - added/removed resource packets;
  - changed stable packet key, atlas page id/revision, page extent, sampler key, UV bounds, layout bounds, upload request/operation id, uploaded byte count, fallback/backend flags, and readiness/blocker status;
  - regression/improvement counts when a packet moves ready -> blocked or blocked -> ready;
  - stable no-change frames;
  - duplicate or missing stable resource packet identity if this can occur at this layer.
- Prefer extending `text_frame_upload_handoff.h` and its focused materialization test rather than adding a new public header that would require integrator-owned FILE_SET registration. If a new public header is genuinely necessary, stop and report the exact integrator-owned registration instead of editing CMake.
- Do not introduce external dependencies or downloads for this task.

Implementation style:
- Prefer C++23 designated initialization.
- Add interface lock assertions only for text-owned public structs/functions you add.
- Split helpers only if it improves cohesion/reviewability, not solely by line count.

Verification:
- Build `quiz_vulkan_interface_contract_compile_tests`.
- Build and run focused text tests covering the new consumption diff/summary:
  `quiz_vulkan_text_frame_resource_packet_materialization_tests`
  plus any directly changed text test.
- Run `git diff --check`.

Commit:
- Commit only scoped files.
- Report changed files, verification, external dependency use, risks, and commit hash.
