You are the long-lived Text Engine worker. Keep this session alive after finishing.

Start from the latest pushed integration baseline:

```bash
git fetch origin
git switch -C codex/text-multiline-atlas-frame-handoff-20260519 origin/codex/quiz-vulkan-remake-baseline
```

Scope:
- Own only `apps/quiz/quiz-vulkan/src/render/text/*` and `apps/quiz/quiz-vulkan/tests/render/text/*`.
- Do not edit `src/app/*`, `app.cpp`, `main.cpp`, top-level `CMakeLists.txt`, renderer integration, scene, UI, domain, input, audio, assets, or image files.
- `text_engine.h` remains the stable interface. Do not rename/move/change its public signatures without stopping and reporting an exact proposal.

Task:
- Move the text engine closer to actual rendering by adding focused coverage for multi-glyph, multi-line text flowing through:
  text layout -> shaping/raster payload -> atlas materialization -> upload handoff -> glyph quad packet -> render-frame handoff summary.
- Use existing FreeType/HarfBuzz/font fixtures and dependencies already present under approved external paths. Do not create a custom shaper/rasterizer if existing adapters can do it.
- Ensure Korean/UTF-8 text and line breaks are represented in diagnostics without renderer/app/domain dependencies.
- Add blockers for missing glyph atlas upload consumption on one line while another line is ready, so partial readiness is observable.

Implementation guidance:
- Prefer splitting focused helpers out of very large text files only when it reduces local read cost and stays inside `src/render/text`.
- If top-level CMake registration is needed for a new source/header, report it instead of editing CMake.
- Use C++23 aggregate initialization style.

Verification:
- Build `quiz_vulkan_interface_contract_compile_tests`.
- Build and run the focused text targets you touch, including at minimum `quiz_vulkan_fake_text_engine_freetype_raster_payload_tests`, `quiz_vulkan_text_renderer_glyph_quad_packet_tests`, and `quiz_vulkan_text_frame_resource_packet_materialization_tests`.
- Run `ctest -R text --output-on-failure` if the worker build is already configured and the focused targets pass.
- Run `git diff --check`.

Commit and report:
- Commit on your branch.
- Report changed files, verification commands/results, risks, and commit hash.
