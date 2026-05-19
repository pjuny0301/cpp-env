You are the Vulkan Backend Codex worker. Keep this session alive after you finish.

Start from the current integration baseline, not from your stale worker branch:

```bash
cd /mnt/c/aa-workers/vulkan-native-command-packet-executor-20260516
git fetch origin
git checkout -B codex/vulkan-image-payload-descriptor-binding-path-20260519 origin/codex/quiz-vulkan-remake-baseline
```

Task: move the Vulkan backend from vertex-buffer bind evidence toward renderer-owned image payload descriptor binding.

Write scope:
- `apps/quiz/quiz-vulkan/src/render/vulkan/*`
- `apps/quiz/quiz-vulkan/tests/render/vulkan/*`

Do not edit app, scene, UI, domain, input, audio, asset, text, image, top-level CMake, or aggregate contract files. You are not alone in the codebase; do not revert or reshape other workers' work.

Current baseline records native vertex buffer bind evidence. Add the next narrow layer:
- Add a Vulkan-owned command/resource operation that can bind an image payload identity/handle from renderer-owned packet data.
- If the existing command packet data is not rich enough for real descriptor writes, add the smallest Vulkan-side packet summary and blocker diagnostic that proves exactly what field is missing.
- Keep the fake/native function-table path testable. Prefer real Vulkan descriptor call records if existing function-table hooks support them; otherwise add an explicit descriptor-bind operation result and evidence.
- Prove success, missing-payload, and stale/invalid-payload paths in Vulkan-owned tests.
- Preserve the boundary: Vulkan consumes renderer-owned packet/resource summaries only. No scene/domain/app/UI includes.
- Use existing Vulkan/VMA artifacts under `build/external/lib/cpp/desktop` if useful. Do not vendor source into `apps/quiz`.

Verification before handoff:
- `cmake --build . --target quiz_vulkan_interface_contract_compile_tests quiz_vulkan_vulkan_command_packet_execution_tests quiz_vulkan_vulkan_resource_binding_diagnostics_tests quiz_vulkan_renderer_tests`
- `ctest -R "vulkan_command_packet_execution|vulkan_resource_binding_diagnostics|renderer_tests" --output-on-failure`
- `git diff --check`

Commit your changes with a concise message and leave the session open.
