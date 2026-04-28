# Audio Engine Boundary

- `audio_engine.h`, `sound_definition.h`, and `sound_event.h` are the stable Audio Engine interfaces for this folder.
- Do not rename, move, or change public signatures in these files without explicit integrator approval.
- Audio remains app/runtime-owned. Renderer, scene, and UI code must not own playback state or include backend audio details.
- Audio workers own only `src/audio/*` and `tests/audio/*` unless the integrator explicitly expands the write set.
- Do not edit `src/app/*`, `app.cpp`, `main.cpp`, top-level `CMakeLists.txt`, or aggregate contract registration from an audio worker branch.
- Before handing off changes, build the aggregate `quiz_vulkan_interface_contract_compile_tests` target.
