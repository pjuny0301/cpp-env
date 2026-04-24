# Bootstrap Subtree Rules

- Start with `setup_codex_env.ps1`, then `install_codex_env.py`.
- Read `README.md` in this folder before opening copied tool sources.
- If you move into `codex_env/tools/`, read `codex_env/tools/AGENTS.md` first.
- Treat `codex_env/tools/*` as repo-managed mirrors of the durable tools under `C:\codex-local\tools`.
- Read mirrored tool source only when debugging bootstrap installation or syncing tool behavior.
- The bootstrap contract is user-confirmed setup of Codex commands and paths, not whole-machine reset.
- Keep changes ASCII-safe because this subtree is used to recreate the environment on another machine.
