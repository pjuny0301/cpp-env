Continue in the same long-lived asset-system session, but start from the latest
pushed integration baseline before editing.

Task: make shader asset byte resolution more useful for the Vulkan backend
without crossing into Vulkan implementation.

Fresh branch:
- Fetch origin.
- Create/switch to a new branch from `origin/codex/quiz-vulkan-remake-baseline`.
- Suggested branch: `codex/asset-shader-byte-pipeline-summary-20260518`.

Ownership:
- You may edit only:
  - `apps/quiz/quiz-vulkan/src/assets/*`
  - `apps/quiz/quiz-vulkan/tests/assets/*`
- Do not edit render/vulkan, render/text, render/image, `src/app/*`,
  `app.cpp`, `main.cpp`, top-level `CMakeLists.txt`, or aggregate interface
  registration.
- If CMake or Vulkan wiring is required, report the exact proposal instead of
  editing it.

Goal:
- Add asset-owned shader byte pipeline summary/evidence that can tell a Vulkan
  consumer whether a shader source/binary asset was resolved from manifest,
  fallback, local fixture, or missing source.
- Include stable cache key/materialized byte identity, source URI, byte count,
  content hash, traversal rejection, stale/missing manifest state, and consumer
  readiness/blocker reason.
- Keep assets generic: font/image/sound/deck/shader should still use the same
  resolver/cache-key principles. Do not introduce Vulkan-specific dependencies
  into the asset layer.
- Prefer approved external items already in `/mnt/c/aa/build/external`; do not
  download or duplicate tools unless materially necessary.

Verification:
- Build `quiz_vulkan_interface_contract_compile_tests`.
- Build and run focused asset tests, including any new test and at least:
  `quiz_vulkan_asset_bytes_provider_tests`,
  `quiz_vulkan_asset_manifest_tests`,
  `quiz_vulkan_asset_runtime_catalog_tests`,
  `quiz_vulkan_asset_runtime_resolver_policy_tests`.
- Run `git diff --check`.

Commit:
- Commit scoped changes on your branch.
- Final report: changed files, exact verification commands/results, external
  dependencies used, risks, integrator-owned wiring proposal if needed, and
  commit hash.
