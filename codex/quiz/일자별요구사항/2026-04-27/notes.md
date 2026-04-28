# 2026-04-27 작업 메모

2026-04-27

- Reorganize C:/aa into top-level codex, bootstrap, and apps.
- Place the quiz product under apps/quiz.
- Inside apps/quiz, keep android-quiz-app, quiz-editor, and quiz-vulkan as the canonical app folders.
- Split codex into common shared material and project-specific shared material.
- Under codex/quiz, keep the quiz Codex context.
- Each dated requirement folder should have requirements.md and per-item implementation files.
- Keep the move command-oriented and avoid token-heavy source reads.

Completed notes:

- Created codex/common and codex/quiz split.
- Moved reusable Codex handoff metadata under codex/common/agent.
- Moved quiz planning context under codex/quiz.
- Added dated requirements under codex/quiz/일자별요구사항.
- Added compact requirements-tree.md for wave/owner/status lookup.
- Preserved the requested folder tree source as `folder-structure-request.txt`.
- Removed the leftover root `C:/aa/quiz-vulkan` Visual Studio cache.
- Moved app screenshots and Playwright verification artifacts under this dated requirement folder.
- Moved quiz build outputs under `build/out/quiz` and external dependencies/state under `build/external/quiz`.
