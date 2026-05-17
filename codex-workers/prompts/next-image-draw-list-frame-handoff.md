# Image Worker: Draw-List Image Frame Handoff

Continue in this same long-lived image worker session, but start a fresh branch from the latest pushed baseline:

```bash
git fetch origin
git switch -C codex/image-draw-list-frame-handoff-20260517 origin/codex/quiz-vulkan-remake-baseline
```

## Goal

Add an image-owned, data-only handoff that preserves UI `render_draw_list` image command identity before image texture planning/cache/upload/materialization consumes it.

The current image pipeline can plan and materialize texture resources from image refs, but the renderer boundary still needs compact evidence connecting `render_draw_list` image commands to frame texture requests without losing node/bounds/command identity.

## Scope

Allowed:

- `apps/quiz/quiz-vulkan/src/render/image/*`
- `apps/quiz/quiz-vulkan/tests/render/image/*`

Do not edit:

- `src/app/*`, `app.cpp`, `main.cpp`
- `src/core/ui/*`, `src/core/layout/*`, `src/core/scene/*`
- `src/render/vulkan/*`
- top-level `CMakeLists.txt`
- aggregate contract registration unless you only touch the existing image interface contract test file.

## Implementation Shape

Create or extend an image-owned header so it can consume `render::render_draw_list` and produce a compact image-frame handoff snapshot.

The handoff should preserve at least:

- frame id or stable frame label,
- draw command index,
- `node_id` and `parent_node_id`,
- command `bounds` and `content_bounds`,
- `render_image_ref` including URI, alt text, aspect ratio, and sampler policy,
- normalized request identity/cache key if existing image helpers provide it,
- blocker evidence for non-image commands, empty/invalid URI, invalid bounds, invalid sampler, duplicate stable ids if applicable.

This must remain data-only. It should not decode images, allocate textures, upload bytes, call Vulkan, or inspect quiz/app/domain state.

If an equivalent handoff already exists, do not duplicate it. Tighten its contract/tests to preserve the fields above and report the existing API name.

## Tests

Add focused tests proving:

- only `render_draw_command_type::image` commands become image handoff entries,
- node id, parent id, command index, bounds, content bounds, URI, alt text, aspect ratio, and sampler policy are preserved,
- invalid URI/sampler/bounds diagnostics are deterministic,
- duplicate or missing stable identities are diagnosed deterministically,
- the type is covered by `image_engine_interface_contract_tests.cpp`,
- no renderer/Vulkan/app/domain dependency leaks into `src/render/image`.

Build and run:

- `quiz_vulkan_interface_contract_compile_tests`
- the focused image test target you touched or added
- `git diff --check`

Commit only the scoped image changes and report commit hash, changed files, verification, risks, and whether external dependencies were used.
