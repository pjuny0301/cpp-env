You are the long-lived Input/IME worker. Keep this session alive after finishing.

Start from the latest pushed integration baseline:

```bash
git fetch origin
git switch -C codex/input-gesture-normalized-route-evidence-20260519 origin/codex/quiz-vulkan-remake-baseline
```

Scope:
- Own only `apps/quiz/quiz-vulkan/src/core/input/*` and `apps/quiz/quiz-vulkan/tests/input/*`.
- Do not edit `src/app/*`, `app.cpp`, `main.cpp`, top-level `CMakeLists.txt`, renderer, scene, UI, domain, audio, assets, image, text, or Vulkan files.
- Do not route directly to domain/app actions. Stay at normalized input, gesture, and route-candidate evidence.

Task:
- Strengthen the input engine's gesture path by recording normalized route evidence for swipe, drag, long press, wheel, and text/IME coexistence.
- Add focused evidence that:
  - pointer capture starts, updates, and releases without leaking across focus loss,
  - drag and swipe are distinguishable by threshold/time policy,
  - long press remains a gesture candidate and does not dispatch domain/app actions,
  - wheel events coexist with active IME preedit without mutating committed text,
  - route diagnostics expose enough stable data for app-owned routing to consume later.

Implementation guidance:
- Keep the output as engine-owned diagnostics or route candidates, not app commands.
- Prefer focused helpers/tests over broad input-engine rewrites.
- Use C++23 aggregate initialization style.

Verification:
- Build `quiz_vulkan_interface_contract_compile_tests`.
- Build and run `quiz_vulkan_input_engine_tests`.
- Build and run `quiz_vulkan_input_engine_gesture_diagnostics_tests`.
- Build and run `quiz_vulkan_input_engine_ime_tests`.
- Build and run `quiz_vulkan_platform_input_translator_tests` if platform translation is touched.
- Run `git diff --check`.

Commit and report:
- Commit on your branch.
- Report changed files, verification commands/results, risks, and commit hash.
