Role: Input/IME/Gesture worker.

First read:
- `apps/quiz/quiz-vulkan/src/core/input/AGENTS.md`
- `apps/quiz/quiz-vulkan/src/platform/platform_input_event.h`
- `apps/quiz/quiz-vulkan/src/core/input/*.h`
- `apps/quiz/quiz-vulkan/tests/input/*`

Task:
Implement the next Input Engine step behind existing input interfaces.

Current assignment:
- Improve focus/caret/IME routing inside `core/input` only.
- Prefer tests that cover preedit cancellation on focus loss, commit/cancel ordering, caret-safe UTF-8 movement, selection replacement, or gesture/text routing diagnostics.
- Do not add app routing; expose normalized input events and route policy data only.

Scope:
- Own only `apps/quiz/quiz-vulkan/src/core/input/*` and `apps/quiz/quiz-vulkan/tests/input/*`.
- Wire or extend raw platform input only through `platform_input_event.h`.
- Keep gesture/text/IME behavior in `core/input`.
- Add tests for edge cases before wiring into app.

Do not:
- Edit `src/app/*`, `app.cpp`, `main.cpp`, top-level `CMakeLists.txt`, or aggregate contract registration.
- Dispatch directly to domain from input layer.
- Put input state in renderer.
- Move or rename input interface headers.

Required verification:
- Build `quiz_vulkan_interface_contract_compile_tests`.
- Run `quiz_vulkan_gesture_recognizer_tests`.
- Run `quiz_vulkan_text_input_model_tests`.
- Run `quiz_vulkan_input_engine_tests` if changed.
- Run `git diff --check`.
