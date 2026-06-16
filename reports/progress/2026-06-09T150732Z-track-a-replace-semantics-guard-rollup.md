# Track A Progress - 2026-06-09T15:07:32Z

## Completed

- Added coverage proving `replace(...)` replaces all non-overlapping matches, not only the first match.
- Documented `replace(...)` repeated-match behavior and empty-needle rejection.
- Documented scene semantics ownership: `src/core/scene` keeps generic role/label/properties while quiz-specific role names and property keys stay in `src/app/app_quiz_scene_semantics.h`.
- Strengthened the architecture boundary guard so `src/core/scene` also rejects quiz semantic property-key strings such as `quiz.stage`.

## Verification

- `cmake --build --preset linux-debug --target quiz_vulkan_app_scene_script_tests quiz_vulkan_app_quiz_screens_tests quiz_vulkan_architecture_boundary_tests`
- `ctest --test-dir ../../../build/out/quiz/quiz-vulkan/linux-ninja -R "quiz_vulkan_(app_scene_script_tests|app_quiz_screens_tests|architecture_boundary_tests)$" --output-on-failure`
- `cmake --build --preset linux-debug --target quiz_vulkan_architecture_boundary_tests`
- `ctest --test-dir ../../../build/out/quiz/quiz-vulkan/linux-ninja -R "quiz_vulkan_architecture_boundary_tests$" --output-on-failure`
- `git diff --check`

## Commits

- `7f2c5fd` - Cover repeated replace script function matches
- `10c1d7e` - Document generic scene semantics ownership
- `ae7cfbd` - Guard scene core against quiz semantic properties
