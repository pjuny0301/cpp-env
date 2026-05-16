You are the long-lived Text Engine worker. Keep this session alive after finishing.

First update your worktree safely:
- `git fetch origin`
- Start a new branch from `origin/codex/quiz-vulkan-remake-baseline`, for example `codex/text-fake-engine-source-split-20260515`.
- Your previous `1e447df` work was already integrated on main as `a059b73`; do not re-cherry-pick it.

Read:
- `/mnt/c/aa/AGENTS.md`
- `apps/quiz/quiz-vulkan/src/render/text/AGENTS.md`

Scope:
- Only `apps/quiz/quiz-vulkan/src/render/text/*`
- Only `apps/quiz/quiz-vulkan/tests/render/text/*`
- Do not edit app, CMake, Vulkan, image, assets, domain, scene, ui, platform, audio, or aggregate contract registration.

Task:
- Reduce future token/read cost in the text engine without changing behavior.
- Split the large implementation-only `fake_text_engine.cpp` into private source fragments if useful.
- Prefer `.inl` fragments included by `fake_text_engine.cpp` so no CMake/public header registration change is needed.
- Good split boundaries: backend context/run shaping, atlas/upload diagnostics, line/run layout diagnostics, or helper-only data assembly.
- Do not move or rename public interface files such as `text_engine.h`, `fake_text_engine.h`, `font_shaping_backend.h`, or `font_rasterizer.h`.
- Do not change public signatures unless you stop and report a proposal instead.
- Keep all changes C++23-compatible and behavior-preserving.

Verification:
- Build `quiz_vulkan_interface_contract_compile_tests`.
- Build and run `quiz_vulkan_fake_text_engine_tests` and `quiz_vulkan_fake_text_engine_layout_diagnostics_tests`.
- Run `git diff --check`.
- Commit only scoped files and report commit hash, changed files, verification, and any risk.
