# Scene Schema

The scene/rendering pipeline owns scene data and draw commands. It must not own
quiz domain state.

`ui_renderer` means the narrow renderer adapter in `src/core/ui/ui_renderer.*`:
it consumes a placed scene and emits a `render_draw_list`. It must not include or
inspect domain headers.

`quiz_screens.h` is a temporary app screen presenter that still converts
`domain::app_snapshot` into a scene patch. Treat that as an app/presentation
bridge, not as the final UI renderer boundary. Do not grow that coupling; the
target split is domain snapshot -> app-owned presentation/modifier -> scene edit
data -> layout -> UI renderer -> Vulkan renderer.

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

Dependency direction:

```text
vulkan_renderer <- ui_renderer <- layout_placer -> scene_layout_data
                                               ^
                    scene_layout_data_modifier - writes scene_layout_edit_data
                                               ^
                                      modifier_interface <- app/main
```

`scene_layout_edit_data` is a restricted subset of `scene_layout_data`. Modifiers
write through that edit surface only. Layout placement does not mutate scene
data, `ui_renderer` does not call into layout placement, and Vulkan does not know
about scene, UI, or domain concepts.

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
