Start a fresh Text Engine task from the latest pushed baseline.

Baseline setup:
- `git fetch origin`
- create/switch to a fresh branch from `origin/codex/quiz-vulkan-remake-baseline`, suggested name `codex/text-harfbuzz-layout-handoff-20260515`
- Do not reuse the previous ahead commit branch for new work.

Scope:
- Only `apps/quiz/quiz-vulkan/src/render/text/*` and `apps/quiz/quiz-vulkan/tests/render/text/*`.
- Do not edit app/main/top-level CMake/aggregate contract registration; HarfBuzz is already wired in baseline.
- Do not include scene/UI/app/domain/input/audio headers.
- Split large text files only if it improves cohesion/reviewability and does not move stable public interfaces without explicit integrator approval.

Task:
- Continue from the HarfBuzz memory shaping adapter.
- Add a narrow handoff that lets text layout diagnostics distinguish HarfBuzz-shaped output from deterministic fallback shaping when materialized font bytes and backend capability are present.
- Preserve existing fake deterministic behavior when HarfBuzz is unavailable, bytes are missing, or no fixture font exists.
- Keep the handoff data-only: shaped glyph ids, cluster byte/codepoint ranges, advances, backend label/status, fallback reason, and atlas-readiness impact are enough.

Verification:
- Build `quiz_vulkan_interface_contract_compile_tests`, `quiz_vulkan_font_shaping_backend_tests`, `quiz_vulkan_fake_text_engine_tests`, and any new/changed focused text tests.
- Run the relevant focused text tests and `git diff --check`.
- Commit scoped files and report hash, changed files, verification, external dependency use, and risks.
