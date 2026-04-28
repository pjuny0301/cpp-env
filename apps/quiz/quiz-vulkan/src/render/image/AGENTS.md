# Image/Texture Engine Boundary

- `image_types.h`, `image_resolver.h`, `image_decoder.h`, and `image_texture_cache.h` are the stable Image/Texture Engine interfaces for this folder.
- Do not rename, move, or change public signatures in these files without explicit integrator approval.
- Work in this folder must preserve the renderer boundary: image resolution, decode, sampler, and texture-cache contracts consume render image references only, not quiz/domain/UI state.
- Before handing off changes, build the aggregate `quiz_vulkan_interface_contract_compile_tests` target.
