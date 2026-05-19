You are the long-lived Text Engine worker. Keep this session alive after finishing.

Start from the latest pushed integration baseline:

```bash
git fetch origin
git switch -C codex/text-font-atlas-cache-residency-20260519 origin/codex/quiz-vulkan-remake-baseline
```

Scope:
- Own only `apps/quiz/quiz-vulkan/src/render/text/*` and `apps/quiz/quiz-vulkan/tests/render/text/*`.
- Do not edit `src/app/*`, `app.cpp`, `main.cpp`, top-level `CMakeLists.txt`, renderer integration, scene, UI, domain, input, audio, assets, or image files.
- `text_engine.h` remains the stable interface. Do not rename/move/change public signatures without stopping and reporting an exact proposal.

Task:
- Move text rendering closer to a real engine path by adding cache/residency coverage for actual font glyph atlas data behind the existing text interfaces.
- Use approved external FreeType/HarfBuzz/font fixtures already available under `build/external` when useful. Do not invent a custom shaper/rasterizer when an existing adapter can satisfy the task.
- Prove that repeated UTF-8/Hangul text across two frames reuses atlas residency instead of rematerializing every glyph, while a new glyph produces a new atlas upload request.
- Keep diagnostics renderer-usable but domain/app/UI-free:
  - font/source identity;
  - shaped run identity;
  - glyph ids/codepoints;
  - atlas page/slot residency;
  - upload requests added/reused/skipped.
- Add blockers for glyphs that are shaped but not resident in an atlas slot.

Implementation guidance:
- Prefer focused helpers inside `src/render/text`; split oversized helpers only when it reduces local read cost.
- If top-level CMake registration is needed for a new source/header, report it instead of editing CMake.
- Use C++23 aggregate initialization style.

Verification:
- Build `quiz_vulkan_interface_contract_compile_tests`.
- Build and run focused text targets you touch, including `quiz_vulkan_font_shaped_atlas_update_tests`, `quiz_vulkan_text_renderer_glyph_quad_packet_tests`, and `quiz_vulkan_text_frame_resource_packet_materialization_tests`.
- Run `ctest -R text --output-on-failure` if the worker build is already configured and focused tests pass.
- Run `git diff --check`.

Commit and report:
- Commit on your branch.
- Report changed files, verification commands/results, risks, and commit hash.
