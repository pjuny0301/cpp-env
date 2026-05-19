You are the long-lived Image/Texture worker. Keep this session alive after finishing.

Start from the latest pushed integration baseline:

```bash
git fetch origin
git switch -C codex/image-standard-decode-cache-upload-residency-20260519 origin/codex/quiz-vulkan-remake-baseline
```

Scope:
- Own only `apps/quiz/quiz-vulkan/src/render/image/*` and `apps/quiz/quiz-vulkan/tests/render/image/*`.
- Do not edit `src/app/*`, `app.cpp`, `main.cpp`, top-level `CMakeLists.txt`, renderer integration, scene, UI, domain, input, audio, assets, or text files.
- Do not rename or move the stable image interfaces.

Task:
- Move image handling toward a real runtime path by connecting standard decode results to cache/upload residency behavior behind the existing image interfaces.
- Reuse the existing STB adapter and standard decoder chain for PNG/JPEG. Do not add a custom decoder when the existing approved dependency can decode the fixture.
- Prove that:
  - a standard decoded image produces a stable texture cache key;
  - the first frame requests upload/materialization;
  - a second frame using the same URI/cache key reuses residency;
  - a decode failure or missing bytes routes to placeholder/fallback without pretending a texture was resident.
- Keep sampler/cache/upload policy in image-owned code only. Renderer/app wiring is integrator-owned.

Implementation guidance:
- Avoid broad rewrites of `image_texture_pipeline.h`.
- If the existing cache/uploader fake is too large, split focused helpers inside `src/render/image` or tests only.
- Keep all external data under `build/external` or image tests; no assets under `apps/quiz` except source/test fixtures already allowed by the repo pattern.
- Use C++23 aggregate initialization style.

Verification:
- Build `quiz_vulkan_interface_contract_compile_tests`.
- Build and run `quiz_vulkan_standard_image_texture_pipeline_tests`.
- Build and run `quiz_vulkan_image_texture_pipeline_tests`.
- Build and run cache/upload focused tests you touch.
- Run `git diff --check`.

Commit and report:
- Commit on your branch.
- Report changed files, verification commands/results, risks, and commit hash.
