# Vulkan Worker: Descriptor Payload Command Recording Evidence

Continue in the same long-lived Vulkan worker session, but start a fresh branch
from the latest pushed baseline before editing:

```bash
git fetch origin
git switch -C codex/vulkan-descriptor-payload-command-recording-20260517 origin/codex/quiz-vulkan-remake-baseline
```

## Goal

Build the next Vulkan-owned handoff after descriptor write payload evidence:
native command-recording readiness that consumes descriptor write payload
handoff results.

Use existing command packet bridge, descriptor set allocation, descriptor write
payload handoff, and command recorder operation data. Produce data-only evidence
that proves command recording can see:

- descriptor set handles,
- descriptor write payload readiness,
- image view/sampler/layout payload availability for image descriptors,
- per-packet bind/draw readiness,
- deterministic blockers when descriptor writes are missing, duplicated,
  incomplete, or mismatched.

Stay Vulkan-owned. Do not include scene/UI/app/domain/input/audio. Do not call
real Vulkan functions unless the existing fake/native dispatch boundary already
allows it; this task may remain evidence-only.

## Tests

Focused Vulkan tests should prove:

- ready descriptor write payload handoff makes command packet recording evidence
  ready for packets that need image descriptors,
- missing or blocked descriptor payloads block command recording,
- duplicate packet/set/binding payloads are diagnosed,
- descriptor write payloads are merged into executor evidence without
  fabricating handles,
- interface contract covers any new public structs/helpers.

Verify:

- `quiz_vulkan_interface_contract_compile_tests`
- `quiz_vulkan_vulkan_command_packet_execution_tests`
- `quiz_vulkan_renderer_tests` or a more focused Vulkan target if you add one
- `quiz_vulkan_architecture_boundary_tests`
- `git diff --check`

Commit only scoped Vulkan files/tests and report hash, changed files,
verification, risks, and external deps used.
