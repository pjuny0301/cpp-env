You are the Image/Texture Codex worker. Keep this session alive after you finish.

Start from the current integration baseline, not from your stale worker branch:

```bash
cd /mnt/c/aa-workers/image-texture-next-20260514
git fetch origin
git checkout -B codex/image-upload-snapshot-side-interface-20260519 origin/codex/quiz-vulkan-remake-baseline
```

Task: expose image upload/cache diagnostics through an image-owned side interface so app/integrator code does not need concrete fake/standard pipeline casts.

Write scope:
- `apps/quiz/quiz-vulkan/src/render/image/*`
- `apps/quiz/quiz-vulkan/tests/render/image/*`

Do not edit app, scene, UI, domain, input, audio, asset, text, Vulkan, top-level CMake, or aggregate contract files. You are not alone in the codebase; do not revert or reshape other workers' work.

Current baseline proves standard decode/upload cache reuse, but app wiring still has to ask concrete fake/standard pipelines for upload snapshots. Add the smallest image-owned contract to fix that:
- Prefer adding a narrow optional side interface or free helper owned by `render/image`, not changing stable public signatures in `image_types.h`, `image_resolver.h`, `image_decoder.h`, or `image_texture_cache.h` unless unavoidable.
- Make both fake and standard pipelines expose the same upload/cache snapshot summary.
- Prove the common path with fake and standard pipeline tests: same URI reuse, one upload snapshot, placeholder/failure snapshot, and no app/domain includes.
- Keep decoded bytes, texture upload identity, sampler policy, and cache diagnostics image-owned.
- Use existing STB/decoder dependencies under `build/external/lib/cpp/desktop` when useful. Do not vendor new source into `apps/quiz`.

Verification before handoff:
- `cmake --build . --target quiz_vulkan_interface_contract_compile_tests quiz_vulkan_image_texture_pipeline_tests quiz_vulkan_standard_image_texture_pipeline_tests quiz_vulkan_fake_image_texture_cache_residency_tests`
- `ctest -R "image_texture_pipeline|standard_image_texture_pipeline|fake_image_texture_cache_residency" --output-on-failure`
- `git diff --check`

Commit your changes with a concise message and leave the session open.
