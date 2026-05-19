You are the Input/IME/Gesture Codex worker. Keep this session alive after you finish.

Start from the current integration baseline, not from your stale worker branch:

```bash
cd /mnt/c/aa-workers/input-ime
git fetch origin
git checkout -B codex/input-focus-caret-route-contract-20260519 origin/codex/quiz-vulkan-remake-baseline
```

Task: add the next missing input contract for text focus/caret routing without dispatching directly to domain or renderer code.

Write scope:
- `apps/quiz/quiz-vulkan/src/core/input/*`
- `apps/quiz/quiz-vulkan/tests/input/*`

Do not edit app, scene, UI, domain, renderer, Vulkan, audio, asset, text, top-level CMake, or aggregate contract files. You are not alone in the codebase; do not revert or reshape other workers' work.

Current baseline covers IME preedit/commit/cancel, Hangul backspace, wheel routing, and pointer capture arbitration. Add one small implementation layer:
- Represent focused text target and caret/selection route state as normalized input-owned data.
- Prove text insert, backspace, IME preedit update, and click/tap focus changes update the input-owned route state consistently.
- If scene hit-test data is needed, use a fake input-owned target id or existing input contract. Do not include layout/UI/renderer/app headers.
- Emit diagnostics that an app-owned router can consume later; do not dispatch app/domain actions directly.

Verification before handoff:
- `cmake --build . --target quiz_vulkan_interface_contract_compile_tests quiz_vulkan_input_engine_ime_tests quiz_vulkan_input_engine_keyboard_text_tests quiz_vulkan_input_engine_tests quiz_vulkan_input_engine_gesture_diagnostics_tests`
- `ctest -R "input_engine_ime|input_engine_keyboard_text|input_engine_tests|input_engine_gesture_diagnostics" --output-on-failure`
- `git diff --check`

Commit your changes with a concise message and leave the session open.
