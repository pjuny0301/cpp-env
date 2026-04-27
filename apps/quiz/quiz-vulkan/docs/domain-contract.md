# Domain Contract

The domain layer owns quiz data, learning state, session state, persistence, and loading. It does not know about Vulkan, scene nodes, layout, or draw commands.

## Types

- `deck`: collection of `day`.
- `day`: collection of `question`.
- `question`: prompt, optional long text, optional image, type, and answer options.
- `content_ref`: source URI/page/paragraph reference for PDF or imported content traceability.
- `concept_ref`: concept graph node with prerequisite concept IDs.
- `answer_explanation`: explanation text plus linked concept IDs.
- `question_ref`: stable `deck_id`, `day_id`, `question_id` triple.
- `option`: answer text plus correctness flag.
- `question_type`: `short`, `long`, `answer`, `blank`, `multi_answer`, `multi_blank`, `multiselect`.
- `learning_state`: `learning`, `known`, `unknown`.
- `question_learning`: `correct_streak`, `wrong_count`, `review_count`, `state`, `known_at_ms`, `due_at_ms`, `updated_at_ms`.
- `quiz_session`: active ordered question list, current index, answer records, random/wrong/known modes.
- `quiz_session_phase`: `idle`, `active`, `feedback`, `completed`; UI and renderer use this instead of inferring state from multiple fields.

Question learning keys use the shared app shape `deck_id::day_id::question_id`. Requirement numbers are not part of runtime identity.

## Snapshot And Action

`app_snapshot` is an immutable view of current domain state for the UI subsystem.

`app_action` is the only way UI asks domain services to change state. Examples:

- `load_source`
- `select_deck`
- `select_day`
- `start_quiz`
- `submit_option`
- `submit_text_answer`
- `submit_multiselect`
- `skip_question`
- `mark_question_unknown`
- `continue_after_feedback`
- `update_setting`

The app shell applies actions, updates domain state, and provides a fresh `app_snapshot` on the next frame.

`app_state` is the current app-shell implementation of that rule. It handles deck/day selection, starts sessions, applies submitted answers to learning state, remembers previous outcomes for wrong-only sessions, and returns `app_snapshot`.

## Ported Behavior From React App

- Known questions are hidden from normal quiz runs.
- `mode=known` shows known questions only.
- Wrong-only mode uses the previous result record.
- Correct streak promotes `learning` or `unknown` to `known`.
- Promotion to `known` schedules a due review timestamp for SRS-style re-entry.
- Normal sessions hide known questions unless they are due for review.
- Wrong answers can release `known` back to `learning`.
- Releasing a known item clears its review due timestamp.
- Long press marks a question `unknown`.
- Swipe skips without feedback.
- Submitted answers move the session to `feedback`; duplicate answers are blocked until `continue_after_feedback`.
- Typed answers compare normalized text.
- Multi-answer typed input is split by comma, semicolon, or newline.
- `multiselect` requires the selected set to match all correct options.
