# Text Worker: Renderer Glyph Quad Packet Evidence

Continue in this same long-lived text worker session, but start a fresh branch from the latest pushed baseline:

```bash
git fetch origin
git switch -C codex/text-renderer-glyph-quad-packets-20260517 origin/codex/quiz-vulkan-remake-baseline
```

## Goal

Build the next text-owned data handoff after draw-list/frame composition: renderer-facing glyph quad packet evidence.

Use existing text frame draw packets/resource packet materialization data. Produce compact glyph quad packet records that preserve:

- source draw command/node identity when available,
- glyph/layout bounds,
- atlas page id/revision,
- UV bounds,
- sampler/page/resource packet identity,
- readiness/blocker diagnostics.

Stay text-owned and data-only. Do not call Vulkan, do not edit UI/app/layout/scene, and do not allocate real GPU resources.

## Tests

Focused text tests should prove:

- ready materialized text packets become glyph quad packets preserving layout bounds and UVs,
- blocked/missing packet evidence stays blocked,
- packet order is deterministic,
- duplicate or missing identities are diagnosed,
- interface contract covers any new public structs/helpers.

Verify:

- `quiz_vulkan_interface_contract_compile_tests`
- focused text test target(s)
- `git diff --check`

Commit only scoped text files and report hash, changed files, verification, risks, and external deps used.
