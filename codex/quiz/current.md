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

- Vulkan: present image execution evidence and descriptor allocation merge
  guard tests are integrated. Active worker work is descriptor payload bind
  recording before real queue submission is widened.
- Text: atlas packet consumption, HarfBuzz shaping handoff diagnostics, and real
  font atlas draw evidence are integrated. Text worker is idle.
- Image: staging payload blocker coverage and staging payload diff summaries are
  integrated. Active worker work is decoded-byte resource materialization.
- Asset: materialized byte payload request/review evidence and shader byte
  pipeline summary are integrated.
- Input/IME: wheel modifier diagnostics are integrated; input is idle unless
  gesture routing becomes the active bottleneck again.

## Active Workers

- `codex-vulkan-native-command-packet-executor-20260516`: busy on
  `codex/vulkan-descriptor-payload-bind-recording-20260518`.
- `codex-text-freetype-prototype-20260514`: idle after real font atlas draw
  evidence integration.
- `codex-asset-unified-cache-key-20260514`: idle after shader byte source
  pipeline summary and focused-header split.
- `codex-image-texture-next-20260514`: busy on
  `codex/image-decoded-byte-resource-materialization-20260518`.
- `codex-input-ime`: idle after wheel/drag/touch-like diagnostics work.
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

- Main branch `codex/quiz-vulkan-remake-baseline` is at `d369e09` after
  HarfBuzz shaping handoff diagnostics, image staging payload diff summaries,
  Vulkan descriptor allocation merge guard integration, next engine worker
  prompts, and real font atlas draw evidence.
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
- After `f513fa7`, Windows MinGW built the Vulkan renderer/native frame/swapchain
  focused targets; focused CTest passed 3/3 for `quiz_vulkan_renderer_tests`,
  `quiz_vulkan_vulkan_native_frame_operation_tests`, and
  `quiz_vulkan_vulkan_swapchain_create_plan_tests`.
- After `ad6287e`, repository assignment source documents were removed; Desktop
  PDF exports were regenerated and checked for student id, repository URL, and
  absence of assignment/LMS/source-MD metadata.
- After `c56535c`, Windows MinGW built
  `quiz_vulkan_interface_contract_compile_tests`,
  `quiz_vulkan_vulkan_native_frame_operation_tests`,
  `quiz_vulkan_vulkan_present_completion_plan_tests`,
  `quiz_vulkan_vulkan_queue_submit_adapter_tests`,
  `quiz_vulkan_vulkan_swapchain_create_plan_tests`, and
  `quiz_vulkan_renderer_tests`; focused CTest passed 5/5 for renderer,
  native-frame, present-completion, queue-submit, and swapchain-create targets.
- After `91408ae`, Windows MinGW built
  `quiz_vulkan_interface_contract_compile_tests`,
  `quiz_vulkan_asset_bytes_provider_tests`, `quiz_vulkan_asset_manifest_tests`,
  `quiz_vulkan_asset_runtime_catalog_tests`, and
  `quiz_vulkan_asset_runtime_resolver_policy_tests`; focused CTest passed 4/4
  for asset bytes, manifest, runtime catalog, and runtime resolver policy.
- After `cd5f0c5`, Windows MinGW built
  `quiz_vulkan_interface_contract_compile_tests`,
  `quiz_vulkan_fake_text_engine_freetype_raster_payload_tests`,
  `quiz_vulkan_font_shaped_atlas_update_tests`,
  `quiz_vulkan_text_renderer_glyph_quad_packet_tests`, and
  `quiz_vulkan_text_frame_resource_packet_materialization_tests`; focused CTest
  passed 4/4 for the text raster payload, shaped atlas update, renderer glyph
  quad packet, and text frame resource packet materialization targets.
- After `d8f8f6f`, Windows MinGW built
  `quiz_vulkan_interface_contract_compile_tests`,
  `quiz_vulkan_fake_image_texture_uploader_tests`, and
  `quiz_vulkan_image_texture_pipeline_tests`; focused CTest passed 2/2 for image
  texture uploader and image texture pipeline.
- After `889ee2d`, Windows MinGW built
  `quiz_vulkan_interface_contract_compile_tests`,
  `quiz_vulkan_font_shaping_backend_tests`, `quiz_vulkan_fake_text_engine_tests`,
  and `quiz_vulkan_fake_text_engine_layout_diagnostics_tests`; focused CTest
  passed 3/3 for font shaping backend, fake text engine, and fake text engine
  layout diagnostics.
- After `aaa0256`, Windows MinGW built
  `quiz_vulkan_interface_contract_compile_tests`,
  `quiz_vulkan_fake_image_texture_uploader_tests`, and
  `quiz_vulkan_image_texture_pipeline_tests`; focused CTest passed 8/8 for the
  image texture filtered suite, including uploader, staging materialization diff,
  packet materialization/plan, pipeline, and standard image texture pipeline.
- After `0a5bed7`, Windows MinGW built
  `quiz_vulkan_interface_contract_compile_tests` and
  `quiz_vulkan_vulkan_command_packet_execution_tests`; focused CTest passed 1/1
  for Vulkan command packet execution.
- After `d369e09`, Windows MinGW built
  `quiz_vulkan_interface_contract_compile_tests`,
  `quiz_vulkan_fake_text_engine_tests`,
  `quiz_vulkan_fake_text_engine_layout_diagnostics_tests`,
  `quiz_vulkan_fake_text_engine_freetype_raster_payload_tests`,
  `quiz_vulkan_font_shaped_atlas_update_tests`,
  `quiz_vulkan_text_renderer_glyph_quad_packet_tests`, and
  `quiz_vulkan_text_frame_resource_packet_materialization_tests`; focused CTest
  passed 6/6 across the changed text engine, FreeType raster payload, shaped
  atlas update, glyph quad packet, and text frame resource packet targets.

## Useful Commands

```bash
git status --short --branch
./codex-workers/worker-status.sh
tmux capture-pane -pt codex-image-texture-next-20260514 -S -120
tmux capture-pane -pt codex-text-freetype-prototype-20260514 -S -120
tmux capture-pane -pt codex-vulkan-real-backend-probe-20260514 -S -120
```
