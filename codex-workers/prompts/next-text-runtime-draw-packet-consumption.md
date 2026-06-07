You are the long-lived Text Engine worker. Keep this tmux session alive after finishing.

Refresh safely:
- `git fetch origin`
- Start a fresh branch `codex/text-runtime-draw-packet-consumption-20260531`
  from `origin/codex/quiz-vulkan-remake-baseline`.
- Do not stack this on old ahead worker branches.

Read:
- `/mnt/c/aa/AGENTS.md`
- `apps/quiz/quiz-vulkan/src/render/text/AGENTS.md`
- `apps/quiz/quiz-vulkan/src/render/text/text_engine.h`
- the existing text draw payload / frame handoff headers and focused tests.

Scope only:
- `apps/quiz/quiz-vulkan/src/render/text/*`
- `apps/quiz/quiz-vulkan/tests/render/text/*`

Do not edit app, UI, layout, Vulkan, image, asset, input, audio, top-level CMake,
or aggregate contract registration.

Task:
- Move text one step closer to real runtime drawing behind the existing text
  interfaces.
- Current baseline can produce glyph quads, atlas upload evidence, draw payload
  records, and frame diffs. Add the next text-owned data handoff that turns
  ready text frame draw payloads into stable renderer-facing draw packet
  descriptors.
- The handoff must preserve:
  - source draw payload id;
  - glyph run / glyph count;
  - atlas page id and revision;
  - UV/bounds payload needed by a renderer;
  - readiness/blocker reason when a packet cannot be emitted;
  - stable identity for diff/reuse diagnostics.
- Keep it data-only. Do not call Vulkan, app, domain, UI, layout, image, or
  asset code from text.
- If the existing public text interface is insufficient, stop and report the
  minimal interface proposal instead of moving/renaming/changing it.

Implementation style:
- Prefer C++23 designated initialization.
- Split helpers only when it improves ownership/reviewability; do not split just
  by line count.
- Use approved external dependencies already under `/mnt/c/aa/build/external`
  when needed. Do not duplicate FreeType/HarfBuzz/utf8proc.

Verification:
- Build `quiz_vulkan_interface_contract_compile_tests`.
- Run focused text tests covering your changes, including relevant draw payload
  or frame handoff tests.
- Run `git diff --check`.

Commit scoped files and report changed files, verification, risks, and commit
hash.
