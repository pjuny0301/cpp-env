You are the long-lived Asset System worker. Keep this session alive after finishing.

Start from the latest pushed integration baseline:

```bash
git fetch origin
git switch -C codex/asset-render-address-byte-payload-bridge-20260519 origin/codex/quiz-vulkan-remake-baseline
```

Scope:
- Own only `apps/quiz/quiz-vulkan/src/assets/*` and `apps/quiz/quiz-vulkan/tests/assets/*`.
- Do not edit `src/app/*`, `app.cpp`, `main.cpp`, top-level `CMakeLists.txt`, renderer/image/text/Vulkan integration, scene, UI, domain, input, or audio files.
- Do not rename or move stable asset interfaces.

Task:
- Bridge the existing `asset_render_resource_address` summary with typed materialized byte payloads so render-facing systems can ask: "for this render resource address, what materialized byte payload and cache identity should be consumed?"
- Keep the result asset-owned and neutral: no renderer, Vulkan, image, text, app, or domain includes.
- Add status/diagnostics for:
  - accepted address with matching materialized payload,
  - missing materialized bytes,
  - type mismatch,
  - cache-key mismatch,
  - duplicate canonical render resource identity.
- Add tests that cover image/font/shader-like typed assets without requiring renderer wiring.

Implementation guidance:
- Prefer a focused new asset header if that avoids further growing `asset_typed_materialized_bytes.h`.
- If top-level CMake registration is needed, report it instead of editing CMake.
- Use C++23 aggregate initialization style.

Verification:
- Build `quiz_vulkan_interface_contract_compile_tests`.
- Build and run focused asset targets you touch.
- Run `ctest -R asset --output-on-failure` if the worker build is already configured and focused targets pass.
- Run `git diff --check`.

Commit and report:
- Commit on your branch.
- Report changed files, verification commands/results, risks, and commit hash.
