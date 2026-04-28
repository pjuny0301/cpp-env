# Asset System Boundary

- Asset workers own only `src/assets/*` and `tests/assets/*` unless the integrator explicitly expands the write set.
- Asset lookup must stay neutral: no renderer, scene, app runtime, PDF/OCR/AI worker, or domain execution ownership from this folder.
- Do not edit `src/app/*`, `app.cpp`, `main.cpp`, top-level `CMakeLists.txt`, or aggregate contract registration from an asset worker branch.
- If deck, font, image, sound, or shader runtime wiring is needed, write a short proposal and let the integrator apply it.
- Before handing off changes, build the aggregate `quiz_vulkan_interface_contract_compile_tests` target.
