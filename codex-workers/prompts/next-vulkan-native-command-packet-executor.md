Long-lived Vulkan Backend worker. Keep this session alive after finishing.

Refresh safely:
- `git fetch origin`
- Start a fresh branch `codex/vulkan-native-command-packet-executor-20260516`
  from `origin/codex/quiz-vulkan-remake-baseline`.
- Previous Vulkan worker branches are already integrated or stale; do not stack
  this work on them.

Read `/mnt/c/aa/AGENTS.md` and
`apps/quiz/quiz-vulkan/src/render/vulkan/AGENTS.md`.

Scope only:
- `apps/quiz/quiz-vulkan/src/render/vulkan/*`
- `apps/quiz/quiz-vulkan/tests/render/vulkan/*`

Do not edit:
- `src/app/*`, `app.cpp`, `main.cpp`
- top-level `CMakeLists.txt`
- scene, domain, UI, input, audio, text, image, asset, platform files
- aggregate contract registration

Task:
- Move the scoped command packet path one step closer to real Vulkan command
  recording.
- Current gap: `fake_vulkan_command_packet_executor` only records packet
  execution evidence. The native function table already requests
  `vkCmdBindPipeline`, `vkCmdBindDescriptorSets`, `vkCmdSetViewport`,
  `vkCmdSetScissor`, and `vkCmdDraw`, but there is no backend-owned adapter that
  proves command packets can be translated into those native command calls.
- Add a Vulkan-owned native command packet executor boundary behind
  `vulkan_command_packet_executor_interface`, or an equivalent narrow adapter if
  the existing interface shape requires it.
- The new path should:
  - consume only `vulkan_command_packet_bridge_result` and backend-owned native
    command evidence;
  - preserve the existing fake executor and fake-path tests;
  - report begin/packet/end evidence just like the current executor;
  - expose clear diagnostics for missing native command symbols, invalid command
    buffer evidence, invalid packet/scissor/vertex data, and successful draw
    packet translation;
  - avoid owning scene/UI/domain state and avoid app/runtime wiring.
- If invoking real `vkCmd*` safely requires a broader public contract change,
  do not change the public contract silently. Implement the smallest readiness
  or adapter evidence that proves the missing contract and report the exact
  integrator-owned change needed.

Implementation style:
- Prefer C++23 designated initialization.
- Keep public interfaces stable unless the new executor interface is truly the
  owned Vulkan backend interface. If you must add fields, lock them in
  Vulkan-focused contract tests only if they are inside your write scope.
- Split private helpers only when it improves cohesion/reviewability. Do not
  split solely by line count.

Tests:
- Add or update focused Vulkan tests under `tests/render/vulkan/*`.
- Prove the happy path maps packets to native command-call evidence.
- Prove at least one blocker path, preferably missing `vkCmdDraw` or an invalid
  command buffer, fails without falling through as ready.
- Keep tests data-only if a real Vulkan device/window is unavailable.

Required verification:
- Build `quiz_vulkan_interface_contract_compile_tests`.
- Run focused Vulkan tests covering your changes plus renderer/architecture
  boundary tests if touched by includes.
- Run `git diff --check`.
- Commit scoped files and report changed files, verification, risks, and commit
  hash.
