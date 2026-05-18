#pragma once

#include "render/text/font_glyph_atlas.h"

#include <cstddef>
#include <cstdint>

namespace quiz_vulkan::render {

struct render_text_atlas_packet_consumption_evidence {
  std::size_t cluster_index{};
  std::size_t line_index{};
  std::size_t run_index{};
  std::size_t cluster_byte_offset{};
  std::size_t cluster_byte_count{};
  glyph_atlas_key cache_key{};
  std::uint32_t resolved_glyph_id{};
  font_face_id resolved_face_id{};
  float pen_x{};
  float pen_y{};
  float baseline{};
  render_text_revision upload_generation{};
  bool missing_glyph{};
  bool used_fallback_glyph_id{};
  bool clean_reuse{};
};

struct render_text_atlas_packet_consumption_evidence_diff {
  bool glyph_identity_changed{};
  bool line_run_changed{};
  bool pen_position_changed{};
  bool baseline_changed{};
  bool upload_generation_changed{};
  bool reuse_status_changed{};
  bool fallback_glyph_changed{};
  bool missing_glyph_changed{};

  [[nodiscard]] constexpr bool has_changes() const noexcept {
    return glyph_identity_changed || line_run_changed || pen_position_changed ||
           baseline_changed || upload_generation_changed ||
           reuse_status_changed || fallback_glyph_changed ||
           missing_glyph_changed;
  }
};

struct render_text_atlas_packet_consumption_evidence_diff_policy {
  std::size_t glyph_identity_changed_count{};
  std::size_t line_run_changed_count{};
  std::size_t pen_position_changed_count{};
  std::size_t baseline_changed_count{};
  std::size_t upload_generation_changed_count{};
  std::size_t reuse_status_changed_count{};
  std::size_t fallback_glyph_changed_count{};
  std::size_t missing_glyph_changed_count{};
};

[[nodiscard]] constexpr render_text_atlas_packet_consumption_evidence_diff
diff_render_text_atlas_packet_consumption_evidence(
    const render_text_atlas_packet_consumption_evidence& before,
    const render_text_atlas_packet_consumption_evidence& after) noexcept {
  return {
      .glyph_identity_changed =
          before.resolved_glyph_id != after.resolved_glyph_id ||
          before.resolved_face_id != after.resolved_face_id ||
          before.cache_key != after.cache_key,
      .line_run_changed =
          before.line_index != after.line_index ||
          before.run_index != after.run_index ||
          before.cluster_index != after.cluster_index ||
          before.cluster_byte_offset != after.cluster_byte_offset ||
          before.cluster_byte_count != after.cluster_byte_count,
      .pen_position_changed =
          before.pen_x != after.pen_x || before.pen_y != after.pen_y,
      .baseline_changed = before.baseline != after.baseline,
      .upload_generation_changed =
          before.upload_generation != after.upload_generation,
      .reuse_status_changed = before.clean_reuse != after.clean_reuse,
      .fallback_glyph_changed =
          before.used_fallback_glyph_id != after.used_fallback_glyph_id,
      .missing_glyph_changed = before.missing_glyph != after.missing_glyph,
  };
}

inline void count_render_text_atlas_packet_consumption_evidence_diff(
    render_text_atlas_packet_consumption_evidence_diff_policy& policy,
    const render_text_atlas_packet_consumption_evidence_diff& diff) {
  if (diff.glyph_identity_changed) {
    ++policy.glyph_identity_changed_count;
  }
  if (diff.line_run_changed) {
    ++policy.line_run_changed_count;
  }
  if (diff.pen_position_changed) {
    ++policy.pen_position_changed_count;
  }
  if (diff.baseline_changed) {
    ++policy.baseline_changed_count;
  }
  if (diff.upload_generation_changed) {
    ++policy.upload_generation_changed_count;
  }
  if (diff.reuse_status_changed) {
    ++policy.reuse_status_changed_count;
  }
  if (diff.fallback_glyph_changed) {
    ++policy.fallback_glyph_changed_count;
  }
  if (diff.missing_glyph_changed) {
    ++policy.missing_glyph_changed_count;
  }
}

}  // namespace quiz_vulkan::render
