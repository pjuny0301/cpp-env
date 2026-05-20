You are the Text Engine Codex worker. Keep this session alive after you finish.

Start from the latest integration baseline:

```bash
cd /mnt/c/aa-workers/text-freetype-prototype-20260514
git fetch origin
git checkout -B codex/text-real-frame-draw-payload-consumption-20260520 origin/codex/quiz-vulkan-remake-baseline
```

Task: move real atlas glyph quad evidence toward text-owned renderer draw payload consumption.

Write scope:
- `apps/quiz/quiz-vulkan/src/render/text/*`
- `apps/quiz/quiz-vulkan/tests/render/text/*`

Do not edit app, scene, UI, domain, input, audio, asset, image, Vulkan, top-level CMake, or aggregate contract files. You are not alone in the codebase; do not revert or reshape other workers' work.

Current baseline proves HarfBuzz/FreeType atlas residency, upload consumption, glyph quad packets, and clean reuse. Add the next narrow text-owned layer:
- Convert ready glyph quad packets into renderer-facing text draw payload records with stable atlas/cache identity, quad bounds, UV bounds, upload/reuse evidence, and blocker summary.
- Preserve first-frame uploaded payloads and second-frame clean-reuse payloads in tests.
- Preserve blocker diagnostics for missing font bytes or unconsumed atlas uploads.
- Use existing HarfBuzz/FreeType/utf8proc dependencies under `build/external/lib/cpp/desktop`; do not hand-roll shaping/rasterization.
- Keep public text interface signatures stable unless the integrator explicitly approves a change.
- Keep all output as renderer-owned glyph/atlas packet data. No quiz/domain/UI semantics.

Verification before handoff:
- `build_dir="$(/mnt/c/aa/codex-workers/quiz-vulkan-worker-build-dir.sh "$(pwd)" windows-mingw-ascii)"`
- `/mnt/c/aa/codex-workers/with-build-lock.sh "/mnt/c/Program Files/CMake/bin/cmake.exe" --build "$build_dir" --target quiz_vulkan_interface_contract_compile_tests quiz_vulkan_fake_text_engine_freetype_raster_payload_tests quiz_vulkan_text_renderer_glyph_quad_packet_tests quiz_vulkan_text_frame_resource_packet_materialization_tests`
- `/mnt/c/aa/codex-workers/with-build-lock.sh "/mnt/c/Program Files/CMake/bin/ctest.exe" --test-dir "$build_dir" -R "fake_text_engine_freetype_raster_payload|text_renderer_glyph_quad_packet|text_frame_resource_packet_materialization" --output-on-failure`
- `git diff --check`

Commit your changes with a concise message and leave the session open.
