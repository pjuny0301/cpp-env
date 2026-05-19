You are the long-lived Vulkan Backend worker. Keep this session alive after finishing.

Start from the latest pushed integration baseline:

```bash
git fetch origin
git switch -C codex/vulkan-native-draw-packet-resource-execution-20260519 origin/codex/quiz-vulkan-remake-baseline
```

Scope:
- Own only `apps/quiz/quiz-vulkan/src/render/vulkan/*` and `apps/quiz/quiz-vulkan/tests/render/vulkan/*`.
- Do not edit `src/app/*`, `app.cpp`, `main.cpp`, top-level `CMakeLists.txt`, scene, UI, domain, input, audio, assets, image, or text files.
- Do not change public interface names/signatures unless you stop and report a proposal first.

Task:
- Move the native Vulkan path beyond readiness-only evidence by proving that a renderer draw packet can produce a complete native draw resource execution record.
- Consume only renderer/Vulkan-owned data: command packet identity, pipeline readiness, descriptor readiness, vertex-buffer binding evidence, vertex/instance counts, and draw call evidence.
- Add a focused execution summary that records:
  - packet identity;
  - pipeline bind call readiness/result;
  - descriptor bind call readiness/result when required;
  - vertex buffer bind call readiness/result;
  - draw call readiness/result;
  - exact fallback blocker when any required native resource step is missing.
- Add tests that a packet with all required resources reaches the draw-call stage and a packet missing vertex buffer evidence does not falsely report native success.
- Keep CPU fallback available and keep Vulkan unaware of scene/UI/domain/app/input/audio.

Implementation guidance:
- Prefer small Vulkan-private helper structs/functions near the existing command packet execution code.
- If a new Vulkan-private header/source would materially reduce file size or review cost, add it under `src/render/vulkan` and report any integrator-owned CMake registration needed.
- Use C++23 aggregate initialization style.

Verification:
- Build `quiz_vulkan_interface_contract_compile_tests`.
- Build and run `quiz_vulkan_vulkan_command_packet_execution_tests`.
- Build and run `quiz_vulkan_renderer_tests` and `quiz_vulkan_architecture_boundary_tests`.
- Run `git diff --check`.

Commit and report:
- Commit on your branch.
- Report changed files, verification commands/results, risks, and commit hash.
