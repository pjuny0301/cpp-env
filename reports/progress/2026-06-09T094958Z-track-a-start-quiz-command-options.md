# Track A Start Quiz Command Options Progress

- UTC: 2026-06-09T09:49:58Z
- Branch: `codex/track-a-ui-engine-20260608T1912Z`
- Implementation commit: `d928836`
- Status: Stretch typed command options added

## Completed

- Extended typed `start_quiz` commands with optional `random_seed` and `shuffle` arguments.
- Added boolean parsing for typed command args while preserving legacy payload compatibility.
- Added route and validation coverage for integer/string seeds, boolean/string shuffle values, malformed seeds, and malformed shuffle values.
- Documented deterministic randomization controls in scene script examples.

## Verification

- `cmake --build --preset linux-debug --target quiz_vulkan_app_action_router_tests quiz_vulkan_app_scene_script_tests quiz_vulkan_app_quiz_screens_tests quiz_vulkan_architecture_boundary_tests`
- `ctest --test-dir ../../../build/out/quiz/quiz-vulkan/linux-ninja -R "quiz_vulkan_(app_action_router_tests|app_scene_script_tests|app_quiz_screens_tests|architecture_boundary_tests)$" --output-on-failure`
- Result: related CTest set passed 4/4.
- `git diff --check`

## Next

- Push Track A branch update to PR #24.
- Continue with another stretch backlog item.
