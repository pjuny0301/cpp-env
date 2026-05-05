Role: Image/Texture Engine worker.

First read:
- `apps/quiz/quiz-vulkan/src/render/image/AGENTS.md`
- `apps/quiz/quiz-vulkan/src/render/image/*.h`
- `apps/quiz/quiz-vulkan/tests/render/image/image_engine_interface_contract_tests.cpp`
- `apps/quiz/quiz-vulkan/tests/render/fake_image_texture_cache_tests.cpp`

Task:
Implement the next Image/Texture step behind the existing resolver/decoder/cache interfaces.

Current assignment:
- Implement filesystem/local source-byte loading behind `image_source_bytes_loader_interface` if it is still missing on this branch.
- Keep `data:` URI behavior intact, reject unsupported remote/network sources, and make path failures placeholder-friendly.
- Add tests for local path/file URI success, missing file, empty file, unsupported source kind, and cache-key diagnostics.

Scope:
- Own only `apps/quiz/quiz-vulkan/src/render/image/*`, `apps/quiz/quiz-vulkan/tests/render/image/*`, and existing image-owned tests explicitly named in the prompt.
- Add concrete local/asset resolver behavior, cache-key validation, or fake decoder/cache implementation.
- Keep failure behavior explicit and placeholder-friendly.
- Keep image contracts render-neutral.

Do not:
- Edit `src/app/*`, `app.cpp`, `main.cpp`, top-level `CMakeLists.txt`, renderer integration, or aggregate contract registration.
- Add network image loading.
- Move or rename image interface headers.
- Make renderer depend on quiz/domain/UI.

Required verification:
- Build `quiz_vulkan_interface_contract_compile_tests`.
- Run `quiz_vulkan_fake_image_texture_cache_tests`.
- Run any new `tests/render/image/*` test added by this task.
- Run `git diff --check`.
