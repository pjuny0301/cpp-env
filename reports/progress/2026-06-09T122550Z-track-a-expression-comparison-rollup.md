# Track A Progress - 2026-06-09T12:25:50Z

## Completed

- Added `length(value)` as a pure scene script expression function that returns the rendered string length as an integer.
- Added `greater_than(left, right)` and `less_than(left, right)` for numeric script conditions and rendered boolean bindings.
- Extended the scene script fixture so the new functions render into text nodes and drive both true and false conditions.
- Documented the new functions in `apps/quiz/quiz-vulkan/docs/scene-schema.md`.

## Commits

- `fc01eeb` - Add scene script length function
- `409c8ae` - Add scene script numeric comparison functions

## Verification

- `cmake --build --preset linux-debug --target quiz_vulkan_app_scene_script_tests quiz_vulkan_app_quiz_screens_tests quiz_vulkan_architecture_boundary_tests` passed for both changes.
- `ctest --test-dir build/out/quiz/quiz-vulkan/linux-ninja -R "quiz_vulkan_(app_scene_script_tests|app_quiz_screens_tests|architecture_boundary_tests)$" --output-on-failure` passed 3/3 for both changes.
- `git diff --check` passed.

## Next

- Continue expression-engine stretch work only where it stays app/presentation-owned and does not add quiz-specific concepts to `core/scene`.
