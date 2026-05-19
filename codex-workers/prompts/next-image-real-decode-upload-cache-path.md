You are the Image/Texture Codex worker. Keep this session alive after you finish.

Start by rebasing your working copy onto the current integration baseline:

```bash
cd /mnt/c/aa-workers/image-texture-next-20260514
git fetch origin
git checkout -B codex/image-real-decode-upload-cache-path-20260519 origin/codex/quiz-vulkan-remake-baseline
```

Task: deepen the real image decode -> texture cache/upload path without crossing into Vulkan/app integration.

Write scope:
- `apps/quiz/quiz-vulkan/src/render/image/*`
- `apps/quiz/quiz-vulkan/tests/render/image/*`

Do not edit app, Vulkan, text, asset, scene, UI, domain, input, audio, top-level CMake, or aggregate contract registration. You are not alone in the codebase; do not revert or reshape other workers' work.

Requirements:
- Use the existing standard decoder chain and optional `stb` snapshot under `build/external/lib/cpp/desktop` when useful. Do not hand-roll decoders if the external-backed decoder is already wired.
- Prove one real fixture image path for standard decode and texture cache/upload residency, including repeated URI reuse and stable sampler/cache identity.
- Add one failure path that keeps placeholder behavior intact without crashing or leaking app-specific logic.
- If you need real Vulkan upload, stop at the image-owned upload request/result contract and write the exact Vulkan handoff expectation; Vulkan worker owns backend execution.

Verification before handoff:
- `cmake --build . --target quiz_vulkan_interface_contract_compile_tests quiz_vulkan_standard_image_texture_pipeline_tests quiz_vulkan_image_texture_pipeline_tests quiz_vulkan_fake_image_texture_cache_tests`
- `ctest -R "standard_image_texture_pipeline|image_texture_pipeline|fake_image_texture_cache" --output-on-failure`
- `git diff --check`

Commit your changes with a concise message and leave the session open.
