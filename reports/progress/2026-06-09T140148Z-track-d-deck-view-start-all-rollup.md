# Track D Progress - 2026-06-09T14:01:48Z

## Completed

- Added scenario replay coverage for the Deck View `deck_view_start_all` button.
- Verified the trace from deck list selection to deck view, then from deck view start-all to quiz active.
- Captured the actual deck view focus handoff: the selected deck initially focuses the first day, while the start-all button is the tap target.
- Verified the final session starts in normal mode without requiring a selected day.

## Verification

- `cmake --build --preset linux-debug --target quiz_vulkan_app_scene_scenario_tests quiz_vulkan_app_scene_preview_tests quiz_vulkan_app_quiz_screens_tests`
- `ctest --test-dir build/out/quiz/quiz-vulkan/linux-ninja -R "quiz_vulkan_app_(scene_scenario|scene_preview|quiz_screens)_tests$" --output-on-failure`
- `git diff --check`

## Commits

- `d56c36e` - Cover deck view start all scenario replay
