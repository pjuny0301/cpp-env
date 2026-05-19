You are the Image/Texture Codex worker. Keep this session alive after you finish.

Start from the latest integration baseline:

```bash
cd /mnt/c/aa-workers/image-texture-next-20260514
git fetch origin
git checkout -B codex/image-decoded-resource-upload-payload-summary-20260519 origin/codex/quiz-vulkan-remake-baseline
```

Task: make the common image upload/cache side interface richer for renderer payload consumption without exposing concrete pipeline types.

Write scope:
- `apps/quiz/quiz-vulkan/src/render/image/*`
- `apps/quiz/quiz-vulkan/tests/render/image/*`

Do not edit app, scene, UI, domain, input, audio, asset, text, Vulkan, top-level CMake, or aggregate contract files. You are not alone in the codebase; do not revert or reshape other workers' work.

Current baseline exposes `image_texture_pipeline_upload_cache_diagnostics_interface` for fake and standard pipelines. Add the next narrow layer:
- Add a compact upload/cache payload summary that includes stable cache key, texture id, revision/generation, decoded dimensions, byte counts, placeholder/fallback state, and sampler/cache identity.
- Derive it from the common side interface, not from concrete fake/standard pipeline types.
- Prove fake and standard pipelines produce equivalent summary fields for the same URI and that failed/missing source paths expose explicit blocker diagnostics.
- Keep decode, upload, sampler, and cache data image-owned; app/Vulkan should consume summaries later.
- Use existing STB/decoder dependencies under `build/external/lib/cpp/desktop` when useful. Do not vendor new source into `apps/quiz`.

Verification before handoff:
- `cmake --build . --target quiz_vulkan_interface_contract_compile_tests quiz_vulkan_image_texture_pipeline_tests quiz_vulkan_standard_image_texture_pipeline_tests quiz_vulkan_image_texture_frame_resource_packet_materialization_tests`
- `ctest -R "image_texture_pipeline|standard_image_texture_pipeline|image_texture_frame_resource_packet_materialization" --output-on-failure`
- `git diff --check`

Commit your changes with a concise message and leave the session open.
