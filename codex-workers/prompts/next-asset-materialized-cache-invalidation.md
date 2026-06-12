You are the Asset System Codex worker. Keep this session alive after you finish.

Start from the current integration baseline, not from your stale worker branch:

```bash
cd /mnt/c/aa-workers/asset-unified-cache-key-20260514
git fetch origin
git checkout -B codex/asset-materialized-cache-invalidation-20260519 origin/codex/quiz-vulkan-remake-baseline
```

Task: add materialized bytes cache invalidation/diff evidence using content hash or revision identity, while keeping absolute paths out of renderer-facing cache keys.

Write scope:
- `apps/quiz/quiz-vulkan/src/assets/*`
- `apps/quiz/quiz-vulkan/tests/assets/*`

Do not edit app, scene, UI, domain, input, audio, text, image, Vulkan, top-level CMake, or aggregate contract files. You are not alone in the codebase; do not revert or reshape other workers' work.

Current baseline has render resource materialized cache summaries. Add the next narrow layer:
- Model how a materialized byte entry is invalidated or replaced when content hash/revision changes.
- Prove that identical logical asset identity + unchanged content reuses the stable cache identity.
- Prove that changed content/revision creates a new materialized entry and a clear diff/diagnostic.
- Prove that absolute host paths do not leak into renderer-facing cache keys.
- Keep the asset layer neutral: no renderer execution, no image decode, no font shaping, no app/domain assumptions.

Verification before handoff:
- `cmake --build . --target quiz_vulkan_interface_contract_compile_tests quiz_vulkan_asset_bytes_provider_tests quiz_vulkan_asset_manifest_tests quiz_vulkan_asset_runtime_catalog_tests`
- `ctest -R "asset_bytes_provider|asset_manifest|asset_runtime_catalog" --output-on-failure`
- `git diff --check`

Commit your changes with a concise message and leave the session open.
