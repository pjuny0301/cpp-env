# Scene Schema

The UI subsystem owns scene data and rendering. It does not own quiz domain state.

## Pipeline

```text
modifier_interface
  -> scene_layout_data_modifier
      -> scene_layout_patch / scene_layout_edit_data
          -> scene_layout_data
              -> layout_placer
                  -> ui_renderer
                      -> vulkan_renderer
```

## `scene_layout_data`

Retained UI tree for the current frame:

- root screen id
- nodes
- style tokens
- text runs
- image refs
- input regions
- focus id
- animation state
- route/screen metadata

## `scene_layout_edit_data`

Restricted write surface for modifiers. It may append, update, remove, bind actions, request navigation, or start transitions. It must not expose raw renderer state.

## Patch Operations

- `append_node`
- `remove_node`
- `set_text`
- `set_style`
- `set_bounds_rule`
- `set_image`
- `bind_action`
- `set_focus`
- `set_route`
- `start_transition`

## Layout Rules

`layout_placer` resolves constraints to final rectangles and hit regions. It receives text metrics through an interface so tests can run without Vulkan.

## Rendering Rules

`ui_renderer` converts placed nodes to draw commands:

- colored quad
- text run
- image quad
- clip/scissor
- debug bounds

`vulkan_renderer` owns GPU resources and submits draw commands only.

