# Quiz Shared UI

Shared React UI primitives used by the quiz reference apps.

- `src/components/ui/` contains the generated UI primitive set that was previously duplicated in both `android-quiz-app` and `quiz-editor`.
- `src/components/figma/` contains generated Figma helper components shared by the same apps.
- Apps consume this source through the `@quiz/shared-ui` Vite alias. Keep app-specific screens, state, and domain logic in each app.

Before deleting or changing a shared primitive, run both app builds:

```sh
npm --prefix apps/quiz/android-quiz-app run build
npm --prefix apps/quiz/quiz-editor run build
```
