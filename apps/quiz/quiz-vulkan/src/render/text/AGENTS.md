# Text Engine Boundary

- `text_engine.h` is the stable Text Engine interface for this folder.
- Do not rename, move, or change the public signatures in `text_engine.h` without explicit integrator approval.
- Work in this folder must preserve the renderer boundary: text layout and atlas data consume render draw-list types only, not quiz/domain/UI state.
- Before handing off changes, build the aggregate `quiz_vulkan_interface_contract_compile_tests` target.
