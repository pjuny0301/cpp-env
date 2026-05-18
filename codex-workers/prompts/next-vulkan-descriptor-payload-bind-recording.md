# Vulkan Worker: Descriptor Payload Bind Recording

Continue in the same long-lived Vulkan worker session. Keep the session alive
after finishing.

Refresh safely:

```bash
git fetch origin
git switch -C codex/vulkan-descriptor-payload-bind-recording-20260518 origin/codex/quiz-vulkan-remake-baseline
```

## Scope

Edit only:

- `apps/quiz/quiz-vulkan/src/render/vulkan/*`
- `apps/quiz/quiz-vulkan/tests/render/vulkan/*`

Do not edit app/main/top-level CMake, scene, UI, domain, input, text, image,
asset, or audio files. If a CMake/FILE_SET registration is needed, stop and
report the exact integrator-owned change instead of editing it.

## Goal

Move one step closer to the real Vulkan path by making command-packet execution
consume descriptor payload command-recording evidence for image packets.

Use the existing command packet bridge, descriptor set allocation, descriptor
write payload handoff, and descriptor payload command-recording result. Add
data-only Vulkan-owned evidence for:

- descriptor set bind readiness before image draw packets;
- pipeline layout + descriptor set handle ownership per packet;
- descriptor payload identities used by each bind;
- deterministic blockers for missing, duplicate, incomplete, or mismatched
  descriptor payload command-recording evidence;
- no fabrication of descriptor set handles when allocation/write evidence is
  incomplete.

Do not include scene/UI/app/domain/input/audio. Do not widen ownership. Real
Vulkan calls are allowed only behind the existing Vulkan native dispatch
boundary; this task may remain evidence/fake-dispatch driven if that is the
smallest correct step.

## Verification

Build:

- `quiz_vulkan_interface_contract_compile_tests`
- `quiz_vulkan_vulkan_command_packet_execution_tests`
- any focused Vulkan target you add/change

Run the focused Vulkan CTest filters and `git diff --check`.

Commit only scoped files and report commit hash, changed files, verification,
external dependencies used, and risks.
