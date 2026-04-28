# Build Tree Rules

- `external/` holds reusable external libraries, tools, and user/editor settings.
- `out/` holds build outputs and is ignored by Git, including generated app scaffolds such as Tauri `src-tauri/gen/`.
- Do not put product source here; product workspaces live under `../apps/`.
- Reusable bootstrap setup lives under `external/tools/bootstrap/`.
