# Quiz Vulkan Codex Rules

- Mirrors `apps/quiz/quiz-vulkan`.
- Use this folder for native C++/Vulkan rewrite requirements, renderer plans, domain contracts, and Codex handoff notes.
- Keep `big_plan.md`, `구현/`, and `일자별요구사항/` in sync with quiz-wide requirements when renderer or C++ domain behavior changes.
- Project-specific implementation plans live under `구현/NN.md`.
- Read `../AGENTS.md`, `../../common/agent/token_budget.json`, and `../../../apps/quiz/quiz-vulkan/AGENTS.md` before broad scans.
- For cross-project source context, read `../app-context/AGENTS.md` and `../app-context/CODEMAP.md` first.
- Keep domain, scene, layout, UI, render, and platform owner boundaries clear.
- Preserve the engine dependency direction:
  `main -> modifier_interface -> scene_layout_data_modifier -> scene_layout_edit_data -> scene_layout_patch -> scene_layout_data -> layout_placer -> ui_renderer -> vulkan_renderer`.
  Modifiers write only through `scene_layout_edit_data`; layout/UI/Vulkan are downstream consumers and must not reach back into app/domain state.
