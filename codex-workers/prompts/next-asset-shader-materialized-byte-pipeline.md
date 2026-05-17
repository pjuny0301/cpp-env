You are the long-lived Asset System worker. Keep this session alive after finishing.

Refresh safely:
- `git fetch origin`
- Start a fresh branch `codex/asset-shader-materialized-byte-pipeline-20260517`
  from `origin/codex/quiz-vulkan-remake-baseline`.
- Do not stack this on previous ahead worker branches.

Read:
- `/mnt/c/aa/AGENTS.md`
- `apps/quiz/quiz-vulkan/src/assets/AGENTS.md`
- `apps/quiz/quiz-vulkan/src/assets/asset_manifest.h`
- `apps/quiz/quiz-vulkan/src/assets/asset_runtime_catalog.h`
- `apps/quiz/quiz-vulkan/src/assets/asset_typed_materialized_bytes.h`
- `apps/quiz/quiz-vulkan/tests/assets/asset_runtime_catalog_tests.cpp`
- `apps/quiz/quiz-vulkan/tests/assets/asset_bytes_provider_tests.cpp`

Scope only:
- `apps/quiz/quiz-vulkan/src/assets/*`
- `apps/quiz/quiz-vulkan/tests/assets/*`

Do not edit:
- `src/app/*`, `app.cpp`, `main.cpp`
- top-level `CMakeLists.txt`
- render/Vulkan/text/image/input/audio/scene/domain/platform files
- aggregate test registration or build system files

Task:
- Add a narrow asset-owned shader materialized-byte pipeline summary that lets a later Vulkan integrator consume shader assets without hard-coded paths or direct file IO.
- Build on the existing runtime catalog/materialized bytes contracts.
- Keep it data-only and engine-neutral: no Vulkan headers, no shader compilation, no renderer calls, no app/domain references.
- The summary should classify shader payloads by stable asset id/cache key/source uri/materialized path/byte count/content hash and readiness.
- Add enough shader-specific validation evidence to catch:
  - ready materialized shader bytes;
  - blocked materialization;
  - blocked byte load;
  - integrity failure;
  - empty shader bytes;
  - non-SPIR-V-looking bytes when a `.spv` shader is expected;
  - duplicate shader asset ids in the input summary if that can happen at this layer.
- Prefer extending `asset_typed_materialized_bytes.h` rather than adding a new public header that would need integrator-owned FILE_SET registration. If a new public header is genuinely necessary, stop and report the exact integrator-owned registration instead of editing CMake.
- If you need external tools or libraries, first inspect the approved external dependency area under `/mnt/c/aa/build/external`; do not duplicate existing dependencies. This task should not need new downloads.

Implementation style:
- Prefer C++23 designated initialization.
- Add interface lock assertions only for asset-owned public structs/functions you add.
- Split helpers only if it improves cohesion/reviewability, not solely by line count.

Verification:
- Build `quiz_vulkan_interface_contract_compile_tests`.
- Build and run focused asset tests covering the shader materialized-byte pipeline.
- Run `git diff --check`.

Commit:
- Commit only scoped files.
- Report changed files, verification, external dependency use, risks, and commit hash.
