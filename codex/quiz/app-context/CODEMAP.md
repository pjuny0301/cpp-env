# Code Map

This map explains where the main code lives and which files to read or edit for common changes. The built EXE/APK can differ from this source tree; treat this repository as the refactor and development source.

## Read First

1. `README.md`
   - Repository entry point and project locations.
2. `android-quiz-app/src/app/routes.tsx`
   - Android quiz app route table.
3. `android-quiz-app/src/app/context.tsx`
   - Shared app state, settings, progress, learning state, and remote deck reload.
4. `android-quiz-app/src/app/components/QuizActive.tsx`
   - Main quiz-playing flow.
5. `quiz-editor/src/app/App.tsx`
   - Editor shell, state, Git tree, file operations, panels, and export flow.
6. `quiz-editor/src/app/editorTransforms.ts`
   - Editor import/export transformation logic.
7. `figma-make/quiz-web`
   - Browser-only export for Figma Make. Use this when asking Make to render or iterate on the app like a web app.

## Android Quiz App

Root: `android-quiz-app`

### App Shell

`src/app/App.tsx`
- Thin wrapper that renders the router outlet.

`src/app/routes.tsx`
- Defines routes for home, deck view, day intro, active quiz, and results.
- Wraps routes with `AppProvider`.

`src/app/components/Layout.tsx`
- App-level layout frame and navigation wrapper.

### State And Data

`src/app/context.tsx`
- Central React context for:
  - loaded decks
  - remote source URL
  - quiz delay/manual continue settings
  - long text font size
  - day progress
  - known/learning/unknown question state
  - latest quiz results
- Calls `fetchDecksFromGitUrl` from `remoteDecks.ts`.
- Calls learning transition helpers from `learning.ts`.

`src/app/data.ts`
- Core app data types:
  - `Option`
  - `Question`
  - `Day`
  - `Deck`
- Built-in mock decks.
- Normalizes imported quiz JSON into app `Deck[]`.
- Rebuilds multiple-choice peer options from answer pools.

`src/app/remoteDecks.ts`
- Remote deck loading from:
  - GitHub folder URLs
  - GitHub raw/blob JSON URLs
  - jsDelivr GitHub CDN folder listing
  - generic HTTP directory listings
- Resolves relative image URLs against their source JSON URL.
- Recursively discovers deck/day JSON folders.

`src/app/learning.ts`
- Question learning model:
  - `learning`
  - `known`
  - `unknown`
- Generates learning keys.
- Applies correct/incorrect answer transitions.
- Marks questions as unknown from long press.

### Quiz Flow

`src/app/components/Home.tsx`
- Home screen and deck list entry.
- Remote source controls live through app context.

`src/app/components/DeckView.tsx`
- Deck detail page.
- Lists days and routes into day/random modes.

`src/app/components/DayIntro.tsx`
- Pre-quiz day summary.
- Shows progress/learning counts.
- Starts normal, wrong-only, known-only, or random flows.

`src/app/components/QuizActive.tsx`
- Main quiz controller.
- Owns current question index, selected options, typed answer state, swipe/long-press handlers, and result submission.
- Delegates rendering to the smaller quiz components below.

`src/app/components/QuizProgressHeader.tsx`
- Quiz title, count, progress bar, and settings button.

`src/app/components/QuizQuestionStage.tsx`
- Question body rendering:
  - short prompt
  - long text
  - image
  - multiselect blank slots
- Receives long-press handlers from `QuizActive.tsx`.

`src/app/components/QuizAnswerDock.tsx`
- Bottom answer area:
  - pass button
  - multiple-choice options
  - multiselect options
  - typed answer input/textarea
  - manual continue button
  - typed-answer feedback

`src/app/components/QuizSettingsModal.tsx`
- In-quiz settings modal:
  - manual continue
  - auto-advance delay
  - long text font size
  - known threshold
  - wrong release count

`src/app/components/QuizResults.tsx`
- Result summary after a quiz session.
- Reads latest results from context.

`src/app/quizRuntime.ts`
- Runtime-only quiz helpers:
  - deterministic seeded shuffle
  - swipe/long-press constants
  - visual variants
  - runtime option/question types
  - typed answer normalization and correctness checks

`src/app/audio.ts`
- Success/error sound playback.

## Quiz Editor

Root: `quiz-editor`

### Editor Shell

`src/app/App.tsx`
- Main editor container.
- Still the largest file and the next refactor target.
- Owns:
  - deck/day/editor state
  - current Git file selection
  - Git tree loading and mutation actions
  - legacy desktop/dev fallback invocation
  - file open/import/export
  - save/commit/push/pull actions
  - floating/docked panel placement
  - Codex request/reply handling

### Editor Types And Transforms

`src/app/editorTypes.ts`
- Shared editor-only types:
  - `ManagerDay`
  - `ManagerDeck`
  - dock state
  - Git tree/repository/identity state
  - Codex messages
  - resize targets

`src/app/editorTransforms.ts`
- Pure transformation helpers for editor data:
  - sanitize relative Git paths
  - file name extraction
  - editor HTML escaping
  - text-to-editor-HTML conversion
  - divider HTML generation
  - imported JSON to editor deck model
  - answer/distractor parsing
  - exported option generation

### Editor Components

`src/app/components/BulkEditor.tsx`
- Rich text editing surface.
- Handles quiz divider insertion and divider type shortcuts.
- Exposes editor imperative methods through `BulkEditorRef`.

`src/app/components/PreviewPane.tsx`
- Extracts quizzes from editor HTML.
- Shows parsed quiz cards and answer/distractor inputs.
- Exports helpers used by `App.tsx`:
  - `extractTextWithNewlines`
  - `splitQuizHtml`

`src/app/components/RepositoryModal.tsx`
- Git repository selector and Git identity form.

`src/app/components/CommitModal.tsx`
- Floating commit message dialog.

`src/app/components/CodexPromptPanel.tsx`
- Floating Codex prompt/reply panel.

## Generated UI And Shared Small Pieces

`src/app/components/ui/*`
- Generated UI primitives.
- Usually do not refactor these unless fixing a shared UI primitive.

`src/app/components/figma/ImageWithFallback.tsx`
- Small fallback image wrapper.
- Shared pattern from the generated app files.

## Figma Make Web Export

Root: `figma-make/quiz-web`

This is a copied, browser-only React/Vite version of the Android quiz app source for Figma Make. It intentionally excludes `src-tauri`, generated unused UI primitives, packaged outputs, and desktop/Android build config.

`README.md`
- Explains what the export includes and excludes.

`MAKE_IMPORT_PROMPT.md`
- Prompt/context to use when asking Figma Make to load, preview, or iterate on the app.

`src/app/*`
- Mirrors the Android quiz app's route, state, remote deck, learning, audio, runtime, and screen component files.

`vite.config.ts`
- Keeps the Make-compatible `figma:asset/...` resolver and Vite/Tailwind/React setup.

For behavior changes that should ship to the Android app, edit `android-quiz-app` first, then copy or port the relevant pieces into this export.

## Build And Workspace Files

`.vscode/settings.json`
- VS Code defaults for this repo.

`.vscode/tasks.json`
- VS Code tasks:
  - `Android app: dev`
  - `Android app: build`
  - `Figma Make web: dev`
  - `Figma Make web: build`
  - `Editor: dev`
  - `Editor: build`
  - `Build all`

`.vscode/launch.json`
- Browser debug launch targets for the app and editor dev servers.

`quiz-platform.code-workspace`
- Multi-root VS Code workspace for the root, Android quiz app, Figma Make web export, and editor.

## Common Change Guide

### Add Or Change Quiz Question Behavior

Start with:
- `android-quiz-app/src/app/components/QuizActive.tsx`
- `android-quiz-app/src/app/quizRuntime.ts`
- `android-quiz-app/src/app/components/QuizAnswerDock.tsx`

Use `quizRuntime.ts` for pure answer/shuffle/type logic. Use `QuizActive.tsx` only when the change needs React state, navigation, or side effects.

### Change Learning Rules

Start with:
- `android-quiz-app/src/app/learning.ts`
- `android-quiz-app/src/app/context.tsx`
- `android-quiz-app/src/app/components/DayIntro.tsx`

### Change Remote Google/Git JSON Loading

Start with:
- `android-quiz-app/src/app/remoteDecks.ts`
- `android-quiz-app/src/app/data.ts`

Put URL/fetch/folder traversal logic in `remoteDecks.ts`. Put normalized app data shape changes in `data.ts`.

### Change Editor Import Or Export Format

Start with:
- `quiz-editor/src/app/editorTransforms.ts`
- `quiz-editor/src/app/App.tsx`
- `quiz-editor/src/app/components/PreviewPane.tsx`

Use `editorTransforms.ts` for pure conversion logic. Use `App.tsx` when the change involves file IO, legacy desktop commands, or UI state.

### Change Editor Text Editing UX

Start with:
- `quiz-editor/src/app/components/BulkEditor.tsx`
- `quiz-editor/src/app/components/PreviewPane.tsx`
- `quiz-editor/src/app/App.tsx`

### Change Git Tree, Commit, Push, Pull, Or Repository Selection

Start with:
- `quiz-editor/src/app/App.tsx`
- `quiz-editor/src/app/components/RepositoryModal.tsx`
- `quiz-editor/src/app/components/CommitModal.tsx`

The Git/desktop service calls are still inside `App.tsx`; extracting those into an editor Git service is a good next refactor.

## Next Refactor Targets

1. Split the editor Git tree UI out of `App.tsx`.
2. Split editor file/desktop/Git command helpers out of `App.tsx`.
3. Split `PreviewPane.tsx` into parsing helpers and presentational quiz cards.
4. Split `BulkEditor.tsx` divider handling into smaller hooks/helpers.
5. Consider a package-level shared schema for quiz JSON if Android app and editor formats continue to evolve together.
