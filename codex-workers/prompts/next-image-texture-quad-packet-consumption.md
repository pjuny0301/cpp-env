# Image Worker: Texture Quad Packet Consumption Path

Continue in the same long-lived image worker session, but start a fresh branch
from the latest pushed baseline before editing:

```bash
git fetch origin
git switch -C codex/image-texture-quad-packet-consumption-20260517 origin/codex/quiz-vulkan-remake-baseline
```

## Goal

Move image rendering one step from evidence toward actual consumption.

Use the existing draw-list image handoff, texture frame resource packet plan,
materialization, and renderer texture quad packet summary. Add the next
image-owned consumption layer that turns ready renderer texture quad packets
into a compact renderer-facing texture draw payload that can later be handed to
Vulkan without making image code depend on Vulkan.

This should be implementation-oriented, not another broad diff report. Preserve
only the diagnostics needed to explain why a texture quad payload is ready,
placeholder-backed, or blocked.

Stay inside:

- `apps/quiz/quiz-vulkan/src/render/image/*`
- `apps/quiz/quiz-vulkan/tests/render/image/*`

Do not edit UI, scene, layout, app, Vulkan, top-level `CMakeLists.txt`, or the
aggregate contract registration. If a new public header needs FILE_SET/CMake
wiring, implement the engine-local side and report the exact integrator-owned
change instead of editing CMake.

## Requirements

- Consume `render_image_renderer_texture_quad_packet_summary`.
- Produce stable per-quad payload records with bounds, content bounds, texture
  id/revision/size, sampler key, cache key, source command/node identity, and
  placeholder/blocker state.
- Ready packets should produce draw-ready payloads.
- Blocked packets should produce deterministic placeholder payloads only when a
  placeholder policy says that is allowed; otherwise they must stay blocked.
- Do not allocate GPU resources and do not include Vulkan headers.
- Prefer existing approved decoders/caches under `build/external` if you need
  image bytes. Do not duplicate `stb_image` or custom-decode formats already
  supported.

## Tests

Focused image tests should prove:

- ready texture quad packets produce draw-ready payloads,
- blocked packets can produce placeholder payloads under policy,
- blocked packets stay blocked when placeholder policy disallows fallback,
- payload identity is stable and deterministic,
- no Vulkan/UI/app dependency is introduced.

Verify:

- `quiz_vulkan_interface_contract_compile_tests`
- focused image test target(s)
- `git diff --check`

Commit only scoped image files and report hash, changed files, verification,
risks, and external deps used.
