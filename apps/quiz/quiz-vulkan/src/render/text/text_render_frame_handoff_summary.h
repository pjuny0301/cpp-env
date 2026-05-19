#pragma once

#include "render/text/text_frame_upload_handoff.h"

namespace quiz_vulkan::render {

struct render_text_render_frame_handoff_summary_policy {
  std::size_t draw_packet_count{};
  std::size_t draw_ready_count{};
  std::size_t draw_blocked_count{};
  std::size_t draw_skipped_count{};
  std::size_t glyph_quad_count{};
  std::size_t glyph_quad_ready_count{};
  std::size_t glyph_quad_blocked_count{};
  std::size_t atlas_page_count{};
  std::size_t upload_request_count{};
  std::size_t added_packet_count{};
  std::size_t removed_packet_count{};
  std::size_t changed_packet_count{};
  std::size_t missing_atlas_upload_blocker_count{};
  std::size_t missing_materialization_blocker_count{};
  std::size_t missing_glyph_quad_packet_count{};
  std::size_t non_upload_ready_atlas_payload_blocker_count{};
  std::size_t skipped_fallback_packet_count{};
  std::size_t deterministic_fallback_count{};
  std::size_t real_backend_count{};
  std::size_t total_upload_rgba_bytes{};
  bool frame_ready_for_renderer{};
  bool has_blockers{};
  bool used_deterministic_fallback{};
  bool used_real_backend{};
  bool stable_no_change{};
};

struct render_text_render_frame_handoff_summary_request {
  render_text_frame_snapshot frame{};
  render_text_frame_draw_plan_snapshot draw_plan{};
  render_text_renderer_glyph_quad_packet_snapshot glyph_quads{};
  render_text_frame_draw_plan_diff draw_packet_diff{};
  bool has_draw_packet_diff{};
  render_text_renderer_glyph_quad_packet_diff_snapshot glyph_quad_diff{};
  bool has_glyph_quad_diff{};
};

struct render_text_render_frame_handoff_summary_snapshot {
  std::string frame_id{};
  std::string source_label{};
  render_text_frame_snapshot_status frame_status{
      render_text_frame_snapshot_status::pending_atlas_updates};
  render_text_render_frame_handoff_summary_policy policy{};
  std::vector<std::string> atlas_page_ids{};
  std::vector<std::string> upload_request_ids{};
  std::vector<std::string> ready_draw_packet_ids{};
  std::vector<std::string> blocked_draw_packet_ids{};
  std::vector<std::string> ready_glyph_quad_packet_ids{};
  std::vector<std::string> blocked_glyph_quad_packet_ids{};
  std::vector<std::string> added_packet_ids{};
  std::vector<std::string> removed_packet_ids{};
  std::vector<std::string> changed_packet_ids{};
  std::vector<std::string> missing_glyph_quad_draw_packet_ids{};
  std::vector<std::string> blocker_packet_ids{};
  std::string summary{};
  std::string diagnostic{};

  [[nodiscard]] constexpr bool ok() const noexcept {
    return policy.frame_ready_for_renderer && !policy.has_blockers;
  }

  [[nodiscard]] constexpr bool has_blockers() const noexcept {
    return policy.has_blockers;
  }
};

[[nodiscard]] inline std::string render_text_render_frame_handoff_page_id_for(
    const render_text_atlas_page_id page_id,
    const render_text_revision page_revision) {
  if (page_id == 0U) {
    return {};
  }
  return "text-atlas-page:v1:page=" + std::to_string(page_id) +
         ":rev=" + std::to_string(page_revision);
}

[[nodiscard]] inline const render_text_renderer_glyph_quad_packet_record*
find_render_text_renderer_glyph_quad_packet_for_draw_packet(
    const render_text_renderer_glyph_quad_packet_snapshot& glyph_quads,
    const std::string& draw_packet_id) {
  if (draw_packet_id.empty()) {
    return nullptr;
  }
  const auto match = std::find_if(
      glyph_quads.packets.begin(), glyph_quads.packets.end(),
      [&](const render_text_renderer_glyph_quad_packet_record& packet) {
        return packet.draw_packet_id == draw_packet_id;
      });
  return match == glyph_quads.packets.end() ? nullptr : &*match;
}

inline void append_render_text_render_frame_handoff_summary_atlas_page(
    render_text_render_frame_handoff_summary_snapshot& summary,
    const render_text_atlas_page_id page_id,
    const render_text_revision page_revision) {
  detail::append_unique_string(
      summary.atlas_page_ids,
      render_text_render_frame_handoff_page_id_for(page_id, page_revision));
}

inline void append_render_text_render_frame_handoff_summary_upload_request(
    render_text_render_frame_handoff_summary_snapshot& summary,
    const std::string& upload_request_id) {
  detail::append_unique_string(summary.upload_request_ids, upload_request_id);
}

inline void append_render_text_render_frame_handoff_summary_blocker(
    render_text_render_frame_handoff_summary_snapshot& summary,
    const std::string& packet_id) {
  detail::append_unique_string(summary.blocker_packet_ids, packet_id);
  summary.policy.has_blockers = true;
}

[[nodiscard]] inline render_text_render_frame_handoff_summary_snapshot
make_render_text_render_frame_handoff_summary(
    render_text_render_frame_handoff_summary_request request) {
  render_text_render_frame_handoff_summary_snapshot summary{
      .frame_id = !request.frame.frame_id.empty()
          ? request.frame.frame_id
          : (!request.draw_plan.frame_id.empty() ? request.draw_plan.frame_id
                                                 : request.glyph_quads.frame_id),
      .source_label = !request.frame.source_label.empty()
          ? request.frame.source_label
          : (!request.draw_plan.source_label.empty() ? request.draw_plan.source_label
                                                     : request.glyph_quads.source_label),
      .frame_status = request.frame.status,
  };
  summary.policy.frame_ready_for_renderer =
      request.frame.ready_for_renderer() ||
      (request.draw_plan.frame_ready_for_renderer &&
       request.glyph_quads.frame_ready_for_renderer);
  summary.policy.draw_packet_count = request.draw_plan.policy.packet_count;
  summary.policy.draw_ready_count = request.draw_plan.policy.draw_ready_count;
  summary.policy.draw_blocked_count =
      request.draw_plan.policy.packet_count >= request.draw_plan.policy.draw_ready_count
          ? request.draw_plan.policy.packet_count -
                request.draw_plan.policy.draw_ready_count
          : 0U;
  summary.policy.draw_skipped_count = request.draw_plan.policy.skipped_count;
  summary.policy.glyph_quad_count = request.glyph_quads.policy.quad_packet_count;
  summary.policy.glyph_quad_ready_count =
      request.glyph_quads.policy.ready_quad_count;
  summary.policy.glyph_quad_blocked_count =
      request.glyph_quads.policy.blocked_quad_count;
  summary.policy.total_upload_rgba_bytes =
      request.glyph_quads.policy.total_upload_rgba_bytes;
  if (summary.policy.total_upload_rgba_bytes == 0U) {
    summary.policy.total_upload_rgba_bytes =
        request.frame.policy.total_upload_rgba_bytes;
  }

  std::size_t frame_upload_rgba_bytes = 0U;
  for (const render_text_frame_atlas_upload_snapshot& upload :
       request.frame.atlas_uploads) {
    append_render_text_render_frame_handoff_summary_upload_request(
        summary, upload.request_id);
    append_render_text_render_frame_handoff_summary_atlas_page(
        summary, upload.page.id, upload.page.revision);
    frame_upload_rgba_bytes += upload.upload_rgba_bytes;
  }
  if (summary.policy.total_upload_rgba_bytes == 0U) {
    summary.policy.total_upload_rgba_bytes = frame_upload_rgba_bytes;
  }

  for (const render_text_frame_draw_packet_snapshot& packet :
       request.draw_plan.packets) {
    append_render_text_render_frame_handoff_summary_upload_request(
        summary, packet.atlas_upload_request_id);
    append_render_text_render_frame_handoff_summary_atlas_page(
        summary, packet.page_id, packet.page_revision);
    if (packet.drawable()) {
      detail::append_unique_string(summary.ready_draw_packet_ids,
                                   packet.packet_id);
    } else {
      detail::append_unique_string(summary.blocked_draw_packet_ids,
                                   packet.packet_id);
      append_render_text_render_frame_handoff_summary_blocker(
          summary, packet.packet_id);
    }
    if (render_text_frame_draw_packet_missing_atlas_upload(packet)) {
      ++summary.policy.missing_atlas_upload_blocker_count;
    }
    if (render_text_frame_draw_packet_missing_materialization(packet)) {
      ++summary.policy.missing_materialization_blocker_count;
    }
    if (render_text_frame_draw_packet_non_upload_ready_payload(packet)) {
      ++summary.policy.non_upload_ready_atlas_payload_blocker_count;
    }
    if (render_text_frame_draw_packet_fallback_or_skipped(packet)) {
      ++summary.policy.skipped_fallback_packet_count;
    }
    if (packet.used_deterministic_fallback) {
      ++summary.policy.deterministic_fallback_count;
    }
    if (packet.drawable() && packet.used_real_backend) {
      ++summary.policy.real_backend_count;
    }

    const render_text_renderer_glyph_quad_packet_record* glyph_quad =
        find_render_text_renderer_glyph_quad_packet_for_draw_packet(
            request.glyph_quads, packet.packet_id);
    if (packet.drawable() && glyph_quad == nullptr) {
      ++summary.policy.missing_glyph_quad_packet_count;
      detail::append_unique_string(summary.missing_glyph_quad_draw_packet_ids,
                                   packet.packet_id);
      append_render_text_render_frame_handoff_summary_blocker(
          summary, packet.packet_id);
    }
  }

  for (const render_text_renderer_glyph_quad_packet_record& packet :
       request.glyph_quads.packets) {
    append_render_text_render_frame_handoff_summary_upload_request(
        summary, packet.upload_request_id);
    append_render_text_render_frame_handoff_summary_atlas_page(
        summary, packet.page_id, packet.page_revision);
    if (packet.drawable()) {
      detail::append_unique_string(summary.ready_glyph_quad_packet_ids,
                                   packet.quad_packet_id);
    } else {
      detail::append_unique_string(summary.blocked_glyph_quad_packet_ids,
                                   packet.quad_packet_id);
      append_render_text_render_frame_handoff_summary_blocker(
          summary, packet.quad_packet_id);
    }
    const bool blocked_non_upload_ready_payload =
        packet.blocked && !packet.uploaded && !packet.clean_reuse;
    if (blocked_non_upload_ready_payload) {
      ++summary.policy.non_upload_ready_atlas_payload_blocker_count;
    }
    if (blocked_non_upload_ready_payload ||
        packet.resource_status ==
            render_text_frame_resource_packet_materialization_status::
                blocked_missing_upload_handoff ||
        packet.resource_status ==
            render_text_frame_resource_packet_materialization_status::
                blocked_draw_packet) {
      ++summary.policy.missing_materialization_blocker_count;
    }
    if (blocked_non_upload_ready_payload) {
      ++summary.policy.skipped_fallback_packet_count;
    }
    if (packet.used_deterministic_fallback) {
      ++summary.policy.deterministic_fallback_count;
    }
    if (packet.drawable() && packet.used_real_backend) {
      ++summary.policy.real_backend_count;
    }
  }

  if (request.has_draw_packet_diff) {
    summary.policy.added_packet_count +=
        request.draw_packet_diff.policy.added_packet_count;
    summary.policy.removed_packet_count +=
        request.draw_packet_diff.policy.removed_packet_count;
    summary.policy.changed_packet_count +=
        request.draw_packet_diff.policy.changed_packet_count;
    for (const std::string& id : request.draw_packet_diff.added_packet_ids) {
      detail::append_unique_string(summary.added_packet_ids, id);
    }
    for (const std::string& id : request.draw_packet_diff.removed_packet_ids) {
      detail::append_unique_string(summary.removed_packet_ids, id);
    }
    for (const std::string& id : request.draw_packet_diff.changed_packet_ids) {
      detail::append_unique_string(summary.changed_packet_ids, id);
    }
  }
  if (request.has_glyph_quad_diff) {
    summary.policy.added_packet_count +=
        request.glyph_quad_diff.policy.added_packet_count;
    summary.policy.removed_packet_count +=
        request.glyph_quad_diff.policy.removed_packet_count;
    summary.policy.changed_packet_count +=
        request.glyph_quad_diff.policy.changed_packet_count;
    for (const std::string& id : request.glyph_quad_diff.added_quad_packet_ids) {
      detail::append_unique_string(summary.added_packet_ids, id);
    }
    for (const std::string& id : request.glyph_quad_diff.removed_quad_packet_ids) {
      detail::append_unique_string(summary.removed_packet_ids, id);
    }
    for (const std::string& id : request.glyph_quad_diff.changed_quad_packet_ids) {
      detail::append_unique_string(summary.changed_packet_ids, id);
    }
  }

  summary.policy.atlas_page_count = summary.atlas_page_ids.size();
  summary.policy.upload_request_count = summary.upload_request_ids.size();
  summary.policy.has_blockers =
      summary.policy.has_blockers || request.glyph_quads.policy.has_blockers ||
      summary.policy.draw_blocked_count > 0U ||
      summary.policy.missing_atlas_upload_blocker_count > 0U ||
      summary.policy.missing_materialization_blocker_count > 0U ||
      summary.policy.missing_glyph_quad_packet_count > 0U ||
      summary.policy.non_upload_ready_atlas_payload_blocker_count > 0U;
  summary.policy.used_deterministic_fallback =
      summary.policy.used_deterministic_fallback ||
      summary.policy.deterministic_fallback_count > 0U ||
      summary.policy.skipped_fallback_packet_count > 0U;
  summary.policy.used_real_backend = summary.policy.real_backend_count > 0U;
  summary.policy.stable_no_change =
      (!request.has_draw_packet_diff ||
       request.draw_packet_diff.stable_no_change()) &&
      (!request.has_glyph_quad_diff ||
       request.glyph_quad_diff.stable_no_change()) &&
      summary.policy.added_packet_count == 0U &&
      summary.policy.removed_packet_count == 0U &&
      summary.policy.changed_packet_count == 0U;
  summary.summary = summary.policy.has_blockers
      ? std::to_string(summary.policy.draw_blocked_count) +
            " blocked draw packet(s), " +
            std::to_string(summary.policy.glyph_quad_blocked_count) +
            " blocked glyph quad packet(s)"
      : std::to_string(summary.policy.glyph_quad_ready_count) +
            " renderer-ready text glyph quad packet(s)";
  summary.diagnostic = summary.ok()
      ? "text render frame handoff summary is ready for renderer consumption"
      : "text render frame handoff summary has renderer consumption blockers";
  return summary;
}

}  // namespace quiz_vulkan::render
