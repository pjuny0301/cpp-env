# Track A Stretch - Scene Script Formatters

## Completed

- Added pipe formatter support for scene script expressions such as `{{ question.prompt | upper }}`.
- Supported `string`, `trim`, `upper`, and `lower` formatter names with left-to-right chaining.
- Preserved unformatted single-interpolation scene value typing while converting formatted values to strings.
- Added scene script tests for single and chained formatter bindings.

## Commit

- `cccfb6f` - Add scene script formatter pipeline

## Verification

- `cmake --build --preset linux-debug --target quiz_vulkan_app_scene_script_tests quiz_vulkan_app_quiz_screens_tests quiz_vulkan_architecture_boundary_tests quiz_vulkan_interface_contract_compile_tests` passed.
- `ctest --test-dir build/out/quiz/quiz-vulkan/linux-ninja -R "quiz_vulkan_(app_scene_script_tests|app_quiz_screens_tests|architecture_boundary_tests)$" --output-on-failure` passed 3/3.
- `git diff --check` passed.
- `git diff --cached --check` passed before commit.

## Next

- Push the Track A PR branch update.
- Continue with another independent stretch item.
