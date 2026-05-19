You are the Vulkan Backend Codex worker. Keep this session alive after you finish.

Start from the latest integration baseline:

```bash
cd /mnt/c/aa-workers/vulkan-native-command-packet-executor-20260516
git fetch origin
git checkout -B codex/vulkan-native-descriptor-write-bind-call-evidence-20260519 origin/codex/quiz-vulkan-remake-baseline
```

Task: move image payload descriptor binding one step closer to real native descriptor write/bind calls.

Write scope:
- `apps/quiz/quiz-vulkan/src/render/vulkan/*`
- `apps/quiz/quiz-vulkan/tests/render/vulkan/*`

Do not edit app, scene, UI, domain, input, audio, asset, text, image, top-level CMake, or aggregate contract files. You are not alone in the codebase; do not revert or reshape other workers' work.

Current baseline has image payload descriptor binding evidence and blocker paths. Add the next narrow Vulkan-owned layer:
- Add fake/native-function-table evidence for descriptor write and descriptor bind calls using the existing image payload descriptor binding result.
- If existing native function table symbols are not sufficient for real `vkUpdateDescriptorSets` or bind calls, add exact missing-symbol diagnostics and a fake-call record that makes the blocker explicit.
- Keep command packet execution honest: do not fabricate descriptor writes unless completed descriptor payload/binding evidence is provided.
- Prove success plus missing native symbol, invalid descriptor handle, and mismatched payload identity paths.
- Preserve boundary: Vulkan consumes renderer-owned command/resource summaries only. No scene/domain/app/UI includes.
- Use existing Vulkan/VMA artifacts under `build/external/lib/cpp/desktop` if useful. Do not vendor source into `apps/quiz`.

Verification before handoff:
- `cmake --build . --target quiz_vulkan_interface_contract_compile_tests quiz_vulkan_vulkan_command_packet_execution_tests quiz_vulkan_vulkan_resource_binding_diagnostics_tests quiz_vulkan_renderer_tests`
- `ctest -R "vulkan_command_packet_execution|vulkan_resource_binding_diagnostics|renderer_tests" --output-on-failure`
- `git diff --check`

Commit your changes with a concise message and leave the session open.
