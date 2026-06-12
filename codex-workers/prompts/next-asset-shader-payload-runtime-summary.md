You are the Asset System Codex worker. Keep this session alive after you finish.

Start from the latest integration baseline:

```bash
cd /mnt/c/aa-workers/asset-unified-cache-key-20260514
git fetch origin
git checkout -B codex/asset-shader-payload-runtime-summary-20260519 origin/codex/quiz-vulkan-remake-baseline
```

Task: make shader materialized bytes ready for later Vulkan shader-module consumption while keeping assets renderer-neutral.

Write scope:
- `apps/quiz/quiz-vulkan/src/assets/*`
- `apps/quiz/quiz-vulkan/tests/assets/*`

Do not edit app, scene, UI, domain, input, audio, text, image, Vulkan, top-level CMake, or aggregate contract files. You are not alone in the codebase; do not revert or reshape other workers' work.

Current baseline can bridge render resources to materialized bytes and diff cache invalidation. Add the next narrow layer:
- Add a shader payload runtime summary that classifies materialized shader bytes by stage-like metadata, cache identity, byte count, content hash, and revision.
- Do not parse SPIR-V deeply and do not include Vulkan headers. This is asset-owned metadata only.
- Prove unchanged shader bytes reuse stable runtime identity, revision changes invalidate the runtime identity, and absolute build/external paths do not leak into renderer-facing keys.
- Keep generic assets neutral: no renderer execution, no shader module creation, no app/domain assumptions.

Verification before handoff:
- `build_dir="$(/mnt/c/aa/codex-workers/quiz-vulkan-worker-build-dir.sh "$(pwd)" windows-mingw-ascii)"`
- `/mnt/c/aa/codex-workers/with-build-lock.sh "/mnt/c/Program Files/CMake/bin/cmake.exe" --build "$build_dir" --target quiz_vulkan_interface_contract_compile_tests quiz_vulkan_asset_bytes_provider_tests quiz_vulkan_asset_manifest_tests quiz_vulkan_asset_runtime_catalog_tests`
- `/mnt/c/aa/codex-workers/with-build-lock.sh "/mnt/c/Program Files/CMake/bin/ctest.exe" --test-dir "$build_dir" -R "asset_bytes_provider|asset_manifest|asset_runtime_catalog" --output-on-failure`
- `git diff --check`

Commit your changes with a concise message and leave the session open.
