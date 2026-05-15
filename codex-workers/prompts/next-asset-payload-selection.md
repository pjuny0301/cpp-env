Start a fresh Asset System task from the latest pushed baseline.

Baseline setup:
- `git fetch origin`
- create/switch to a fresh branch from `origin/codex/quiz-vulkan-remake-baseline`, suggested name `codex/asset-payload-selection-20260515`
- Do not reuse the previous ahead commit branch for new work.

Scope:
- Only `apps/quiz/quiz-vulkan/src/assets/*` and `apps/quiz/quiz-vulkan/tests/assets/*`.
- Do not edit app/main/top-level CMake/aggregate contract registration.
- Do not include renderer/text/image/Vulkan/domain/input/audio headers.

Task:
- Continue from typed materialized byte payload bundles and bundle diffs.
- Add a small data-only selection/filter contract for engine consumers to request payloads by asset type, id, cache key, readiness, and integrity status.
- Preserve stable diagnostics for missing id, wrong type, blocked payload, integrity failure, duplicate id, and cache-key mismatch.
- Do not move bytes into engine folders; this remains asset-owned handoff evidence.

Verification:
- Build `quiz_vulkan_interface_contract_compile_tests`, `quiz_vulkan_asset_bytes_provider_tests`, and any new/changed focused asset tests.
- Run the relevant focused asset tests and `git diff --check`.
- Commit scoped files and report hash, changed files, verification, and risks.
