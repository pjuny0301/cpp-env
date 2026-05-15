Finish the current scoped task before taking any new feature work.

Do not broaden scope. Do not edit app/main/top-level CMake/aggregate contract files unless this current task already had explicit integrator permission.

Required handoff steps:
- Re-read the nearest AGENTS.md for your edited folder.
- Check `git status --short --branch` and `git diff --name-only`.
- Verify your changes with the narrow role tests, `quiz_vulkan_interface_contract_compile_tests`, and `git diff --check`.
- If tests pass and the diff is scoped, commit your current changes on this worker branch.
- If blocked, do not force a workaround. Report the exact blocker, changed files, and the smallest integrator-owned follow-up needed.
- Final report must include commit hash if committed, changed files, verification commands/results, and any risks.

After the report, keep the session alive and wait for the next prompt.
