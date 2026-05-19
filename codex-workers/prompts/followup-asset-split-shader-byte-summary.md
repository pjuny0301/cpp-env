Continue in the same asset-system session on your current branch.

Do not start a new feature. Refine the already committed shader byte source
pipeline summary before integrator merge.

Issue:
- The feature direction is acceptable, but `src/assets/asset_typed_materialized_bytes.h`
  grew by about 500 lines and is now too large to keep expanding.

Task:
- Split the shader-byte source summary types/helpers into a focused asset-owned
  header, for example:
  `apps/quiz/quiz-vulkan/src/assets/asset_shader_byte_pipeline_source_summary.h`
- Keep `asset_typed_materialized_bytes.h` as the compatibility include point if
  existing tests/users include it. It may include the new header, but should not
  keep the full implementation block inline there.
- Keep test changes in `apps/quiz/quiz-vulkan/tests/assets/*`.
- Do not edit top-level `CMakeLists.txt`; if adding the new header to FILE_SET
  is needed, report the exact integrator-owned CMake line instead of editing it.
- Do not change public signatures unless required by the split.

Verification:
- Build `quiz_vulkan_interface_contract_compile_tests`.
- Build/run:
  `quiz_vulkan_asset_bytes_provider_tests`,
  `quiz_vulkan_asset_manifest_tests`,
  `quiz_vulkan_asset_runtime_catalog_tests`,
  `quiz_vulkan_asset_runtime_resolver_policy_tests`.
- Run `git diff --check`.

Commit:
- Commit the split as a second scoped commit on the same branch.
- Final report changed files, verification, whether CMake FILE_SET needs an
  integrator update, risks, and commit hash.
