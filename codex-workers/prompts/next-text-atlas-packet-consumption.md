Continue in the same long-lived text engine session, but start from the latest
pushed integration baseline before editing.

Task: advance text rendering from atlas/upload readiness toward renderer packet
consumption.

Fresh branch:
- Fetch origin.
- Create/switch to a new branch from `origin/codex/quiz-vulkan-remake-baseline`.
- Suggested branch: `codex/text-atlas-packet-consumption-20260518`.

Ownership:
- You may edit only:
  - `apps/quiz/quiz-vulkan/src/render/text/*`
  - `apps/quiz/quiz-vulkan/tests/render/text/*`
- Do not edit `src/app/*`, `app.cpp`, `main.cpp`, top-level `CMakeLists.txt`,
  aggregate interface registration, Vulkan files, image files, or asset files.
- If CMake/renderer/app wiring is required, report the exact proposal instead of
  editing it.

Goal:
- Define/implement text-owned evidence that a shaped line/run atlas upload can
  be consumed as stable renderer glyph-quad/resource packets.
- Cover glyph identity, atlas page/slot, UV rect, pen position, baseline,
  line/run grouping, missing glyph, fallback font, upload generation, and packet
  diff/reuse evidence.
- Prefer existing FreeType/HarfBuzz/utf8proc dependencies under
  `/mnt/c/aa/build/external` or existing configured targets. Do not hand-roll a
  shaper/rasterizer when an approved dependency already exists behind the
  current interface.
- If a file is becoming hard to review, split by responsibility, not by an
  arbitrary line count. Good split reasons: interface contract, atlas planning,
  packet materialization, diff diagnostics, or fixture helpers.

Verification:
- Build `quiz_vulkan_interface_contract_compile_tests`.
- Build and run focused text tests, including any new test and at least:
  `quiz_vulkan_fake_text_engine_freetype_raster_payload_tests`,
  `quiz_vulkan_font_shaped_atlas_update_tests`,
  `quiz_vulkan_text_renderer_glyph_quad_packet_tests`,
  `quiz_vulkan_text_frame_resource_packet_materialization_tests`.
- Run `git diff --check`.

Commit:
- Commit scoped changes on your branch.
- Final report: changed files, exact verification commands/results, external
  dependencies used, risks, integrator-owned wiring proposal if needed, and
  commit hash.
