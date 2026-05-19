You are the Input/IME/Gesture Codex worker. Keep this session alive after you finish.

Start by rebasing your working copy onto the current integration baseline:

```bash
cd /mnt/c/aa-workers/input-ime
git fetch origin
git checkout -B codex/input-ime-composition-wheel-route-evidence-20260519 origin/codex/quiz-vulkan-remake-baseline
```

Task: deepen normalized input functionality without app/domain dispatch.

Write scope:
- `apps/quiz/quiz-vulkan/src/core/input/*`
- `apps/quiz/quiz-vulkan/tests/input/*`

Do not edit app, renderer, scene, domain, audio, text, image, Vulkan, top-level CMake, or aggregate contract registration. You are not alone in the codebase; do not revert or reshape other workers' work.

Requirements:
- Keep stable input interfaces in `input_event.h`, `gesture_recognizer.h`, `text_input_model.h`, and `text_input_types.h`.
- Add/extend fixture tests for IME preedit -> commit/cancel, wheel normalization, UTF-8 backspace over a composed Hangul grapheme, and gesture arbitration if those paths already exist.
- If one path is missing in the public contract, add the smallest input-owned contract field/type and explain why in the commit note. Do not route directly to quiz/domain actions.
- Preserve the existing rule: input normalizes and models events; app-owned router decides domain actions.

Verification before handoff:
- `cmake --build . --target quiz_vulkan_interface_contract_compile_tests quiz_vulkan_input_engine_ime_tests quiz_vulkan_input_engine_keyboard_text_tests quiz_vulkan_platform_input_translator_tests quiz_vulkan_text_input_model_tests`
- `ctest -R "input_engine_ime|input_engine_keyboard_text|platform_input_translator|text_input_model" --output-on-failure`
- `git diff --check`

Commit your changes with a concise message and leave the session open.
