# Text Worker: Real Font Atlas Handoff

Continue in the same long-lived Text Engine worker session. Keep the session
alive after finishing.

Refresh safely:

```bash
git fetch origin
git switch -C codex/text-real-font-atlas-handoff-20260518 origin/codex/quiz-vulkan-remake-baseline
```

## Scope

Edit only:

- `apps/quiz/quiz-vulkan/src/render/text/*`
- `apps/quiz/quiz-vulkan/tests/render/text/*`

Do not edit app/main/top-level CMake, scene, UI, domain, input, image, Vulkan,
asset, or audio files. `text_engine.h` is the stable public Text Engine
interface; stop and propose before changing or moving public interface
signatures.

## Goal

Move from diagnostics toward real text data by threading one font-backed path
through the existing contracts:

font source bytes -> HarfBuzz/fallback shaped run -> raster payload -> glyph
atlas materialization/upload result -> renderer-bound draw packet evidence.

Use approved external FreeType/HarfBuzz/font fixtures already under
`build/external` when possible; do not duplicate libraries or large fixtures. If
a genuinely useful external fixture/library is needed, place it under
`build/external` or the worker worktree equivalent and report URL,
version/commit, license, path, and reason.

Keep this narrow and data-only:

- preserve fallback behavior when HarfBuzz or fixture font bytes are absent;
- prove which glyph/run/cluster maps to which atlas key/page/upload request;
- expose blocker reasons for missing font bytes, missing glyph coverage,
  missing raster payload, atlas overflow, and payload byte-count mismatch;
- do not make the renderer know quiz/domain semantics.

If files are becoming too broad, split by cohesive responsibility, not by an
arbitrary line count. Do not move stable public interfaces without approval.

## Verification

Build:

- `quiz_vulkan_interface_contract_compile_tests`
- `quiz_vulkan_fake_text_engine_tests`
- `quiz_vulkan_fake_text_engine_layout_diagnostics_tests`
- focused text atlas/raster/draw tests you add or change

Run the focused text CTest filters and `git diff --check`.

Commit only scoped files and report commit hash, changed files, verification,
external dependencies used, and risks.
