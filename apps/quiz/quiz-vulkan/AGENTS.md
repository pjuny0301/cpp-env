# quiz-vulkan Agent Rules

- This project is the native Vulkan quiz app rewrite.
- Treat the UI pipeline as a subsystem: `modifier_interface -> scene_layout_data_modifier -> scene_layout_patch / scene_layout_edit_data -> scene_layout_data -> layout_placer -> ui_renderer -> vulkan_renderer`.
- Keep domain state outside UI. UI modifiers emit `app_action`; the app shell applies actions to domain services and feeds the next `app_snapshot` back into UI.
- Use snake_case for C++ type names, function names, file names, and enum values in new code.
- Do not edit generated build output, dependency folders, or package artifacts.
- If multiple agents are active, read `agent/command_brief.json` first and only edit your assigned write set.

