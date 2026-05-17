# Input Worker: Wheel, Drag, And Touch Gesture Path

Continue in the same long-lived input worker session, but start a fresh branch
from the latest pushed baseline before editing:

```bash
git fetch origin
git switch -C codex/input-wheel-drag-touch-gestures-20260517 origin/codex/quiz-vulkan-remake-baseline
```

## Goal

Move input beyond click/text/IME by implementing the next real normalized input
and gesture path for wheel, drag, and touch-like pointer streams.

Stay input-owned. The input layer may emit normalized input events, gesture
records, route policy data, and diagnostics. It must not dispatch app/domain
actions directly and must not include scene/UI/render/Vulkan/audio headers.

Stay inside:

- `apps/quiz/quiz-vulkan/src/core/input/*`
- `apps/quiz/quiz-vulkan/tests/input/*`
- `apps/quiz/quiz-vulkan/src/platform/platform_input_event.h` only if a raw
  event shape is needed.

Do not edit `src/app/*`, `main.cpp`, top-level `CMakeLists.txt`, or aggregate
contract registration. If app/runtime wiring is needed, report the exact
integrator-owned follow-up.

## Requirements

- Normalize wheel deltas into input-owned events with timestamp, position,
  delta, and modifier evidence.
- Track drag start/update/end from pointer movement after press using existing
  gesture thresholds where possible.
- Preserve long press and swipe behavior; do not regress existing gesture tests.
- Support touch-like pointer ids in the normalized stream if the raw platform
  event model already carries enough identity; otherwise add the smallest raw
  field needed in `platform_input_event.h`.
- Keep IME/text focus cleanup behavior from the current baseline intact.

## Tests

Focused input tests should prove:

- wheel events normalize without app/domain dispatch,
- drag only starts after movement threshold and emits deterministic lifecycle
  records,
- touch-like pointer ids do not corrupt mouse pointer capture,
- existing long press/swipe routes still behave,
- IME/text focus cleanup tests still pass.

Verify:

- `quiz_vulkan_interface_contract_compile_tests`
- `quiz_vulkan_gesture_recognizer_tests`
- `quiz_vulkan_input_engine_tests`
- `quiz_vulkan_input_engine_ime_tests`
- `quiz_vulkan_text_input_model_tests`
- `git diff --check`

Commit only scoped input files and report hash, changed files, verification,
risks, and external deps used.
