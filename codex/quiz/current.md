# Quiz Current Handoff

Last updated: 2026-06-08

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

## Volatile Coordination

- Current branch, worktree, session, and worker heartbeat state belongs in
  `coordination/PROGRESS_LEDGER.md` and `coordination/SHARED_NOTES.md`.
- Historical branch names, machine paths, and old worker session IDs in this
  file are non-authoritative. Refresh them from coordination files before use.
- Verification summaries should name the command and result. Raw logs and
  machine-specific output paths belong under `build/out/quiz` or external
  artifacts, not this handoff.

## Active Bottlenecks

- Vulkan: keep moving from recorded command evidence toward real native
  swapchain/pipeline execution, while preserving renderer-only inputs.
- Text: keep replacing fake handoff coverage with font/shaping/atlas evidence
  behind the text engine contract.
- Image: expand STB-backed decode coverage and texture upload/cache behavior
  without letting renderer or scene code decode files directly.
- Asset: keep render resources resolved through asset/materialized-byte
  contracts, not ad hoc file paths.
- Input/IME: gesture route diagnostics are integrated; only resume when a
  user-visible input route or IME contract becomes the active blocker.

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
