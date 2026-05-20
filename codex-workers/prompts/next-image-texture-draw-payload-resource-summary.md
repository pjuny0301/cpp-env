You are the Image/Texture Engine Codex worker. Keep this session alive after you finish.

Start from the latest integration baseline:

```bash
cd /mnt/c/aa-workers/image-texture-next-20260514
git fetch origin
git checkout -B codex/image-texture-draw-payload-resource-summary-20260520 origin/codex/quiz-vulkan-remake-baseline
```

Task: connect decoded/upload-cache payload summaries to renderer texture quad payload data.

Write scope:
- `apps/quiz/quiz-vulkan/src/render/image/*`
- `apps/quiz/quiz-vulkan/tests/render/image/*`

Do not edit app, scene, UI, domain, input, audio, asset, text, Vulkan, top-level CMake, or aggregate contract files. You are not alone in the codebase; do not revert or reshape other workers' work.

Current baseline exposes common fake/standard upload-cache diagnostics and payload summaries. Add the next narrow image-owned layer:
- Attach decoded resource/upload-cache summary evidence to renderer texture quad payload records.
- Prove ready decoded/uploaded entries produce stable renderer payload identities without app/concrete pipeline casts.
- Preserve placeholder and blocked payload paths with explicit blocker summaries.
- Reuse existing STB/image dependencies under `build/external/lib/cpp/desktop`; do not add custom decoders unless an approved dependency cannot cover the case.
- Keep output renderer-owned image/texture payload data only. No Vulkan, app, scene, UI, or domain references.

Verification before handoff:
- `build_dir="$(/mnt/c/aa/codex-workers/quiz-vulkan-worker-build-dir.sh "$(pwd)" windows-mingw-ascii)"`
- `/mnt/c/aa/codex-workers/with-build-lock.sh "/mnt/c/Program Files/CMake/bin/cmake.exe" --build "$build_dir" --target quiz_vulkan_interface_contract_compile_tests quiz_vulkan_image_texture_pipeline_tests quiz_vulkan_standard_image_texture_pipeline_tests quiz_vulkan_image_texture_frame_resource_packet_materialization_tests`
- `/mnt/c/aa/codex-workers/with-build-lock.sh "/mnt/c/Program Files/CMake/bin/ctest.exe" --test-dir "$build_dir" -R "image_texture_pipeline|standard_image_texture_pipeline|image_texture_frame_resource_packet_materialization" --output-on-failure`
- `git diff --check`

Commit your changes with a concise message and leave the session open.
