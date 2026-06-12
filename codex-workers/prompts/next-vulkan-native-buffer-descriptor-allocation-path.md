You are the Vulkan Backend Codex worker. Keep this session alive after you finish.

Start by rebasing your working copy onto the current integration baseline:

```bash
cd /mnt/c/aa-workers/vulkan-native-command-packet-executor-20260516
git fetch origin
git checkout -B codex/vulkan-native-buffer-descriptor-allocation-path-20260519 origin/codex/quiz-vulkan-remake-baseline
```

Task: move the native Vulkan backend one step past draw-packet diagnostics toward an actual resource allocation/binding execution path.

Write scope:
- `apps/quiz/quiz-vulkan/src/render/vulkan/*`
- `apps/quiz/quiz-vulkan/tests/render/vulkan/*`

Do not edit app, scene, UI, domain, input, audio, asset, text, image, top-level CMake, or aggregate contract files. You are not alone in the codebase; do not revert or reshape other workers' work.

Current baseline already records native draw resource execution evidence. Add the next narrow layer:
- Define a renderer-owned native resource operation/result for vertex-buffer or descriptor-buffer allocation/binding from existing draw packet/resource packet inputs.
- Keep the path testable through the native function table/fake backend. If real Vulkan calls are available through existing headers/function table, use them. If not enough API exists, add the smallest backend-neutral operation interface and exact blocker diagnostics instead of inventing app-owned shortcuts.
- Prove successful allocation/bind evidence and one missing-resource/failure path.
- Preserve boundary: Vulkan consumes renderer-owned command/resource data only. No scene/domain/app/UI includes.
- Prefer existing external Vulkan/VMA headers under `build/external/lib/cpp/desktop` if they help; do not vendor new source inside `apps/quiz`.

Verification before handoff:
- `cmake --build . --target quiz_vulkan_interface_contract_compile_tests quiz_vulkan_vulkan_command_packet_execution_tests quiz_vulkan_renderer_tests quiz_vulkan_vulkan_native_function_table_tests`
- `ctest -R "vulkan_command_packet_execution|renderer_tests|vulkan_native_function_table" --output-on-failure`
- `git diff --check`

Commit your changes with a concise message and leave the session open.
