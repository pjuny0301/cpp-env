You are the Vulkan Backend Codex worker. Keep this session alive after you finish.

Start from the latest integration baseline:

```bash
cd /mnt/c/aa-workers/vulkan-native-command-packet-executor-20260516
git fetch origin
git checkout -B codex/vulkan-native-descriptor-update-command-path-20260520 origin/codex/quiz-vulkan-remake-baseline
```

Task: move descriptor write/bind evidence one step closer to actual native command execution.

Write scope:
- `apps/quiz/quiz-vulkan/src/render/vulkan/*`
- `apps/quiz/quiz-vulkan/tests/render/vulkan/*`

Do not edit app, scene, UI, domain, input, audio, asset, text, image, top-level CMake, or aggregate contract files. You are not alone in the codebase; do not revert or reshape other workers' work.

Current baseline has image payload descriptor binding and descriptor write/bind call evidence. Add the next narrow Vulkan-owned layer:
- Build a descriptor update command/path summary that consumes completed descriptor write/bind call evidence and records the native update/bind operation plan.
- If a real `vkUpdateDescriptorSets` call cannot be safely made without broader lifetime/device contracts, expose that exact blocker while preserving fake/data success evidence for completed handles.
- Prove success, missing native symbol, invalid descriptor set/image view/sampler, and stale payload identity paths.
- Keep the default executor honest: no fabricated native writes unless completed descriptor payload and descriptor write/bind evidence is provided.
- Preserve boundary: Vulkan consumes renderer-owned command/resource summaries only. No scene/domain/app/UI includes.

Verification before handoff:
- `build_dir="$(/mnt/c/aa/codex-workers/quiz-vulkan-worker-build-dir.sh "$(pwd)" windows-mingw-ascii)"`
- `/mnt/c/aa/codex-workers/with-build-lock.sh "/mnt/c/Program Files/CMake/bin/cmake.exe" --build "$build_dir" --target quiz_vulkan_interface_contract_compile_tests quiz_vulkan_vulkan_command_packet_execution_tests quiz_vulkan_vulkan_resource_binding_diagnostics_tests quiz_vulkan_renderer_tests`
- `/mnt/c/aa/codex-workers/with-build-lock.sh "/mnt/c/Program Files/CMake/bin/ctest.exe" --test-dir "$build_dir" -R "vulkan_command_packet_execution|vulkan_resource_binding_diagnostics|renderer_tests" --output-on-failure`
- `git diff --check`

Commit your changes with a concise message and leave the session open.
