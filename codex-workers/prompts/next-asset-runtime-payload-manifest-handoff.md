You are the Asset System Codex worker. Keep this session alive after you finish.

Start from the latest integration baseline:

```bash
cd /mnt/c/aa-workers/asset-unified-cache-key-20260514
git fetch origin
git checkout -B codex/asset-runtime-payload-manifest-handoff-20260520 origin/codex/quiz-vulkan-remake-baseline
```

Task: add a generic runtime payload manifest handoff for assets.

Write scope:
- `apps/quiz/quiz-vulkan/src/assets/*`
- `apps/quiz/quiz-vulkan/tests/assets/*`

Do not edit app, scene, UI, domain, input, audio, text, image, Vulkan, top-level CMake, or aggregate contract files.

Current baseline has shader runtime payload summaries and diffs. Add one generic asset-owned handoff layer:
- Build a runtime payload manifest summary that can list shader/font/image/sound/deck payload identities without importing those engine headers.
- Include asset type, source URI, cache key, revision, content hash, byte count, ready/blocked status, and blocker summary.
- Include stable manifest identity and diff evidence for added/removed/changed payloads.
- Keep absolute host paths out of stable identities.
- Do not add renderer or Vulkan semantics; this is asset metadata only.

Verification before handoff:
- `build_dir="$(/mnt/c/aa/codex-workers/quiz-vulkan-worker-build-dir.sh "$(pwd)" windows-mingw-ascii)"`
- `/mnt/c/aa/codex-workers/with-build-lock.sh "/mnt/c/Program Files/CMake/bin/cmake.exe" --build "$build_dir" --target quiz_vulkan_interface_contract_compile_tests quiz_vulkan_asset_bytes_provider_tests quiz_vulkan_asset_runtime_catalog_tests quiz_vulkan_asset_runtime_resolver_policy_tests`
- `/mnt/c/aa/codex-workers/with-build-lock.sh "/mnt/c/Program Files/CMake/bin/ctest.exe" --test-dir "$build_dir" -R "asset_bytes_provider|asset_runtime_catalog|asset_runtime_resolver_policy" --output-on-failure`
- `git diff --check`

Commit your changes with a concise message and leave the session open.
