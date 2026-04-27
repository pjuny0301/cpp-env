# Quiz Project Codex Rules

- This folder holds Codex-side shared context for `apps/quiz`.
- The project source tree is mirrored here with `android-quiz-app/`, `quiz-editor/`, and `quiz-vulkan/`.
- Treat `android-quiz-app` and `quiz-editor` as working baselines to absorb and replace in the native C++/Vulkan remake, not as separate final products to preserve unchanged.
- The source requirement list is `big_plan.md`.
- Requirement numbers are trace IDs, not implementation order. Use the dependency-based execution order in `big_plan.md`.
- Root implementation connection plans live under `구현/NN.md`. They route each requirement into project-level plans without duplicating project details.
- Date-based requirements live under `일자별요구사항/YYYY-MM-DD/requirements.md`.
- Root `구현/NN.md` files include the requirement text, owner project links, waterfall handoff order, alternatives, pros/cons, and handoff criteria.
- Project-specific implementation plans live under `android-quiz-app/구현/`, `quiz-editor/구현/`, and `quiz-vulkan/구현/`; these contain the detailed prose implementation plans.
- Plan reading order is `big_plan.md`, then root `구현/NN.md`, then the relevant project `big_plan.md`, then project `구현/NN.md`, then dated `requirements.md`, then `archive/` only if historical context is needed.
- Execution order is phase-based: common contract/domain first, Android UX baseline second, editor/data pipeline third, Vulkan renderer fourth, external AI/OCR/proof workers fifth, study/theme expansion sixth, and deploy/QA last.
- Generated plan-doc bundles live under dated `generated/` folders. Read their `bundle_manifest.json` and `prompt_overrides.json` before opening render specs, prompts, PDFs, or DOCX files.
- Items 17 and 18 are duplicates. Items 73 and 73 are separate entries in the source; refer to them as 73a and 73b when needed.
- Item 48 is blank and item 53 is incomplete; keep them as clarification placeholders.
- Items 50 through 56 are explicitly marked as work for another branch.
- Do not scan the full `../../apps/quiz` tree for this plan. Use `app-context/README.md`, `app-context/CODEMAP.md`, and status files first.
