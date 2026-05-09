# Quiz Current Handoff

Last updated: 2026-05-09

## Top priorities

- Keep the native C++/Vulkan remake aligned with the Android quiz UX baseline while routing UX actions through app/domain actions.
- Treat long press as the unknown-question path: UI button contract `mark_question_unknown`, app input route `mark_question_unknown`, domain payload `mark_question_unknown_action`.
- Keep test status counts derived from the configured build with `ctest -N`; do not freeze global CTest totals in docs.
- Build the engine stack in dependency order: Vulkan backend implementation, image decode/texture path, text layout/atlas, asset materialization, and input/IME. Audio/backend wiring is intentionally deferred for now.
- Worker branches should start from the current baseline with a fresh branch. Do not rebase stale worker branches that contain already-cherry-picked historical commits.
- Native desktop dependency source snapshots live locally under `build/external/lib/cpp/desktop/`, but only `README.md` and `native-deps-manifest.md` are tracked. The downloaded source directories are intentionally ignored until a specific integration decision justifies committing vendored source.
- Latest integrated baseline includes architecture boundary locks, worker build-lock serialization, input key/pointer/text route policy helpers, keyboard text input test split, input gesture diagnostics split tests, platform input translator boundary, platform input engine adapter, platform input boundary contracts, text line-break/glyph cache/font-resolution/readiness/font-face catalog/source/source-byte/file-byte-loader/file-byte-loader split tests/SFNT font bytes inspector/cmap coverage inspector/Unicode coverage resolver/catalog adapter/glyph-id resolver/font backend capability probe/font backend capability diagnostics/real font backend adapter boundary/fake text backend-adapter injection/shaping backend/shaped-glyph atlas-update tracing/resolved glyph-id atlas diagnostics/line-layout helper split/run-box/caret-hit-test/atlas-page/upload-policy/eviction/rasterizer atlas-readiness diagnostics, fake text layout diagnostics split tests, image sampler/cache/upload queue/retry/lifetime eviction/decoder/decoder-chain/standard decoder chain/data-URI/filesystem source-byte/filesystem pipeline/standard decoder-backed texture pipeline/standard pipeline cache-reuse diagnostics/manifest texture pipeline boundary/manifest texture pipeline header split/manifest texture diagnostics/manifest boundary hardening/placeholder texture/BMP decoder diagnostics, BMP decoder header split, PNG header/chunk/decode-boundary/unfilter/decoder-chain wrapper/zlib stored-block inflater diagnostics, image texture placeholder policy split, image texture upload helper split, image engine boundary lock, image texture cache residency split tests, image texture diagnostics helper split, image upload retry policy split, asset pack index/lookup/fallback/validation/manifest/version-policy split/integrity/runtime resolver/catalog/materialization/byte-provider/materialized byte-integrity/cache-key cache-policy validation, Vulkan loader probe, Vulkan instance creation boundary/split, Vulkan device readiness boundary, Vulkan render-pass/framebuffer readiness boundary, Vulkan command-recording readiness boundary, Vulkan command-submit readiness boundary, Vulkan queue submit/present adapter boundary and renderer/backend summary diagnostics, Vulkan shader module readiness boundary, Vulkan pipeline layout readiness boundary, Vulkan graphics pipeline readiness boundary, Vulkan pipeline readiness summary, Vulkan frame lifecycle helper, Vulkan frame/pipeline/command-buffer-submit/command-recorder-gate split tests/resource-binding split tests/pipeline cache split/resource binding split/frame-present/frame-resource/descriptor-validation/swapchain policy/pipeline compatibility diagnostics, Vulkan adapter name diagnostics split, and input routing/action/text-edit/UTF-8-safe IME preedit/selection/IME split tests/gesture-cancel/focus-traversal/pointer-capture arbitration/gesture route-policy/multipointer touch diagnostics. Audio work is not a current priority.

## Active requirement IDs

- 06: swipe/known/unknown quiz flow.
- 20: known/unknown learning state aging.
- 02: quiz UI/design upgrade through native renderer contracts.
- 24: FSM/app action routing and input normalization.
- 30: previous-question swipe routing.
- 31: keyboard-safe blank input and caret/IME handling.
- 46: source/translation split-view scene support.
- 57: known bucket and long-press unknown behavior.
- 79: verification discipline and interface locks.
- 82: pure C++/Vulkan remake engine path.

## Contracts

- Preserve `modifier_interface -> scene_layout_data_modifier -> scene_layout_patch / scene_layout_edit_data -> scene_layout_data -> layout_placer -> ui_renderer -> vulkan_renderer`.
- `scene_layout_edit_data` is the only write surface for modifiers; modifiers must not mutate full scene/layout/renderer state directly.
- `ui_renderer`, `layout_placer`, and `vulkan_renderer` must not include domain headers or infer quiz semantics.
- `src/app/app_quiz_screens.h` reads `domain::app_snapshot` as the app-owned presentation bridge. Do not move that coupling into `src/core/ui`.
- Scene/UI modifiers emit actions only; app/domain services own state changes.
- Renderer layers must not own quiz, domain, UI, input, or audio state.
- Architecture boundary tests now lock the intended direction: layout does not depend on UI/render, UI does not depend on Vulkan backend, scene layout data does not depend on render, Vulkan backend does not depend on scene/UI/app/domain, and asset/text engine core files do not depend on upper layers except explicit bridge adapters.
- Scene core is also locked against app/domain/input/audio/assets/platform includes so `scene_layout_data` remains a renderer-agnostic data contract instead of a domain or platform state owner.
- Architecture boundary tests also reject host-specific source paths such as `/mnt/c/aa`, `C:/aa`, and direct `build/external/lib/cpp/desktop` references in app source. External libraries are selected through engine metadata/adapters, not hard-coded checkout paths.
- Engine workers own only their engine folders. App/runtime, top-level CMake, and aggregate contract wiring stay with the integrator unless explicitly assigned.
- Large file splitting is allowed when it improves module cohesion, worker ownership, reviewability, or conflict isolation. Do not split files only because they exceed a line-count threshold, and do not move stable public interfaces without explicit integrator approval.
- Build `quiz_vulkan_interface_contract_compile_tests` before handoff.
- Latest integration note: `ff7a822` threads text input presentation snapshots into normalized input replay diagnostics, so fixture replays expose focus/caret/selection/preedit/submit read-model state with diff evidence outside UI/app/domain coupling.
- Latest integration note: `5597b54` threads Vulkan SDK readiness diagnostics through native function-table, command recording, submit, present, and frame handoff summaries, while preserving default fake-path behavior unless checked SDK data is supplied.
- Latest integration note: `dd3d5ca` threads stb/external decoder selection diagnostics into image decoder chain and texture pipeline snapshots so internal decoder, adapter-ready, and fallback states are visible per texture request.
- Latest integration note: `d786fa8` adds text input presentation diff helpers so focus/target/display text/caret/selection/preedit/submit/clean-flag and byte-count changes can be reviewed without UI/app/domain coupling.
- Latest integration note: `4bfecb0` threads external font backend dependency/capability probe results into fake text engine layout diagnostics, distinguishing fake-only, adapter-ready, and fallback states without linking external libraries.
- Latest integration note: `0c044e5` adds a Vulkan SDK/header capability boundary for future native backend wiring, modeling API version, header availability, and function/extension readiness without including Vulkan headers yet.
- Latest integration note: `47763c0` adds a text input presentation snapshot read model for focus, caret, selection, preedit, submit, and byte-count state without UI/app/domain coupling.
- Latest integration note: `4bfc2ee` adds stb image adapter selection diagnostics for future `stb_image` routing while preserving the current internal decoder chain and avoiding hard-coded external paths.
- Latest integration note: `6b2507f` adds external font backend dependency probes for future FreeType/HarfBuzz/utf8proc wiring without linking or including those libraries yet.
- Latest integration note: `93b294d` adds Vulkan swapchain image acquire planning diagnostics after swapchain/frame lifecycle readiness and before command recording, modeling timeout/out-of-date/suboptimal/error gates without real Vulkan calls.
- Latest integration note: `9696ac1` adds input routing diagnostics diff helpers for comparing semantic-free normalized event, route, keyboard, focus, IME, pointer-capture, and text-edit deltas.
- Latest integration note: `9e20085` adds image binding plan diff diagnostics for renderer-facing texture binding packet deltas without exposing image cache/uploader internals.
- Latest integration note: `53336e4` adds text frame draw plan diff diagnostics for renderer-facing glyph packet deltas without Vulkan/domain coupling.
- Latest integration note: `73755f8` splits input routing diagnostics into `input_routing_diagnostics.h`, keeping `input_engine.h` as the preserved include surface while reducing the engine header payload.
- Latest integration note: `7dbfb76` splits Vulkan native readiness helpers into `vulkan_backend_native_readiness.h`, keeping `vulkan_backend_adapter.h` as the preserved include surface while isolating data-only readiness threading logic.
- Latest integration note: `cc58697` adds image frame binding plan diagnostics that turn public image texture frame snapshots into renderer-facing texture binding packet readiness and frame delta data without Vulkan/cache/uploader internals.
- Latest integration note: `db49c0d` adds text frame draw plan diagnostics that convert text frame snapshots and atlas update/page metadata into renderer-facing glyph packet readiness data without Vulkan/domain coupling.
- Latest integration note: `345e768` splits normalized input replay diff diagnostics into `normalized_input_replay_diff.h`; the integrator registered both replay public headers in the input contract FILE_SET and verified the interface compile target plus focused replay CTest.
- Latest integration note: `7644763` splits text frame snapshot diagnostics into `text_frame_snapshot.h`, and `11dec3e` registers that public header in the render contract FILE_SET.
- Latest integration note: `ef48ce4` splits image texture frame snapshot diagnostics into `image_texture_frame_snapshot.h`, and `95f24d9` registers that public header in the render contract FILE_SET.
- Latest integration note: `c656cfb` threads Vulkan native function-table readiness through command-buffer recording, submit-batch planning, present-completion planning, and renderer/backend summary diagnostics without calling real Vulkan APIs.
- Latest integration note: `f4b32aa` adds normalized input replay diff diagnostics for final focus/caret/selection/text/preedit state, pointer capture, gesture timeline, IME timeline, keyboard intent counts, and semantic-free regression summaries.
- Latest integration note: `56f201b` and `155398b` add text frame snapshot diff diagnostics and image texture frame snapshot diff diagnostics, giving renderer handoff reviews added/removed/changed upload or texture handle deltas without exposing engine internals.
- Latest integration note: `e340728` routes `input_engine.update_time()` through the app loop, so held pointer gestures such as long press can emit before release and travel through the same app input router as raw platform events.
- Latest integration note: `23cefb6` bridges platform shell input beyond legacy synthetic taps by forwarding pointer down/move/up/cancel, wheel, key, and focus events into the raw input engine path for long-press, drag, wheel, and keyboard navigation support.
- Latest integration note: `90e3f43` adds text frame snapshot diagnostics that combine request batch planning, fallback-chain planning, atlas materialization, upload bridge request IDs, and consumed atlas-update IDs into a renderer-agnostic text handoff snapshot.
- Latest integration note: `33f64bb` adds Vulkan native function table diagnostics with opaque entrypoint availability for command-buffer recording, queue submit, and present paths through injectable fake/system symbol resolvers.
- Latest integration note: `09ea764` and `004e70c` add focus/caret replay timeline diagnostics and image texture frame snapshot diagnostics, giving input and image engines compact per-frame/replay handoff views without app or Vulkan coupling.
- Latest integration note: `5c4d0d9` threads atlas upload bridge IDs through fake text engine layout/consume diagnostics so queued atlas updates and consumed upload request IDs can be matched without renderer/Vulkan coupling.
- Latest integration note: `6201935` adds image texture handle-map diagnostics after batch execution, exposing stable request-index/cache-key to texture-id mappings, sampler policy, placeholder flags, and residency pressure for renderer handoff without Vulkan/cache internals.
- Latest integration note: `ba8bc04` adds pointer/touch gesture replay timeline diagnostics to normalized input replay, covering tap/long-press/swipe/drag/cancel/wheel entries, pointer capture lifecycle, and final capture state.
- Latest integration note: `2f60e89` adds a text atlas upload request bridge that turns text request batch/materialization results into deterministic `render_text_atlas_update`-style upload request diagnostics and stable request IDs.
- Latest integration note: `85dd390` adds Vulkan present-completion planning diagnostics after submit-batch planning, producing present request/result summaries and frame completion status before real `vkQueuePresentKHR`.
- Latest integration note: `ada6db8` and `f61ffe2` extend fallback-chain diagnostics to text helper paths and thread image residency/budget summaries into batch execution diagnostics.
- Latest integration note: `4afdd42` adds IME replay timeline diagnostics to normalized input replay, covering composition start/update/commit/cancel, preedit validity, caret/selection snapshots, stale-preedit clearing, and committed text state.
- Latest integration note: `7004837` and `67d0552` thread fallback-chain diagnostics through fake text layout and add image texture residency/budget planning, keeping both text fallback and image memory pressure reasoning data-only.
- Latest integration note: `e50a2b0` threads keyboard shortcut/chord diagnostics into normalized input replay summaries so replayed key sequences expose intent counts, modifier/repeat policy, and final focus/text/preedit state.
- Latest integration note: `6bd7973` adds Vulkan submit-batch planning diagnostics after command-buffer recording, recording wait/signal/present intent and queue submit prerequisites before real Vulkan queue submission.
- Latest integration note: `85f4b86` and `7f3710b` add image texture batch execution diagnostics and semantic-free keyboard shortcut route diagnostics, keeping image pipeline execution and key/chord intent routing inside their owning engines.
- Latest integration note: `fb8e404` adds text font fallback chain planning diagnostics for mixed-script batches, producing per-run fallback chains, missing-glyph summaries, and deterministic selected-face order before shaping.
- Latest integration note: `38466be` adds image texture batch planning diagnostics that normalize image refs into texture requests, deduplicate source/texture keys, and preserve placeholder/fallback intent without exposing cache/uploader internals.
- Latest integration note: `9c8e7ed` adds Vulkan command-buffer recording diagnostics after recorder operation planning, producing data-only record events/counts/statuses before submit/present.
- Latest integration note: `94569e6` adds text request batch planning diagnostics that normalize text/layout requests, deduplicate glyph atlas materialization work, and report planned atlas/update summaries before renderer upload.
- Latest integration note: `5335929` adds normalized input replay diagnostics so pointer, touch, wheel, text, and IME fixture batches can be replayed with end-state summaries without app/domain/renderer coupling.
- Latest integration note: `da544dc` adds Vulkan command recorder operation plan diagnostics after packet execution, converting approved rect/text/image/debug packets into data-only recorder operation summaries before real Vulkan command recording exists.
- Latest integration note: `6ac0d43` adds glyph atlas materialization diagnostics to the fake text engine so shaped glyph/layout output, backend selection metadata, atlas requests, and summary policy stay visible before renderer/Vulkan upload exists.
- Latest integration note: `6170c74` threads image decoder capability manifest snapshots into image texture pipeline diagnostics while preserving decoder selection, fallback, cache reuse, and placeholder outcomes inside the image engine boundary.
- Latest integration note: `1cbc4cf` adds Vulkan command packet execution diagnostics that verify command packet bridge output can be executed only when frame, resource binding, command recorder, and packet prerequisites are satisfied.
- Latest integration note: `83e0e1d`, `9bd7bff`, and `4f54fac` thread text backend selection diagnostics through fake layout output, add image decoder capability manifest diagnostics, and add input diagnostic summary counts across normalized events and routed actions.
- Latest integration note: `81514a6` adds a Vulkan command packet bridge that converts prepared draw-list batches into backend packet diagnostics, counts rect/text/image/debug packets, and blocks command recording on missing pipeline/resource prerequisites.
- Latest integration note: `77ecf32` wires the optional third-party image decoder adapter through the existing image texture pipeline and preserves adapter diagnostics through fallback; `5b080c8` hardens input gesture/focus diagnostics for wheel, touch cancel, focus traversal, and pointer capture restart/release.
- Latest integration note: `2656970` adds a Vulkan frame pipeline handoff summary/result that composes loader, instance, device, swapchain, render pass, pipeline, resource binding, command recording, submit, present, and fallback readiness without scene/UI/app/domain coupling.
- Latest integration note: text/image/input worker commits added font backend selection metadata, optional third-party image decoder adapter boundary, and IME/focus/caret hardening; CMake render contract FILE_SET registration was handled by the integrator.
- Latest integration note: `15d77ce` reports app scene modifier errors in `app_render_report`; `0a721e2` blocks host/external source paths in architecture tests; `7505a63` tracks native dependency manifest/README while ignoring downloaded source directories.
- Latest verification: Windows MinGW focused text CTest passed 3/3 after `7644763` and `11dec3e`; focused image/Vulkan CTest passed 9/9 after `ef48ce4`, `c656cfb`, and `95f24d9`; latest full CTest passed 89/89. Current `ctest -N` reports 90 tests.

## Verification commands

```powershell
# From apps/quiz/quiz-vulkan
& "C:\Program Files\CMake\bin\cmake.exe" --preset windows-mingw-ascii
$buildDir = Resolve-Path ..\..\..\build\out\quiz\quiz-vulkan\windows-mingw-ascii
& "C:\Program Files\CMake\bin\cmake.exe" --build --preset windows-mingw-ascii-debug --target quiz_vulkan_app_input_router_tests
& "C:\Program Files\CMake\bin\ctest.exe" --test-dir "$buildDir" -R "^quiz_vulkan_app_input_router_tests$" --output-on-failure
& "C:\Program Files\CMake\bin\cmake.exe" --build --preset windows-mingw-ascii-debug --target quiz_vulkan_interface_contract_compile_tests
& "C:\Program Files\CMake\bin\ctest.exe" --test-dir "$buildDir" -N
& "C:\Program Files\CMake\bin\ctest.exe" --test-dir "$buildDir" --output-on-failure
git diff --check
```
