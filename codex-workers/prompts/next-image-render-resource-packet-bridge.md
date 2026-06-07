You are the long-lived Image/Texture Engine worker. Keep this tmux session alive after finishing.

Refresh safely:
- `git fetch origin`
- Start a fresh branch `codex/image-render-resource-packet-bridge-20260531`
  from `origin/codex/quiz-vulkan-remake-baseline`.
- Do not stack this on old ahead worker branches.

Read:
- `/mnt/c/aa/AGENTS.md`
- `apps/quiz/quiz-vulkan/src/render/image/AGENTS.md`
- existing image texture frame resource packet, upload/cache payload, and draw
  payload tests.

Scope only:
- `apps/quiz/quiz-vulkan/src/render/image/*`
- `apps/quiz/quiz-vulkan/tests/render/image/*`

Do not edit app, UI, layout, Vulkan, text, asset, input, audio, top-level CMake,
or aggregate contract registration.

Task:
- Move image/texture one step closer to renderer consumption without app-side
  concrete pipeline casts.
- Current baseline exposes common upload/cache diagnostics and texture draw
  payload evidence for ready, placeholder, and blocked paths. Add a narrow
  image-owned bridge that converts those draw payloads into stable renderer
  resource packet descriptors.
- The bridge should preserve:
  - image URI / render image reference identity;
  - texture cache/upload identity;
  - sampler policy;
  - decoded/staging/upload byte evidence where available;
  - placeholder vs real texture state;
  - blocker reason when the packet is not renderer-ready;
  - stable packet identity for diff/reuse diagnostics.
- Keep it data-only and renderer/domain/app independent. Do not call Vulkan.
- If a top-level app/CMake/runtime wire-up is needed, report the exact proposal
  and keep the engine-local side scoped.

Implementation style:
- Prefer C++23 designated initialization.
- Consume approved external dependencies already under `/mnt/c/aa/build/external`
  before adding anything. Do not duplicate image libraries.

Verification:
- Build `quiz_vulkan_interface_contract_compile_tests`.
- Run focused image tests covering resource packet/materialization/pipeline
  behavior.
- Run `git diff --check`.

Commit scoped files and report changed files, verification, risks, and commit
hash.
