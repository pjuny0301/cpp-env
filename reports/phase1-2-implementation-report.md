# Phase 1-2 Implementation Report

Date: 2026-06-08 UTC

## Scope

Implemented the requested Phase 1-2 bridge from legacy string scene actions toward typed scene commands and a minimal app scene script compiler.

This work keeps the existing renderer boundaries intact:

- `ui_renderer` does not parse scripts.
- `vulkan_renderer` does not know about scripts, app domain actions, or command routing.
- `scene_action_binding { trigger, action_type, payload }` remains available and compatible.
- App command routing still produces `domain::app_action` and does not bypass `app_state::dispatch`.
- Generic `core/scene` additions are command/event primitives only; quiz-specific command names stay in the app layer.

## Implemented

### Core Scene Command Model

Added generic scene command/event data in `src/core/scene/scene_layout_data.h`:

- `scene_value`
- `scene_command { name, args }`
- `scene_event_handler { trigger, commands, condition, legacy_binding }`
- `make_scene_command`
- `make_scene_event_handler`

Scene nodes and placed nodes now store `event_handlers` and `has_event_handlers`.

Legacy `bind_action` still writes the existing `scene_action_binding`, and now also wraps it into a typed command handler. Directly appended nodes with a pre-populated legacy action are normalized the same way.

### Patch/Edit/Layout Propagation

Added `bind_event_handler` through:

- `scene_layout_data`
- `scene_layout_edit_data`
- `scene_layout_patch`

`layout_placer` now copies event handlers to placed nodes and input regions. Typed-only handlers also get input regions, so a future typed dispatcher can use the same hit-test geometry.

### App Command Registry

Added `src/app/app_command_registry.h`.

The registry maps the required typed commands to `domain::app_action`:

- `start_quiz` with `mode` argument
- `submit_option` with `option_index` argument
- `continue_after_feedback`

Compatibility fallbacks allow legacy-wrapped command payloads for `start_quiz` and `submit_option`, so wrapped old bindings and new typed commands route equivalently.

### Minimal Scene Script Compiler

Added `src/app/app_scene_script.h`.

Implemented a minimal JSON-like script document for the first converted screen:

```json
{
  "schema_version": 1,
  "template": "builtin:quiz.day_intro.v1",
  "screen": "day_intro"
}
```

The compiler maps this script plus an `app_snapshot` to the existing day-intro screen patch via `make_day_intro_screen_patch(snapshot)`. This is intentionally a narrow Phase 2 template selector, not a full node DSL.

### One Screen Conversion/Equivalence

Converted the day-intro screen path into the first script template entry point:

- Script constant: `presentation::day_intro_screen_script_json`
- Parser: `parse_app_scene_script_json`
- Compiler: `compile_app_scene_script`

The app screen test compares the placed render output from:

- direct builder: `make_quiz_screen_patch(day_intro_snapshot)`
- script compiler: `compile_app_scene_script(day_intro_script, day_intro_snapshot)`

The comparison checks route selection, node ordering, bounds, content bounds, text runs, input regions, action bindings, and handler counts.

## Tests Added/Updated

- `tests/scene/scene_layout_data_tests.cpp`
  - legacy action wrapping into typed command
  - typed event handler insertion with condition
- `tests/scene/scene_layout_interface_contract_tests.cpp`
  - compile-time contracts for `scene_value`, `scene_command`, `scene_event_handler`, and `bind_event_handler`
- `tests/layout/layout_placer_interface_contract_tests.cpp`
  - placed node and input region handler contracts
- `tests/layout/layout_placer_tests.cpp`
  - legacy handler propagation to placed data
  - typed-only handler hit-test region propagation
- `tests/app/app_action_router_tests.cpp`
  - old string route and new typed command route equivalence for required commands
- `tests/app/app_quiz_screens_tests.cpp`
  - day-intro script compile and placed-render equivalence

## Verification

Main session verification:

```text
cmake --preset linux-ninja
cmake --build build/out/quiz/quiz-vulkan/linux-ninja --target \
  quiz_vulkan_interface_contract_compile_tests \
  quiz_vulkan_app_action_router_tests \
  quiz_vulkan_app_quiz_screens_tests \
  quiz_vulkan_layout_placer_tests \
  quiz_vulkan_scene_layout_data_tests
ctest --test-dir build/out/quiz/quiz-vulkan/linux-ninja -R "quiz_vulkan_(app_action_router_tests|app_quiz_screens_tests|layout_placer_tests|scene_layout_data_tests)" --output-on-failure
cmake --build build/out/quiz/quiz-vulkan/linux-ninja --target quiz_vulkan_input_hit_test_tests
ctest --test-dir build/out/quiz/quiz-vulkan/linux-ninja -R "quiz_vulkan_input_hit_test_tests" --output-on-failure
cmake --build build/out/quiz/quiz-vulkan/linux-ninja --target quiz_vulkan_app_input_router_tests
ctest --test-dir build/out/quiz/quiz-vulkan/linux-ninja -R "quiz_vulkan_app_input_router_tests" --output-on-failure
cmake --build build/out/quiz/quiz-vulkan/linux-ninja --target quiz_vulkan_architecture_boundary_tests
ctest --test-dir build/out/quiz/quiz-vulkan/linux-ninja -R "quiz_vulkan_architecture_boundary_tests" --output-on-failure
git diff --check
```

Results:

- Configure passed with `linux-ninja`.
- Focused build targets passed.
- Final targeted CTest pass: 7/7 tests.
- Architecture boundary test passed.
- `git diff --check` passed.

Subagent verification:

- `s-cmdtype` independently verified `quiz_vulkan_interface_contract_compile_tests`, `quiz_vulkan_scene_layout_data_tests`, targeted CTest for scene layout data, and `git diff --check`.
- Its Windows MinGW configure attempt through the worker script failed with exit code 9 and no diagnostics; the Linux fallback succeeded.

## Follow-Up Notes

The script compiler is deliberately minimal for Phase 2. A broader Phase 3 could replace the template selector with a real schema for nodes, text slots, handler commands, and style tokens while continuing to emit `scene_layout_patch` outside the renderer.
