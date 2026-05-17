# Input Worker: IME Focus Loss Diagnostics

Continue in the same long-lived input worker session, but start a fresh branch
from the latest pushed baseline before editing:

```bash
git fetch origin
git switch -C codex/input-ime-focus-loss-diagnostics-20260517 origin/codex/quiz-vulkan-remake-baseline
```

## Goal

Improve input-owned diagnostics for focus loss while IME composition or text
selection is active.

Stay inside `src/core/input/*` and `tests/input/*`. Preserve the rule that input
emits normalized input events and diagnostics only; it must not dispatch app or
domain actions.

Prefer the smallest useful behavior gap around:

- preedit cancellation on focus loss,
- stale composition clearing when the focused target changes,
- selection/caret snapshot evidence before and after focus changes,
- route summary flags that prove no app/domain action was emitted.

## Tests

Focused input tests should prove:

- active preedit is cancelled or clearly diagnosed when focus is lost,
- changing focus target does not leak stale preedit into the next target,
- caret/selection snapshots remain UTF-8 safe,
- summary diagnostics distinguish focus/preedit cleanup from submitted text,
- no app/domain action is emitted by the input layer.

Verify:

- `quiz_vulkan_interface_contract_compile_tests`
- `quiz_vulkan_input_engine_ime_tests`
- `quiz_vulkan_input_engine_tests`
- `quiz_vulkan_text_input_model_tests`
- `git diff --check`

Commit only scoped input files and report hash, changed files, verification,
risks, and external deps used.
