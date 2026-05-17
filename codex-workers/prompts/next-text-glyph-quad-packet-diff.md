# Text Worker: Glyph Quad Packet Diff Evidence

Continue in the same long-lived text worker session, but start a fresh branch
from the latest pushed baseline before editing:

```bash
git fetch origin
git switch -C codex/text-glyph-quad-packet-diff-20260517 origin/codex/quiz-vulkan-remake-baseline
```

## Goal

Build a text-owned frame-to-frame diff over renderer glyph quad packet evidence.

Use the existing renderer glyph quad packet snapshot as input. Produce compact
diff records that preserve:

- added/removed/changed quad packet ids,
- layout bounds, atlas bounds, UV, page revision, sampler, upload, and
  readiness changes,
- duplicate or missing identity changes,
- regression/recovery classification for ready versus blocked quads.

Stay text-owned and data-only. Do not call Vulkan, do not edit UI/app/layout/
scene, and do not allocate real GPU resources.

## Tests

Focused text tests should prove:

- stable identical snapshots produce a stable/no-op diff,
- ready-to-blocked and blocked-to-ready transitions are classified,
- layout/UV/page/sampler/upload changes are counted separately,
- added/removed/duplicate/missing identities are deterministic,
- interface contract covers new public structs/helpers.

Verify:

- `quiz_vulkan_interface_contract_compile_tests`
- focused text test target(s)
- `git diff --check`

Commit only scoped text files and report hash, changed files, verification,
risks, and external deps used.
