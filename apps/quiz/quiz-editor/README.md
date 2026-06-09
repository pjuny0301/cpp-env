# Quiz Editor

Reference quiz authoring workflow exported from Figma and maintained as a repo workspace.

## Workspace

- Editor screens, transforms, dialogs, and quiz-data workflow code live under `src/app/`.
- Generated shared primitives are consumed from `../shared-ui/src` through the `@quiz/shared-ui` Vite alias.
- The local development API for quiz-data operations is implemented in `vite.config.ts`.
- The Tauri shell lives under `src-tauri/`.
- Browser build output is written outside source control to `build/out/quiz/quiz-editor/dist`.

## Commands

Run these from the repository root:

```sh
npm --prefix apps/quiz/quiz-editor ci
npm --prefix apps/quiz/quiz-editor run dev
npm --prefix apps/quiz/quiz-editor run build
```

When changing `apps/quiz/shared-ui`, also build `apps/quiz/android-quiz-app` because both React apps consume the same primitive source.

## Notes

- Keep editor-specific Git, prompt, bulk-edit, and quiz-data behavior inside this app.
- Keep reusable generated UI primitives in `apps/quiz/shared-ui` instead of reintroducing duplicated `src/components/ui` copies.
- The dev API defaults quiz-data paths under `build/external/quiz/quiz-data` unless overridden.
- Existing dependency installs may report the current `react-router@7.13.0` Node engine warning on Node 18; Track C did not change dependency manifests.
