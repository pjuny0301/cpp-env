You are the Text Engine Codex worker. Keep this session alive after you finish.

Start by rebasing your working copy onto the current integration baseline:

```bash
cd /mnt/c/aa-workers/text-freetype-prototype-20260514
git fetch origin
git checkout -B codex/text-real-freetype-atlas-raster-cache-20260519 origin/codex/quiz-vulkan-remake-baseline
```

Task: replace more fake text evidence with a real font-source -> FreeType raster -> glyph atlas/cache path behind the existing TextEngine boundary.

Write scope:
- `apps/quiz/quiz-vulkan/src/render/text/*`
- `apps/quiz/quiz-vulkan/tests/render/text/*`

Do not edit app, renderer integration, top-level CMake, aggregate contract registration, scene, domain, UI, input, audio, asset, or image files. You are not alone in the codebase; do not revert or reshape other workers' work.

Requirements:
- `text_engine.h` stays the stable public Text Engine interface. Do not rename/move it or change public signatures without an explicit blocker note.
- Use the existing optional dependencies under `build/external/lib/cpp/desktop` when useful: FreeType, HarfBuzz, utf8proc. Do not hand-roll shaping/rasterization when the external library is already wired.
- Add/extend tests that feed real font bytes into the current adapter/rasterizer, materialize glyph bitmap payloads, and prove atlas residency/cache reuse across repeated shaped text.
- Include a Hangul/UTF-8 fixture path if the currently available font/test fixture supports it; otherwise add a clear diagnostic for missing coverage without faking success.
- Keep renderer/domain/UI semantics out of text.

Verification before handoff:
- `cmake --build . --target quiz_vulkan_interface_contract_compile_tests quiz_vulkan_fake_text_engine_freetype_raster_payload_tests quiz_vulkan_font_shaped_atlas_update_tests quiz_vulkan_text_frame_resource_packet_materialization_tests quiz_vulkan_text_renderer_glyph_quad_packet_tests`
- `ctest -R "fake_text_engine_freetype_raster_payload|font_shaped_atlas_update|text_frame_resource_packet_materialization|text_renderer_glyph_quad_packet" --output-on-failure`
- `git diff --check`

Commit your changes with a concise message and leave the session open.
