# Asset Worker: Render Resource Addressing Contract

Continue in the same long-lived Asset System worker session. Keep the session
alive after finishing.

Refresh safely:

```bash
git fetch origin
git switch -C codex/asset-render-resource-addressing-20260519 origin/codex/quiz-vulkan-remake-baseline
```

## Scope

Edit only:

- `apps/quiz/quiz-vulkan/src/assets/*`
- `apps/quiz/quiz-vulkan/tests/assets/*`

Do not edit app/main/top-level CMake, render, Vulkan, image, text, input,
domain, scene, UI, or audio files. If a renderer/app/CMake follow-up is needed,
report it precisely instead of editing outside scope.

## Goal

Add the next small asset-owned contract needed by render resources without
building a giant asset system.

Focus on stable resource addressing for fonts/images/shaders/decks:

- canonical `asset://` identity with type/kind evidence;
- local fixture/file identity without leaking absolute build paths into cache
  keys;
- cache-key components for `font`, `image`, `shader`, `sound`, and `deck`
  resources;
- path traversal rejection and external/build-output boundary diagnostics;
- compact manifest/address summary that image/text/Vulkan workers can consume
  later without reading app or renderer code.

Keep this data-only and asset-owned. Do not include render/Vulkan/app/domain
headers.

## File Size Guidance

If an existing asset header is too broad, split only the section you touch into a
focused header while preserving compatibility includes.

## Verification

Build:

- `quiz_vulkan_interface_contract_compile_tests`
- `quiz_vulkan_asset_bytes_provider_tests`
- `quiz_vulkan_asset_manifest_tests`
- `quiz_vulkan_asset_path_policy_tests`
- any focused asset resolver/catalog tests you change

Run focused asset CTest filters and `git diff --check`.

Commit only scoped files and report commit hash, changed files, verification,
external dependencies used, and risks.
