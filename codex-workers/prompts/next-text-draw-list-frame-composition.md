# Text Worker: Draw-List Handoff To Text Frame Composition

Continue in this same long-lived text worker session, but start a fresh branch from the latest pushed baseline:

```bash
git fetch origin
git switch -C codex/text-draw-list-frame-composition-20260517 origin/codex/quiz-vulkan-remake-baseline
```

## Goal

The previous text task added a draw-list text frame handoff. Now compose that handoff into the existing text request batch/frame path so a test can prove:

`render_draw_list text command -> text draw-list handoff -> text request batch item -> text frame/layout/materialization-ready evidence`

Stay inside the text engine boundary. This is still data/evidence composition, not Vulkan rendering and not app/UI ownership.

## Scope

Allowed:

- `apps/quiz/quiz-vulkan/src/render/text/*`
- `apps/quiz/quiz-vulkan/tests/render/text/*`

Do not edit app, UI, layout, scene, Vulkan, top-level CMake, or aggregate contract registration except the existing text interface contract test.

## Implementation Guidance

- If the prior handoff already added `make_render_text_request_batch_item(...)` for a handoff entry, build the next small composition helper around that instead of duplicating it.
- Preserve command index, node id, parent id, bounds/content bounds, text runs, style evidence, and blocker status through the composed output.
- Blocked handoff entries must stay blocked and must not be silently shaped/layouted as ready text.
- Do not shape/rasterize with real backends unless existing fake/text helpers already do that behind current interfaces.
- If an equivalent composition helper already exists, tighten tests and report the existing API.

## Tests

Add or extend focused text tests proving:

- ready draw-list text commands can become request batch items with identity/bounds/style/options preserved,
- blocked handoff entries remain blocked with diagnostics,
- multiple text commands preserve command order,
- non-text draw commands never enter the composed text frame path,
- interface contract exposes the composition helper/result shape if new.

Verify:

- `quiz_vulkan_interface_contract_compile_tests`
- focused text test target(s)
- `git diff --check`

Commit only scoped text changes and report commit hash, changed files, verification, risks, and external deps used.
