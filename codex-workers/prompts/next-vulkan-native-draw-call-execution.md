# Vulkan Worker: Native Draw Call Execution Evidence

Continue in the same long-lived Vulkan worker session. Keep the session alive
after finishing.

Refresh safely:

```bash
git fetch origin
git switch -C codex/vulkan-native-draw-call-execution-20260519 origin/codex/quiz-vulkan-remake-baseline
```

## Scope

Edit only:

- `apps/quiz/quiz-vulkan/src/render/vulkan/*`
- `apps/quiz/quiz-vulkan/tests/render/vulkan/*`

Do not edit app/main/top-level CMake, scene, UI, domain, input, text, image,
asset, or audio files. If app/runtime/CMake wiring is needed, report the exact
integrator-owned follow-up instead of editing outside scope.

## Goal

Continue after native descriptor bind-call evidence by adding the smallest
Vulkan-owned step that proves ready command packets can produce ordered native
draw-call evidence.

Use the existing command packet executor, native command function table, and
descriptor bind-call records. Add or extend evidence for:

- native draw call readiness after required pipeline/descriptor binds;
- ordered bind-before-draw sequencing for image/text-like packets;
- draw packet identity, vertex/index count, first vertex/index, instance count,
  and payload identity used by the native call;
- blockers for missing command buffer, missing draw function pointer, missing
  required bind call, invalid draw counts, and payload identity mismatch;
- no native draw call for packets that are explicitly blocked or placeholder-only.

This may remain fake-dispatch/data-only if real `vkCmdDraw*` invocation needs a
broader runtime contract, but prefer using the same native function table style
already used for descriptor bind evidence. Do not pull scene/UI/app/domain/input
or audio state into Vulkan.

## Verification

Build:

- `quiz_vulkan_interface_contract_compile_tests`
- `quiz_vulkan_vulkan_command_packet_execution_tests`
- `quiz_vulkan_renderer_tests`
- `quiz_vulkan_architecture_boundary_tests`

Run focused Vulkan CTest filters and `git diff --check`.

Commit only scoped files and report commit hash, changed files, verification,
external dependencies used, and risks.
