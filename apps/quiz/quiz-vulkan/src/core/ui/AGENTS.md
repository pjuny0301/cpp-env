# UI Folder Rules

- `ui_renderer.*` is the UI renderer boundary. It consumes `scene::placed_scene`
  and emits `render_draw_list`; it must not include domain, app, input, audio,
  persistence, or Vulkan backend headers.
- `quiz_screens.h` is currently a compatibility presenter that maps
  `domain::app_snapshot` to scene edits. Do not add new domain mutation here, and
  do not use it as a precedent for renderer/domain coupling.
- New UI rendering work should target scene data, placed scene data, style
  tokens, text/image refs, and draw-list contracts.
- If snapshot-to-scene presentation needs to grow, propose an app-owned
  presentation/modifier layer instead of expanding `ui_renderer`.
