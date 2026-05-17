# Image Worker: Renderer Texture Quad Packet Evidence

Continue in the same long-lived image worker session, but start a fresh branch
from the latest pushed baseline before editing:

```bash
git fetch origin
git switch -C codex/image-renderer-texture-quad-packets-20260517 origin/codex/quiz-vulkan-remake-baseline
```

## Goal

Build the next image-owned data handoff after draw-list texture frame
composition: renderer-facing texture quad packet evidence.

Use existing draw-list image handoff, texture batch/frame snapshot, and texture
resource packet plan data. Produce compact texture quad packet records that
preserve:

- source draw command/node identity,
- layout/content bounds,
- image URI/alt/aspect evidence,
- texture id/revision/size,
- sampler/cache/resource packet identity,
- readiness/blocker diagnostics.

Stay image-owned and data-only. Do not call Vulkan, do not edit UI/app/layout/
scene, and do not allocate real GPU resources.

## Tests

Focused image tests should prove:

- ready composed image entries become texture quad packets preserving bounds,
  texture identity, sampler identity, and source command/node evidence,
- blocked/missing batch/frame/resource evidence stays blocked,
- packet order is deterministic,
- duplicate or missing identities are diagnosed,
- interface contract covers new public structs/helpers.

Verify:

- `quiz_vulkan_interface_contract_compile_tests`
- focused image test target(s)
- `git diff --check`

Commit only scoped image files and report hash, changed files, verification,
risks, and external deps used.
