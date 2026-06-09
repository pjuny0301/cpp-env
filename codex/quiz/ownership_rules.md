# Quiz Ownership Rules

Stable ownership rules live here. Volatile session state belongs in `current.md` or `coordination/`.

## Stable Project Ownership

- `codex/quiz/big_plan.md`: source requirement list and dependency-based execution order.
- `codex/quiz/requirements_traceability_matrix.md`: requirement index, owner links, compact current evidence, and next checks.
- `codex/quiz/구현/NN.md`: root routing summary for a requirement. It must not duplicate project-level detail.
- `codex/quiz/<project>/구현/NN.md`: detailed implementation plan owned by that project.
- `apps/quiz/android-quiz-app`: reference mobile UX implementation.
- `apps/quiz/quiz-editor`: reference authoring and quiz-data workflow implementation.
- `apps/quiz/shared-ui`: shared generated React UI primitives consumed by both React apps.
- `apps/quiz/quiz-vulkan`: native C++/Vulkan convergence target.
- `build/external/quiz`: external material snapshots and non-source inputs.
- `build/out/quiz`: reproducible build outputs and raw logs.

## Volatile State

- Branch names, worktree paths, active session IDs, current blockers, and last heartbeats are coordination data.
- Put active multi-worker state in `coordination/PROGRESS_LEDGER.md` and cross-track claims or intrusion requests in `coordination/SHARED_NOTES.md`.
- Keep `codex/quiz/current.md` short and refreshable. Do not use it as a long-term worker log.
