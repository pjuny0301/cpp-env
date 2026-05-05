Role: Text Engine worker.

First read:
- `apps/quiz/quiz-vulkan/src/render/text/AGENTS.md`
- `apps/quiz/quiz-vulkan/src/render/text/text_engine.h`
- `apps/quiz/quiz-vulkan/src/render/text/scene_text_metrics_adapter.h`
- `apps/quiz/quiz-vulkan/tests/render/text/text_engine_interface_contract_tests.cpp`
- `apps/quiz/quiz-vulkan/tests/render/fake_text_engine_tests.cpp`

Task:
Implement the next Text Engine step behind the existing `text_engine_interface`.

Current assignment:
- Add one focused improvement to the text backend contract or fake implementation that moves toward real font/glyph rendering without app wiring.
- Prefer font source resolution, glyph atlas diagnostics, shaping/line-break edge cases, caret/selection/preedit metric behavior, or a small dependency-backed prototype hidden behind the existing interface.
- Do not rewrite the existing fake engine wholesale; preserve the current public contract and add measurable behavior with tests.

Scope:
- Own only `apps/quiz/quiz-vulkan/src/render/text/*`, `apps/quiz/quiz-vulkan/tests/render/text/*`, and existing text-owned tests explicitly named in the prompt.
- Add concrete text-engine implementation files only if they consume the existing interface.
- Prefer a fake/stub concrete engine with measurable behavior before adding external dependencies.
- If real font/glyph/shaping work needs open-source dependencies or test fonts, download them only under `/mnt/c/aa/build/external` or the worker worktree's `build/external` equivalent.
- Report every downloaded dependency/test font with source URL, version or commit, license, and exact local path.
- Improve UTF-8/Hangul measurement/line-breaking tests if needed.
- Keep `text_engine.h` stable.

Do not:
- Edit `src/app/*`, `app.cpp`, `main.cpp`, top-level `CMakeLists.txt`, renderer integration, or aggregate contract registration.
- Move or rename `text_engine.h`.
- Change public signatures without integrator approval.
- Add renderer/domain/UI dependencies to text engine.

Required verification:
- Build `quiz_vulkan_interface_contract_compile_tests`.
- Run `quiz_vulkan_fake_text_engine_tests`.
- Run any new `tests/render/text/*` test added by this task.
- Run `git diff --check`.
