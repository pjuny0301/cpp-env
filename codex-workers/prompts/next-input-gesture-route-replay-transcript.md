You are the Input/IME/Gesture Codex worker. Keep this session alive after you finish.

Start from the latest integration baseline:

```bash
cd /mnt/c/aa-workers/input-ime
git fetch origin
git checkout -B codex/input-gesture-route-replay-transcript-20260520 origin/codex/quiz-vulkan-remake-baseline
```

Task: add gesture/pointer route replay transcript diagnostics.

Write scope:
- `apps/quiz/quiz-vulkan/src/core/input/*`
- `apps/quiz/quiz-vulkan/tests/input/*`

Do not edit app, scene, UI, domain, audio, asset, text, image, Vulkan, top-level CMake, or aggregate contract files.

Current baseline has text route replay transcripts. Add the next narrow input-owned layer:
- Summarize consecutive routing diagnostics for pointer capture, swipe, long press, tap, wheel, touch arbitration, and gesture cancellation.
- Keep output as normalized input/route diagnostics only. Do not dispatch to app/domain and do not put input state in renderer.
- Preserve the text route transcript behavior and tests.

Verification before handoff:
- `build_dir="$(/mnt/c/aa/codex-workers/quiz-vulkan-worker-build-dir.sh "$(pwd)" windows-mingw-ascii)"`
- `/mnt/c/aa/codex-workers/with-build-lock.sh "/mnt/c/Program Files/CMake/bin/cmake.exe" --build "$build_dir" --target quiz_vulkan_interface_contract_compile_tests quiz_vulkan_input_engine_gesture_diagnostics_tests quiz_vulkan_input_engine_touch_arbitration_tests quiz_vulkan_gesture_recognizer_tests`
- `/mnt/c/aa/codex-workers/with-build-lock.sh "/mnt/c/Program Files/CMake/bin/ctest.exe" --test-dir "$build_dir" -R "input_engine_gesture_diagnostics|input_engine_touch_arbitration|gesture_recognizer" --output-on-failure`
- `git diff --check`

Commit your changes with a concise message and leave the session open.
