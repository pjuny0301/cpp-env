You are the Text Engine Codex worker. Keep this session alive after you finish.

Start from the current integration baseline, not from your stale worker branch:

```bash
cd /mnt/c/aa-workers/text-freetype-prototype-20260514
git fetch origin
git checkout -B codex/text-harfbuzz-freetype-atlas-residency-path-20260519 origin/codex/quiz-vulkan-remake-baseline
```

Task: move text from fake handoff evidence toward a real HarfBuzz + FreeType shaped atlas residency path behind the existing text engine contract.

Write scope:
- `apps/quiz/quiz-vulkan/src/render/text/*`
- `apps/quiz/quiz-vulkan/tests/render/text/*`

Do not edit app, scene, UI, domain, input, audio, asset, image, Vulkan, top-level CMake, or aggregate contract files. You are not alone in the codebase; do not revert or reshape other workers' work.

Current baseline has FreeType-backed atlas raster payload cache-hit evidence. Add the next narrow layer:
- Use existing external dependencies under `build/external/lib/cpp/desktop` for HarfBuzz/FreeType/utf8proc. Do not hand-roll shaping or vendor new source into `apps/quiz`.
- Prove a real file-backed font bytes path that shapes text, resolves glyph ids, rasterizes missing glyphs into atlas payloads, and reuses atlas residency on repeated shaping.
- Include at least one Hangul or mixed UTF-8 fixture. If the available test font cannot shape a glyph, report a structured fallback diagnostic rather than pretending success.
- Keep public text interface signatures stable unless the integrator explicitly approves a change.
- Keep all output as renderer-owned glyph/atlas packet data. No quiz/domain/UI semantics.

Verification before handoff:
- `cmake --build . --target quiz_vulkan_interface_contract_compile_tests quiz_vulkan_fake_text_engine_freetype_raster_payload_tests quiz_vulkan_font_shaping_backend_tests quiz_vulkan_font_shaped_atlas_update_tests quiz_vulkan_text_frame_resource_packet_materialization_tests`
- `ctest -R "fake_text_engine_freetype_raster_payload|font_shaping_backend|font_shaped_atlas_update|text_frame_resource_packet_materialization" --output-on-failure`
- `git diff --check`

Commit your changes with a concise message and leave the session open.
