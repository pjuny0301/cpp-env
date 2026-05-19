# Vulkan Worker: Native Descriptor Bind Calls

Continue in the same long-lived Vulkan worker session. Keep the session alive
after finishing.

Refresh safely:

```bash
git fetch origin
git switch -C codex/vulkan-native-descriptor-bind-calls-20260518 origin/codex/quiz-vulkan-remake-baseline
```

## Scope

Edit only:

- `apps/quiz/quiz-vulkan/src/render/vulkan/*`
- `apps/quiz/quiz-vulkan/tests/render/vulkan/*`

Do not edit app/main/top-level CMake, scene, UI, domain, input, text, image,
asset, or audio files. If app/runtime/CMake wiring is needed, report the exact
integrator-owned follow-up instead of editing outside scope.

## Goal

Continue after descriptor payload bind recording by proving the native command
packet executor can translate ready descriptor bind evidence into ordered native
command-call evidence.

Use the existing descriptor payload bind records and native command function
table. Add the smallest Vulkan-owned step that records:

- `vkCmdBindDescriptorSets` readiness/call evidence before image draw packets;
- pipeline layout, descriptor set handle, set index, and payload identity used
  by the bind call;
- bind-before-draw ordering for packets that require image descriptors;
- blockers for missing bind records, incomplete bind records, mismatched payload
  identity, invalid descriptor set handle, invalid pipeline layout, and missing
  `vkCmdBindDescriptorSets`;
- no descriptor bind calls for packets that do not require descriptors.

This may remain fake-dispatch/data-only if real Vulkan command invocation needs
a broader runtime contract. Do not pull scene/UI/app/domain/input/audio state
into Vulkan.

## Verification

Build:

- `quiz_vulkan_interface_contract_compile_tests`
- `quiz_vulkan_vulkan_command_packet_execution_tests`
- `quiz_vulkan_renderer_tests`
- `quiz_vulkan_architecture_boundary_tests`

Run focused Vulkan CTest filters and `git diff --check`.

Commit only scoped files and report commit hash, changed files, verification,
external dependencies used, and risks.
