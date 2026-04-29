# UI Folder Rules

- `ui_renderer.*` is the UI renderer boundary. It consumes `scene::placed_scene`
  and emits `render_draw_list`; it must not include domain, app, input, audio,
  persistence, or Vulkan backend headers.
- App snapshot-to-scene presentation lives under `src/app/`, not this folder.
  Do not add domain headers here.
- New UI rendering work should target scene data, placed scene data, style
  tokens, text/image refs, and draw-list contracts.
- If snapshot-to-scene presentation needs to grow, keep it in the app-owned
  presentation/modifier layer instead of expanding `ui_renderer`.
