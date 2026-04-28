# Theme/Motion Boundary

- `theme_tokens.h` is the stable contract for theme tokens, color/text/motion policy, and theme resolution.
- Theme code may describe visual policy but must not own renderer, scene, domain, input, or audio state.
- Renderers and UI builders consume resolved tokens; they must not hard-code quiz-specific theme decisions.
- Do not rename, move, or change public signatures in `theme_tokens.h` without explicit integrator approval.
- Before handing off changes, build the aggregate `quiz_vulkan_interface_contract_compile_tests` target.
