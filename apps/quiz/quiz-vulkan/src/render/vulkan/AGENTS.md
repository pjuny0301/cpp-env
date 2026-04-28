# Vulkan Renderer Rules

- Keep this directory behind render contracts only: `render_draw_list`, text render contracts, image render contracts, and Vulkan backend internals.
- Do not include scene, domain, app, UI, input, audio, persistence, theme, worker, asset, or platform headers here.
- Renderer code consumes prepared draw data and owns only backend frame/device/surface lifecycle details.
- Keep CPU fallback available while backend pieces are incremental.
- Before handoff, build `quiz_vulkan_interface_contract_compile_tests` and run `quiz_vulkan_renderer_tests`.
