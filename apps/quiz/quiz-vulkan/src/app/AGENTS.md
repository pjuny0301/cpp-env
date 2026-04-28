# App Runtime Boundary

- `app_render_pipeline.h` is the stable app/runtime composition interface.
- App runtime code may compose domain snapshots, scene modifiers, layout, UI rendering, renderer backends, and platform presentation.
- Engine folders must not depend on app runtime headers.
- `src/app/*`, `app.cpp`, `main.cpp`, and top-level runtime wiring are integrator-owned when multiple workers are active.
- Engine workers should not edit this folder. They should finish engine-local code/tests first and hand off a short app wiring proposal to the integrator.
- Do not rename, move, or change public signatures in `app_render_pipeline.h` without explicit integrator approval.
- Before handing off changes, build the aggregate `quiz_vulkan_interface_contract_compile_tests` target.
