You are the long-lived Vulkan Backend worker. Keep this session alive after finishing.

Start from the latest pushed integration baseline:

```bash
git fetch origin
git switch -C codex/vulkan-native-vertex-buffer-bind-evidence-20260519 origin/codex/quiz-vulkan-remake-baseline
```

Scope:
- Own only `apps/quiz/quiz-vulkan/src/render/vulkan/*` and `apps/quiz/quiz-vulkan/tests/render/vulkan/*`.
- Do not edit `src/app/*`, `app.cpp`, `main.cpp`, top-level `CMakeLists.txt`, scene, UI, domain, input, audio, assets, image, or text files.
- Do not change public interface names/signatures unless you stop and report a proposal first.

Task:
- Continue the real Vulkan path behind the existing command packet executor by recording vertex-buffer bind readiness/evidence before native draw calls.
- Use the existing command packet vertices, packet identities, and native command call evidence.
- Add a focused contract that a drawable packet has:
  - pipeline bind ready,
  - descriptor bind ready when required,
  - vertex buffer bind ready,
  - nonzero vertex count and instance count,
  - stable packet identity preserved through the draw call.
- Add blockers for missing/invalid vertex-buffer evidence so draw calls do not appear successful if the vertex data is not bound.
- Keep this as Vulkan-owned execution evidence. Do not make Vulkan include scene/UI/domain/app/input/audio.

Implementation guidance:
- Prefer adding small helper structs/functions near the existing native command packet evidence instead of growing unrelated app code.
- If `vulkan_backend_adapter.cpp` becomes harder to review, you may split Vulkan-private helpers into a focused `src/render/vulkan/*` file, but report any integrator-owned CMake registration needed instead of editing top-level CMake yourself.
- Use C++23 aggregate initialization style.

Verification:
- Build `quiz_vulkan_interface_contract_compile_tests`.
- Build and run `quiz_vulkan_vulkan_command_packet_execution_tests`.
- Build and run `quiz_vulkan_renderer_tests` and `quiz_vulkan_architecture_boundary_tests`.
- Run `git diff --check`.

Commit and report:
- Commit on your branch.
- Report changed files, verification commands/results, risks, and commit hash.
