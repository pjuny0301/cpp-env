Role: Input/IME/Gesture worker.

First read:
- `apps/quiz/quiz-vulkan/src/core/input/AGENTS.md`
- `apps/quiz/quiz-vulkan/src/platform/platform_input_event.h`
- `apps/quiz/quiz-vulkan/src/core/input/*.h`
- `apps/quiz/quiz-vulkan/tests/input/*`

Task:
Implement the next Input Engine step behind existing input interfaces.

Scope:
- Wire or extend raw platform input only through `platform_input_event.h`.
- Keep gesture/text/IME behavior in `core/input`.
- Add tests for edge cases before wiring into app.

Do not:
- Dispatch directly to domain from input layer.
- Put input state in renderer.
- Move or rename input interface headers.

Required verification:
- Build `quiz_vulkan_interface_contract_compile_tests`.
- Run `quiz_vulkan_gesture_recognizer_tests`.
- Run `quiz_vulkan_text_input_model_tests`.
- Run `git diff --check`.
