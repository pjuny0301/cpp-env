# Workspace Rules

- Top-level structure is `codex/`, `build/`, and `apps/`.
- `codex/common/` contains reusable Codex planning, agent handoff, MCP, and token-budget material.
- `codex/quiz/` contains quiz-specific requirements, daily requirement snapshots, and implementation mapping notes.
- `build/external/tools/bootstrap/` contains reusable environment/bootstrap setup for any project.
- `build/external/` contains reusable external libraries, tools, and user/editor settings.
- `build/out/` is for ignored build output, including generated scaffolds from legacy app wrappers such as `src-tauri/gen/`.
- `apps/quiz/` contains only quiz source project folders; generated output and external dependencies stay under `build/`.
- `apps/quiz/android-quiz-app` is the current Android-first behavior baseline and a target to absorb into the native C++/Vulkan remake.
- `apps/quiz/quiz-editor` is the current authoring baseline and a target to absorb into the native C++/Vulkan remake plus external worker pipeline.
- `apps/quiz/quiz-vulkan` is the native C++/Vulkan rewrite and final convergence point.
