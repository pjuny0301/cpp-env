# Quiz Current Handoff

Last updated: 2026-04-28

## Top priorities

- Keep the native C++/Vulkan remake aligned with the Android quiz UX baseline while routing UX actions through app/domain actions.
- Treat long press as the unknown-question path: UI button contract `mark_question_unknown`, app input route `mark_question_unknown`, domain payload `mark_question_unknown_action`.
- Keep test status counts derived from the configured build with `ctest -N`; do not freeze global CTest totals in docs.

## Active requirement IDs

- 06: swipe/known/unknown quiz flow.
- 20: known/unknown learning state aging.
- 24: FSM/app action routing.
- 30: previous-question swipe routing.
- 57: known bucket and long-press unknown behavior.
- 79: verification discipline and interface locks.

## Contracts

- Preserve `modifier_interface -> scene_layout_data_modifier -> scene_layout_patch / scene_layout_edit_data -> scene_layout_data -> layout_placer -> ui_renderer -> vulkan_renderer`.
- `scene_layout_edit_data` is the only write surface for modifiers; modifiers must not mutate full scene/layout/renderer state directly.
- `ui_renderer`, `layout_placer`, and `vulkan_renderer` must not include domain headers or infer quiz semantics.
- `src/core/ui/quiz_screens.h` currently reads `domain::app_snapshot` as a temporary app screen presenter. Treat it as a refactor target, not as permission for UI renderer/domain coupling.
- Scene/UI modifiers emit actions only; app/domain services own state changes.
- Renderer layers must not own quiz, domain, UI, input, or audio state.
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
git diff --check
```
