You are the Input/IME/Gesture Codex worker. Keep this session alive after you finish.

Start from the latest integration baseline:

```bash
cd /mnt/c/aa-workers/input-ime
git fetch origin
git checkout -B codex/input-route-replay-diff-focus-caret-20260519 origin/codex/quiz-vulkan-remake-baseline
```

Task: add replay/diff evidence for text focus/caret route state without app/domain dispatch.

Write scope:
- `apps/quiz/quiz-vulkan/src/core/input/*`
- `apps/quiz/quiz-vulkan/tests/input/*`

Do not edit app, scene, UI, domain, renderer, Vulkan, audio, asset, text, top-level CMake, or aggregate contract files. You are not alone in the codebase; do not revert or reshape other workers' work.

Current baseline carries `text_focus_route_state` in routing diagnostics. Add one small implementation layer:
- Add a replay or diff summary that compares focus/caret route state before/after normalized input sequences.
- Prove insert, selection replacement, backspace, IME preedit, IME cancel, and focus loss update route state predictably.
- If scene hit-test data is needed, use input-owned fake target ids only. Do not include layout/UI/renderer/app headers.
- Emit diagnostics for an app-owned router to consume later; do not dispatch app/domain actions directly.

Verification before handoff:
- `cmake --build . --target quiz_vulkan_interface_contract_compile_tests quiz_vulkan_input_engine_tests quiz_vulkan_input_engine_ime_tests quiz_vulkan_input_engine_keyboard_text_tests`
- `ctest -R "input_engine_tests|input_engine_ime|input_engine_keyboard_text" --output-on-failure`
- `git diff --check`

Commit your changes with a concise message and leave the session open.
