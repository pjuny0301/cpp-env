# Input Worker: IME Composition Focus Lifecycle

Continue in the same long-lived input worker session. Keep the session alive
after finishing.

Refresh safely:

```bash
git fetch origin
git switch -C codex/input-ime-composition-focus-lifecycle-20260519 origin/codex/quiz-vulkan-remake-baseline
```

## Scope

Edit only:

- `apps/quiz/quiz-vulkan/src/core/input/*`
- `apps/quiz/quiz-vulkan/tests/input/*`
- `apps/quiz/quiz-vulkan/src/platform/platform_input_event.h` only if the raw
  event shape lacks a field required for IME composition lifecycle.

Do not edit app/main/top-level CMake, scene, UI, domain, render, Vulkan, text,
image, asset, or audio files. If app/runtime wiring is needed, report the exact
integrator-owned follow-up instead of editing outside scope.

## Goal

Move the input engine from basic text commit toward a real IME lifecycle while
staying input-owned.

Add or extend normalized input/text model behavior for:

- IME preedit start/update/commit/cancel records;
- focus target changes while preedit is active;
- focus loss cancellation without leaking stale composition text into the next
  target;
- UTF-8-safe caret/selection snapshots around preedit and commit;
- route/diagnostic evidence proving the input layer emits normalized events only
  and does not dispatch app/domain actions.

Keep the output as normalized input events, text input model state, gesture/input
diagnostics, and tests. Do not include scene/UI/app/domain/render/Vulkan/audio
headers.

## Verification

Build:

- `quiz_vulkan_interface_contract_compile_tests`
- `quiz_vulkan_input_engine_ime_tests`
- `quiz_vulkan_input_engine_tests`
- `quiz_vulkan_text_input_model_tests`
- `quiz_vulkan_gesture_recognizer_tests` if gesture code is touched

Run focused input CTest filters and `git diff --check`.

Commit only scoped files and report commit hash, changed files, verification,
external dependencies used, and risks.
