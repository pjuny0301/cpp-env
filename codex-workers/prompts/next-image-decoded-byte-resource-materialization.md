# Image Worker: Decoded Byte Resource Materialization

Continue in the same long-lived Image/Texture worker session. Keep the session
alive after finishing.

Refresh safely:

```bash
git fetch origin
git switch -C codex/image-decoded-byte-resource-materialization-20260518 origin/codex/quiz-vulkan-remake-baseline
```

## Scope

Edit only:

- `apps/quiz/quiz-vulkan/src/render/image/*`
- `apps/quiz/quiz-vulkan/tests/render/image/*`

Do not edit app/main/top-level CMake, scene, UI, domain, input, text, Vulkan,
asset, or audio files. If a CMake/FILE_SET registration is needed, stop and
report the exact integrator-owned change instead of editing it.

## Goal

Move from upload/cache diagnostics toward a real image data path by threading
decoded image bytes through the existing image-owned contracts:

URI/source bytes -> standard decoder chain -> decoded pixel payload -> texture
upload payload/layout -> frame resource packet materialization/consumption.

Keep this data-only and image-owned. Do not implement Vulkan upload here. Prove
that renderer/Vulkan can later consume compact resource packet facts without
rereading decoder/cache/upload internals.

Cover:

- successful decoded image bytes becoming upload payload bytes;
- stable texture cache key and sampler policy surviving materialization;
- placeholder/fallback path when decode fails;
- blocker reasons for missing source bytes, unsupported format, invalid
  dimensions, pixel byte-count mismatch, and staging/layout mismatch;
- no duplicate external library implementation when an approved decoder/helper
  already exists under `build/external`.

Use existing fixtures and external dependencies when possible. If a new useful
fixture/library is needed, place it under `build/external` or the worker
worktree equivalent and report URL, version/commit, license, path, and reason.

## Verification

Build:

- `quiz_vulkan_interface_contract_compile_tests`
- focused image decoder/upload/materialization tests you add or change

Run the focused image CTest filters and `git diff --check`.

Commit only scoped files and report commit hash, changed files, verification,
external dependencies used, and risks.
