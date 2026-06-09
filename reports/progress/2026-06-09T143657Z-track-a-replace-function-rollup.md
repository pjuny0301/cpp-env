# Track A Progress - 2026-06-09T14:36:57Z

## Completed

- Added `replace(value, needle, replacement)` to the app-owned scene script expression engine.
- Covered normal string replacement against a selected deck source URI.
- Added validation for missing arguments and empty replacement needles.
- Documented the function in the scene schema and examples as an app/presentation-only string cleanup helper.

## Verification

- `cmake --build --preset linux-debug --target quiz_vulkan_app_scene_script_tests quiz_vulkan_app_quiz_screens_tests quiz_vulkan_architecture_boundary_tests`
- `ctest --test-dir ../../../build/out/quiz/quiz-vulkan/linux-ninja -R "quiz_vulkan_(app_scene_script_tests|app_quiz_screens_tests|architecture_boundary_tests)$" --output-on-failure`
- `git diff --check`

## Commits

- `bb7d057` - Add scene script replace function
