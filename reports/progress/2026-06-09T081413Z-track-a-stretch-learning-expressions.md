# Track A Stretch - Learning Expressions

## Completed

- Added scene script expressions for learning summary state.
- Covered `learning.question_count`, `learning.learning_count`, `learning.known_count`, `learning.unknown_count`, `learning.wrong_note_count`, and `learning.summary`.
- Added focused scene script tests for learning summary and known count bindings.
- Updated scene script docs and examples.

## Commit

- `0a4ebfc` - Add scene script learning expressions

## Verification

- `cmake --build --preset linux-debug --target quiz_vulkan_app_scene_script_tests quiz_vulkan_app_quiz_screens_tests quiz_vulkan_architecture_boundary_tests quiz_vulkan_interface_contract_compile_tests` passed.
- `ctest --test-dir build/out/quiz/quiz-vulkan/linux-ninja -R "quiz_vulkan_(app_scene_script_tests|app_quiz_screens_tests|architecture_boundary_tests)$" --output-on-failure` passed 3/3.
- `git diff --check` passed.
- `git diff --cached --check` passed before commit.

## Next

- Push the Track A PR branch update.
- Continue with another independent stretch item.
