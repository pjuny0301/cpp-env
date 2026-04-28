# External Worker Artifact Boundary

- `external_worker_artifact.h` is the stable contract for consuming artifacts produced outside the native runtime.
- Native app/runtime code may load worker manifests and artifact references, but must not run PDF/OCR/AI/proof/code workers directly.
- Worker execution, network access, model selection, sandbox details, and editor pipelines belong outside this contract.
- Do not rename, move, or change public signatures in `external_worker_artifact.h` without explicit integrator approval.
- Before handing off changes, build the aggregate `quiz_vulkan_interface_contract_compile_tests` target.
