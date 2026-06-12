You are the Asset System Codex worker. Keep this session alive after you finish.

Start from the latest integration baseline:

```bash
cd /mnt/c/aa-workers/asset-unified-cache-key-20260514
git fetch origin
git checkout -B codex/asset-shader-runtime-cache-diff-20260520 origin/codex/quiz-vulkan-remake-baseline
```

Task: make shader runtime payload summaries diffable and cache-invalidation friendly.

Write scope:
- `apps/quiz/quiz-vulkan/src/assets/*`
- `apps/quiz/quiz-vulkan/tests/assets/*`

Do not edit app, scene, UI, domain, input, audio, text, image, Vulkan, top-level CMake, or aggregate contract files. You are not alone in the codebase; do not revert or reshape other workers' work.

Current baseline classifies shader runtime payload entries by stage, revision, runtime identity, ready entries, and blocked entries. Add the next narrow asset-owned layer:
- Add a runtime summary diff or cache invalidation summary that detects stable reuse, content-hash changes, revision changes, missing revision, stage changes, and blocked/ready transitions.
- Keep host absolute paths out of stable runtime/cache identities.
- Preserve path traversal and manifest/root policy behavior.
- Do not add renderer or Vulkan semantics; asset output should remain generic shader payload metadata usable later by Vulkan.

Verification before handoff:
- `build_dir="$(/mnt/c/aa/codex-workers/quiz-vulkan-worker-build-dir.sh "$(pwd)" windows-mingw-ascii)"`
- `/mnt/c/aa/codex-workers/with-build-lock.sh "/mnt/c/Program Files/CMake/bin/cmake.exe" --build "$build_dir" --target quiz_vulkan_interface_contract_compile_tests quiz_vulkan_asset_bytes_provider_tests quiz_vulkan_asset_runtime_catalog_tests quiz_vulkan_asset_runtime_resolver_policy_tests`
- `/mnt/c/aa/codex-workers/with-build-lock.sh "/mnt/c/Program Files/CMake/bin/ctest.exe" --test-dir "$build_dir" -R "asset_bytes_provider|asset_runtime_catalog|asset_runtime_resolver_policy" --output-on-failure`
- `git diff --check`

Commit your changes with a concise message and leave the session open.
