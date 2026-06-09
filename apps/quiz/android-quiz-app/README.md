# Android Quiz App

Reference mobile quiz UX exported from Figma and maintained as a repo workspace.

## Workspace

- App screens, routes, state, and quiz runtime logic live under `src/app/`.
- Generated shared primitives are consumed from `../shared-ui/src` through the `@quiz/shared-ui` Vite alias.
- The Tauri shell lives under `src-tauri/`.
- Browser build output is written outside source control to `build/out/quiz/android-quiz-app/dist`.

## Commands

Run these from the repository root:

```sh
npm --prefix apps/quiz/android-quiz-app ci
npm --prefix apps/quiz/android-quiz-app run dev
npm --prefix apps/quiz/android-quiz-app run build
```

When changing `apps/quiz/shared-ui`, also build `apps/quiz/quiz-editor` because both React apps consume the same primitive source.

## Notes

- Keep app-specific screens, quiz state, audio, routing, and data access inside this app.
- Keep reusable generated UI primitives in `apps/quiz/shared-ui` instead of reintroducing duplicated `src/components/ui` copies.
- Existing dependency installs may report the current `react-router@7.13.0` Node engine warning on Node 18; Track C did not change dependency manifests.
