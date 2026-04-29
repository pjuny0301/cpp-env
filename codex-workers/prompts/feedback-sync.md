Role: Feedback Sync Codex

Purpose:
Apply repository feedback that crosses documentation, traceability, and app-level UX routing without drifting into engine implementation work.

Allowed write scope for this task:
- `apps/quiz/quiz-vulkan/docs/test-plan.md`
- `apps/quiz/quiz-vulkan/requirements_traceability_matrix.md`
- `apps/quiz/quiz-vulkan/tests/test_manifest.json`
- `apps/quiz/quiz-vulkan/src/app/app_input_router.cpp`
- `apps/quiz/quiz-vulkan/tests/app/app_input_router_tests.cpp`
- `codex/quiz/current.md`
- nearest AGENTS/docs only if needed to keep the new current-file rule discoverable

Do not edit:
- engine implementation folders except for reading,
- top-level `CMakeLists.txt`,
- aggregate contract registration,
- Android or editor app sources,
- build/output/external folders.

Tasks:
1. Read `/mnt/c/aa/AGENTS.md`, `/mnt/c/aa/apps/quiz/quiz-vulkan/AGENTS.md`, and `/mnt/c/aa/codex/quiz/AGENTS.md` if present.
2. Fix the long-press UX mismatch if confirmed:
   - Docs say long press marks a question unknown.
   - `quiz_screens.h` button contract uses `mark_question_unknown`.
   - Check `src/app/app_input_router.cpp` and `tests/app/app_input_router_tests.cpp`.
   - If router/test currently map long press to known, change them to unknown.
3. Reduce test status drift:
   - Avoid manually asserting stale global CTest totals like 5, 15, 17, or 24 as if they are permanent.
   - Prefer documenting the command to get the current count: `ctest -N` from the Windows build dir.
   - Update stale docs only within the allowed files.
4. Reconcile `tests/test_manifest.json` with reality in the least expensive way:
   - Either make it explicitly a smoke-command manifest, or update it so it no longer pretends to be a full manual inventory.
   - Do not try to hand-list every source test unless the file is already structured that way and the update is small.
5. Create or update `codex/quiz/current.md` as a thin handoff file:
   - current top priorities,
   - active requirement IDs if locally obvious,
   - contracts that must not be broken,
   - latest verification commands.
   Keep it short.

Verification:
- Configure/build as needed with Windows CMake.
- Run at least:
  `quiz_vulkan_app_input_router_tests`
  and any doc/manifest checks you add.
- Run the aggregate interface lock:
  `quiz_vulkan_interface_contract_compile_tests`
- Run `git diff --check`.

Commit:
- Commit on `codex/feedback-sync`.
- Report changed files, verification, risks, and commit hash.
