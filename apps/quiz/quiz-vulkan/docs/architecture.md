# Architecture

This project separates quiz domain logic from the UI scene pipeline.

## App Layers

```text
platform/app shell
  -> input/app action routing
      -> app_state action dispatcher
          -> domain services
          -> app_snapshot
          <- app_action
  -> modifier_interface
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

## Domain Services

Domain services own quiz data, learning state, quiz session state, persistence, and loading. They expose immutable `app_snapshot` values to UI and accept `app_action` commands from the app shell.

`app_state` is the app-shell dispatcher. It owns the current decks, selected deck/day, active session, learning map, previous answer outcomes, settings, and error message. UI action bindings are converted to `app_action`, `app_state::dispatch` applies them to domain services, and `app_state::snapshot` creates the next immutable view for scene building.

## UI Subsystem

The UI subsystem owns scene data and rendering only. Scene nodes may contain action binding data, but input routing and domain dispatch live in the app/input layer outside `src/core/ui`.

`scene_layout_data_modifier` builds scene patches from the latest `app_snapshot`. `layout_placer` resolves node constraints to final rectangles. `ui_renderer` turns placed nodes into draw commands. `vulkan_renderer` submits those draw commands.

## Naming

New C++ types, functions, enum values, and files use snake_case. Existing product names and third-party proper nouns keep their conventional spelling.

Namespaces:

- `quiz_vulkan::domain` for quiz data, learning state, sessions, actions, and snapshots.
- `quiz_vulkan::app_state` for the app-shell action dispatcher that bridges UI actions to domain services.
- `quiz_vulkan::scene` for scene data, patches, modifiers, placement, and eventual UI draw data.
