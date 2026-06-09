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
- current-question learning bindings such as `{{ question.learning }}` and
  `{{ question.is_known }}`
- question media bindings such as `{{ question.has_image }}` and
  `{{ question.image_uri }}`
- node image bindings such as `image.uri`, `image.alt_text`, and
  `image.aspect_ratio`
- numeric style/layout bindings such as `style.opacity`,
  `style.border_radius`, `layout.width`, `layout.height`, and `layout.gap`
- session bindings such as `{{ session.progress }}` and
  `{{ session.mode }} / {{ session.phase }}`
- feedback bindings such as `{{ feedback.exists }}` and
  `{{ feedback.outcome }}`
- app-status bindings such as `{{ settings.count }}` and
  `{{ error.exists }}`
- learning summary bindings such as `{{ learning.summary }}` and
  `{{ learning.known_count }}`
- deck/day bindings such as `{{ selected_deck.title }}` and
  `{{ selected_deck.source_uri }}` or `{{ selected_day.question_count }}`
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

The expression engine also supports pure function calls:

- `concat(a, b, ...)`
- `equals(a, b)`
- `not(value)`
- `all(value, ...)`
- `any(value, ...)`
- `empty(value)`
- `contains(value, needle)`
- `starts_with(value, prefix)`
- `ends_with(value, suffix)`
- `replace(value, needle, replacement)`
- `length(value)`
- `greater_than(left, right)`
- `less_than(left, right)`
- `greater_or_equal(left, right)`
- `less_or_equal(left, right)`
- `between(value, min, max)`
- `format_count(count, singular, plural?)`
- `choose(condition, value_when_true, value_when_false)`
- `safe_id(value, fallback?)`
- `setting(name, fallback?)`

Function arguments may be other expressions or quoted string literals, and
function results can still flow through formatter chains. `choose(...)` only
evaluates the selected branch, which allows fallback bindings such as
`{{ choose(question.has_long_text, question.long_text, "No long text") }}`.
Use `all(...)` and `any(...)` when a condition needs to combine multiple
boolean-style expressions without moving that logic into renderer code. They
evaluate arguments from left to right and stop once the result is known.
Use `replace(...)` for small deterministic string cleanup in app-owned labels,
for example removing a known source URI prefix before rendering it. It replaces
all non-overlapping occurrences and rejects an empty needle.
Use `safe_id(...)` when dynamic node IDs need stable slug text, for example
`option_{{ safe_id(option.text, option.index) }}`. Use `setting(...)` to read
app settings by key without exposing the map shape to the renderer. Use
`length(...)` when script-authored copy or conditions need the rendered string
length, and pair it with comparison helpers or inclusive `between(...)` for
simple numeric conditions. Use `format_count(...)` for stable singular/plural
count labels in script-authored copy.

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
