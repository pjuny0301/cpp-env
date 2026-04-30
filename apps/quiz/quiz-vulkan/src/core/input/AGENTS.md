# Input/IME/Gesture Boundary

- `input_event.h`, `gesture_recognizer.h`, `text_input_model.h`, and `text_input_types.h` are the stable Input Engine interfaces for this folder.
- `src/platform/platform_input_event.h` is the raw platform input contract consumed by this layer.
- Do not rename, move, or change public signatures in these files without explicit integrator approval.
- Input code normalizes platform events and models text/gestures; it must not dispatch directly to domain state or depend on renderer internals.
- Input workers own only `src/core/input/*` and `tests/input/*` unless the integrator explicitly expands the write set.
- Do not edit `src/app/*`, `app.cpp`, `main.cpp`, top-level `CMakeLists.txt`, or aggregate contract registration from an input worker branch.
- Before handing off changes, build the aggregate `quiz_vulkan_interface_contract_compile_tests` target.
