Role: Audio Engine worker.

First read:
- `apps/quiz/quiz-vulkan/src/audio/AGENTS.md`
- `apps/quiz/quiz-vulkan/src/audio/*.h`
- `apps/quiz/quiz-vulkan/tests/audio/audio_engine_interface_contract_tests.cpp`
- `apps/quiz/quiz-vulkan/tests/audio/fake_audio_engine_tests.cpp`

Task:
Implement the next Audio Engine step behind `audio_engine_interface`.

Current assignment:
- Add the next sound event/catalog/mixer diagnostic behavior only if it is not already present.
- Prefer event ordering, idempotent stop/stop-all behavior, bus gain/mute edge cases, procedural/null backend diagnostics, or sample-asset request metadata that does not load real samples yet.
- Do not add device playback or sample loading until asset resolver integration is assigned by the integrator.

Scope:
- Own only `apps/quiz/quiz-vulkan/src/audio/*` and `apps/quiz/quiz-vulkan/tests/audio/*`.
- Add a concrete null/procedural or fake-testable backend implementation.
- Add sound event/catalog tests without real device playback.
- Keep audio owned by app/runtime, not renderer/scene/UI.

Do not:
- Edit `src/app/*`, `app.cpp`, `main.cpp`, top-level `CMakeLists.txt`, or aggregate contract registration.
- Add sample asset loading before asset resolver rules are stable.
- Move or rename audio interface headers.
- Put playback state in renderer/scene/UI.

Required verification:
- Build `quiz_vulkan_interface_contract_compile_tests`.
- Run `quiz_vulkan_fake_audio_engine_tests`.
- Run `quiz_vulkan_procedural_audio_engine_tests` or `quiz_vulkan_audio_mixer_event_tests` if changed.
- Run `git diff --check`.
