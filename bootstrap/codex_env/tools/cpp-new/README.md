# cpp-new

`cpp-new` creates Windows-first, Linux-portable C++ desktop and game project scaffolds.

## Commands

- `cpp-new --ensure-machine-layout`
- `cpp-new <name> --kind desktop`
- `cpp-new <name> --kind game`
- `cpp-new --self-test`

## Behavior

- Uses `CMakeLists.txt` and `CMakePresets.json` as the source of truth.
- Generates `vcpkg.json` and `vcpkg-configuration.json`.
- Keeps cross-platform code in `src/core` and OS-specific code in `src/platform/win32` and `src/platform/linux`.
- Seeds `AGENTS.md`, `README.md`, `docs/stack.md`, and `plan/plan.md` for AI-readable workflows.
- Seeds `agent/command_brief.json` so multiple local agents can share one compact handoff file.
- Creates projects under `%DEV_ROOT%\\projects` by default.
