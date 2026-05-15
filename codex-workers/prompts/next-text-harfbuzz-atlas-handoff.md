Start a fresh Text Engine task from the latest pushed baseline.

Baseline setup:
- `git fetch origin`
- create/switch to a fresh branch from `origin/codex/quiz-vulkan-remake-baseline`, suggested name `codex/text-harfbuzz-atlas-handoff-20260515`
- Do not reuse a previous ahead branch for new work.

Scope:
- Only `apps/quiz/quiz-vulkan/src/render/text/*` and `apps/quiz/quiz-vulkan/tests/render/text/*`.
- Do not edit app/main/top-level CMake/aggregate contract registration.
- Do not include scene/UI/app/domain/input/audio headers.

Task:
- Continue from HarfBuzz layout handoff diagnostics.
- Add a narrow data-only bridge from HarfBuzz-shaped glyph handoff records to existing glyph atlas/update readiness evidence.
- Preserve glyph id, cluster ranges, backend label/status, cache key/page key, atlas-ready/fallback/blocker summary, and deterministic fallback when HarfBuzz is unavailable or fixture bytes are missing.
- Do not create GPU resources or call renderer/Vulkan.
- If a large text file becomes harder to review, split only cohesive private diagnostics helpers; do not move stable public interfaces without explicit integrator approval.

Verification:
- Build `quiz_vulkan_interface_contract_compile_tests`, `quiz_vulkan_fake_text_engine_tests`, `quiz_vulkan_fake_text_engine_layout_diagnostics_tests`, and any new/changed focused text tests.
- Run the relevant focused text tests and `git diff --check`.
- Commit scoped files and report hash, changed files, verification, external dependency use, and risks.
