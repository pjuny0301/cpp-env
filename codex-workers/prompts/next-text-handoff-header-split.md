# Text Worker: Focus Text Handoff Headers

Continue in the same long-lived Text Engine worker session. Keep the session
alive after finishing.

Refresh safely:

```bash
git fetch origin
git switch -C codex/text-handoff-header-split-20260519 origin/codex/quiz-vulkan-remake-baseline
```

## Scope

Edit only:

- `apps/quiz/quiz-vulkan/src/render/text/*`
- `apps/quiz/quiz-vulkan/tests/render/text/*`

Do not edit app/main/top-level CMake, scene, UI, domain, input, image, Vulkan,
asset, or audio files. If CMake/FILE_SET registration is needed, report the
exact integrator-owned follow-up instead of editing outside scope.

## Goal

Reduce token/read cost in the Text Engine without changing public behavior.

`text_frame_upload_handoff.h` remains the compatibility include, but it is still
too broad. Split the most obvious self-contained sections into focused text
headers while preserving existing include compatibility:

- upload handoff core types/helpers;
- resource packet materialization types/helpers;
- renderer glyph quad packet types/helpers;
- resource packet consumption diff helpers;
- upload handoff diff helpers.

Do not move or change `text_engine.h`. Do not change public signatures except
for include relocation required by the split. Keep tests behaviorally identical,
adding compile/contract coverage only where it proves the focused headers work.

Prefer small focused headers and a wrapper compatibility include over one large
header. If a section is too entangled to split safely in this pass, leave it and
report the exact reason instead of forcing a risky rewrite.

## Verification

Build:

- `quiz_vulkan_interface_contract_compile_tests`
- `quiz_vulkan_text_renderer_glyph_quad_packet_tests`
- `quiz_vulkan_text_frame_resource_packet_materialization_tests`
- `quiz_vulkan_fake_text_engine_freetype_raster_payload_tests`
- any focused text diff/handoff tests affected

Run focused text CTest filters and `git diff --check`.

Commit only scoped files and report commit hash, changed files, verification,
external dependencies used, and risks.
