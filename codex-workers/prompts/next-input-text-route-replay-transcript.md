You are the Input/IME/Gesture Codex worker. Keep this session alive after you finish.

Start from the latest integration baseline:

```bash
cd /mnt/c/aa-workers/input-ime
git fetch origin
git checkout -B codex/input-text-route-replay-transcript-20260520 origin/codex/quiz-vulkan-remake-baseline
```

Task: make text focus/caret route-state diffs replayable as input-owned diagnostics.

Write scope:
- `apps/quiz/quiz-vulkan/src/core/input/*`
- `apps/quiz/quiz-vulkan/tests/input/*`

Do not edit app, scene, UI, domain, audio, asset, text, image, Vulkan, top-level CMake, or aggregate contract files. You are not alone in the codebase; do not revert or reshape other workers' work.

Current baseline tracks text focus/caret route state and diffs. Add the next narrow input-owned layer:
- Build a route replay transcript/summary from consecutive routing diagnostics diffs.
- Capture text commit, selection replacement, IME preedit start/update/commit/cancel, focus loss cleanup, and UTF-8-safe caret movement.
- Keep the output as normalized input/route diagnostics only. Do not dispatch to app/domain and do not put input state in renderer.
- Preserve gesture and pointer capture behavior.

Verification before handoff:
- `build_dir="$(/mnt/c/aa/codex-workers/quiz-vulkan-worker-build-dir.sh "$(pwd)" windows-mingw-ascii)"`
- `/mnt/c/aa/codex-workers/with-build-lock.sh "/mnt/c/Program Files/CMake/bin/cmake.exe" --build "$build_dir" --target quiz_vulkan_interface_contract_compile_tests quiz_vulkan_input_engine_tests quiz_vulkan_text_input_model_tests quiz_vulkan_gesture_recognizer_tests`
- `/mnt/c/aa/codex-workers/with-build-lock.sh "/mnt/c/Program Files/CMake/bin/ctest.exe" --test-dir "$build_dir" -R "input_engine_tests|text_input_model|gesture_recognizer" --output-on-failure`
- `git diff --check`

Commit your changes with a concise message and leave the session open.
