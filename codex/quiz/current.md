# Quiz Current Handoff

Last updated: 2026-05-19

Read this first before `big_plan.md`, `requirements_traceability_matrix.md`,
or per-requirement implementation notes. Keep this file short; old handoff
details belong in git history.

## Current Focus

- Build the native C++23 quiz remake toward the engine stack:
  Vulkan backend, text layout/atlas, image/texture, asset materialized bytes,
  input/IME, then audio later.
- Audio is deferred until render/resource paths are less fragile.
- Existing Android/editor code is reference material; convergence target is
  `apps/quiz/quiz-vulkan`.
- Keep `apps/quiz` as code only. Build outputs stay under `build/out`;
  external dependency snapshots stay under `build/external`.

## Integrated Baseline

- Main branch `codex/quiz-vulkan-remake-baseline` is at `d65cca5`.
- Integrated since the previous remote baseline:
  - app image payload extraction now works for the standard image pipeline, not
    only the fake pipeline;
  - asset render resource materialized byte cache summaries preserve stable
    identities for renderer-facing payloads;
  - standard image decode/upload cache behavior is covered through repeated URI
    reuse and malformed image placeholder paths;
  - input/IME fixture coverage now includes IME preedit commit/cancel, Hangul
    backspace, wheel routing, and pointer capture arbitration;
  - Vulkan command packet execution records native vertex buffer bind evidence;
  - FreeType-backed text atlas raster payloads now expose cache-hit reuse
    evidence behind the text engine contract.
- Full Windows MinGW CTest passed 108/108 in
  `C:/aa/build/out/quiz/quiz-vulkan/windows-mingw-ascii`.

## Active Bottlenecks

- Vulkan: native vertex buffer bind evidence is tracked; keep moving toward
  descriptor/buffer allocation and command packet execution while preserving
  renderer-only inputs.
- Text: FreeType atlas payload reuse is tracked; keep replacing fake handoff
  behavior with font/shaping/atlas evidence behind the text engine contract.
- Image: standard decode cache/upload residency is covered; expose a stable
  image-owned upload snapshot/diagnostic contract so app wiring does not need
  concrete fake/standard pipeline casts.
- Asset: render resources can bridge to materialized bytes with stable cache
  identities; keep cache invalidation based on content/revision, not absolute
  file paths.
- Input/IME: gesture route diagnostics are integrated; only resume when a
  user-visible input route or IME contract becomes the active blocker.

## Active Workers

- `codex-vulkan-native-command-packet-executor-20260516`: latest commit
  `5855938` integrated as `88c721c`; session remains alive.
- `codex-text-freetype-prototype-20260514`: latest commit `417bcd3`
  integrated as `d65cca5`; session remains alive.
- `codex-asset-unified-cache-key-20260514`: latest commit `06a9859`
  integrated as `e499fe3`; session remains alive.
- `codex-image-texture-next-20260514`: latest commit `117bc0c`
  integrated as `34f80b8`; session remains alive.
- `codex-input-ime`: latest commit `3eb456d` integrated as `47884d0`;
  session remains alive.
- Idle sessions are intentionally kept alive. Give them fresh baseline branches
  before new work; do not re-merge historical ahead commits.

## Architecture Contract

- Required direction:
  `main -> modifier_interface -> scene_layout_data_modifier -> scene_layout_edit_data -> scene_layout_patch -> scene_layout_data -> layout_placer -> ui_renderer -> vulkan_renderer`.
- Modifiers write only through `scene_layout_edit_data` / patch flow.
- `layout_placer` reads scene/layout data and must not mutate scene data or call
  UI/render/Vulkan.
- `ui_renderer` consumes placed UI data and must not call Vulkan or domain/app.
  It also must not include or call `layout_placer`; it receives placed scene
  data through the scene-owned `placed_scene` contract.
- Vulkan consumes renderer-owned command/resource data and must not include
  scene, UI, app, domain, input, or audio.
- App/domain presentation coupling is allowed only in app-owned bridge files
  such as `src/app/app_quiz_screens.h`.
- Engine workers own engine folders and focused tests. App wiring, top-level
  CMake integration, and cross-engine contracts stay with the integrator unless
  explicitly delegated.

## Verification Policy

- Worker handoff: focused engine tests plus
  `quiz_vulkan_interface_contract_compile_tests` and `git diff --check`.
- Integrator after cherry-pick: Windows MinGW focused tests for touched areas.
- Full Windows CTest: run after meaningful batches, not after every small patch.
- Do not hard-code global CTest counts in docs; derive them from `ctest -N`.
