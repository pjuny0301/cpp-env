# Image Worker: STB JPEG Standard Pipeline Path

Continue in the same long-lived Image/Texture worker session. Keep the session
alive after finishing.

Refresh safely:

```bash
git fetch origin
git switch -C codex/image-stb-jpeg-standard-pipeline-20260519 origin/codex/quiz-vulkan-remake-baseline
```

## Scope

Edit only:

- `apps/quiz/quiz-vulkan/src/render/image/*`
- `apps/quiz/quiz-vulkan/tests/render/image/*`

Do not edit app/main/top-level CMake, scene, UI, domain, input, text, Vulkan,
asset, or audio files. If CMake/FILE_SET registration is needed, report the
exact integrator-owned follow-up instead of editing outside scope.

## Goal

Use the already-installed `stb_image` external dependency instead of inventing
or extending a custom JPEG decoder.

Continue after decoded draw payload evidence by proving the standard image
pipeline can drive a real STB-backed JPEG memory decode into the same decoded
payload, upload, resource packet, and draw payload evidence path used for PNG,
BMP, and PPM.

Add or extend tests and image-owned code for:

- a tiny fixture JPEG byte array or existing fixture loaded through the current
  source-bytes loader;
- `standard_image_decoder_chain` preferring the STB adapter for JPEG when the
  linked/header-backed STB implementation is available;
- decoded RGBA dimensions/byte counts reaching `standard_image_texture_pipeline`;
- decoded payload hash/byte count preserved into resource packet materialization
  and renderer texture quad draw payload evidence;
- fallback diagnostics when STB is not available, without adding a handwritten
  JPEG decoder.

Keep this image-owned and data/texture-pipeline focused. Do not include Vulkan,
UI, scene, domain, app, text, input, or audio headers.

## File Size Guidance

If you need to add a large fixture or helper, keep it in a focused test helper
or existing image test fixture area rather than growing one already-large file.

## Verification

Build:

- `quiz_vulkan_interface_contract_compile_tests`
- `quiz_vulkan_standard_image_decoder_chain_tests`
- `quiz_vulkan_standard_image_texture_pipeline_tests`
- focused image materialization/packet tests you change

Run focused image CTest filters and `git diff --check`.

Commit only scoped files and report commit hash, changed files, verification,
external dependencies used, and risks.
