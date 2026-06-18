# Track D Progress - 2026-06-09T13:25:47Z

## Completed

- Added button replay coverage for incorrect text answer submission through `quiz_active_submit_text`.
- Added Day Intro learning-mode replay coverage for `day_intro_start_known`.
- Added Day Intro wrong-note replay coverage for `day_intro_start_wrong_note`.
- Preserved the scenario trace checks for before/after screen, target node, focus handoff, final session mode, and learning summary counts.

## Verification

- `cmake --build --preset linux-debug --target quiz_vulkan_app_scene_scenario_tests quiz_vulkan_app_scene_preview_tests quiz_vulkan_app_quiz_screens_tests`
- `ctest --test-dir build/out/quiz/quiz-vulkan/linux-ninja -R "quiz_vulkan_app_(scene_scenario|scene_preview|quiz_screens)_tests$" --output-on-failure`
- `git diff --check`

## Commits

- `ac6d0fb` - Cover incorrect text submit button replay
- `1f204aa` - Cover day intro known mode scenario replay
- `e0587dc` - Cover day intro wrong note scenario replay
