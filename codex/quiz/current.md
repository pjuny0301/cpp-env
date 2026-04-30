# Quiz Current Handoff

Last updated: 2026-04-30

## Top priorities

- Keep the native C++/Vulkan remake aligned with the Android quiz UX baseline while routing UX actions through app/domain actions.
- Treat long press as the unknown-question path: UI button contract `mark_question_unknown`, app input route `mark_question_unknown`, domain payload `mark_question_unknown_action`.
- Keep test status counts derived from the configured build with `ctest -N`; do not freeze global CTest totals in docs.
- Build the engine stack in dependency order: Vulkan backend diagnostics, text layout/atlas, image texture cache, input/IME, then audio/backend wiring.
- Worker branches should start from the current baseline with a fresh branch. Do not rebase stale worker branches that contain already-cherry-picked historical commits.
- Latest integrated baseline includes architecture boundary locks, text line-break/glyph cache/font-resolution/readiness/atlas-page/upload-policy diagnostics, image sampler/cache/upload queue/lifetime eviction and decoder diagnostics, asset pack index/lookup/validation/manifest/runtime resolver validation, Vulkan frame/pipeline/command-buffer-submit lifecycle/resource diagnostics, input routing/action/text-edit/IME preedit/gesture-cancel policy diagnostics, and procedural audio mixer event unification.

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
- Architecture boundary tests now lock the intended direction: layout does not depend on UI/render, UI does not depend on Vulkan backend, scene layout data does not depend on render, and Vulkan backend does not depend on scene/UI/app/domain.
- Engine workers own only their engine folders. App/runtime, top-level CMake, and aggregate contract wiring stay with the integrator unless explicitly assigned.
- Build `quiz_vulkan_interface_contract_compile_tests` before handoff.

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
