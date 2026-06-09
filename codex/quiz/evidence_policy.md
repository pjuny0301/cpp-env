# Quiz Evidence Policy

This policy keeps durable docs readable while preserving enough evidence to reproduce decisions.

## Durable In Git

- Requirement indexes, routing summaries, and owner links.
- Short verification summaries: command, scope, result, commit, date.
- Manifests for external or generated artifacts: source command, inputs, output location, content hash when available.
- Small, essential screenshots or fixtures when they are not reproducible.

## Outside Durable Docs

- Raw build logs, full CTest output, npm install logs, generated PDFs, generated DOCX files, large screenshots, packaged app outputs, and copied dependency trees.
- Reproducible outputs should go under `build/out/quiz` or an external artifact location. They should be summarized in Git by manifest or ledger entry.
- Dependency installs such as `node_modules/` are verification scratch data only and must not be committed.

## Current Track C Evidence

- Before shared UI extraction, both React apps built successfully with `npm run build` after `npm ci`.
- `npm ci` completed in both apps with the existing Node 18 runtime warning for `react-router@7.13.0` requiring Node 20 and one high-severity npm audit finding. Track C did not change dependencies.
- The previous long baseline evidence block in `requirements_traceability_matrix.md` was reproducible handoff history, not requirement index data; it remains recoverable from Git history before this cleanup.
