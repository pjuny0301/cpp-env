# Track D Stretch Rollup — Scenario Validation

- 시각 UTC: 2026-06-09T11:47:00Z
- 범위: Track D stretch backlog after wrong-note mode scenario
- 상태: done; PR #25 updated

## 완료

- Added due restart scenario coverage from quiz results back into a due known
  question.
- Added day-intro random mode scenario coverage.
- Added no-op text-submit trace coverage for screens without text submit
  handlers.
- Added compact 320x480 viewport replay coverage for the start-answer-results
  scenario path.

## 검증

- `cmake --build --preset linux-debug --target quiz_vulkan_app_scene_scenario_tests quiz_vulkan_app_scene_preview_tests quiz_vulkan_app_quiz_screens_tests`
- `ctest --test-dir build/out/quiz/quiz-vulkan/linux-ninja -R "quiz_vulkan_app_(scene_scenario|scene_preview|quiz_screens)_tests$" --output-on-failure`
- `git diff --check`

## 다음

- Continue with independent scenario variants, trace detail checks, or preview
  validation if more stretch budget remains.
