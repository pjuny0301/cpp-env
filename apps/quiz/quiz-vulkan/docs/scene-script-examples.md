# Scene Script Examples

These examples document the app-owned scene script surface used by the quiz
presentation layer. They are not renderer inputs and must stay outside
`src/core/ui` and `src/render`.

## Minimal Legacy Template

Schema version 1 is retained for compatibility with the original day intro
template selector:

```json
{
  "schema_version": 1,
  "template": "builtin:quiz.day_intro.v1",
  "screen": "day_intro"
}
```

The compiler maps this selector to the app-owned day intro presentation path.
New built-in screen work should use schema version 2 node DSL documents.

## Node DSL Shape

The native C++ representation is `app_scene_script_document`. A minimal active
question document is equivalent to this data shape:

```text
schema_version: 2
template_id: test:quiz_active.prompt_options.v1
screen: script_quiz_active
nodes:
  - id: script_root
    kind: container
    layout: vertical gap 8
  - id: question_prompt
    parent_id: script_root
    kind: text
    binding:
      text: {{ question.prompt }}
  - id: session_progress
    parent_id: script_root
    kind: text
    binding:
      text: {{ session.progress }}
  - id: session_mode_phase
    parent_id: script_root
    kind: text
    binding:
      text: {{ session.mode }} / {{ session.phase }}
  - id: settings_count
    parent_id: script_root
    kind: text
    binding:
      text: {{ settings.count }}
  - id: learning_summary
    parent_id: script_root
    kind: text
    binding:
      text: {{ learning.summary }}
  - id: selected_day_summary
    parent_id: script_root
    kind: text
    binding:
      text: {{ selected_day.title }} / {{ selected_day.question_count }} questions
  - id: deck_day_function_title
    parent_id: script_root
    kind: text
    binding:
      text: {{ concat(selected_deck.title, " / ", selected_day.title) | upper }}
  - id: question_prompt_upper
    parent_id: script_root
    kind: text
    binding:
      text: {{ question.prompt | upper }}
  - id: question_options
    parent_id: script_root
    kind: container
    condition: question.has_options
  - id: option_{{ option.index }}
    parent_id: question_options
    kind: input
    repeater:
      item_name: option
      collection: question.options
    binding:
      text: {{ option.text }}
    event:
      trigger: press
      command:
        name: submit_option
        option_index: {{ option.index }}
transitions:
  - name: script_enter
    duration_seconds: 0.2
    condition: question.exists
```

## Built-In Screen Coverage

All built-in quiz screen patch/modifier paths currently compile through schema
version 2 node DSL documents:

- `builtin:quiz.deck_list.v1`
- `builtin:quiz.deck_view.v1`
- `builtin:quiz.day_intro.v1`
- `builtin:quiz.quiz_active.v1`
- `builtin:quiz.quiz_feedback.v1`
- `builtin:quiz.quiz_results.v1`
- `builtin:quiz.settings.v1`
- `builtin:quiz.error.v1`

Each migrated built-in screen has direct-vs-scripted placed-render coverage in
`tests/app/app_quiz_screens_tests.cpp`.

## Event Guidance

Use typed commands for ordinary actions. Command names and argument names must
pass `validate_scene_command`.

Text-answer inputs may use a legacy-only event because the submitted text is
provided by the input router at runtime, after script compilation.

## Expression Functions

Use functions when a binding or condition needs small deterministic composition:

```text
binding:
  text: {{ concat(selected_deck.title, " / ", selected_day.title) }}
condition: equals(session.phase, "active")
condition: not(session.completed)
```

Function calls are app/presentation concerns and stay outside renderer code.
