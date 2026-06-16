# Track A Stretch - Deck List Script Migration

## Completed

- Added `make_deck_list_screen_script_document` for the deck list screen.
- Routed `make_deck_list_screen_patch` and deck-list modifier updates through the app scene script compiler.
- Added a placed-render equivalence check between the direct deck list builder and scripted deck list patch.

## Commit

- `a88e8d1` - Migrate deck list screen to scene script

## Verification

- `cmake --build --preset linux-debug --target quiz_vulkan_app_quiz_screens_tests quiz_vulkan_app_scene_script_tests quiz_vulkan_architecture_boundary_tests quiz_vulkan_interface_contract_compile_tests` passed.
- `ctest --test-dir build/out/quiz/quiz-vulkan/linux-ninja -R "quiz_vulkan_(app_quiz_screens_tests|app_scene_script_tests|architecture_boundary_tests)$" --output-on-failure` passed 3/3.
- `git diff --check` passed.
- `git diff --cached --check` passed before commit.

## Next

- Push the Track A PR branch update.
- Continue with the next independent stretch item.
