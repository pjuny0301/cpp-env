# AA Workspace

Top-level layout:

- `codex/common/`: reusable Codex planning, agent handoff, and token-budget material.
- `codex/quiz/`: quiz-specific requirements and implementation mapping.
- `build/external/`: reusable external libraries, tools, user/editor settings, app data repos, and dependency installs.
- `build/out/`: ignored build outputs.
- `apps/`: source-only product workspaces.

Current app workspace:

- `apps/quiz/android-quiz-app`: current learning UX baseline to absorb into the remake.
- `apps/quiz/quiz-editor`: current authoring baseline to absorb into the remake and worker pipeline.
- `apps/quiz/quiz-vulkan`: native C++/Vulkan remake and final convergence point.
