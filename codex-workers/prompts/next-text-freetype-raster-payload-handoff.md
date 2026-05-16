# Text Engine: FreeType Raster Payload Handoff

Continue in the same long-lived text worker session, but start a fresh branch from the latest pushed baseline before editing.

Baseline:
- Fetch `origin`.
- Create/switch to a new branch from `origin/codex/quiz-vulkan-remake-baseline`, for example `codex/text-freetype-raster-payload-handoff-20260516`.
- Do not stack this work on `codex/text-fake-engine-source-split-20260515`.

Scope:
- Own only `apps/quiz/quiz-vulkan/src/render/text/*` and `apps/quiz/quiz-vulkan/tests/render/text/*`.
- Do not edit `src/app/*`, renderer/Vulkan, top-level `CMakeLists.txt`, or aggregate contract registration.
- Do not move or change `text_engine.h`.

Task:
- Move the fake text engine raster payload path one step closer to the real text engine.
- Current gap to inspect: `record_rasterized_glyph_atlas_payload_diagnostics` always uses `deterministic_fake_font_rasterizer`, even when the selected raster backend is FreeType and file-backed font bytes are available.
- Implement a text-owned handoff that uses the existing `freetype_real_font_backend_rasterize` adapter when all of these are true:
  - rasterization selection chose `render_text_font_backend_library::freetype`;
  - backend capability supports glyph rasterization;
  - `render_text_freetype_memory_face_adapter_available()` is true;
  - the resolved font source bytes load successfully and are non-empty.
- Preserve deterministic fake rasterization as the fallback for fixture/virtual fonts, missing bytes, unavailable FreeType, unsupported glyphs, or failed adapter results.
- Preserve existing diagnostics, and add clear evidence for:
  - real FreeType rasterization used;
  - materialized font bytes used;
  - fallback reason when the path stays deterministic.

External dependency rule:
- FreeType and HarfBuzz are already present under `/mnt/c/aa/build/external/lib/cpp/desktop`; inspect and use those before adding anything.
- Do not download a new text dependency unless you can justify why the existing approved dependencies cannot do the job.

Implementation style:
- If `fake_text_engine.cpp` becomes harder to review, you may split cohesive implementation-only raster payload helpers into a private text-owned `.inl` included by `fake_text_engine.cpp`.
- Do not split solely by line count. Split only if it improves ownership/reviewability and keeps public interfaces stable.

Tests:
- Add or update focused text tests under `tests/render/text/*`.
- Prefer a new focused test file if it avoids expanding an already huge test file.
- Use the existing HarfBuzz external test font fixture search pattern if a real font file is needed.
- The test should prove that file-backed font bytes plus selected FreeType raster backend produce a real-backend raster payload/materialization/atlas-upload-ready diagnostic, and that missing bytes still fall back/skips cleanly.

Required verification:
- Build `quiz_vulkan_interface_contract_compile_tests`.
- Run focused text tests covering your changes.
- Run `git diff --check`.
- Commit scoped files and report changed files, verification, risks, and commit hash.
