# Quiz Current Handoff

Last updated: 2026-05-07

## Top priorities

- Keep the native C++/Vulkan remake aligned with the Android quiz UX baseline while routing UX actions through app/domain actions.
- Treat long press as the unknown-question path: UI button contract `mark_question_unknown`, app input route `mark_question_unknown`, domain payload `mark_question_unknown_action`.
- Keep test status counts derived from the configured build with `ctest -N`; do not freeze global CTest totals in docs.
- Build the engine stack in dependency order: Vulkan backend implementation, image decode/texture path, text layout/atlas, asset materialization, and input/IME. Audio/backend wiring is intentionally deferred for now.
- Worker branches should start from the current baseline with a fresh branch. Do not rebase stale worker branches that contain already-cherry-picked historical commits.
- Latest integrated baseline includes architecture boundary locks, worker build-lock serialization, input key/pointer/text route policy helpers, keyboard text input test split, input gesture diagnostics split tests, text line-break/glyph cache/font-resolution/readiness/font-face catalog/source/source-byte/file-byte-loader/file-byte-loader split tests/SFNT font bytes inspector/cmap coverage inspector/Unicode coverage resolver/catalog adapter/glyph-id resolver/shaping backend/shaped-glyph atlas-update tracing/resolved glyph-id atlas diagnostics/line-layout helper split/run-box/caret-hit-test/atlas-page/upload-policy/eviction/rasterizer atlas-readiness diagnostics, fake text layout diagnostics split tests, image sampler/cache/upload queue/retry/lifetime eviction/decoder/decoder-chain/standard decoder chain/data-URI/filesystem source-byte/filesystem pipeline/standard decoder-backed texture pipeline/standard pipeline cache-reuse diagnostics/placeholder texture/BMP decoder diagnostics, BMP decoder header split, PNG header/chunk/decode-boundary/unfilter/decoder-chain wrapper/zlib stored-block inflater diagnostics, image texture placeholder policy split, image texture upload helper split, image engine boundary lock, image texture cache residency split tests, image texture diagnostics helper split, image upload retry policy split, asset pack index/lookup/fallback/validation/manifest/version-policy split/integrity/runtime resolver/catalog/materialization/byte-provider/materialized byte-integrity/cache-key cache-policy validation, Vulkan loader probe, Vulkan instance creation boundary/split, Vulkan device readiness boundary, Vulkan render-pass/framebuffer readiness boundary, Vulkan command-recording readiness boundary, Vulkan command-submit readiness boundary, Vulkan queue submit/present adapter boundary and renderer/backend summary diagnostics, Vulkan frame lifecycle helper, Vulkan frame/pipeline/command-buffer-submit/command-recorder-gate split tests/resource-binding split tests/pipeline cache split/resource binding split/frame-present/frame-resource/descriptor-validation/swapchain policy/pipeline compatibility diagnostics, Vulkan adapter name diagnostics split, and input routing/action/text-edit/UTF-8-safe IME preedit/selection/IME split tests/gesture-cancel/focus-traversal/pointer-capture arbitration/gesture route-policy/multipointer touch diagnostics. Audio work is not a current priority.

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
- Engine workers own only their engine folders. App/runtime, top-level CMake, and aggregate contract wiring stay with the integrator unless explicitly assigned.
- Large file splitting is allowed when it improves module cohesion, worker ownership, reviewability, or conflict isolation. Do not split files only because they exceed a line-count threshold, and do not move stable public interfaces without explicit integrator approval.
- Build `quiz_vulkan_interface_contract_compile_tests` before handoff.
- Latest verification: Windows MinGW focused input CTest passed 7/7 after integrating input gesture diagnostics split tests. Current `ctest -N` reports 71 tests. Most recent full CTest remains 60/60 after `ddf7271`.

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
