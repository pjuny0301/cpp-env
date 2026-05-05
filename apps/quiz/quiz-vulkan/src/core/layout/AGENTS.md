# Layout Boundary

- `layout_placer.h` is the stable layout placement boundary. It consumes
  `scene_layout_data` plus text metrics and returns `placed_scene`.
- Layout placement must not mutate `scene_layout_data`; scene mutations go
  through `scene_layout_edit_data`, `scene_layout_patch`, and
  `modifier_interface.h`.
- Layout code must not include UI renderer, Vulkan renderer, domain, app,
  input, audio, persistence, asset, or platform headers.
- Layout may depend on scene data contracts and text metrics interfaces only.
- Before handing off changes, build the aggregate
  `quiz_vulkan_interface_contract_compile_tests` target and run focused layout
  tests.
