Role: Text Engine worker.

First read:
- `apps/quiz/quiz-vulkan/src/render/text/AGENTS.md`
- `apps/quiz/quiz-vulkan/src/render/text/text_engine.h`
- `apps/quiz/quiz-vulkan/src/render/text/scene_text_metrics_adapter.h`
- `apps/quiz/quiz-vulkan/tests/render/text/text_engine_interface_contract_tests.cpp`
- `apps/quiz/quiz-vulkan/tests/render/fake_text_engine_tests.cpp`

Task:
Implement the next Text Engine step behind the existing `text_engine_interface`.

Scope:
- Add concrete text-engine implementation files only if they consume the existing interface.
- Prefer a fake/stub concrete engine with measurable behavior before adding external dependencies.
- Improve UTF-8/Hangul measurement/line-breaking tests if needed.
- Keep `text_engine.h` stable.

Do not:
- Move or rename `text_engine.h`.
- Change public signatures without integrator approval.
- Add renderer/domain/UI dependencies to text engine.

Required verification:
- Build `quiz_vulkan_interface_contract_compile_tests`.
- Run `quiz_vulkan_fake_text_engine_tests`.
- Run `git diff --check`.
