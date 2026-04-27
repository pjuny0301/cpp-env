# plan-doc

`plan-doc` turns `plan/plan.md` or `plan/plan.txt` into a polished `plan_summary.docx`.

## Commands

- `plan-doc <project-root>`
- `plan-doc <project-root> --source plan/plan.txt`
- `plan-doc --self-test`

## Behavior

- Writes a source-specific bundle next to the plan file, such as `plan/plan/bundle_manifest.json`, `plan/plan/prompt_overrides.json`, `plan/plan/render_spec.json`, `plan/plan/prompts.json`, and `plan/plan/assets/*`.
- Writes `plan/plan/placeholder_notice.txt` when automatic placeholder fallback is used, so the user can see that generated visuals are not final AI images.
- Uses `OPENAI_API_KEY` and `OPENAI_IMAGE_MODEL` when available.
- Falls back to locally generated placeholder graphics when no image API is configured.
- Reuses existing images when the prompt hash is unchanged, so Codex can iteratively refine the same plan bundle.
- Allows plan-local image pinning through `prompt_overrides.json`, so a visual can reuse an existing file inside the same bundle without crossing into other plan bundles.
- Reads theme defaults from `%PLAN_TEMPLATE_ROOT%\\plan-doc\\theme.json` when present.
- For token efficiency, start with `bundle_manifest.json` and `prompt_overrides.json` before reading the generated DOCX.
