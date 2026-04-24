# Codex Windows C++ Environment

This repository is the source of truth for a Windows-first, Linux-portable Codex C++ environment.

## Repo layout

- `bootstrap/setup_codex_env.ps1`: asks only for global command names and low-risk delegation mode, then installs the environment
- `bootstrap/install_codex_env.py`: installs the repo-managed toolset into `C:\codex-local`
- `bootstrap/codex_env/tools/cpp-new`: reusable C++ scaffold tool source
- `bootstrap/codex_env/tools/plan-doc`: reusable plan document tool source
- `bootstrap/codex_env/defaults/local_agent_policy.json`: default low-risk local-agent and token-budget policy
- `agent/shared_command.json`: single-file handoff for trusted local sub-agents
- `agent/AGENTS.md`, `bootstrap/AGENTS.md`, `plan/AGENTS.md`: subtree-local instructions to keep scans narrow
- `plan/plan.txt`: sample planning input
- `plan/plan/`: generated plan bundle for `plan.txt`

## Intended workflow

1. Clone this repo to an ASCII-only path.
2. Ask what global command names should be used for the scaffold tool and plan tool.
3. Ask whether low-risk read-only work should prefer a trusted local agent or stay direct-only.
4. Run `bootstrap/setup_codex_env.ps1`.
5. Use the installed scaffold tool to create projects under `C:\dev\projects`.
6. Use the installed plan tool to regenerate per-plan bundles next to source plan files.
7. When multiple local agents are needed, write the shared task once in `agent/shared_command.json` and point sub-agents there first.

## New Machine Contract

This repo does not reset the machine.
It asks the user for the global command names and delegation mode, then installs only the Codex-specific command environment under `C:\codex-local` and `C:\dev`.

## Plan bundle contract

For a source file such as `plan/plan.txt`, generated outputs live in a dedicated sibling bundle:

- `plan/plan/bundle_manifest.json`
- `plan/plan/prompt_overrides.json`
- `plan/plan/plan_summary.docx`
- `plan/plan/render_spec.json`
- `plan/plan/prompts.json`
- `plan/plan/assets/*.png`

This keeps images reusable for that plan while isolating them from other plans.
To minimize token usage, future AI runs should read `bundle_manifest.json` and `prompt_overrides.json` before opening `plan_summary.docx`.
