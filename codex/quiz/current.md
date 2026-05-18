# Quiz Current Handoff

Last updated: 2026-05-18

This file is intentionally short. Use it as the first read before `big_plan.md`,
`requirements_traceability_matrix.md`, or per-requirement implementation notes.
Historical integration notes are kept in git history, not repeated here.

## Current Focus

- Build the native C++23 quiz remake toward the required engine stack:
  Vulkan backend, image/texture path, text layout/atlas path, asset materialized
  bytes, input/IME, then audio later.
- Audio is deferred until render/resource paths are less fragile.
- Existing Android/editor code is reference material, but the convergence target
  is `apps/quiz/quiz-vulkan`.
- Keep `apps/quiz` as code only. Build outputs stay under `build/out`; external
  dependency snapshots stay under `build/external`.

## Active Bottlenecks

- Vulkan: descriptor payload command recording and Win32 surface bridge
  diagnostics are integrated. Active work is swapchain image enumeration/acquire
  behind the Vulkan-owned boundary.
- Text: text resource packet handoff, draw-list text frame composition, and
  FreeType raster payload handoff evidence are integrated. Active work is
  line/run atlas upload readiness.
- Image: draw-list texture frame composition coverage is integrated. Active
  work is image-owned resource packet consumption diff evidence.
- Asset: materialized byte payload request transaction review is integrated.
  Active work is shader materialized-byte pipeline summary.
- Input/IME: focus-loss cleanup diagnostics are integrated. Active work is
  wheel, drag, and touch-like pointer gesture normalization.

## Active Workers

- `codex-vulkan-real-backend-probe-20260514`: busy on
  `codex/vulkan-swapchain-image-execution-20260518`.
- `codex-text-freetype-prototype-20260514`: busy on
  `codex/text-line-run-atlas-upload-20260515-224725`.
- `codex-image-texture-next-20260514`: busy on
  `codex/image-resource-packet-consumption-diff-20260517`.
- `codex-asset-unified-cache-key-20260514`: busy on shader materialized-byte
  pipeline work from the latest baseline.
- `codex-input-ime`: busy on
  `codex/input-wheel-drag-touch-gestures-20260517`.
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

- Worker handoff: focused engine tests plus `quiz_vulkan_interface_contract_compile_tests`
  and `git diff --check`.
- Integrator after cherry-pick: Windows MinGW focused tests for touched areas.
- Full Windows CTest: run after meaningful batches, not after every small patch.
- Do not hard-code global CTest counts in docs; derive them from `ctest -N`.

## Latest Known Verification

- Main branch `codex/quiz-vulkan-remake-baseline` is at `1da632d` after
  assignment doc cleanup plus asset payload transaction review, raw focus-loss
  diagnostics, and text draw-list frame composition evidence.
- Last full Windows MinGW CTest batch should be treated as stale. Run focused
  tests during normal integration; run full CTest after the next meaningful
  engine batch.
- After `51e6a40`, Windows MinGW built
  `quiz_vulkan_interface_contract_compile_tests`, `quiz_vulkan_architecture_boundary_tests`,
  `quiz_vulkan_renderer_tests`, and `quiz_vulkan_app_demo_tests`; focused CTest
  passed 3/3 for app demo, architecture boundary, and renderer.
- After `ea2aa04`, Windows MinGW built
  `quiz_vulkan_interface_contract_compile_tests`, `quiz_vulkan_input_engine_ime_tests`,
  `quiz_vulkan_input_engine_tests`, and `quiz_vulkan_text_input_model_tests`;
  focused input CTest passed 3/3.
- After `5f35d5a`, Windows MinGW built
  `quiz_vulkan_interface_contract_compile_tests` and
  `quiz_vulkan_image_texture_frame_resource_packet_plan_tests`; focused image
  CTest passed 1/1.
- After `1da632d`, Windows MinGW built
  `quiz_vulkan_asset_bytes_provider_tests`, `quiz_vulkan_input_engine_ime_tests`,
  and `quiz_vulkan_text_draw_list_frame_composition_tests`; focused CTest passed
  3/3 for those targets.

## Useful Commands

```bash
git status --short --branch
./codex-workers/worker-status.sh
tmux capture-pane -pt codex-image-texture-next-20260514 -S -120
tmux capture-pane -pt codex-text-freetype-prototype-20260514 -S -120
tmux capture-pane -pt codex-vulkan-real-backend-probe-20260514 -S -120
```
