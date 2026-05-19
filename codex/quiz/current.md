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

- Main branch `codex/quiz-vulkan-remake-baseline` is at `31ba743`.
- Integrated since the previous remote baseline:
  - app/runtime image texture cache reuse verification and missing image-engine
    link wiring;
  - asset manifest to typed materialized bytes to render payload bridge coverage;
  - standard image decode to texture cache/upload residency coverage;
  - text atlas residency blocker diagnostics;
  - native Vulkan draw packet resource execution evidence.
- Full Windows MinGW CTest passed 108/108 in
  `C:/aa/build/out/quiz/quiz-vulkan/windows-mingw-ascii`.

## Active Bottlenecks

- Vulkan: native draw resource execution is tracked; keep moving toward real
  buffer/descriptor allocation and swapchain/pipeline execution while preserving
  renderer-only inputs.
- Text: atlas residency blockers are tracked; keep replacing fake handoff
  coverage with font/shaping/atlas evidence behind the text engine contract.
- Image: standard decode cache/upload residency is covered; keep expanding real
  texture upload/cache behavior without letting renderer or scene code decode
  files directly.
- Asset: manifest/render resources can bridge to materialized bytes; keep asset
  payloads resolved through stable cache identities, not ad hoc file paths.
- Input/IME: gesture route diagnostics are integrated; only resume when a
  user-visible input route or IME contract becomes the active blocker.

## Active Workers

- `codex-vulkan-native-command-packet-executor-20260516`: latest commit
  `378ad84` integrated as `31ba743`; session remains alive.
- `codex-text-freetype-prototype-20260514`: latest commit `1819a70`
  integrated as `d64924e`; session remains alive.
- `codex-asset-unified-cache-key-20260514`: latest commit `311b00f`
  integrated as `3f461ca`; session remains alive.
- `codex-image-texture-next-20260514`: latest commit `8731734`
  integrated as `2a22d99`; session remains alive.
- `codex-input-ime`: latest commit `5e87138` integrated as `ce94ecd`;
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
