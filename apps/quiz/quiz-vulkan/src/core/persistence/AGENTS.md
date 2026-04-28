# Persistence Boundary

- `learning_state_store.h` is the stable persistence/sync interface for learning state, previous answers, settings, and answer history.
- Persistence code may depend on domain data contracts, but domain, renderer, scene, UI, input, audio, and platform code must not depend on persistence backends.
- Do not rename, move, or change public signatures in `learning_state_store.h` without explicit integrator approval.
- Backends must keep storage details behind this interface: file paths, git remotes, remote APIs, and cache formats do not belong in app/domain contracts.
- Before handing off changes, build the aggregate `quiz_vulkan_interface_contract_compile_tests` target.
