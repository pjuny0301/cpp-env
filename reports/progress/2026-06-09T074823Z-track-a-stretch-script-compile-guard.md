# Track A Stretch - Script Compile Guard

## Completed

- Added `require_compiled_app_scene_script_patch` to unwrap compile results with a context-rich failure.
- Routed built-in quiz screen patch helpers through the guarded unwrap.
- Added a focused test that failed compile results report the script context and reason instead of dereferencing an empty patch.

## Commit

- `837cd03` - Guard scene script patch unwraps

## Verification

- `cmake --build --preset linux-debug --target quiz_vulkan_app_scene_script_tests quiz_vulkan_app_quiz_screens_tests quiz_vulkan_architecture_boundary_tests quiz_vulkan_interface_contract_compile_tests` passed.
- `ctest --test-dir build/out/quiz/quiz-vulkan/linux-ninja -R "quiz_vulkan_(app_scene_script_tests|app_quiz_screens_tests|architecture_boundary_tests)$" --output-on-failure` passed 3/3.
- `git diff --check` passed.
- `git diff --cached --check` passed before commit.

## Next

- Push the Track A PR branch update.
- Continue with another independent stretch item.
