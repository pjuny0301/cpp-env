# Quiz Subtree Rules

- Read `README.md` and `CODEMAP.md` before opening source files.
- Use `requirements-status.txt` and `editor-requests-status.txt` for already completed work.
- Treat `android-quiz-app` as the Android quiz app source of truth.
- Treat `quiz-editor` as the current React editor source of truth.
- Treat `quiz-vulkan` as the native C++/Vulkan rewrite source of truth.
- Treat `../../../build/external/quiz/figma-make/quiz-web` as a browser export that follows app behavior changes after the Android source is updated.
- `../../../apps/quiz/` should contain only source project folders.
- Avoid generated or heavy folders by default: `../../../build/external/quiz/dependencies`, `../../../build/out/quiz`, `../../../build/external/quiz/.vs`, `../../../build/external/quiz/.vscode`, packaged APK/EXE outputs, screenshots, logs, and `../../../build/external/quiz/legacy`.
- For behavior changes that must survive the C++ rewrite, prefer extracting the rule into schema/domain notes instead of only changing React component code.
