# Codex Environment Bootstrap

This repo stores the source-of-truth files for the local Codex environment.

## What it installs

- `C:\codex-local\tools\cpp-new`
- `C:\codex-local\tools\plan-doc`
- command shims under `C:\codex-local\bin`
- `C:\codex-local\defaults\local_agent_policy.json`
- user environment variables for `DEV_ROOT`, `VCPKG_ROOT`, `VCPKG_DEFAULT_BINARY_CACHE`, and `PLAN_TEMPLATE_ROOT`

## Recreate on another machine

1. Clone this repository to an ASCII-only path.
2. Ask which global command names to use.
3. Ask whether low-risk read-only work should prefer trusted local delegation.
4. Run:

```powershell
.\bootstrap\setup_codex_env.ps1
```

The setup script can prompt for:

- scaffold command name
- plan command name
- low-risk delegation mode
- ASCII Python runtime source when `C:\codex-local\runtime\python` does not already exist

It then runs `install_codex_env.py`, copies repo-managed tool sources into `C:\codex-local`, writes the requested shim names, and installs the local-agent policy.

For multi-agent work, keep the shared task brief in `agent/shared_command.json` and have local agents read that file first.
