Role: Vulkan Backend worker.

First read:
- `apps/quiz/quiz-vulkan/src/render/vulkan/vulkan_renderer.h`
- `apps/quiz/quiz-vulkan/src/render/vulkan/vulkan_renderer.cpp`
- `apps/quiz/quiz-vulkan/src/render/render_draw_list.h`
- `apps/quiz/quiz-vulkan/tests/render/vulkan_renderer_tests.cpp`
- `apps/quiz/quiz-vulkan/tests/render/text/text_engine_interface_contract_tests.cpp`
- `apps/quiz/quiz-vulkan/tests/render/image/image_engine_interface_contract_tests.cpp`

Task:
Implement the next Vulkan backend step while consuming only existing render contracts.

Scope:
- Incrementally introduce real Vulkan backend pieces behind renderer boundaries.
- Keep CPU fallback if useful for tests.
- Add smoke/diagnostic tests that do not require renderer to know scene/domain/UI.

Do not:
- Include scene/domain/app_state/UI/input/audio headers in renderer.
- Change text/image interfaces without integrator approval.

Required verification:
- Build `quiz_vulkan_interface_contract_compile_tests`.
- Run `quiz_vulkan_renderer_tests`.
- Run `git diff --check`.
