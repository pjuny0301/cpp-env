# Image Worker: Decoded Draw Payload Evidence

Continue in the same long-lived Image/Texture worker session. Keep the session
alive after finishing.

Refresh safely:

```bash
git fetch origin
git switch -C codex/image-decoded-draw-payload-evidence-20260518 origin/codex/quiz-vulkan-remake-baseline
```

## Scope

Edit only:

- `apps/quiz/quiz-vulkan/src/render/image/*`
- `apps/quiz/quiz-vulkan/tests/render/image/*`

Do not edit app/main/top-level CMake, scene, UI, domain, input, text, Vulkan,
asset, or audio files. If a CMake/FILE_SET registration is needed, stop and
report the exact integrator-owned change instead of editing it.

## Goal

Continue after decoded-byte resource materialization by carrying compact decoded
resource facts into the renderer-bound image texture quad draw payload layer.

Use the existing resource packet materialization/consumption summary and
`render_image_renderer_texture_quad_draw_payload` / payload frame helpers. Add
or extend image-owned evidence for:

- decoded payload byte count/hash;
- upload layout and staging byte counts;
- decoded resource readiness and blocker summary;
- stable texture cache key + sampler + upload generation already used by the
  draw payload;
- placeholder/fallback payloads preserving decoded-resource blocker evidence;
- stable frame no-change and compact changed-payload summaries if a diff helper
  already exists for this layer.

Keep this data-only and image-owned. Do not implement Vulkan upload and do not
make image code include Vulkan/app/UI/domain headers.

If existing payload/diff helpers already cover part of this, extend them
narrowly and add tests that drive the real standard decoder path into draw
payload evidence rather than duplicating a parallel pipeline.

## Verification

Build:

- `quiz_vulkan_interface_contract_compile_tests`
- `quiz_vulkan_image_texture_frame_resource_packet_plan_tests`
- focused image materialization/standard pipeline tests you change

Run focused image CTest filters and `git diff --check`.

Commit only scoped files and report commit hash, changed files, verification,
external dependencies used, and risks.
