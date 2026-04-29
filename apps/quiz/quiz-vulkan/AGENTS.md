# quiz-vulkan Agent Rules

- This project is the native Vulkan quiz app rewrite.
- Treat the scene/rendering pipeline as: `modifier_interface -> scene_layout_data_modifier -> scene_layout_patch / scene_layout_edit_data -> scene_layout_data -> layout_placer -> ui_renderer -> vulkan_renderer`.
- Keep domain state outside renderer/layout code. `ui_renderer`, `layout_placer`, and `vulkan_renderer` must not include domain headers or infer quiz semantics.
- `src/core/ui/quiz_screens.h` is a temporary app screen presenter that converts `domain::app_snapshot` into scene edits. Do not expand that coupling; app/main owns domain mutation and long-term snapshot-to-scene presentation should sit at the app/presentation boundary.
- Scene/UI modifiers emit `app_action`; the app shell applies actions to domain services and feeds the next snapshot into the presentation bridge.
- Use snake_case for C++ type names, function names, file names, and enum values in new code.
- Do not edit generated build output, dependency folders, or package artifacts.
- If multiple agents are active, read `agent/command_brief.json` first and only edit your assigned write set.
- Worker ownership is folder-scoped. Engine workers must stay inside their engine folder and matching `tests/<engine>` folder unless the integrator explicitly assigns app wiring.
- `src/app/*`, `src/app/app.cpp`, `src/app/main.cpp`, top-level `CMakeLists.txt`, and aggregate contract registration are integrator-owned. Engine workers must propose needed app/CMake wiring instead of editing those files.
- Tests under `tests/input`, `tests/audio`, `tests/assets`, `tests/render/text`, `tests/render/image`, and `tests/render/vulkan` are auto-discovered where possible; do not edit top-level CMake just to add a new test file.
