# Quiz Shared UI

Shared React UI primitives used by the quiz reference apps.

## Contents

- Package identity: `@quiz/shared-ui`.
- `src/components/ui/` contains the generated UI primitive set that was previously duplicated in both `android-quiz-app` and `quiz-editor`.
- `src/components/figma/` contains generated Figma helper components shared by the same apps.
- `package.json` declares the private package name and React peer dependency contract. The apps provide the actual runtime dependencies.

## Consumption

Apps consume this source directly through the `@quiz/shared-ui` Vite alias:

- `apps/quiz/android-quiz-app/vite.config.ts`
- `apps/quiz/quiz-editor/vite.config.ts`

This package is source-only for now; it does not have an independent build step. Keep app-specific screens, state, routing, data access, and domain logic in each app.

## Change Checklist

- Do not reintroduce duplicated generated primitives under the app workspaces.
- Keep shared primitives app-neutral. If a component needs quiz runtime state or editor workflow behavior, it belongs in the consuming app.
- Preserve direct file imports such as `@quiz/shared-ui/components/...` unless both consuming apps are updated together.

Before deleting or changing a shared primitive, run both app builds:

```sh
npm --prefix apps/quiz/android-quiz-app run build
npm --prefix apps/quiz/quiz-editor run build
```
