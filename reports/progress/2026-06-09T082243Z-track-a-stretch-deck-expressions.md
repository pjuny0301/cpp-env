# Track A Stretch - Deck Expressions

## Completed

- Added scene script expressions for deck and selected day state.
- Covered `deck.count`, `selected_deck.*`, and `selected_day.*` bindings.
- Added focused scene script tests for selected deck title, selected day summary, and deck count.
- Updated scene script docs and examples.

## Commit

- `6246eeb` - Add scene script deck expressions

## Verification

- `cmake --build --preset linux-debug --target quiz_vulkan_app_scene_script_tests quiz_vulkan_app_quiz_screens_tests quiz_vulkan_architecture_boundary_tests quiz_vulkan_interface_contract_compile_tests` passed.
- `ctest --test-dir build/out/quiz/quiz-vulkan/linux-ninja -R "quiz_vulkan_(app_scene_script_tests|app_quiz_screens_tests|architecture_boundary_tests)$" --output-on-failure` passed 3/3.
- `git diff --check` passed.
- `git diff --cached --check` passed before commit.

## Next

- Push the Track A PR branch update.
- Continue with another independent stretch item.
