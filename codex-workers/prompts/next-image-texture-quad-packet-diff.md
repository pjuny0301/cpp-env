# Image Worker: Texture Quad Packet Diff Evidence

Continue in the same long-lived image worker session, but start a fresh branch
from the latest pushed baseline before editing:

```bash
git fetch origin
git switch -C codex/image-texture-quad-packet-diff-20260517 origin/codex/quiz-vulkan-remake-baseline
```

## Goal

Build an image-owned frame-to-frame diff over renderer texture quad packet
evidence.

Use the existing renderer texture quad packet summary as input. Produce compact
diff records that preserve:

- added/removed/changed texture quad packet identities,
- bounds/content bounds, texture id/revision/size, sampler/cache, upload, and
  readiness changes,
- duplicate or missing identity changes,
- regression/recovery classification for ready versus blocked texture quads.

Stay image-owned and data-only. Do not call Vulkan, do not edit UI/app/layout/
scene, and do not allocate real GPU resources.

## Tests

Focused image tests should prove:

- stable identical summaries produce a stable/no-op diff,
- ready-to-blocked and blocked-to-ready transitions are classified,
- bounds/texture/sampler/upload changes are counted separately,
- added/removed/duplicate/missing identities are deterministic,
- interface contract covers new public structs/helpers.

Verify:

- `quiz_vulkan_interface_contract_compile_tests`
- focused image test target(s)
- `git diff --check`

Commit only scoped image files and report hash, changed files, verification,
risks, and external deps used.
