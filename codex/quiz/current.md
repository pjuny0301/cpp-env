# Quiz Current Handoff

Last updated: 2026-05-20

Read this first before `big_plan.md`, `requirements_traceability_matrix.md`,
or per-requirement implementation notes. Keep this file short; old handoff
details belong in git history.

## Current Focus

- Build the native C++23 quiz remake toward the engine stack:
  Vulkan backend, text layout/atlas, image/texture, asset materialized bytes,
  input/IME, then audio later.
- Audio is deferred until render/resource paths are less fragile.
- Existing Android/editor code is reference material; convergence target is
  `apps/quiz/quiz-vulkan`.
- Keep `apps/quiz` as code only. Build outputs stay under `build/out`;
  external dependency snapshots stay under `build/external`.

## Integrated Baseline

- Main branch `codex/quiz-vulkan-remake-baseline` includes engine work through
  `60dd909`; this handoff note may be one documentation commit newer.
- Integrated since the previous remote baseline:
  - default app rendering now uses the standard image texture pipeline instead
    of the fake image pipeline;
  - app image payload extraction now works for the standard image pipeline, not
    only the fake pipeline;
  - app image payload extraction now consumes the image-owned upload/cache
    diagnostics side interface instead of concrete fake/standard pipeline casts;
  - asset render resource materialized byte cache summaries preserve stable
    identities for renderer-facing payloads;
  - asset render resource materialized cache diffs track reuse, replacement,
    content-hash invalidation, revision invalidation, and host-path movement;
  - standard image decode/upload cache behavior is covered through repeated URI
    reuse and malformed image placeholder paths;
  - image pipelines expose a common upload/cache diagnostics side interface for
    fake and standard pipelines;
  - input/IME fixture coverage now includes IME preedit commit/cancel, Hangul
    backspace, wheel routing, and pointer capture arbitration;
  - input routing diagnostics now carry text focus/caret route state for app
    routers to consume later without domain dispatch from input;
  - Vulkan command packet execution records native vertex buffer bind evidence;
  - Vulkan native command packet execution records image payload descriptor bind
    evidence, including missing/stale/invalid payload blocker paths;
  - Vulkan native descriptor write/bind call evidence now consumes image payload
    descriptor bindings and records missing symbol/invalid handle/blocker paths;
  - Vulkan descriptor update command path evidence now consumes descriptor
    write/bind calls and records real-dispatch blockers explicitly;
  - FreeType-backed text atlas raster payloads now expose cache-hit reuse
    evidence behind the text engine contract.
  - HarfBuzz-shaped glyph ids now drive FreeType atlas residency fixture
    coverage and repeated glyph atlas reuse evidence.
  - text renderer glyph quad packet tests now prove real atlas upload
    consumption and clean atlas reuse evidence.
  - text renderer draw payload records and draw payload frame diffs now track
    upload/reuse, atlas page/revision, UV/bounds, blocker, and readiness
    transitions.
  - image pipelines now summarize upload/cache payload readiness, placeholder
    state, blocker state, decoded bytes, staging bytes, and uploaded bytes.
  - image texture draw payloads now carry upload/cache evidence for ready,
    placeholder, and blocked payload paths.
  - asset shader payload runtime summaries classify stage, revision, runtime
    identity, ready entries, and blocked entries.
  - asset shader runtime payload diffs and generic runtime payload manifests now
    track stable identities, revisions, content hashes, ready/blocked state, and
    host-path-free cache handoff data.
  - input routing diagnostics diffs now replay text focus/caret/selection/IME
    route-state changes.
  - input routing diagnostics now also summarize gesture/pointer route replay
    for capture, tap, long press, swipe, wheel, touch arbitration, and
    cancellation.
- Full Windows MinGW CTest passed 108/108 in
  `C:/aa/build/out/quiz/quiz-vulkan/windows-mingw-ascii`.

## Active Bottlenecks

- Vulkan: descriptor payload bind, native write/bind calls, and descriptor
  update command path evidence are tracked; keep moving toward real descriptor
  allocation/update calls and command-buffer execution while preserving
  renderer-only inputs.
- Text: HarfBuzz glyph ids, FreeType atlas residency, renderer glyph quads,
  draw payloads, and payload diffs are tracked; keep moving toward actual text
  draw-list/runtime consumption.
- Image: fake and standard pipelines expose common upload/cache diagnostics,
  payload summaries, and texture draw payload evidence; next work should connect
  those payloads to renderer resource packets without app/concrete pipeline
  casts.
- Asset: shader runtime summaries, shader payload diffs, generic runtime payload
  manifests, and materialized cache invalidation evidence exist; keep runtime
  cache keys stable and host-path-free.
- Input/IME: input owns text and gesture route replay diagnostics; only resume
  when app-owned routing needs a new normalized input contract.

## Active Workers

- `codex-vulkan-native-command-packet-executor-20260516`: latest commit
  `41768b7` integrated as `fb1fb4a`; session remains alive.
- `codex-text-freetype-prototype-20260514`: latest commit `ddb7e86`
  integrated as `84447d9`; session remains alive.
- `codex-asset-unified-cache-key-20260514`: latest commit `6fe993d`
  integrated as `1856e03`; session remains alive.
- `codex-image-texture-next-20260514`: latest commit `7f7c300`
  integrated as `fc4feaf`; session remains alive.
- `codex-input-ime`: latest commit `e5d9055` integrated as `60dd909`;
  session remains alive.
- Idle sessions are intentionally kept alive. Give them fresh baseline branches
  before new work; do not re-merge historical ahead commits.

## Architecture Contract

- Required direction:
  `main -> modifier_interface -> scene_layout_data_modifier -> scene_layout_edit_data -> scene_layout_patch -> scene_layout_data -> layout_placer -> ui_renderer -> vulkan_renderer`.
- Modifiers write only through `scene_layout_edit_data` / patch flow.
- `layout_placer` reads scene/layout data and must not mutate scene data or call
  UI/render/Vulkan.
- `ui_renderer` consumes placed UI data and must not call Vulkan or domain/app.
  It also must not include or call `layout_placer`; it receives placed scene
  data through the scene-owned `placed_scene` contract.
- Vulkan consumes renderer-owned command/resource data and must not include
  scene, UI, app, domain, input, or audio.
- App/domain presentation coupling is allowed only in app-owned bridge files
  such as `src/app/app_quiz_screens.h`.
- Engine workers own engine folders and focused tests. App wiring, top-level
  CMake integration, and cross-engine contracts stay with the integrator unless
  explicitly delegated.

## Verification Policy

- Worker handoff: focused engine tests plus
  `quiz_vulkan_interface_contract_compile_tests` and `git diff --check`.
- Integrator after cherry-pick: Windows MinGW focused tests for touched areas.
- Full Windows CTest: run after meaningful batches, not after every small patch.
- Do not hard-code global CTest counts in docs; derive them from `ctest -N`.
