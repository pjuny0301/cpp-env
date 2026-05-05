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

Current assignment:
- Improve backend lifecycle/resource diagnostics behind `src/render/vulkan/*`.
- Prefer frame resource lifetime, shader/pipeline descriptor validation, swapchain acquire/present policy, command batch recording diagnostics, or CPU-fallback boundary tests.
- Do not add scene/domain/app/UI dependencies and do not wire platform windows; keep it contract-level and testable.

Scope:
- Own only `apps/quiz/quiz-vulkan/src/render/vulkan/*`, `apps/quiz/quiz-vulkan/tests/render/vulkan/*`, and renderer-owned tests explicitly named in the prompt.
- Incrementally introduce real Vulkan backend pieces behind renderer boundaries.
- Keep CPU fallback if useful for tests.
- If real backend work needs external Vulkan material, you may download Vulkan SDK headers, Vulkan-Headers, volk, shader tooling, or small open-source helpers only under `/mnt/c/aa/build/external` or the worker worktree's `build/external` equivalent.
- Report every downloaded dependency/tool with source URL, version or commit, license, and exact local path.
- Add smoke/diagnostic tests that do not require renderer to know scene/domain/UI.

Do not:
- Edit `src/app/*`, `app.cpp`, `main.cpp`, top-level `CMakeLists.txt`, scene/domain/UI/input/audio, or aggregate contract registration.
- Include scene/domain/app_state/UI/input/audio headers in renderer.
- Change text/image interfaces without integrator approval.

Required verification:
- Build `quiz_vulkan_interface_contract_compile_tests`.
- Run `quiz_vulkan_renderer_tests`.
- Run any new `tests/render/vulkan/*` test added by this task.
- Run `git diff --check`.
