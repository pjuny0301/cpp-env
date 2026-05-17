# Text Worker: Draw-List Text Frame Handoff

Continue in this same long-lived text worker session, but start a fresh branch from the latest pushed baseline:

```bash
git fetch origin
git switch -C codex/text-draw-list-frame-handoff-20260517 origin/codex/quiz-vulkan-remake-baseline
```

## Goal

Add a text-owned, data-only handoff that preserves UI `render_draw_list` text command identity before text shaping/layout/atlas work consumes it.

The current text pipeline has strong frame/draw-packet/resource-packet evidence, but the renderer boundary still needs a compact bridge from `render_draw_list` text commands to text-frame requests without moving app/UI/domain ownership into the text engine.

## Scope

Allowed:

- `apps/quiz/quiz-vulkan/src/render/text/*`
- `apps/quiz/quiz-vulkan/tests/render/text/*`

Do not edit:

- `src/app/*`, `app.cpp`, `main.cpp`
- `src/core/ui/*`, `src/core/layout/*`, `src/core/scene/*`
- `src/render/vulkan/*`
- top-level `CMakeLists.txt`
- aggregate contract registration unless you only touch the existing text interface contract test file.

## Implementation Shape

Create or extend a text-owned header so it can consume `render::render_draw_list` and produce a compact text-frame handoff snapshot.

The handoff should preserve at least:

- frame id or stable frame label,
- draw command index,
- `node_id` and `parent_node_id`,
- command `bounds` and `content_bounds`,
- `text_runs`,
- resolved or referenced text style id/fallback style evidence,
- `render_text_options`,
- blocker evidence for non-text commands, empty text, missing style, invalid bounds, duplicate stable ids if applicable.

This must remain data-only. It should not shape, rasterize, allocate atlas pages, call Vulkan, or inspect quiz/app/domain state.

If an equivalent handoff already exists, do not duplicate it. Tighten its contract/tests to preserve the fields above and report the existing API name.

## Tests

Add focused tests proving:

- only `render_draw_command_type::text` commands become text handoff entries,
- node id, parent id, command index, bounds, content bounds, runs, style token, and text options are preserved,
- duplicate or missing stable identities are diagnosed deterministically,
- empty text or missing style fallback is reported without crashing,
- the type is covered by `text_engine_interface_contract_tests.cpp`,
- no renderer/Vulkan/app/domain dependency leaks into `src/render/text`.

Build and run:

- `quiz_vulkan_interface_contract_compile_tests`
- the focused text test target you touched or added
- `git diff --check`

Commit only the scoped text changes and report commit hash, changed files, verification, risks, and whether external dependencies were used.
