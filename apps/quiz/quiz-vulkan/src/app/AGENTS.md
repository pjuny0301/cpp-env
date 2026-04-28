# App Runtime Boundary

- `app_render_pipeline.h` is the stable app/runtime composition interface.
- App runtime code may compose domain snapshots, scene modifiers, layout, UI rendering, renderer backends, and platform presentation.
- Engine folders must not depend on app runtime headers.
- Do not rename, move, or change public signatures in `app_render_pipeline.h` without explicit integrator approval.
- Before handing off changes, build the aggregate `quiz_vulkan_interface_contract_compile_tests` target.
