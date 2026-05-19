Continue in the same text-engine session on your current branch.

Do not start a new feature. Refine the already committed atlas packet
consumption evidence before integrator merge.

Issue:
- The feature direction is acceptable, but `src/render/text/text_frame_upload_handoff.h`
  is already very large and your patch adds more evidence/diff logic there.

Task:
- Split the newly added atlas packet consumption evidence/diff responsibility
  into a focused text-owned header where it makes sense, for example:
  `apps/quiz/quiz-vulkan/src/render/text/text_atlas_packet_consumption_evidence.h`
- Keep existing public include points compatible. If existing users include
  `text_frame_upload_handoff.h`, it may include the new focused header, but the
  new evidence-specific types/helpers should live in the focused file.
- Do not perform arbitrary line-count splitting. Split only by responsibility:
  glyph identity, line/run placement, pen/baseline, upload generation, fallback
  and missing-glyph consumption evidence/diff.
- Keep edits limited to:
  - `apps/quiz/quiz-vulkan/src/render/text/*`
  - `apps/quiz/quiz-vulkan/tests/render/text/*`
- Do not edit top-level `CMakeLists.txt`; if the new header needs FILE_SET
  registration, report the exact integrator-owned line instead of editing it.

Verification:
- Build `quiz_vulkan_interface_contract_compile_tests`.
- Build/run:
  `quiz_vulkan_fake_text_engine_freetype_raster_payload_tests`,
  `quiz_vulkan_font_shaped_atlas_update_tests`,
  `quiz_vulkan_text_renderer_glyph_quad_packet_tests`,
  `quiz_vulkan_text_frame_resource_packet_materialization_tests`.
- Run `git diff --check`.

Commit:
- Commit the split as a second scoped commit on the same branch.
- Final report changed files, verification, whether CMake FILE_SET needs an
  integrator update, risks, and commit hash.
