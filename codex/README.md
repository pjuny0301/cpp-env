# Codex Shared Context

This folder contains Codex-side context. Reusable material lives under `common/`; quiz-specific material lives under `quiz/`.

## Folder Layout

- `common/AGENTS.md`: reusable Codex rules for all projects.
- `common/agent/shared_command.json`: single-file handoff for trusted local sub-agents.
- `common/agent/token_budget.json`: broad-scan and artifact-read policy.
- `common/mcp/`: common MCP-related local material.
- `quiz/`: quiz-specific Codex context mirrored from `../apps/quiz`.
- `quiz/big_plan.md`: quiz project big-plan source.
- `quiz/일자별요구사항/`: dated requirement snapshots and per-item implementation files.
- `quiz/일자별요구사항/`: dated requirements, implementation notes, archived plans, generated bundles, and evidence files.

## Intended workflow

1. Read `../AGENTS.md` and `common/AGENTS.md`.
2. For reusable tooling/context, work in `common/`.
3. For quiz-specific context, work in `quiz/`.
4. For quiz source changes, read `quiz/AGENTS.md`, then `quiz/app-context/AGENTS.md`.
5. When multiple local agents are needed, write the shared task once in `common/agent/shared_command.json` and point sub-agents there first.
