You are the long-lived Asset System worker. Keep this session alive after finishing.

Start from the latest pushed integration baseline:

```bash
git fetch origin
git switch -C codex/asset-render-resource-manifest-byte-e2e-20260519 origin/codex/quiz-vulkan-remake-baseline
```

Scope:
- Own only `apps/quiz/quiz-vulkan/src/assets/*` and `apps/quiz/quiz-vulkan/tests/assets/*`.
- Do not edit `src/app/*`, `app.cpp`, `main.cpp`, top-level `CMakeLists.txt`, render, scene, UI, domain, input, audio, or worker files.
- Do not rename/move/change public signatures in stable asset interfaces without stopping and reporting an exact proposal.

Task:
- Move asset handling toward a real runtime path by adding end-to-end asset-owned coverage from manifest entry to typed materialized bytes to render resource payload bridge.
- Use existing asset contracts:
  - `asset_resolver`;
  - `asset_runtime_catalog`;
  - `asset_bytes_provider`;
  - `asset_typed_materialized_bytes`;
  - `asset_render_resource_address`;
  - `asset_render_resource_payload_bridge`.
- Prove that `asset://` render resources can resolve to materialized bytes with stable cache identity for at least image/font/shader-like resource kinds without the renderer directly reading file paths.
- Add negative coverage for path traversal, missing manifest id, and mismatched typed resource kind.
- Keep this neutral: no renderer, scene, app runtime, PDF/OCR/AI worker, or domain ownership in asset code.

Implementation guidance:
- Prefer tests and small asset-owned helpers over broad interface changes.
- If app/runtime or CMake wiring is required, report a proposal instead of editing integrator-owned files.
- Use C++23 aggregate initialization style.

Verification:
- Build `quiz_vulkan_interface_contract_compile_tests`.
- Build and run `quiz_vulkan_asset_bytes_provider_tests`, `quiz_vulkan_asset_manifest_tests`, `quiz_vulkan_asset_runtime_catalog_tests`, and `quiz_vulkan_asset_runtime_resolver_policy_tests`.
- Run `ctest -R asset --output-on-failure` if focused tests pass.
- Run `git diff --check`.

Commit and report:
- Commit on your branch.
- Report changed files, verification commands/results, risks, and commit hash.
