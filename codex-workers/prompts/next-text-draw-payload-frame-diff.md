You are the Text Engine Codex worker. Keep this session alive after you finish.

Start from the latest integration baseline:

```bash
cd /mnt/c/aa-workers/text-freetype-prototype-20260514
git fetch origin
git checkout -B codex/text-draw-payload-frame-diff-20260520 origin/codex/quiz-vulkan-remake-baseline
```

Task: make text renderer draw payload frames diffable for later renderer consumption.

Write scope:
- `apps/quiz/quiz-vulkan/src/render/text/*`
- `apps/quiz/quiz-vulkan/tests/render/text/*`

Do not edit app, scene, UI, domain, input, audio, asset, image, Vulkan, top-level CMake, or aggregate contract files.

Current baseline has renderer-facing text draw payload records. Add the next narrow text-owned layer:
- Add a draw payload frame diff/consumption summary that detects stable reuse, uploaded-to-clean-reuse transition, changed atlas page/revision, changed UV/bounds, blocked-to-ready, and ready-to-blocked.
- Keep stable identities host-path-free and renderer-owned.
- Preserve blocker summaries for missing payload ids and blocked glyph quads.
- Do not hand-roll shaping/rasterization. Use existing text packet data only.

Verification before handoff:
- `build_dir="$(/mnt/c/aa/codex-workers/quiz-vulkan-worker-build-dir.sh "$(pwd)" windows-mingw-ascii)"`
- `/mnt/c/aa/codex-workers/with-build-lock.sh "/mnt/c/Program Files/CMake/bin/cmake.exe" --build "$build_dir" --target quiz_vulkan_interface_contract_compile_tests quiz_vulkan_text_renderer_glyph_quad_packet_tests quiz_vulkan_text_frame_resource_packet_materialization_tests`
- `/mnt/c/aa/codex-workers/with-build-lock.sh "/mnt/c/Program Files/CMake/bin/ctest.exe" --test-dir "$build_dir" -R "text_renderer_glyph_quad_packet|text_frame_resource_packet_materialization" --output-on-failure`
- `git diff --check`

Commit your changes with a concise message and leave the session open.
