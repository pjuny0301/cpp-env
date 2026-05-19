#pragma once

#include "render/text/text_frame_upload_handoff_core.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

enum class render_text_frame_resource_packet_materialization_status {
  materialized_uploaded,
  materialized_clean_reuse,
  blocked_missing_upload_handoff,
  blocked_upload_rejected,
  blocked_frame_not_ready,
  blocked_draw_packet,
  blocked_missing_draw_packet,
  blocked_missing_atlas_page,
  blocked_missing_atlas_bounds,
  blocked_missing_page_extent,
  duplicate_packet_id,
};

struct render_text_frame_resource_packet_materialization_entry {
  std::string resource_packet_id{};
  std::string stable_packet_key{};
  std::string frame_id{};
  std::string source_label{};
  std::string draw_packet_id{};
  std::string upload_handoff_id{};
  std::string upload_operation_id{};
  std::string upload_request_id{};
  std::string stable_page_id{};
  std::string sampler_key{};
  std::string sampler_summary{};
  std::size_t packet_index{};
  std::size_t materialization_index{};
  std::size_t run_index{};
  std::size_t cluster_byte_offset{};
  std::size_t cluster_byte_count{};
  glyph_atlas_key cache_key{};
  std::uint32_t resolved_glyph_id{};
  font_face_id resolved_face_id{};
  render_text_atlas_page_id page_id{};
  render_text_revision page_revision{};
  std::size_t page_width{};
  std::size_t page_height{};
  render_rect layout_bounds{};
  render_rect atlas_bounds{};
  render_rect update_bounds{};
  render_text_frame_draw_uv_rect uv_bounds{};
  render_text_frame_draw_packet_status draw_status{
      render_text_frame_draw_packet_status::frame_not_ready};
  render_text_frame_upload_handoff_packet_status handoff_status{
      render_text_frame_upload_handoff_packet_status::blocked_missing_upload_result};
  render_text_glyph_atlas_upload_result_status upload_result_status{
      render_text_glyph_atlas_upload_result_status::rejected_blocked_packet};
  render_text_frame_resource_packet_materialization_status status{
      render_text_frame_resource_packet_materialization_status::
          blocked_missing_upload_handoff};
  bool has_upload_handoff{};
  bool has_draw_packet{true};
  bool ready{};
  bool blocked{true};
  bool renderer_boundary_ready{};
  bool uploaded{};
  bool clean_reuse{};
  bool missing_upload_handoff{};
  bool missing_draw_packet{};
  bool upload_rejected{};
  bool frame_not_ready{};
  bool missing_atlas_page{};
  bool missing_atlas_bounds{};
  bool missing_page_extent{};
  bool duplicate_packet{};
  bool used_deterministic_fallback{};
  bool used_real_backend{};
  bool glyph_supported{};
  bool upload_consumed{};
  std::size_t upload_rgba_bytes{};
  render_text_atlas_packet_consumption_evidence atlas_consumption{};
  std::string blocker_summary{};
  std::string diagnostic{};

  [[nodiscard]] constexpr bool ok() const noexcept {
    return renderer_boundary_ready && !blocked;
  }
};

struct render_text_frame_resource_packet_materialization_page_snapshot {
  std::string stable_page_id{};
  std::string sampler_key{};
  std::string sampler_summary{};
  render_text_atlas_page_id page_id{};
  render_text_revision page_revision{};
  std::size_t page_width{};
  std::size_t page_height{};
  std::size_t packet_count{};
  std::size_t ready_packet_count{};
  std::size_t blocked_packet_count{};
  std::size_t uploaded_packet_count{};
  std::size_t clean_reuse_packet_count{};
  std::size_t duplicate_packet_count{};
  std::size_t upload_rgba_bytes{};
  bool has_uploads{};
  bool has_clean_reuse{};
  bool has_blockers{};
};

struct render_text_frame_resource_packet_materialization_policy_snapshot {
  std::size_t draw_packet_count{};
  std::size_t handoff_packet_count{};
  std::size_t resource_packet_count{};
  std::size_t ready_packet_count{};
  std::size_t blocked_packet_count{};
  std::size_t uploaded_packet_count{};
  std::size_t clean_reuse_packet_count{};
  std::size_t missing_upload_handoff_count{};
  std::size_t rejected_upload_count{};
  std::size_t frame_not_ready_count{};
  std::size_t draw_packet_blocked_count{};
  std::size_t missing_draw_packet_count{};
  std::size_t missing_atlas_page_count{};
  std::size_t missing_atlas_bounds_count{};
  std::size_t missing_page_extent_count{};
  std::size_t duplicate_packet_id_count{};
  std::size_t deterministic_fallback_count{};
  std::size_t real_backend_count{};
  std::size_t consumed_upload_count{};
  std::size_t total_upload_rgba_bytes{};
  std::size_t page_count{};
  std::size_t sampler_count{};
  bool frame_ready_for_renderer{};
  bool has_blockers{};
  bool has_uploads{};
  bool has_clean_reuse{};
  bool used_deterministic_fallback{};
  bool used_real_backend{};
};

struct render_text_frame_resource_packet_materialization_request {
  render_text_frame_draw_plan_snapshot draw_plan{};
  render_text_frame_upload_handoff_snapshot upload_handoff{};
};

struct render_text_frame_resource_packet_materialization {
  std::string frame_id{};
  std::string source_label{};
  render_text_frame_snapshot_status frame_status{
      render_text_frame_snapshot_status::atlas_upload_incomplete};
  bool frame_ready_for_renderer{};
  render_text_frame_resource_packet_materialization_policy_snapshot policy{};
  std::vector<render_text_frame_resource_packet_materialization_entry> entries{};
  std::vector<render_text_frame_resource_packet_materialization_page_snapshot>
      pages{};
  std::vector<std::string> ready_resource_packet_ids{};
  std::vector<std::string> blocker_resource_packet_ids{};
  std::vector<std::string> duplicate_packet_ids{};
  std::string sampler_summary{};
  std::string blocker_summary{};
  std::string diagnostic{};

  [[nodiscard]] constexpr bool ok() const noexcept {
    return frame_ready_for_renderer && !policy.has_blockers;
  }

  [[nodiscard]] constexpr bool has_blockers() const noexcept {
    return policy.has_blockers;
  }
};

[[nodiscard]] inline std::string
render_text_frame_resource_packet_materialization_status_name(
    const render_text_frame_resource_packet_materialization_status status) {
  switch (status) {
    case render_text_frame_resource_packet_materialization_status::
        materialized_uploaded:
      return "materialized_uploaded";
    case render_text_frame_resource_packet_materialization_status::
        materialized_clean_reuse:
      return "materialized_clean_reuse";
    case render_text_frame_resource_packet_materialization_status::
        blocked_missing_upload_handoff:
      return "blocked_missing_upload_handoff";
    case render_text_frame_resource_packet_materialization_status::
        blocked_upload_rejected:
      return "blocked_upload_rejected";
    case render_text_frame_resource_packet_materialization_status::
        blocked_frame_not_ready:
      return "blocked_frame_not_ready";
    case render_text_frame_resource_packet_materialization_status::
        blocked_draw_packet:
      return "blocked_draw_packet";
    case render_text_frame_resource_packet_materialization_status::
        blocked_missing_draw_packet:
      return "blocked_missing_draw_packet";
    case render_text_frame_resource_packet_materialization_status::
        blocked_missing_atlas_page:
      return "blocked_missing_atlas_page";
    case render_text_frame_resource_packet_materialization_status::
        blocked_missing_atlas_bounds:
      return "blocked_missing_atlas_bounds";
    case render_text_frame_resource_packet_materialization_status::
        blocked_missing_page_extent:
      return "blocked_missing_page_extent";
    case render_text_frame_resource_packet_materialization_status::
        duplicate_packet_id:
      return "duplicate_packet_id";
  }
  return "unknown";
}

[[nodiscard]] inline std::string
render_text_frame_resource_packet_sampler_key_for(
    const render_text_atlas_page_id page_id,
    const render_text_revision page_revision) {
  if (page_id == 0U) {
    return {};
  }
  return "text-atlas-sampler:v1:page=" + std::to_string(page_id) +
         ":rev=" + std::to_string(page_revision) + ":filter=linear:wrap=clamp";
}

[[nodiscard]] inline std::string
render_text_frame_resource_packet_sampler_summary_for(
    const render_text_atlas_page_id page_id,
    const render_text_revision page_revision,
    const std::size_t page_width,
    const std::size_t page_height) {
  if (page_id == 0U) {
    return {};
  }
  return "text atlas sampler page " + std::to_string(page_id) + " revision " +
         std::to_string(page_revision) + " extent " +
         std::to_string(page_width) + "x" + std::to_string(page_height) +
         " linear clamp";
}

[[nodiscard]] inline std::string render_text_frame_resource_packet_id_for(
    const std::string& frame_id,
    const std::string& draw_packet_id) {
  return "text-frame-resource-packet:v1:frame=" + frame_id + ":draw=" +
         draw_packet_id;
}

[[nodiscard]] inline std::string
render_text_frame_resource_packet_missing_draw_id_for(
    const std::string& frame_id,
    const std::string& upload_handoff_id) {
  return "text-frame-resource-packet:v1:frame=" + frame_id +
         ":missing-draw:handoff=" + upload_handoff_id;
}

[[nodiscard]] inline const render_text_frame_upload_handoff_packet_snapshot*
render_text_frame_resource_packet_find_handoff_packet(
    const std::vector<render_text_frame_upload_handoff_packet_snapshot>& packets,
    const std::string& draw_packet_id) {
  const auto match = std::find_if(
      packets.begin(), packets.end(),
      [&](const render_text_frame_upload_handoff_packet_snapshot& packet) {
        return packet.draw_packet_id == draw_packet_id;
      });
  return match == packets.end() ? nullptr : &*match;
}

[[nodiscard]] inline render_text_frame_resource_packet_materialization_status
render_text_frame_resource_packet_materialization_status_for(
    const render_text_frame_draw_packet_snapshot* draw_packet,
    const render_text_frame_upload_handoff_packet_snapshot* handoff_packet,
    const bool duplicate_packet) {
  if (duplicate_packet) {
    return render_text_frame_resource_packet_materialization_status::
        duplicate_packet_id;
  }
  if (draw_packet == nullptr) {
    return render_text_frame_resource_packet_materialization_status::
        blocked_missing_draw_packet;
  }
  if (draw_packet->status ==
      render_text_frame_draw_packet_status::frame_not_ready) {
    return render_text_frame_resource_packet_materialization_status::
        blocked_frame_not_ready;
  }
  if (draw_packet->page_id == 0U) {
    return render_text_frame_resource_packet_materialization_status::
        blocked_missing_atlas_page;
  }
  if (!draw_packet->has_atlas_bounds) {
    return render_text_frame_resource_packet_materialization_status::
        blocked_missing_atlas_bounds;
  }
  if (draw_packet->page_width == 0U || draw_packet->page_height == 0U ||
      !draw_packet->uv_bounds.valid) {
    return render_text_frame_resource_packet_materialization_status::
        blocked_missing_page_extent;
  }
  if (!draw_packet->drawable()) {
    return render_text_frame_resource_packet_materialization_status::
        blocked_draw_packet;
  }
  if (handoff_packet == nullptr ||
      handoff_packet->handoff_status ==
          render_text_frame_upload_handoff_packet_status::
              blocked_missing_upload_result) {
    return render_text_frame_resource_packet_materialization_status::
        blocked_missing_upload_handoff;
  }
  if (handoff_packet->handoff_status ==
      render_text_frame_upload_handoff_packet_status::blocked_missing_draw_packet) {
    return render_text_frame_resource_packet_materialization_status::
        blocked_missing_draw_packet;
  }
  if (handoff_packet->handoff_status ==
      render_text_frame_upload_handoff_packet_status::blocked_upload_rejected) {
    return render_text_frame_resource_packet_materialization_status::
        blocked_upload_rejected;
  }
  if (handoff_packet->handoff_status ==
      render_text_frame_upload_handoff_packet_status::ready_clean_reuse) {
    return render_text_frame_resource_packet_materialization_status::
        materialized_clean_reuse;
  }
  if (handoff_packet->handoff_status ==
      render_text_frame_upload_handoff_packet_status::ready_uploaded) {
    return render_text_frame_resource_packet_materialization_status::
        materialized_uploaded;
  }
  return render_text_frame_resource_packet_materialization_status::
      blocked_draw_packet;
}

namespace detail {

[[nodiscard]] inline std::string resource_packet_blocker_summary_for(
    const render_text_frame_draw_packet_snapshot* draw_packet,
    const render_text_frame_upload_handoff_packet_snapshot* handoff_packet,
    const render_text_frame_resource_packet_materialization_status status) {
  switch (status) {
    case render_text_frame_resource_packet_materialization_status::
        materialized_uploaded:
    case render_text_frame_resource_packet_materialization_status::
        materialized_clean_reuse:
      return {};
    case render_text_frame_resource_packet_materialization_status::
        duplicate_packet_id:
      return "draw packet id appears more than once in the text frame draw plan";
    case render_text_frame_resource_packet_materialization_status::
        blocked_missing_draw_packet:
      return "upload handoff packet has no matching text frame draw packet";
    case render_text_frame_resource_packet_materialization_status::
        blocked_frame_not_ready:
      return "text frame is not ready for renderer-boundary resource packets";
    case render_text_frame_resource_packet_materialization_status::
        blocked_missing_atlas_page:
      return "draw packet is missing atlas page identity";
    case render_text_frame_resource_packet_materialization_status::
        blocked_missing_atlas_bounds:
      return "draw packet is missing atlas bounds";
    case render_text_frame_resource_packet_materialization_status::
        blocked_missing_page_extent:
      return "draw packet cannot derive UV coordinates without atlas page extent";
    case render_text_frame_resource_packet_materialization_status::
        blocked_missing_upload_handoff:
      if (handoff_packet != nullptr && !handoff_packet->blocker_reason.empty()) {
        return handoff_packet->blocker_reason;
      }
      return "draw packet has no ready upload handoff evidence";
    case render_text_frame_resource_packet_materialization_status::
        blocked_upload_rejected:
      if (handoff_packet != nullptr && !handoff_packet->blocker_reason.empty()) {
        return handoff_packet->blocker_reason;
      }
      return "upload handoff rejected the draw packet upload";
    case render_text_frame_resource_packet_materialization_status::
        blocked_draw_packet:
      if (draw_packet != nullptr && !draw_packet->diagnostic.empty()) {
        return draw_packet->diagnostic;
      }
      return "draw packet is not renderer-ready";
  }
  return "unknown text resource packet blocker";
}

[[nodiscard]] inline render_text_frame_resource_packet_materialization_page_snapshot*
find_resource_packet_page(
    std::vector<render_text_frame_resource_packet_materialization_page_snapshot>&
        pages,
    const std::string& stable_page_id) {
  const auto match = std::find_if(
      pages.begin(), pages.end(),
      [&](const render_text_frame_resource_packet_materialization_page_snapshot&
              page) { return page.stable_page_id == stable_page_id; });
  return match == pages.end() ? nullptr : &*match;
}

inline void append_resource_packet_materialization_entry(
    render_text_frame_resource_packet_materialization& materialization,
    render_text_frame_resource_packet_materialization_entry entry) {
  auto& policy = materialization.policy;
  ++policy.resource_packet_count;
  if (entry.ready) {
    ++policy.ready_packet_count;
    materialization.ready_resource_packet_ids.push_back(entry.resource_packet_id);
  }
  if (entry.blocked) {
    ++policy.blocked_packet_count;
    materialization.blocker_resource_packet_ids.push_back(entry.resource_packet_id);
  }
  if (entry.uploaded) {
    ++policy.uploaded_packet_count;
    policy.has_uploads = true;
  }
  if (entry.clean_reuse) {
    ++policy.clean_reuse_packet_count;
    policy.has_clean_reuse = true;
  }
  if (entry.missing_upload_handoff) {
    ++policy.missing_upload_handoff_count;
  }
  if (entry.upload_rejected) {
    ++policy.rejected_upload_count;
  }
  if (entry.frame_not_ready) {
    ++policy.frame_not_ready_count;
  }
  if (entry.status ==
      render_text_frame_resource_packet_materialization_status::
          blocked_draw_packet) {
    ++policy.draw_packet_blocked_count;
  }
  if (entry.missing_draw_packet) {
    ++policy.missing_draw_packet_count;
  }
  if (entry.missing_atlas_page) {
    ++policy.missing_atlas_page_count;
  }
  if (entry.missing_atlas_bounds) {
    ++policy.missing_atlas_bounds_count;
  }
  if (entry.missing_page_extent) {
    ++policy.missing_page_extent_count;
  }
  if (entry.duplicate_packet) {
    ++policy.duplicate_packet_id_count;
    detail::append_unique_string(materialization.duplicate_packet_ids,
                                 entry.draw_packet_id);
  }
  if (entry.used_deterministic_fallback) {
    ++policy.deterministic_fallback_count;
    policy.used_deterministic_fallback = true;
  }
  if (entry.used_real_backend) {
    ++policy.real_backend_count;
    policy.used_real_backend = true;
  }
  if (entry.upload_consumed) {
    ++policy.consumed_upload_count;
  }
  policy.total_upload_rgba_bytes += entry.upload_rgba_bytes;
  policy.has_blockers = policy.has_blockers || entry.blocked;

  auto* page = detail::find_resource_packet_page(materialization.pages,
                                                 entry.stable_page_id);
  if (page == nullptr) {
    materialization.pages.push_back(
        {.stable_page_id = entry.stable_page_id,
         .sampler_key = entry.sampler_key,
         .sampler_summary = entry.sampler_summary,
         .page_id = entry.page_id,
         .page_revision = entry.page_revision,
         .page_width = entry.page_width,
         .page_height = entry.page_height});
    page = &materialization.pages.back();
  }
  ++page->packet_count;
  if (entry.ready) {
    ++page->ready_packet_count;
  }
  if (entry.blocked) {
    ++page->blocked_packet_count;
  }
  if (entry.uploaded) {
    ++page->uploaded_packet_count;
    page->has_uploads = true;
  }
  if (entry.clean_reuse) {
    ++page->clean_reuse_packet_count;
    page->has_clean_reuse = true;
  }
  if (entry.duplicate_packet) {
    ++page->duplicate_packet_count;
  }
  page->upload_rgba_bytes += entry.upload_rgba_bytes;
  page->has_blockers = page->has_blockers || entry.blocked;
  materialization.entries.push_back(std::move(entry));
}

[[nodiscard]] inline std::size_t find_handoff_packet_index_for_draw_packet(
    const std::vector<render_text_frame_upload_handoff_packet_snapshot>& packets,
    const render_text_frame_draw_packet_snapshot& draw_packet) {
  for (std::size_t index = 0; index < packets.size(); ++index) {
    const auto& packet = packets[index];
    if (packet.draw_packet_id == draw_packet.packet_id) {
      return index;
    }
    if (!draw_packet.atlas_upload_request_id.empty() &&
        packet.upload_request_id == draw_packet.atlas_upload_request_id) {
      return index;
    }
  }
  return packets.size();
}

}  // namespace detail

[[nodiscard]] inline render_text_frame_resource_packet_materialization_entry
make_render_text_frame_resource_packet_materialization_entry(
    const render_text_frame_draw_packet_snapshot& draw_packet,
    const render_text_frame_upload_handoff_packet_snapshot* handoff_packet,
    const std::size_t packet_index,
    const bool duplicate_packet) {
  const auto status =
      render_text_frame_resource_packet_materialization_status_for(
          &draw_packet, handoff_packet, duplicate_packet);
  const bool ready =
      status == render_text_frame_resource_packet_materialization_status::
                    materialized_uploaded ||
      status == render_text_frame_resource_packet_materialization_status::
                    materialized_clean_reuse;
  const bool uploaded =
      status == render_text_frame_resource_packet_materialization_status::
                    materialized_uploaded;
  const bool clean_reuse =
      status == render_text_frame_resource_packet_materialization_status::
                    materialized_clean_reuse;
  const bool blocked = !ready;
  const bool missing_upload_handoff =
      status == render_text_frame_resource_packet_materialization_status::
                    blocked_missing_upload_handoff;
  const bool upload_rejected =
      status == render_text_frame_resource_packet_materialization_status::
                    blocked_upload_rejected;
  const std::string stable_page_id =
      handoff_packet != nullptr && !handoff_packet->stable_page_id.empty()
          ? handoff_packet->stable_page_id
          : ("page:" + std::to_string(draw_packet.page_id));
  const std::string sampler_key =
      render_text_frame_resource_packet_sampler_key_for(draw_packet.page_id,
                                                        draw_packet.page_revision);
  const std::string sampler_summary =
      render_text_frame_resource_packet_sampler_summary_for(
          draw_packet.page_id, draw_packet.page_revision, draw_packet.page_width,
          draw_packet.page_height);
  const std::string blocker =
      detail::resource_packet_blocker_summary_for(&draw_packet, handoff_packet,
                                                  status);
  render_text_atlas_packet_consumption_evidence atlas_consumption =
      draw_packet.atlas_consumption;
  atlas_consumption.missing_glyph =
      !draw_packet.glyph_supported ||
      (handoff_packet != nullptr && handoff_packet->missing_glyph);
  atlas_consumption.clean_reuse = clean_reuse;
  return {
      .resource_packet_id = render_text_frame_resource_packet_id_for(
          draw_packet.frame_id, draw_packet.packet_id),
      .stable_packet_key = handoff_packet == nullptr
                               ? draw_packet.packet_id
                               : handoff_packet->stable_packet_key,
      .frame_id = draw_packet.frame_id,
      .source_label = draw_packet.source_label,
      .draw_packet_id = draw_packet.packet_id,
      .upload_handoff_id = handoff_packet == nullptr ? std::string{}
                                                     : handoff_packet->handoff_id,
      .upload_operation_id = handoff_packet == nullptr
                                 ? std::string{}
                                 : handoff_packet->upload_operation_id,
      .upload_request_id = handoff_packet == nullptr
                               ? draw_packet.atlas_upload_request_id
                               : handoff_packet->upload_request_id,
      .stable_page_id = stable_page_id,
      .sampler_key = sampler_key,
      .sampler_summary = sampler_summary,
      .packet_index = packet_index,
      .materialization_index = draw_packet.materialization_index,
      .run_index = draw_packet.run_index,
      .cluster_byte_offset = draw_packet.cluster_byte_offset,
      .cluster_byte_count = draw_packet.cluster_byte_count,
      .cache_key = draw_packet.cache_key,
      .resolved_glyph_id = draw_packet.resolved_glyph_id,
      .resolved_face_id = draw_packet.resolved_face_id,
      .page_id = draw_packet.page_id,
      .page_revision = draw_packet.page_revision,
      .page_width = draw_packet.page_width,
      .page_height = draw_packet.page_height,
      .layout_bounds = draw_packet.layout_bounds,
      .atlas_bounds = draw_packet.atlas_bounds,
      .update_bounds = handoff_packet == nullptr ? draw_packet.atlas_bounds
                                                 : handoff_packet->update_bounds,
      .uv_bounds = draw_packet.uv_bounds,
      .draw_status = draw_packet.status,
      .handoff_status =
          handoff_packet == nullptr
              ? render_text_frame_upload_handoff_packet_status::
                    blocked_missing_upload_result
              : handoff_packet->handoff_status,
      .upload_result_status =
          handoff_packet == nullptr
              ? render_text_glyph_atlas_upload_result_status::
                    rejected_missing_upload_request_id
              : handoff_packet->upload_result_status,
      .status = status,
      .has_upload_handoff = handoff_packet != nullptr,
      .ready = ready,
      .blocked = blocked,
      .renderer_boundary_ready = ready,
      .uploaded = uploaded,
      .clean_reuse = clean_reuse,
      .missing_upload_handoff = missing_upload_handoff,
      .upload_rejected = upload_rejected,
      .frame_not_ready =
          status == render_text_frame_resource_packet_materialization_status::
                        blocked_frame_not_ready,
      .missing_atlas_page =
          status == render_text_frame_resource_packet_materialization_status::
                        blocked_missing_atlas_page,
      .missing_atlas_bounds =
          status == render_text_frame_resource_packet_materialization_status::
                        blocked_missing_atlas_bounds,
      .missing_page_extent =
          status == render_text_frame_resource_packet_materialization_status::
                        blocked_missing_page_extent,
      .duplicate_packet = duplicate_packet,
      .used_deterministic_fallback = draw_packet.used_deterministic_fallback,
      .used_real_backend = draw_packet.used_real_backend,
      .glyph_supported = draw_packet.glyph_supported,
      .upload_consumed = draw_packet.upload_consumed,
      .upload_rgba_bytes = uploaded && handoff_packet != nullptr
                               ? handoff_packet->upload_rgba_bytes
                               : 0U,
      .atlas_consumption = atlas_consumption,
      .blocker_summary = blocker,
      .diagnostic =
          ready
              ? "text resource packet materialized for renderer-boundary handoff"
              : blocker,
  };
}

[[nodiscard]] inline render_text_frame_resource_packet_materialization_entry
make_render_text_frame_resource_packet_materialization_missing_draw_entry(
    const render_text_frame_upload_handoff_packet_snapshot& handoff_packet,
    const std::size_t packet_index) {
  const auto status =
      render_text_frame_resource_packet_materialization_status_for(
          nullptr, &handoff_packet, false);
  const std::string sampler_key =
      render_text_frame_resource_packet_sampler_key_for(
          handoff_packet.page_id, handoff_packet.page_revision);
  const std::string sampler_summary =
      render_text_frame_resource_packet_sampler_summary_for(
          handoff_packet.page_id, handoff_packet.page_revision, 0U, 0U);
  const std::string blocker =
      detail::resource_packet_blocker_summary_for(nullptr, &handoff_packet,
                                                  status);
  return {
      .resource_packet_id =
          render_text_frame_resource_packet_missing_draw_id_for(
              handoff_packet.frame_id, handoff_packet.handoff_id),
      .stable_packet_key = handoff_packet.stable_packet_key,
      .frame_id = handoff_packet.frame_id,
      .source_label = handoff_packet.source_label,
      .draw_packet_id = handoff_packet.draw_packet_id,
      .upload_handoff_id = handoff_packet.handoff_id,
      .upload_operation_id = handoff_packet.upload_operation_id,
      .upload_request_id = handoff_packet.upload_request_id,
      .stable_page_id = handoff_packet.stable_page_id,
      .sampler_key = sampler_key,
      .sampler_summary = sampler_summary,
      .packet_index = packet_index,
      .materialization_index = handoff_packet.materialization_index,
      .run_index = handoff_packet.run_index,
      .cluster_byte_offset = handoff_packet.cluster_byte_offset,
      .cluster_byte_count = handoff_packet.cluster_byte_count,
      .cache_key = handoff_packet.cache_key,
      .resolved_glyph_id = handoff_packet.resolved_glyph_id,
      .resolved_face_id = handoff_packet.resolved_face_id,
      .page_id = handoff_packet.page_id,
      .page_revision = handoff_packet.page_revision,
      .layout_bounds = handoff_packet.layout_bounds,
      .atlas_bounds = handoff_packet.atlas_bounds,
      .update_bounds = handoff_packet.update_bounds,
      .draw_status = handoff_packet.draw_status,
      .handoff_status = handoff_packet.handoff_status,
      .upload_result_status = handoff_packet.upload_result_status,
      .status = status,
      .has_upload_handoff = true,
      .has_draw_packet = false,
      .blocked = true,
      .missing_draw_packet = true,
      .used_deterministic_fallback =
          handoff_packet.used_deterministic_fallback,
      .used_real_backend = handoff_packet.used_real_backend,
      .glyph_supported = handoff_packet.glyph_supported,
      .upload_consumed = handoff_packet.upload_consumed,
      .atlas_consumption = handoff_packet.atlas_consumption,
      .blocker_summary = blocker,
      .diagnostic = blocker,
  };
}

[[nodiscard]] inline render_text_frame_resource_packet_materialization
materialize_render_text_frame_resource_packets(
    render_text_frame_resource_packet_materialization_request request) {
  render_text_frame_resource_packet_materialization materialization{
      .frame_id = request.draw_plan.frame_id.empty()
                      ? request.upload_handoff.frame_id
                      : request.draw_plan.frame_id,
      .source_label = request.draw_plan.source_label.empty()
                          ? request.upload_handoff.source_label
                          : request.draw_plan.source_label,
      .frame_status = request.draw_plan.frame_status,
      .frame_ready_for_renderer =
          request.draw_plan.frame_ready_for_renderer &&
          request.upload_handoff.policy.frame_ready_for_renderer,
      .diagnostic =
          "text atlas resource packets materialized from draw plan and upload "
          "handoff evidence",
  };
  materialization.policy.draw_packet_count = request.draw_plan.packets.size();
  materialization.policy.handoff_packet_count =
      request.upload_handoff.packets.size();
  materialization.policy.frame_ready_for_renderer =
      materialization.frame_ready_for_renderer;
  materialization.entries.reserve(request.draw_plan.packets.size() +
                                  request.upload_handoff.packets.size());

  std::vector<bool> matched_handoff_packets(request.upload_handoff.packets.size(),
                                            false);
  std::vector<std::string> seen_draw_packet_ids{};
  seen_draw_packet_ids.reserve(request.draw_plan.packets.size());
  for (std::size_t index = 0; index < request.draw_plan.packets.size();
       ++index) {
    const auto& draw_packet = request.draw_plan.packets[index];
    const bool duplicate_packet =
        std::find(seen_draw_packet_ids.begin(), seen_draw_packet_ids.end(),
                  draw_packet.packet_id) != seen_draw_packet_ids.end();
    if (!duplicate_packet) {
      seen_draw_packet_ids.push_back(draw_packet.packet_id);
    }

    const std::size_t handoff_index =
        detail::find_handoff_packet_index_for_draw_packet(
            request.upload_handoff.packets, draw_packet);
    const render_text_frame_upload_handoff_packet_snapshot* handoff_packet =
        handoff_index < request.upload_handoff.packets.size()
            ? &request.upload_handoff.packets[handoff_index]
            : nullptr;
    if (handoff_index < matched_handoff_packets.size()) {
      matched_handoff_packets[handoff_index] = true;
    }
    detail::append_resource_packet_materialization_entry(
        materialization,
        make_render_text_frame_resource_packet_materialization_entry(
            draw_packet, handoff_packet, index, duplicate_packet));
  }

  for (std::size_t index = 0; index < request.upload_handoff.packets.size();
       ++index) {
    if (matched_handoff_packets[index]) {
      continue;
    }
    detail::append_resource_packet_materialization_entry(
        materialization,
        make_render_text_frame_resource_packet_materialization_missing_draw_entry(
            request.upload_handoff.packets[index],
            request.draw_plan.packets.size() + index));
  }

  materialization.policy.page_count = materialization.pages.size();
  for (const auto& page : materialization.pages) {
    if (!page.sampler_key.empty()) {
      ++materialization.policy.sampler_count;
    }
  }
  if (!materialization.policy.frame_ready_for_renderer &&
      materialization.entries.empty()) {
    materialization.policy.has_blockers = true;
    materialization.blocker_summary =
        "text frame is not ready for renderer-boundary resource packets";
  } else if (materialization.policy.has_blockers) {
    materialization.blocker_summary =
        std::to_string(materialization.policy.blocked_packet_count) +
        " text resource packet(s) blocked";
  }
  materialization.sampler_summary =
      std::to_string(materialization.policy.sampler_count) +
      " text atlas sampler(s) referenced";
  return materialization;
}

}  // namespace quiz_vulkan::render
