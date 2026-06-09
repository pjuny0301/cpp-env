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
  - id: question_learning
    parent_id: script_root
    kind: text
    binding:
      text: {{ question.learning }}
  - id: question_learning_flags
    parent_id: script_root
    kind: text
    binding:
      text: {{ question.is_learning }} / {{ question.is_known }} / {{ question.is_unknown }} / {{ question.is_wrong_note }}
  - id: question_image_uri
    parent_id: script_root
    kind: text
    binding:
      text: {{ choose(question.has_image, question.image_uri, "no-image") }}
  - id: question_image
    parent_id: script_root
    kind: image
    binding:
      image.uri: {{ choose(question.has_image, question.image_uri, "") }}
      image.alt_text: {{ concat("Question image for ", question.id) }}
      image.aspect_ratio: 1.6
      style.opacity: 0.75
      style.border_radius: 8
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
  - id: settings_count_label
    parent_id: script_root
    kind: text
    binding:
      text: {{ format_count(settings.count, "setting") }}
  - id: setting_route
    parent_id: script_root
    kind: text
    binding:
      text: {{ setting("ui_screen", "unset") }}
  - id: feedback_label
    parent_id: script_root
    kind: text
    binding:
      text: {{ choose(feedback.exists, feedback.outcome, "none") }}
  - id: learning_summary
    parent_id: script_root
    kind: text
    binding:
      text: {{ learning.summary }}
  - id: selected_deck_source
    parent_id: script_root
    kind: text
    binding:
      text: {{ choose(selected_deck.has_source, selected_deck.source_uri, "no-source") }}
  - id: selected_deck_source_label
    parent_id: script_root
    kind: text
    binding:
      text: {{ replace(selected_deck.source_uri, "fixture://", "") }}
  - id: selected_day_summary
    parent_id: script_root
    kind: text
    binding:
      text: {{ selected_day.title }} / {{ selected_day.question_count }} questions
  - id: selected_day_question_count_label
    parent_id: script_root
    kind: text
    binding:
      text: {{ format_count(selected_day.question_count, "question") }}
  - id: deck_day_function_title
    parent_id: script_root
    kind: text
    binding:
      text: {{ concat(selected_deck.title, " / ", selected_day.title) | upper }}
  - id: error_label
    parent_id: script_root
    kind: text
    binding:
      text: {{ choose(error.exists, error.message, "No error") }}
  - id: long_text_fallback
    parent_id: script_root
    kind: text
    binding:
      text: {{ choose(question.has_long_text, question.long_text, "No long text") }}
  - id: question_prompt_upper
    parent_id: script_root
    kind: text
    binding:
      text: {{ question.prompt | upper }}
  - id: question_prompt_length
    parent_id: script_root
    kind: text
    binding:
      text: {{ length(question.prompt) }}
  - id: prompt_length_gate
    parent_id: script_root
    kind: text
    condition: greater_or_equal(length(question.prompt), 10)
    binding:
      text: Long prompt
  - id: question_options
    parent_id: script_root
    kind: container
    condition: question.has_options
    binding:
      layout.gap: 6
  - id: option_{{ safe_id(option.text, option.index) }}
    parent_id: question_options
    kind: input
    repeater:
      item_name: option
      collection: question.options
    binding:
      layout.height: 48
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

`start_quiz` accepts `mode` plus optional deterministic randomization controls:

```text
event:
  trigger: press
  command:
    name: start_quiz
    mode: random
    random_seed: 123
    shuffle: true
```

Text-answer inputs may use a legacy-only event because the submitted text is
provided by the input router at runtime, after script compilation.

## Expression Functions

Use functions when a binding or condition needs small deterministic composition:

```text
binding:
  text: {{ concat(selected_deck.title, " / ", selected_day.title) }}
condition: equals(session.phase, "active")
condition: not(session.completed)
condition: all(question.exists, contains(question.prompt, "Korea"))
condition: any(error.exists, feedback.exists)
condition: contains(question.prompt, "Korea")
condition: starts_with(selected_deck.source_uri, "fixture://")
condition: ends_with(selected_deck.source_uri, ".quizdeck")
binding:
  text: {{ replace(selected_deck.source_uri, "fixture://", "") }}
binding:
  text: {{ safe_id("!!!", "fallback_id") }}
condition: greater_or_equal(length(question.prompt), 10)
condition: less_than(settings.count, 3)
condition: between(length(question.prompt), 10, 20)
binding:
  text: {{ format_count(selected_day.question_count, "question") }}
```

Function calls are app/presentation concerns and stay outside renderer code.
