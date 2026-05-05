Role: Asset System worker.

First read:
- `apps/quiz/quiz-vulkan/src/render/image/*.h`
- `apps/quiz/quiz-vulkan/src/render/text/*.h`
- `apps/quiz/quiz-vulkan/src/audio/*.h`
- `apps/quiz/quiz-vulkan/CMakePresets.json`
- relevant AGENTS.md files under each subsystem.

Task:
Define and implement the next asset resolver/cache-key step used by font/image/sound/shader/deck assets.

Current assignment:
- Add a small asset resolver/cache-key policy improvement that can be consumed later by text/image/audio/Vulkan without editing those engines.
- Prefer asset URI normalization, manifest lookup diagnostics, path traversal rejection, pack/version compatibility checks, or shader/font/image/sound/deck cache-key classification.
- Keep it data-only and testable; no runtime app wiring.

Scope:
- Own only `apps/quiz/quiz-vulkan/src/assets/*` and `apps/quiz/quiz-vulkan/tests/assets/*`.
- Keep asset lookup neutral and independent of renderer/domain where possible.
- Define `asset://` normalization, cache key rules, and path traversal rejection.
- Add tests before integrating into any engine.

Do not:
- Edit `src/app/*`, `app.cpp`, `main.cpp`, top-level `CMakeLists.txt`, or aggregate contract registration.
- Pull PDF/OCR/AI workers into native runtime.
- Change existing engine public interfaces without integrator approval.

Required verification:
- Build `quiz_vulkan_interface_contract_compile_tests`.
- Run any new asset tests.
- Run the focused existing asset test touched by this task.
- Run `git diff --check`.
