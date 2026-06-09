# Scene Schema

The scene/rendering pipeline owns scene data and draw commands. It must not own
quiz domain state.

`ui_renderer` means the narrow renderer adapter in `src/core/ui/ui_renderer.*`:
it consumes a placed scene and emits a `render_draw_list`. It must not include or
inspect domain headers.

`app_quiz_screens.h` is an app screen presenter that converts
`domain::app_snapshot` into app-owned scene script documents and then into a
scene patch. Treat that as an app/presentation bridge, not as the UI renderer
boundary. Do not grow that coupling into `src/core/ui`; the split is domain
snapshot -> app-owned scene script/presentation/modifier -> scene edit data ->
layout -> UI renderer -> Vulkan renderer.

## Pipeline

```text
modifier_interface
  -> scene_layout_data_modifier
      -> app_scene_script_document / compile_app_scene_script
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

## App Scene Script

`app_scene_script_document` is an app/presentation-layer node DSL. It is not a
renderer format and it is not owned by `src/core/ui` or `src/render`.

For concrete examples, see `docs/scene-script-examples.md`.

Schema version 2 documents contain:

- screen/template identity
- optional route state and focus id
- node definitions with layout rules, style tokens, text runs, image refs, and
  generic scene semantics
- data bindings such as `{{ question.prompt }}`
- session bindings such as `{{ session.progress }}` and
  `{{ session.mode }} / {{ session.phase }}`
- `question.options` repeaters
- conditions
- event handlers with typed commands and optional legacy action bindings
- transitions

The expression engine supports pipe formatters:

- `string`
- `trim`
- `upper`
- `lower`

Formatter chains run left to right, for example
`{{ question.type | upper | lower }}`. Unformatted single interpolation keeps
the original `scene_value` type where the target expects a typed value; formatted
expressions render as strings.

Text-answer controls may use legacy-only events because the submitted text is
provided by the input router at runtime. Other script commands should prefer the
typed command path and must pass the app command registry allowlist and argument
validation.

Current built-in quiz screens route through the node DSL compiler for their
patch/modifier paths:

- deck list
- deck view
- day intro
- quiz active
- quiz feedback
- quiz results
- settings
- error

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
- `bind_event_handler`
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
