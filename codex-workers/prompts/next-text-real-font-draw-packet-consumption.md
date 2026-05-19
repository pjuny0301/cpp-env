# Text Worker: Real Font Draw Packet Consumption

Continue in the same long-lived Text Engine worker session. Keep the session
alive after finishing.

Refresh safely:

```bash
git fetch origin
git switch -C codex/text-real-font-draw-packet-consumption-20260518 origin/codex/quiz-vulkan-remake-baseline
```

## Scope

Edit only:

- `apps/quiz/quiz-vulkan/src/render/text/*`
- `apps/quiz/quiz-vulkan/tests/render/text/*`

Do not edit app/main/top-level CMake, scene, UI, domain, input, image, Vulkan,
asset, or audio files. `text_engine.h` stays the stable public Text Engine
interface; stop and propose before moving or changing public signatures.

## Goal

Continue after real font atlas draw evidence by adding a compact text-owned
consumption/diff summary for renderer-bound draw packets.

Use the existing text frame snapshot, atlas upload result/materialization data,
and text frame draw plan. Add evidence that lets a later renderer integration
tell what changed without rereading all font/raster/materialization internals:

- added/removed/changed draw packet identities;
- line/run/cluster identity used by each draw packet;
- atlas page/cache key/upload request/generation used by each packet;
- glyph quad count and draw readiness;
- blockers for missing atlas upload, missing materialization, non-upload-ready
  payload, missing glyph quad facts, and fallback/skipped packets;
- stable no-change frame evidence;
- ready -> blocked regressions and blocked -> ready recoveries.

Keep this data-only and text-owned. Do not call UI, scene, app, image, Vulkan,
domain, input, or audio.

If existing draw-packet diff/summary helpers already cover part of this, extend
them narrowly and add real-font-backed tests rather than duplicating a parallel
system.

## Verification

Build:

- `quiz_vulkan_interface_contract_compile_tests`
- focused text draw plan / glyph quad / frame resource packet tests you change
- `quiz_vulkan_fake_text_engine_freetype_raster_payload_tests`

Run focused text CTest filters and `git diff --check`.

Commit only scoped files and report commit hash, changed files, verification,
external dependencies used, and risks.
