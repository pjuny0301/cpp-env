# Scene Modifier Boundary

- `modifier_interface.h` is the stable modifier-stack interface for scene layout mutation.
- Modifiers may write only through `scene_layout_edit_data`; they must not bypass the edit-data/patch flow to mutate layout state.
- Do not rename, move, or change public signatures in `modifier_interface.h` without explicit integrator approval.
- Before handing off changes, build the aggregate `quiz_vulkan_interface_contract_compile_tests` target.
