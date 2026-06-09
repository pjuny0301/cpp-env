# Track A Progress - 2026-06-09T13:30:54Z

## Completed

- Added `between(value, min, max)` as an inclusive numeric scene script function.
- Covered `between(...)` in bindings, conditions, argument-count errors, and numeric parse errors.
- Documented `between(...)` in the scene schema and scene script examples.
- Added an architecture boundary guard that keeps quiz-specific semantic tokens out of `src/core/scene`.

## Verification

- `cmake --build --preset linux-debug --target quiz_vulkan_app_scene_script_tests quiz_vulkan_app_quiz_screens_tests quiz_vulkan_architecture_boundary_tests`
- `cmake --build --preset linux-debug --target quiz_vulkan_architecture_boundary_tests quiz_vulkan_app_scene_script_tests quiz_vulkan_app_quiz_screens_tests`
- `ctest --test-dir ../../../build/out/quiz/quiz-vulkan/linux-ninja -R "quiz_vulkan_(app_scene_script_tests|app_quiz_screens_tests|architecture_boundary_tests)$" --output-on-failure`
- `git diff --check`

## Commits

- `b6ce2b5` - Add scene script between function
- `0d8a7c9` - Guard scene core against quiz semantics
