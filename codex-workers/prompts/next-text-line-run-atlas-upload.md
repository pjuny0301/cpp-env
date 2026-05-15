Start a fresh Text Engine task from the latest pushed baseline.

Baseline setup:
- `git fetch origin`
- create/switch to a fresh branch from `origin/codex/quiz-vulkan-remake-baseline`, suggested name `codex/text-line-run-atlas-upload-20260515`
- Do not reuse a previous ahead branch for new work.

Scope:
- Only `apps/quiz/quiz-vulkan/src/render/text/*` and `apps/quiz/quiz-vulkan/tests/render/text/*`.
- Do not edit app/main/top-level CMake/aggregate contract registration.
- Do not include scene/UI/app/domain/input/audio headers.
- `text_engine.h` is the stable public Text Engine interface; stop and propose if you think it must move or change public signatures.

Task:
- Continue from shaping handoff, shaping atlas handoff, and line/run evidence diagnostics.
- Add a narrow data-only bridge from line/run evidence to glyph atlas materialization/upload readiness.
- Preserve which line/run/cluster requires which atlas key/page, whether it has a raster payload, whether upload is ready, and why it is blocked or reused.
- The evidence should make it possible to verify that a shaped line can be rendered from atlas/upload facts without renderer, UI, app, domain, input, or audio knowledge.
- Keep deterministic fallback behavior stable when HarfBuzz or fixture font bytes are unavailable.
- If fake text diagnostics files are becoming too broad, split only cohesive private diagnostics helpers/tests where it improves reviewability; do not move stable public interfaces without integrator approval.
- Existing external text libraries under `build/external` may be used; do not duplicate them. If you need a new useful library or fixture, place it under `build/external` or the worker worktree equivalent and report URL, version/commit, license, path, and reason.

Verification:
- Build `quiz_vulkan_interface_contract_compile_tests`, `quiz_vulkan_fake_text_engine_tests`, `quiz_vulkan_fake_text_engine_layout_diagnostics_tests`, and any new/changed focused text tests.
- Run the relevant focused text tests and `git diff --check`.
- Commit scoped files and report hash, changed files, verification, external dependency use, and risks.
