# Quiz Current Handoff

Last updated: 2026-05-17

This file is intentionally short. Use it as the first read before `big_plan.md`,
`requirements_traceability_matrix.md`, or per-requirement implementation notes.
Historical integration notes are kept in git history, not repeated here.

## Current Focus

- Build the native C++23 quiz remake toward the required engine stack:
  Vulkan backend, image/texture path, text layout/atlas path, asset materialized
  bytes, input/IME, then audio later.
- Audio is deferred until render/resource paths are less fragile.
- Existing Android/editor code is reference material, but the convergence target
  is `apps/quiz/quiz-vulkan`.
- Keep `apps/quiz` as code only. Build outputs stay under `build/out`; external
  dependency snapshots stay under `build/external`.

## Active Bottlenecks

- Vulkan: descriptor write payload handoff is now integrated; the next Vulkan
  gap is consuming those payloads in the native command recording path without
  pulling scene/UI/domain state into the backend.
- Text: glyph quad packet evidence is now integrated; the next text gap is real
  renderer/font atlas consumption after the image/Vulkan resource contracts
  settle.
- Image: draw-list texture frame composition is now integrated; the next image
  gap is renderer-bound texture quad/resource consumption after Vulkan
  descriptor evidence settles.

## Active Workers

- `codex-image-texture-next-20260514`: busy on
  `codex/image-draw-list-frame-composition-20260517`, integrated as `3677875`.
  Keep the session alive and assign a fresh baseline branch for the next image
  task.
- `codex-text-freetype-prototype-20260514`: busy on
  `codex/text-renderer-glyph-quad-packets-20260517`, integrated as `7889fa1`.
  Keep the session alive and assign a fresh baseline branch for the next text
  task.
- `codex-vulkan-native-descriptor-set-evidence-20260516`: busy on
  `codex/vulkan-descriptor-write-payload-handoff-20260517`, integrated as
  `d6aa9cd`. Keep the session alive and assign a fresh baseline branch for the
  next Vulkan task.
- Idle sessions are intentionally kept alive. Give them fresh baseline branches
  before new work; do not re-merge historical ahead commits.

## Architecture Contract

- Required direction:
  `main -> modifier_interface -> scene_layout_data_modifier -> scene_layout_edit_data -> scene_layout_patch -> scene_layout_data -> layout_placer -> ui_renderer -> vulkan_renderer`.
- Modifiers write only through `scene_layout_edit_data` / patch flow.
- `layout_placer` reads scene/layout data and must not mutate scene data or call
  UI/render/Vulkan.
- `ui_renderer` consumes placed UI data and must not call Vulkan or domain/app.
- Vulkan consumes renderer-owned command/resource data and must not include
  scene, UI, app, domain, input, or audio.
- App/domain presentation coupling is allowed only in app-owned bridge files
  such as `src/app/app_quiz_screens.h`.
- Engine workers own engine folders and focused tests. App wiring, top-level
  CMake integration, and cross-engine contracts stay with the integrator unless
  explicitly delegated.

## Verification Policy

- Worker handoff: focused engine tests plus `quiz_vulkan_interface_contract_compile_tests`
  and `git diff --check`.
- Integrator after cherry-pick: Windows MinGW focused tests for touched areas.
- Full Windows CTest: run after meaningful batches, not after every small patch.
- Do not hard-code global CTest counts in docs; derive them from `ctest -N`.

## Latest Known Verification

- Main branch `codex/quiz-vulkan-remake-baseline` has integrated Vulkan
  descriptor write payload handoff at `d6aa9cd`, image draw-list texture frame
  composition at `3677875`, and text renderer glyph quad packet evidence at
  `7889fa1`.
- Last full Windows MinGW CTest batch passed `105/105` from
  `C:/aa/build/out/quiz/quiz-vulkan/windows-mingw-ascii`.
- After `d6aa9cd`, Windows MinGW focused architecture/renderer/Vulkan CTest
  passed 3/3 for descriptor write payload handoff.

## Useful Commands

```bash
git status --short --branch
./codex-workers/worker-status.sh
tmux capture-pane -pt codex-image-texture-next-20260514 -S -120
tmux capture-pane -pt codex-text-freetype-prototype-20260514 -S -120
tmux capture-pane -pt codex-vulkan-native-descriptor-set-evidence-20260516 -S -120
```
