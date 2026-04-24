# Local Codex Rules

## Reusable Local Tools

Before creating, modifying, registering, or reusing a durable local tool, use the `local-tooling` MCP.

Required flow:

1. Call `get_policy_summary`.
2. Call `search_global_tools` before creating a new tool.
3. If creating a new tool, call `get_tool_recipe`.
4. Before writing durable global-tool files, call `validate_tool_plan`.
5. After verification, call `register_global_tool`.

Durable local tools must use ASCII-only paths:

- Tool root: `C:\codex-local\tools`
- Shim root: `C:\codex-local\bin`
- Registry: `C:\codex-local\registry\tool-registry.json`
- Policy: `C:\codex-local\policy\tool-policy.json`

Do not place reusable tools under user-profile paths containing non-ASCII characters.

## Korean IME Input

For Korean text input, especially React controlled inputs and `contentEditable` editors:

- Handle `compositionstart`, `compositionupdate`, and `compositionend`.
- Do not treat `keydown` as committed text while composition is active.
- Prefer `input`/`change` events for committed text.
- When reading `contentEditable` DOM during composition, synchronize after the DOM update with `requestAnimationFrame`.
- Do not rewrite `innerHTML` or controlled values during active composition unless caret preservation is explicitly handled.

## Repo Bootstrap

When this repo is opened on a new machine:

1. Ask only for the scaffold command name, the plan command name, and whether low-risk read-only work should prefer trusted local delegation.
2. Run `bootstrap/setup_codex_env.ps1`.
3. Keep durable runtime and tool paths ASCII-only under `C:\codex-local` and `C:\dev`.
4. When coordinating multiple local agents, write the shared task once in `agent/shared_command.json` and tell sub-agents to read that file first.
5. Unless the user explicitly asks otherwise, keep the working set to `.gitignore`, `AGENTS.md`, `README.md`, `agent/`, `bootstrap/`, and `plan/`, and ignore `quiz/` and `mcp/`.
6. When working inside `agent/`, `bootstrap/`, or `plan/`, prefer that subtree's local `AGENTS.md` before broad repo files.

## Token Budget

- Prefer compact metadata over large artifacts.
- Read `agent/token_budget.json` before broad repo scans.
- For generated plan bundles, read `bundle_manifest.json` and `prompt_overrides.json` before `render_spec.json`, `prompts.json`, or `plan_summary.docx`.
- For low-risk bounded work such as small file inspection, simple issue detection, installation waits, or long-running log monitoring, a trusted local agent is acceptable when the user has allowed the configured delegation mode.
- If the task is safety-critical or otherwise high-stakes, keep it direct even when token use is higher.
