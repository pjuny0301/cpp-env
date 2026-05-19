You are the long-lived Image/Texture worker. Keep this session alive after finishing.

Start from the latest pushed integration baseline:

```bash
git fetch origin
git switch -C codex/image-stb-png-standard-pipeline-20260519 origin/codex/quiz-vulkan-remake-baseline
```

Scope:
- Own only `apps/quiz/quiz-vulkan/src/render/image/*` and `apps/quiz/quiz-vulkan/tests/render/image/*`.
- Do not edit `src/app/*`, `app.cpp`, `main.cpp`, top-level `CMakeLists.txt`, renderer integration, scene, UI, domain, input, audio, assets, or text files.
- Do not rename or move the stable image interfaces.

Task:
- Continue the standard image path so PNG can route through the existing STB adapter when STB is available, while preserving internal decoder fallback when STB is unavailable or fails.
- Reuse the existing `third_party_image_decoder_adapter`, `stb_image_decoder_selection.inl`, and standard chain diagnostics. Do not write a custom PNG decoder.
- Preserve the optional third-party adapter semantics: optional adapter fallback must not recursively use the standard STB route.
- Add a small PNG fixture if needed under image tests only.
- Verify decoded RGBA8 metadata, external decoder selection diagnostics, fallback diagnostics, and texture pipeline handoff for PNG.

Implementation guidance:
- Avoid broad rewrites of `image_texture_pipeline.h`.
- Keep all external dependencies under `/mnt/c/aa/build/external` or the worker worktree equivalent if you need anything new; inspect existing STB first.
- Use C++23 aggregate initialization style.

Verification:
- Build `quiz_vulkan_interface_contract_compile_tests`.
- Build and run `quiz_vulkan_standard_image_decoder_chain_tests`.
- Build and run `quiz_vulkan_standard_image_texture_pipeline_tests`.
- Build and run `quiz_vulkan_image_texture_pipeline_tests`.
- Build and run `quiz_vulkan_third_party_image_decoder_adapter_tests`.
- Run `git diff --check`.

Commit and report:
- Commit on your branch.
- Report changed files, verification commands/results, risks, and commit hash.
