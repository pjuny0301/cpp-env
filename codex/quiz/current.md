# Quiz Current Handoff

Last updated: 2026-05-19

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

- Vulkan: present image execution evidence, descriptor allocation merge guard
  tests, descriptor payload bind recording, native descriptor bind-call evidence,
  and native draw-call evidence are integrated.
- Text: atlas packet consumption, HarfBuzz shaping handoff diagnostics, and real
  font atlas draw evidence, draw packet consumption diffs, and render-frame
  handoff summary/header split are integrated.
- Image: staging payload blocker coverage, staging payload diff summaries,
  decoded-byte resource materialization, and decoded draw payload evidence are
  integrated. The oversized resource packet plan header is split into focused
  image render handoff headers, and standard JPEG decode now routes through the
  existing STB adapter path.
- Asset: materialized byte payload request/review evidence and shader byte
  pipeline summary are integrated; render resource address summaries are also
  integrated.
- Input/IME: wheel modifier diagnostics and IME composition lifecycle
  diagnostics are integrated; input is idle unless gesture routing becomes the
  active bottleneck again.

## Active Workers

- `codex-vulkan-native-command-packet-executor-20260516`: integrated native
  descriptor bind-call readiness and native draw-call evidence.
- `codex-text-freetype-prototype-20260514`: integrated compact text render-frame
  handoff summary and focused header split.
- `codex-asset-unified-cache-key-20260514`: idle after shader byte source
  pipeline summary, focused-header split, and asset render resource addressing.
- `codex-image-texture-next-20260514`: integrated decoded draw payload evidence
  and standard JPEG decode through STB.
- `codex-input-ime`: idle after wheel/drag/touch-like diagnostics and IME
  composition lifecycle diagnostics.
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

- Main branch `codex/quiz-vulkan-remake-baseline` is at `0c92941` after
  text handoff header splitting, asset render resource addressing, IME
  composition lifecycle diagnostics, Vulkan native draw-call evidence, and
  standard JPEG decode through the existing STB adapter path.
- After `0c92941`, full Windows MinGW CTest passed 108/108 in
  `C:/aa/build/out/quiz/quiz-vulkan/windows-mingw-ascii`.
- After `a09ce6d`, full Windows MinGW CTest passed 108/108.
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
- After `86640cd`, Windows MinGW built
  `quiz_vulkan_interface_contract_compile_tests`,
  `quiz_vulkan_standard_image_texture_pipeline_tests`,
  `quiz_vulkan_image_texture_frame_resource_packet_materialization_tests`,
  `quiz_vulkan_fake_image_texture_uploader_tests`, and
  `quiz_vulkan_image_texture_pipeline_tests`; focused CTest passed 8/8 for the
  image texture filtered suite, including the standard decoder pipeline and
  resource packet materialization.
- After `9a7bc2f`, Windows MinGW built
  `quiz_vulkan_interface_contract_compile_tests`,
  `quiz_vulkan_vulkan_command_packet_execution_tests`,
  `quiz_vulkan_renderer_tests`, and `quiz_vulkan_architecture_boundary_tests`;
  focused CTest passed 3/3 for Vulkan command packet execution, renderer, and
  architecture boundary.
- After `7165401`, Windows MinGW built
  `quiz_vulkan_interface_contract_compile_tests`,
  `quiz_vulkan_font_shaped_atlas_update_tests`,
  `quiz_vulkan_text_renderer_glyph_quad_packet_tests`,
  `quiz_vulkan_text_frame_resource_packet_materialization_tests`, and
  `quiz_vulkan_fake_text_engine_freetype_raster_payload_tests`; focused CTest
  passed 4/4 for shaped atlas update, glyph quad packet, text frame resource
  packet materialization, and FreeType raster payload.
- After `7a06bc6` plus the image header split, Windows MinGW built
  `quiz_vulkan_interface_contract_compile_tests`,
  `quiz_vulkan_standard_image_texture_pipeline_tests`,
  `quiz_vulkan_image_texture_frame_resource_packet_plan_tests`, and
  `quiz_vulkan_image_texture_frame_resource_packet_materialization_tests`;
  focused `ctest -R image_texture --output-on-failure` passed 8/8.
- After `efb00b2`, Windows MinGW built
  `quiz_vulkan_vulkan_command_packet_execution_tests`,
  `quiz_vulkan_renderer_tests`, and `quiz_vulkan_architecture_boundary_tests`;
  focused CTest passed 3/3 for Vulkan command packet execution, renderer, and
  architecture boundary.
- After `93c61ba`, Windows MinGW built
  `quiz_vulkan_interface_contract_compile_tests`,
  `quiz_vulkan_text_renderer_glyph_quad_packet_tests`,
  `quiz_vulkan_font_shaped_atlas_update_tests`,
  `quiz_vulkan_text_frame_resource_packet_materialization_tests`, and
  `quiz_vulkan_fake_text_engine_freetype_raster_payload_tests`; focused
  `ctest -R text --output-on-failure` passed 20/20.
- After `3385bdc`, Windows MinGW built the interface, focused text handoff,
  glyph quad packet, frame packet materialization, FreeType raster payload, and
  shaped atlas targets; focused `ctest -R text --output-on-failure` passed
  20/20.
- After `097c680`, Windows MinGW built the interface and asset targets; focused
  `ctest -R asset --output-on-failure` passed 11/11.
- After `3edca66`, Windows MinGW built the interface, input engine IME, input
  engine, and text input model targets; focused IME and text input model CTest
  passed 2/2.
- After `ca41fb1`, Windows MinGW built the interface, Vulkan command packet
  execution, renderer, and architecture boundary targets; focused CTest passed
  3/3 for those areas.
- After `0c92941`, Windows MinGW built the interface, standard image decoder
  chain, standard image texture pipeline, image texture pipeline, and
  third-party image decoder adapter targets; focused CTest passed 5/5 for the
  effective image decoder/texture areas.
- After `0c92941`, full Windows MinGW CTest passed 108/108.

## Useful Commands

```bash
git status --short --branch
./codex-workers/worker-status.sh
tmux capture-pane -pt codex-image-texture-next-20260514 -S -120
tmux capture-pane -pt codex-text-freetype-prototype-20260514 -S -120
tmux capture-pane -pt codex-vulkan-real-backend-probe-20260514 -S -120
```
