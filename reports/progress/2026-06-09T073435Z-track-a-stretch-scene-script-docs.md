# Track A Stretch - Scene Script Documentation

## Completed

- Updated `docs/scene-schema.md` to describe the app scene script node DSL.
- Documented formatter support, legacy-only text-answer events, and typed command validation.
- Recorded that all built-in quiz screen patch/modifier paths now route through the node DSL compiler.
- Added the missing `bind_event_handler` patch operation to the schema document.

## Commit

- `d37f7c4` - Document scene script node DSL coverage

## Verification

- `git diff --check` passed.
- `git diff --cached --check` passed before commit.

## Next

- Push the Track A PR branch update.
- Continue with another independent stretch item.
