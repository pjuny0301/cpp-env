You are the long-lived Text Engine worker. Keep this session alive after finishing.

Refresh safely:
- `git fetch origin`
- Start a fresh branch `codex/text-atlas-resource-packet-materialization-20260517`
  from `origin/codex/quiz-vulkan-remake-baseline`.
- Do not stack this on previous ahead worker branches.

Read:
- `/mnt/c/aa/AGENTS.md`
- `apps/quiz/quiz-vulkan/src/render/text/AGENTS.md`
- `apps/quiz/quiz-vulkan/src/render/text/text_frame_draw_plan.h`
- `apps/quiz/quiz-vulkan/src/render/text/text_frame_upload_handoff.h`
- `apps/quiz/quiz-vulkan/tests/render/text/font_shaped_atlas_update_tests.cpp`
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
- Continue from the existing text frame snapshot, draw plan, upload result, and upload handoff contracts.
- Add a narrow text-owned resource packet materialization handoff for text atlas pages/glyph draw packets, analogous in spirit to the image frame resource packet materialization but without depending on image or Vulkan folders.
- The handoff should consume text frame draw plan plus text frame upload handoff evidence and produce stable records for renderer-boundary consumption:
  frame id, packet id, atlas page id/revision, cache key, upload request id, sampler key or text-atlas sampler summary, UV/layout bounds, page dimensions, readiness, blocker summary, and deterministic fallback/real-backend flags.
- Prove it distinguishes ready uploaded glyph packets, clean reuse, missing upload handoff, rejected upload, frame-not-ready draw packets, missing atlas/page data, and duplicate packet ids.
- Keep this data-only. Do not call renderer/Vulkan, do not create GPU resources, and do not add image-engine dependencies.
- Prefer extending existing text-owned headers/tests rather than creating a new public header that would require integrator-owned CMake FILE_SET registration. If a new public header is genuinely necessary, stop and report the exact integrator-owned registration needed instead of editing CMake.
- Existing external text libraries under `build/external` may be used if useful, but do not duplicate them. If no external dependency is needed, say so.

Implementation style:
- Prefer C++23 designated initialization.
- Add interface lock assertions only for text-owned public structs/functions you add.
- Split helpers only when it improves cohesion/reviewability, not solely by line count.

Verification:
- Build `quiz_vulkan_interface_contract_compile_tests`.
- Build and run focused text tests covering the handoff:
  `quiz_vulkan_font_shaped_atlas_update_tests`
  and any new/changed focused text test.
- Run `git diff --check`.

Commit:
- Commit only scoped files.
- Report changed files, verification, external dependency use, risks, and commit hash.
