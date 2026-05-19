You are the Text Engine Codex worker. Keep this session alive after you finish.

Start from the latest integration baseline:

```bash
cd /mnt/c/aa-workers/text-freetype-prototype-20260514
git fetch origin
git checkout -B codex/text-real-atlas-glyph-quad-consumption-20260519 origin/codex/quiz-vulkan-remake-baseline
```

Task: move real HarfBuzz + FreeType atlas residency evidence into text-owned renderer glyph quad packet consumption.

Write scope:
- `apps/quiz/quiz-vulkan/src/render/text/*`
- `apps/quiz/quiz-vulkan/tests/render/text/*`

Do not edit app, scene, UI, domain, input, audio, asset, image, Vulkan, top-level CMake, or aggregate contract files. You are not alone in the codebase; do not revert or reshape other workers' work.

Current baseline proves HarfBuzz-shaped glyph ids drive FreeType atlas residency and repeated atlas reuse. Add the next narrow layer:
- Prove real shaped/rasterized atlas materializations become drawable renderer glyph quad packets through existing text packet/handoff functions.
- Include first-frame upload-ready quads and second-frame clean-reuse quads.
- Preserve blocker diagnostics for missing font bytes or skipped atlas materialization.
- Do not hand-roll shaping. Use existing HarfBuzz/FreeType/utf8proc dependencies under `build/external/lib/cpp/desktop`.
- Keep public text interface signatures stable unless the integrator explicitly approves a change.
- Keep all output as renderer-owned glyph/atlas packet data. No quiz/domain/UI semantics.

Verification before handoff:
- `cmake --build . --target quiz_vulkan_interface_contract_compile_tests quiz_vulkan_fake_text_engine_freetype_raster_payload_tests quiz_vulkan_text_renderer_glyph_quad_packet_tests quiz_vulkan_text_frame_resource_packet_materialization_tests`
- `ctest -R "fake_text_engine_freetype_raster_payload|text_renderer_glyph_quad_packet|text_frame_resource_packet_materialization" --output-on-failure`
- `git diff --check`

Commit your changes with a concise message and leave the session open.
