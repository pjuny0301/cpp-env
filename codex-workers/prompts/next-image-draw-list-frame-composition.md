# Image Worker: Draw-List Handoff To Texture Frame Composition

Continue in this same long-lived image worker session, but start a fresh branch from the latest pushed baseline:

```bash
git fetch origin
git switch -C codex/image-draw-list-frame-composition-20260517 origin/codex/quiz-vulkan-remake-baseline
```

## Goal

The previous image task added a draw-list image frame handoff. Now compose that handoff into the existing texture request/batch/frame path so a test can prove:

`render_draw_list image command -> image draw-list handoff -> texture batch plan/request -> texture frame snapshot/resource packet evidence`

Stay inside the image engine boundary. This is still data/evidence composition, not Vulkan upload and not app/UI ownership.

## Scope

Allowed:

- `apps/quiz/quiz-vulkan/src/render/image/*`
- `apps/quiz/quiz-vulkan/tests/render/image/*`

Do not edit app, UI, layout, scene, Vulkan, top-level CMake, or aggregate contract registration except the existing image interface contract test.

## Implementation Guidance

- Reuse the new draw-list handoff entries and existing texture batch/frame helpers.
- Preserve command index, node id, parent id, bounds/content bounds, image ref fields, sampler diagnostics, request/cache identity, and blocker status through the composed output.
- Blocked handoff entries must remain blocked and must not silently enter ready texture planning.
- Do not decode images, allocate textures, upload bytes, or call Vulkan.
- If an equivalent composition helper already exists, tighten tests and report the existing API.

## Tests

Add or extend focused image tests proving:

- ready draw-list image commands can become texture batch requests with identity/bounds/image/sampler fields preserved,
- blocked handoff entries remain blocked with diagnostics,
- multiple image commands preserve command order,
- non-image draw commands never enter the composed texture frame path,
- interface contract exposes the composition helper/result shape if new.

Verify:

- `quiz_vulkan_interface_contract_compile_tests`
- focused image test target(s)
- `git diff --check`

Commit only scoped image changes and report commit hash, changed files, verification, risks, and external deps used.
