Start a fresh Asset System task from the latest pushed baseline.

Baseline setup:
- `git fetch origin`
- create/switch to a fresh branch from `origin/codex/quiz-vulkan-remake-baseline`, suggested name `codex/asset-payload-transaction-diff-20260515`
- Do not reuse a previous ahead branch for new work.

Scope:
- Only `apps/quiz/quiz-vulkan/src/assets/*` and `apps/quiz/quiz-vulkan/tests/assets/*`.
- Do not edit app/main/top-level CMake/aggregate contract registration.
- Do not include renderer/text/image/Vulkan/domain/input/audio headers.

Task:
- Continue from ordered materialized byte payload request transactions.
- Add compact diff/summary evidence for two request transactions: added/removed request ids, changed statuses, changed selected snapshots, changed readiness/integrity/cache-key mismatch counts.
- Keep diffs data-only and do not copy payload byte vectors.
- Preserve deterministic diagnostics suitable for future font/image/sound/shader/deck handoff review.

Verification:
- Build `quiz_vulkan_interface_contract_compile_tests`, `quiz_vulkan_asset_bytes_provider_tests`, and any new/changed focused asset tests.
- Run the relevant focused asset tests and `git diff --check`.
- Commit scoped files and report hash, changed files, verification, and risks.
