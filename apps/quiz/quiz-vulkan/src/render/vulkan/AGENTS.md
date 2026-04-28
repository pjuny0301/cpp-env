# Vulkan Renderer Rules

- Keep this directory behind render contracts only: `render_draw_list`, text render contracts, image render contracts, and Vulkan backend internals.
- Do not include scene, domain, app, UI, input, audio, persistence, theme, worker, asset, or platform headers here.
- Renderer code consumes prepared draw data and owns only backend frame/device/surface lifecycle details.
- Keep CPU fallback available while backend pieces are incremental.
- Vulkan workers own only `src/render/vulkan/*`, `tests/render/vulkan/*`, and renderer-owned tests unless the integrator explicitly expands the write set.
- Do not edit `src/app/*`, `app.cpp`, `main.cpp`, top-level `CMakeLists.txt`, scene, domain, UI, input, audio, or aggregate contract registration from a Vulkan worker branch.
- Before handoff, build `quiz_vulkan_interface_contract_compile_tests` and run `quiz_vulkan_renderer_tests`.
