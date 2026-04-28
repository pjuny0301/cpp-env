Role: Audio Engine worker.

First read:
- `apps/quiz/quiz-vulkan/src/audio/AGENTS.md`
- `apps/quiz/quiz-vulkan/src/audio/*.h`
- `apps/quiz/quiz-vulkan/tests/audio/audio_engine_interface_contract_tests.cpp`
- `apps/quiz/quiz-vulkan/tests/audio/fake_audio_engine_tests.cpp`

Task:
Implement the next Audio Engine step behind `audio_engine_interface`.

Scope:
- Add a concrete null/procedural or fake-testable backend implementation.
- Add sound event/catalog tests without real device playback.
- Keep audio owned by app/runtime, not renderer/scene/UI.

Do not:
- Add sample asset loading before asset resolver rules are stable.
- Move or rename audio interface headers.
- Put playback state in renderer/scene/UI.

Required verification:
- Build `quiz_vulkan_interface_contract_compile_tests`.
- Run `quiz_vulkan_fake_audio_engine_tests`.
- Run `git diff --check`.
