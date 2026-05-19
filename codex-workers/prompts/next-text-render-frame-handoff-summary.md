# Text Worker: Render Frame Handoff Summary

Continue in the same long-lived Text Engine worker session. Keep the session
alive after finishing.

Refresh safely:

```bash
git fetch origin
git switch -C codex/text-render-frame-handoff-summary-20260518 origin/codex/quiz-vulkan-remake-baseline
```

## Scope

Edit only:

- `apps/quiz/quiz-vulkan/src/render/text/*`
- `apps/quiz/quiz-vulkan/tests/render/text/*`

Do not edit app/main/top-level CMake, scene, UI, domain, input, image, Vulkan,
asset, or audio files. `text_engine.h` remains the stable Text Engine public
interface; stop and propose before changing or moving public signatures.

## Goal

Continue after draw packet consumption diffs by adding or extending a compact
text-owned render frame handoff summary.

Use existing frame snapshot, draw packet plan, glyph quad packet, atlas upload,
and draw packet consumption/diff data. The summary should let a renderer later
consume text frame facts without rereading the full shaping/raster/layout
diagnostics:

- frame id/source label;
- draw packet count and ready/blocked/skipped counts;
- glyph quad count and atlas page/upload request ids;
- added/removed/changed packet ids if an existing diff helper supports this;
- blockers for missing atlas upload, missing materialization, missing glyph
  quad packet, non-upload-ready atlas payload, and skipped fallback packets;
- real-font-backed test path proving the summary preserves the FreeType raster
  payload route when available and deterministic fallback route when not.

Keep this data-only and text-owned. Do not include or call UI, scene, app,
image, Vulkan, domain, input, or audio.

If existing frame handoff helpers already cover part of this, extend them
narrowly and add tests instead of duplicating a parallel contract.

## Verification

Build:

- `quiz_vulkan_interface_contract_compile_tests`
- focused text frame handoff / draw plan / glyph quad tests you change
- `quiz_vulkan_fake_text_engine_freetype_raster_payload_tests`

Run focused text CTest filters and `git diff --check`.

Commit only scoped files and report commit hash, changed files, verification,
external dependencies used, and risks.
