# Text Engine Boundary

- `text_engine.h` is the stable Text Engine interface for this folder.
- Do not rename, move, or change the public signatures in `text_engine.h` without explicit integrator approval.
- Work in this folder must preserve the renderer boundary: text layout and atlas data consume render draw-list types only, not quiz/domain/UI state.
- Text workers own only `src/render/text/*`, `tests/render/text/*`, and existing text-owned tests unless the integrator explicitly expands the write set.
- Do not edit `src/app/*`, `app.cpp`, `main.cpp`, top-level `CMakeLists.txt`, renderer integration, or aggregate contract registration from a text worker branch.
- Before handing off changes, build the aggregate `quiz_vulkan_interface_contract_compile_tests` target.
