You are the Asset System Codex worker. Keep this session alive after you finish.

Start by rebasing your working copy onto the current integration baseline:

```bash
cd /mnt/c/aa-workers/asset-unified-cache-key-20260514
git fetch origin
git checkout -B codex/asset-runtime-manifest-materialized-bytes-cache-20260519 origin/codex/quiz-vulkan-remake-baseline
```

Task: make manifest-resolved materialized bytes more useful as a runtime cache layer for render resources.

Write scope:
- `apps/quiz/quiz-vulkan/src/assets/*`
- `apps/quiz/quiz-vulkan/tests/assets/*`

Do not edit app, renderer, scene, UI, domain, input, audio, text, image, Vulkan, top-level CMake, or aggregate contract registration. You are not alone in the codebase; do not revert or reshape other workers' work.

Requirements:
- Keep asset interfaces stable (`asset_resolver.h`, `asset_path_policy.h`, `asset_manifest.h`, `asset_runtime_catalog.h`, `asset_bytes_provider.h`).
- Add/extend a cache identity or materialized-bytes reuse path for manifest entries that feed render resources.
- Prove repeated resolve/read returns stable identity/evidence and path traversal or missing-entry failures stay explicit.
- Keep the folder neutral: no renderer or app includes. If renderer-specific wiring is needed, leave a short proposal in the commit message or final note.

Verification before handoff:
- `cmake --build . --target quiz_vulkan_interface_contract_compile_tests quiz_vulkan_asset_bytes_provider_tests quiz_vulkan_asset_manifest_tests quiz_vulkan_asset_runtime_catalog_tests quiz_vulkan_asset_runtime_resolver_policy_tests`
- `ctest -R "asset_bytes_provider|asset_manifest|asset_runtime_catalog|asset_runtime_resolver_policy" --output-on-failure`
- `git diff --check`

Commit your changes with a concise message and leave the session open.
