# Track A Stretch Rollup — Scene Script Surface

- 시각 UTC: 2026-06-09T11:44:54Z
- 범위: Track A stretch backlog after `5c78eae`
- 상태: done; PR #24 updated

## 완료

- Added current question learning expressions: `question.learning`,
  `question.is_learning`, `question.is_known`, `question.is_unknown`, and
  `question.is_wrong_note`.
- Added deterministic string predicate functions: `contains`, `starts_with`,
  and `ends_with`.
- Added `format_count(count, singular, plural?)` for script-authored count
  labels.
- Added numeric node binding targets for image/style/layout refs:
  `image.aspect_ratio`, `style.opacity`, `style.border_radius`,
  `layout.width`, `layout.height`, and `layout.gap`.
- Updated `scene-script-examples.md` and `scene-schema.md` with the expanded
  expression/function/binding surface.

## 검증

- `cmake --build --preset linux-debug --target quiz_vulkan_app_scene_script_tests quiz_vulkan_app_quiz_screens_tests quiz_vulkan_architecture_boundary_tests`
- `ctest --test-dir build/out/quiz/quiz-vulkan/linux-ninja -R "quiz_vulkan_(app_scene_script_tests|app_quiz_screens_tests|architecture_boundary_tests)$" --output-on-failure`
- `git diff --check`

## 다음

- Continue with independent stretch items: additional scenario coverage,
  compact validation, or low-risk script binding/function expansion.
