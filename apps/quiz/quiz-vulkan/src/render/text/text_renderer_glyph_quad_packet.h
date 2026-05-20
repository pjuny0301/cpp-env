#pragma once

#include "render/text/text_frame_resource_packet_materialization.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

enum class render_text_renderer_glyph_quad_packet_status {
  quad_ready,
  blocked_resource_packet,
  blocked_missing_resource_packet_id,
  blocked_missing_stable_packet_key,
  duplicate_packet_id,
  blocked_missing_atlas_page,
  blocked_missing_atlas_bounds,
  blocked_missing_page_extent,
  blocked_missing_layout_bounds,
  blocked_missing_uv_bounds,
  blocked_missing_sampler_key,
};

struct render_text_renderer_glyph_quad_packet_record {
  std::string quad_packet_id{};
  std::string resource_packet_id{};
  std::string stable_packet_key{};
  std::string source_label{};
  render_node_id source_node_id_hint{};
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
  render_text_frame_draw_uv_rect uv_bounds{};
  render_text_frame_resource_packet_materialization_status resource_status{
      render_text_frame_resource_packet_materialization_status::
          blocked_missing_upload_handoff};
  render_text_renderer_glyph_quad_packet_status status{
      render_text_renderer_glyph_quad_packet_status::blocked_resource_packet};
  bool ready{};
  bool blocked{true};
  bool renderer_boundary_ready{};
  bool duplicate_packet{};
  bool missing_identity{};
  bool missing_stable_packet_key{};
  bool uploaded{};
  bool clean_reuse{};
  bool used_deterministic_fallback{};
  bool used_real_backend{};
  bool glyph_supported{};
  bool upload_consumed{};
  std::size_t upload_rgba_bytes{};
  render_text_atlas_packet_consumption_evidence atlas_consumption{};
  std::string blocker_summary{};
  std::string diagnostic{};

  [[nodiscard]] constexpr bool drawable() const noexcept {
    return ready && !blocked;
  }
};

struct render_text_renderer_glyph_quad_packet_policy_snapshot {
  std::size_t resource_packet_count{};
  std::size_t quad_packet_count{};
  std::size_t ready_quad_count{};
  std::size_t blocked_quad_count{};
  std::size_t uploaded_quad_count{};
  std::size_t clean_reuse_quad_count{};
  std::size_t duplicate_packet_id_count{};
  std::size_t missing_identity_count{};
  std::size_t missing_stable_packet_key_count{};
  std::size_t resource_blocked_count{};
  std::size_t missing_atlas_page_count{};
  std::size_t missing_atlas_bounds_count{};
  std::size_t missing_page_extent_count{};
  std::size_t missing_layout_bounds_count{};
  std::size_t missing_uv_bounds_count{};
  std::size_t missing_sampler_key_count{};
  std::size_t missing_glyph_count{};
  std::size_t fallback_glyph_count{};
  std::size_t deterministic_fallback_count{};
  std::size_t real_backend_count{};
  std::size_t consumed_upload_count{};
  std::size_t total_upload_rgba_bytes{};
  bool frame_ready_for_renderer{};
  bool has_blockers{};
  bool used_deterministic_fallback{};
  bool used_real_backend{};
};

struct render_text_renderer_glyph_quad_packet_request {
  render_text_frame_resource_packet_materialization resource_packets{};
};

struct render_text_renderer_glyph_quad_packet_snapshot {
  std::string frame_id{};
  std::string source_label{};
  bool frame_ready_for_renderer{};
  render_text_renderer_glyph_quad_packet_policy_snapshot policy{};
  std::vector<render_text_renderer_glyph_quad_packet_record> packets{};
  std::vector<std::string> ready_quad_packet_ids{};
  std::vector<std::string> blocker_quad_packet_ids{};
  std::vector<std::string> duplicate_quad_packet_ids{};
  std::vector<std::string> missing_identity_quad_packet_ids{};
  std::string blocker_summary{};
  std::string diagnostic{};

  [[nodiscard]] constexpr bool ok() const noexcept {
    return frame_ready_for_renderer && !policy.has_blockers;
  }

  [[nodiscard]] constexpr bool has_blockers() const noexcept {
    return policy.has_blockers;
  }
};

enum class render_text_renderer_draw_payload_status {
  payload_ready,
  blocked_glyph_quad,
  blocked_missing_payload_id,
};

struct render_text_renderer_draw_payload_record {
  std::string payload_id{};
  std::string quad_packet_id{};
  std::string resource_packet_id{};
  std::string stable_packet_key{};
  std::string source_label{};
  render_node_id source_node_id_hint{};
  std::string draw_packet_id{};
  std::string upload_handoff_id{};
  std::string upload_operation_id{};
  std::string upload_request_id{};
  std::string stable_page_id{};
  std::string sampler_key{};
  std::string sampler_summary{};
  std::size_t payload_index{};
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
  render_rect quad_bounds{};
  render_rect atlas_bounds{};
  render_text_frame_draw_uv_rect uv_bounds{};
  render_text_renderer_glyph_quad_packet_status quad_status{
      render_text_renderer_glyph_quad_packet_status::blocked_resource_packet};
  render_text_renderer_draw_payload_status status{
      render_text_renderer_draw_payload_status::blocked_glyph_quad};
  bool ready{};
  bool blocked{true};
  bool renderer_boundary_ready{};
  bool uploaded{};
  bool clean_reuse{};
  bool used_deterministic_fallback{};
  bool used_real_backend{};
  bool glyph_supported{};
  bool upload_consumed{};
  std::size_t upload_rgba_bytes{};
  render_text_atlas_packet_consumption_evidence atlas_consumption{};
  std::string blocker_summary{};
  std::string diagnostic{};

  [[nodiscard]] constexpr bool drawable() const noexcept {
    return ready && !blocked;
  }
};

struct render_text_renderer_draw_payload_policy_snapshot {
  std::size_t glyph_quad_packet_count{};
  std::size_t payload_count{};
  std::size_t ready_payload_count{};
  std::size_t blocked_payload_count{};
  std::size_t uploaded_payload_count{};
  std::size_t clean_reuse_payload_count{};
  std::size_t missing_payload_id_count{};
  std::size_t glyph_quad_blocked_count{};
  std::size_t missing_glyph_count{};
  std::size_t fallback_glyph_count{};
  std::size_t deterministic_fallback_count{};
  std::size_t real_backend_count{};
  std::size_t consumed_upload_count{};
  std::size_t total_upload_rgba_bytes{};
  bool frame_ready_for_renderer{};
  bool has_blockers{};
  bool used_deterministic_fallback{};
  bool used_real_backend{};
};

struct render_text_renderer_draw_payload_request {
  render_text_renderer_glyph_quad_packet_snapshot glyph_quads{};
};

struct render_text_renderer_draw_payload_snapshot {
  std::string frame_id{};
  std::string source_label{};
  bool frame_ready_for_renderer{};
  render_text_renderer_draw_payload_policy_snapshot policy{};
  std::vector<render_text_renderer_draw_payload_record> payloads{};
  std::vector<std::string> ready_payload_ids{};
  std::vector<std::string> blocker_payload_ids{};
  std::vector<std::string> missing_payload_ids{};
  std::string blocker_summary{};
  std::string diagnostic{};

  [[nodiscard]] constexpr bool ok() const noexcept {
    return frame_ready_for_renderer && !policy.has_blockers;
  }

  [[nodiscard]] constexpr bool has_blockers() const noexcept {
    return policy.has_blockers;
  }
};

struct render_text_renderer_glyph_quad_packet_diff_policy {
  std::ptrdiff_t quad_packet_count_delta{};
  std::ptrdiff_t ready_quad_count_delta{};
  std::ptrdiff_t blocked_quad_count_delta{};
  std::ptrdiff_t uploaded_quad_count_delta{};
  std::ptrdiff_t clean_reuse_quad_count_delta{};
  std::ptrdiff_t total_upload_rgba_bytes_delta{};
  std::size_t added_packet_count{};
  std::size_t removed_packet_count{};
  std::size_t changed_packet_count{};
  std::size_t unchanged_packet_count{};
  std::size_t readiness_regression_count{};
  std::size_t readiness_recovery_count{};
  std::size_t layout_bounds_changed_count{};
  std::size_t atlas_bounds_changed_count{};
  std::size_t uv_bounds_changed_count{};
  render_text_atlas_packet_consumption_evidence_diff_policy
      atlas_consumption_diff{};
  std::size_t page_revision_changed_count{};
  std::size_t sampler_key_changed_count{};
  std::size_t upload_request_id_changed_count{};
  std::size_t upload_operation_id_changed_count{};
  std::size_t uploaded_byte_count_changed_count{};
  std::size_t readiness_changed_count{};
  std::size_t status_changed_count{};
  std::size_t duplicate_identity_count{};
  std::size_t missing_identity_count{};
  bool frame_id_changed{};
  bool frame_ready_changed{};
  bool blockers_changed{};
  bool stable_no_change{};
};

struct render_text_renderer_glyph_quad_packet_diff_record {
  std::string quad_packet_id{};
  std::string previous_resource_packet_id{};
  std::string current_resource_packet_id{};
  std::string previous_sampler_key{};
  std::string current_sampler_key{};
  std::string previous_upload_request_id{};
  std::string current_upload_request_id{};
  std::string previous_upload_operation_id{};
  std::string current_upload_operation_id{};
  bool added{};
  bool removed{};
  bool changed{};
  bool unchanged{};
  bool missing_identity{};
  bool duplicate_identity{};
  bool layout_bounds_changed{};
  bool atlas_bounds_changed{};
  bool uv_bounds_changed{};
  render_text_atlas_packet_consumption_evidence_diff atlas_consumption_diff{};
  bool page_revision_changed{};
  bool sampler_key_changed{};
  bool upload_request_id_changed{};
  bool upload_operation_id_changed{};
  bool uploaded_byte_count_changed{};
  bool readiness_changed{};
  bool readiness_regressed{};
  bool readiness_recovered{};
  bool status_changed{};
  std::ptrdiff_t upload_rgba_bytes_delta{};
  bool previous_ready{};
  bool current_ready{};
  render_text_atlas_page_id previous_page_id{};
  render_text_atlas_page_id current_page_id{};
  render_text_revision previous_page_revision{};
  render_text_revision current_page_revision{};
  render_text_renderer_glyph_quad_packet_status previous_status{
      render_text_renderer_glyph_quad_packet_status::blocked_resource_packet};
  render_text_renderer_glyph_quad_packet_status current_status{
      render_text_renderer_glyph_quad_packet_status::blocked_resource_packet};
};

struct render_text_renderer_glyph_quad_packet_diff_snapshot {
  std::string previous_frame_id{};
  std::string current_frame_id{};
  bool previous_ready_for_renderer{};
  bool current_ready_for_renderer{};
  render_text_renderer_glyph_quad_packet_diff_policy policy{};
  std::vector<render_text_renderer_glyph_quad_packet_diff_record> packet_diffs{};
  std::vector<std::string> added_quad_packet_ids{};
  std::vector<std::string> removed_quad_packet_ids{};
  std::vector<std::string> changed_quad_packet_ids{};
  std::vector<std::string> unchanged_quad_packet_ids{};
  std::vector<std::string> readiness_regressed_quad_packet_ids{};
  std::vector<std::string> readiness_recovered_quad_packet_ids{};
  std::vector<std::string> duplicate_identity_quad_packet_ids{};
  std::vector<std::string> missing_identity_quad_packet_ids{};
  std::string summary{};

  [[nodiscard]] constexpr bool has_changes() const noexcept {
    return !policy.stable_no_change;
  }

  [[nodiscard]] constexpr bool stable_no_change() const noexcept {
    return policy.stable_no_change;
  }
};

[[nodiscard]] inline std::string render_text_renderer_glyph_quad_packet_status_name(
    const render_text_renderer_glyph_quad_packet_status status) {
  switch (status) {
    case render_text_renderer_glyph_quad_packet_status::quad_ready:
      return "quad_ready";
    case render_text_renderer_glyph_quad_packet_status::blocked_resource_packet:
      return "blocked_resource_packet";
    case render_text_renderer_glyph_quad_packet_status::
        blocked_missing_resource_packet_id:
      return "blocked_missing_resource_packet_id";
    case render_text_renderer_glyph_quad_packet_status::
        blocked_missing_stable_packet_key:
      return "blocked_missing_stable_packet_key";
    case render_text_renderer_glyph_quad_packet_status::duplicate_packet_id:
      return "duplicate_packet_id";
    case render_text_renderer_glyph_quad_packet_status::
        blocked_missing_atlas_page:
      return "blocked_missing_atlas_page";
    case render_text_renderer_glyph_quad_packet_status::
        blocked_missing_atlas_bounds:
      return "blocked_missing_atlas_bounds";
    case render_text_renderer_glyph_quad_packet_status::
        blocked_missing_page_extent:
      return "blocked_missing_page_extent";
    case render_text_renderer_glyph_quad_packet_status::
        blocked_missing_layout_bounds:
      return "blocked_missing_layout_bounds";
    case render_text_renderer_glyph_quad_packet_status::
        blocked_missing_uv_bounds:
      return "blocked_missing_uv_bounds";
    case render_text_renderer_glyph_quad_packet_status::
        blocked_missing_sampler_key:
      return "blocked_missing_sampler_key";
  }
  return "unknown";
}

[[nodiscard]] inline render_node_id render_text_renderer_glyph_quad_node_id_hint_for(
    const std::string& source_label) {
  const std::string marker = ":node=";
  const std::size_t offset = source_label.find(marker);
  if (offset == std::string::npos) {
    return {};
  }
  return source_label.substr(offset + marker.size());
}

[[nodiscard]] inline std::string render_text_renderer_glyph_quad_packet_id_for(
    const render_text_frame_resource_packet_materialization_entry& entry,
    const std::size_t packet_index) {
  if (!entry.resource_packet_id.empty()) {
    return "text-renderer-glyph-quad:v1:resource=" + entry.resource_packet_id;
  }
  if (!entry.stable_packet_key.empty()) {
    return "text-renderer-glyph-quad:v1:stable=" + entry.stable_packet_key;
  }
  return "text-renderer-glyph-quad:v1:frame=" + entry.frame_id +
         ":missing-resource-id:index=" + std::to_string(packet_index);
}

[[nodiscard]] inline render_text_renderer_glyph_quad_packet_status
render_text_renderer_glyph_quad_packet_status_for(
    const render_text_frame_resource_packet_materialization_entry& entry,
    const bool duplicate_packet_id) {
  if (duplicate_packet_id) {
    return render_text_renderer_glyph_quad_packet_status::duplicate_packet_id;
  }
  if (entry.resource_packet_id.empty()) {
    return render_text_renderer_glyph_quad_packet_status::
        blocked_missing_resource_packet_id;
  }
  if (entry.stable_packet_key.empty()) {
    return render_text_renderer_glyph_quad_packet_status::
        blocked_missing_stable_packet_key;
  }
  if (entry.page_id == 0U) {
    return render_text_renderer_glyph_quad_packet_status::
        blocked_missing_atlas_page;
  }
  if (entry.status ==
      render_text_frame_resource_packet_materialization_status::
          blocked_missing_atlas_bounds) {
    return render_text_renderer_glyph_quad_packet_status::
        blocked_missing_atlas_bounds;
  }
  if (entry.page_width == 0U || entry.page_height == 0U) {
    return render_text_renderer_glyph_quad_packet_status::
        blocked_missing_page_extent;
  }
  if (entry.draw_status ==
      render_text_frame_draw_packet_status::missing_layout_bounds) {
    return render_text_renderer_glyph_quad_packet_status::
        blocked_missing_layout_bounds;
  }
  if (!entry.uv_bounds.valid) {
    return render_text_renderer_glyph_quad_packet_status::
        blocked_missing_uv_bounds;
  }
  if (entry.sampler_key.empty()) {
    return render_text_renderer_glyph_quad_packet_status::
        blocked_missing_sampler_key;
  }
  if (!entry.renderer_boundary_ready || entry.blocked || !entry.ready) {
    return render_text_renderer_glyph_quad_packet_status::blocked_resource_packet;
  }
  return render_text_renderer_glyph_quad_packet_status::quad_ready;
}

[[nodiscard]] inline std::string render_text_renderer_glyph_quad_blocker_summary_for(
    const render_text_frame_resource_packet_materialization_entry& entry,
    const render_text_renderer_glyph_quad_packet_status status) {
  switch (status) {
    case render_text_renderer_glyph_quad_packet_status::quad_ready:
      return {};
    case render_text_renderer_glyph_quad_packet_status::
        blocked_missing_resource_packet_id:
      return "glyph quad packet is missing resource packet identity";
    case render_text_renderer_glyph_quad_packet_status::
        blocked_missing_stable_packet_key:
      return "glyph quad packet is missing stable packet key";
    case render_text_renderer_glyph_quad_packet_status::duplicate_packet_id:
      return "glyph quad packet resource identity is duplicated";
    case render_text_renderer_glyph_quad_packet_status::
        blocked_missing_atlas_page:
      return "glyph quad packet is missing atlas page identity";
    case render_text_renderer_glyph_quad_packet_status::
        blocked_missing_atlas_bounds:
      return "glyph quad packet is missing atlas bounds";
    case render_text_renderer_glyph_quad_packet_status::
        blocked_missing_page_extent:
      return "glyph quad packet is missing atlas page extent";
    case render_text_renderer_glyph_quad_packet_status::
        blocked_missing_layout_bounds:
      return "glyph quad packet is missing glyph layout bounds";
    case render_text_renderer_glyph_quad_packet_status::
        blocked_missing_uv_bounds:
      return "glyph quad packet is missing atlas UV bounds";
    case render_text_renderer_glyph_quad_packet_status::
        blocked_missing_sampler_key:
      return "glyph quad packet is missing text atlas sampler key";
    case render_text_renderer_glyph_quad_packet_status::blocked_resource_packet:
      if (!entry.blocker_summary.empty()) {
        return entry.blocker_summary;
      }
      return "resource packet is not renderer-boundary ready";
  }
  return "unknown glyph quad packet blocker";
}

[[nodiscard]] inline render_text_renderer_glyph_quad_packet_record
make_render_text_renderer_glyph_quad_packet(
    const render_text_frame_resource_packet_materialization_entry& entry,
    const std::size_t packet_index,
    const bool duplicate_packet_id) {
  const render_text_renderer_glyph_quad_packet_status status =
      render_text_renderer_glyph_quad_packet_status_for(entry, duplicate_packet_id);
  const bool ready =
      status == render_text_renderer_glyph_quad_packet_status::quad_ready;
  const bool missing_identity = entry.resource_packet_id.empty();
  const bool missing_stable_packet_key = entry.stable_packet_key.empty();
  const std::string blocker =
      render_text_renderer_glyph_quad_blocker_summary_for(entry, status);
  return {
      .quad_packet_id =
          render_text_renderer_glyph_quad_packet_id_for(entry, packet_index),
      .resource_packet_id = entry.resource_packet_id,
      .stable_packet_key = entry.stable_packet_key,
      .source_label = entry.source_label,
      .source_node_id_hint =
          render_text_renderer_glyph_quad_node_id_hint_for(entry.source_label),
      .draw_packet_id = entry.draw_packet_id,
      .upload_handoff_id = entry.upload_handoff_id,
      .upload_operation_id = entry.upload_operation_id,
      .upload_request_id = entry.upload_request_id,
      .stable_page_id = entry.stable_page_id,
      .sampler_key = entry.sampler_key,
      .sampler_summary = entry.sampler_summary,
      .packet_index = packet_index,
      .materialization_index = entry.materialization_index,
      .run_index = entry.run_index,
      .cluster_byte_offset = entry.cluster_byte_offset,
      .cluster_byte_count = entry.cluster_byte_count,
      .cache_key = entry.cache_key,
      .resolved_glyph_id = entry.resolved_glyph_id,
      .resolved_face_id = entry.resolved_face_id,
      .page_id = entry.page_id,
      .page_revision = entry.page_revision,
      .page_width = entry.page_width,
      .page_height = entry.page_height,
      .layout_bounds = entry.layout_bounds,
      .atlas_bounds = entry.atlas_bounds,
      .uv_bounds = entry.uv_bounds,
      .resource_status = entry.status,
      .status = status,
      .ready = ready,
      .blocked = !ready,
      .renderer_boundary_ready = ready,
      .duplicate_packet = duplicate_packet_id,
      .missing_identity = missing_identity,
      .missing_stable_packet_key = missing_stable_packet_key,
      .uploaded = entry.uploaded,
      .clean_reuse = entry.clean_reuse,
      .used_deterministic_fallback = entry.used_deterministic_fallback,
      .used_real_backend = entry.used_real_backend,
      .glyph_supported = entry.glyph_supported,
      .upload_consumed = entry.upload_consumed,
      .upload_rgba_bytes = entry.upload_rgba_bytes,
      .atlas_consumption = entry.atlas_consumption,
      .blocker_summary = blocker,
      .diagnostic = ready
          ? "glyph quad packet is ready for renderer-boundary consumption"
          : blocker,
  };
}

inline void append_render_text_renderer_glyph_quad_packet(
    render_text_renderer_glyph_quad_packet_snapshot& snapshot,
    render_text_renderer_glyph_quad_packet_record packet) {
  auto& policy = snapshot.policy;
  ++policy.quad_packet_count;
  if (packet.ready) {
    ++policy.ready_quad_count;
    snapshot.ready_quad_packet_ids.push_back(packet.quad_packet_id);
  } else {
    ++policy.blocked_quad_count;
    snapshot.blocker_quad_packet_ids.push_back(packet.quad_packet_id);
    policy.has_blockers = true;
  }
  if (packet.uploaded) {
    ++policy.uploaded_quad_count;
  }
  if (packet.clean_reuse) {
    ++policy.clean_reuse_quad_count;
  }
  if (packet.duplicate_packet) {
    ++policy.duplicate_packet_id_count;
    detail::append_unique_string(snapshot.duplicate_quad_packet_ids,
                                 packet.quad_packet_id);
  }
  if (packet.missing_identity) {
    ++policy.missing_identity_count;
    detail::append_unique_string(snapshot.missing_identity_quad_packet_ids,
                                 packet.quad_packet_id);
  }
  if (packet.missing_stable_packet_key) {
    ++policy.missing_stable_packet_key_count;
  }
  if (packet.atlas_consumption.missing_glyph) {
    ++policy.missing_glyph_count;
  }
  if (packet.atlas_consumption.used_fallback_glyph_id) {
    ++policy.fallback_glyph_count;
  }
  switch (packet.status) {
    case render_text_renderer_glyph_quad_packet_status::quad_ready:
      break;
    case render_text_renderer_glyph_quad_packet_status::blocked_resource_packet:
      ++policy.resource_blocked_count;
      break;
    case render_text_renderer_glyph_quad_packet_status::
        blocked_missing_resource_packet_id:
      break;
    case render_text_renderer_glyph_quad_packet_status::
        blocked_missing_stable_packet_key:
      break;
    case render_text_renderer_glyph_quad_packet_status::duplicate_packet_id:
      break;
    case render_text_renderer_glyph_quad_packet_status::
        blocked_missing_atlas_page:
      ++policy.missing_atlas_page_count;
      break;
    case render_text_renderer_glyph_quad_packet_status::
        blocked_missing_atlas_bounds:
      ++policy.missing_atlas_bounds_count;
      break;
    case render_text_renderer_glyph_quad_packet_status::
        blocked_missing_page_extent:
      ++policy.missing_page_extent_count;
      break;
    case render_text_renderer_glyph_quad_packet_status::
        blocked_missing_layout_bounds:
      ++policy.missing_layout_bounds_count;
      break;
    case render_text_renderer_glyph_quad_packet_status::
        blocked_missing_uv_bounds:
      ++policy.missing_uv_bounds_count;
      break;
    case render_text_renderer_glyph_quad_packet_status::
        blocked_missing_sampler_key:
      ++policy.missing_sampler_key_count;
      break;
  }
  if (packet.used_deterministic_fallback) {
    ++policy.deterministic_fallback_count;
    policy.used_deterministic_fallback = true;
  }
  if (packet.used_real_backend) {
    ++policy.real_backend_count;
    policy.used_real_backend = true;
  }
  if (packet.upload_consumed) {
    ++policy.consumed_upload_count;
  }
  policy.total_upload_rgba_bytes += packet.upload_rgba_bytes;
  snapshot.packets.push_back(std::move(packet));
}

[[nodiscard]] inline render_text_renderer_glyph_quad_packet_snapshot
make_render_text_renderer_glyph_quad_packets(
    render_text_renderer_glyph_quad_packet_request request) {
  render_text_renderer_glyph_quad_packet_snapshot snapshot{
      .frame_id = request.resource_packets.frame_id,
      .source_label = request.resource_packets.source_label,
      .frame_ready_for_renderer =
          request.resource_packets.frame_ready_for_renderer,
      .diagnostic =
          "renderer glyph quad packets built from text resource packet evidence",
  };
  snapshot.policy.resource_packet_count = request.resource_packets.entries.size();
  snapshot.policy.frame_ready_for_renderer =
      request.resource_packets.frame_ready_for_renderer;
  snapshot.packets.reserve(request.resource_packets.entries.size());

  std::vector<std::string> seen_resource_packet_ids{};
  seen_resource_packet_ids.reserve(request.resource_packets.entries.size());
  for (std::size_t index = 0; index < request.resource_packets.entries.size();
       ++index) {
    const auto& entry = request.resource_packets.entries[index];
    const bool duplicate_packet_id =
        !entry.resource_packet_id.empty() &&
        std::find(seen_resource_packet_ids.begin(),
                  seen_resource_packet_ids.end(),
                  entry.resource_packet_id) != seen_resource_packet_ids.end();
    if (!entry.resource_packet_id.empty() && !duplicate_packet_id) {
      seen_resource_packet_ids.push_back(entry.resource_packet_id);
    }
    append_render_text_renderer_glyph_quad_packet(
        snapshot,
        make_render_text_renderer_glyph_quad_packet(entry, index,
                                                    duplicate_packet_id));
  }

  if (!snapshot.frame_ready_for_renderer && snapshot.packets.empty()) {
    snapshot.policy.has_blockers = true;
    snapshot.blocker_summary =
        "text frame resource packets are not ready for renderer glyph quads";
  } else if (snapshot.policy.has_blockers) {
    snapshot.blocker_summary =
        std::to_string(snapshot.policy.blocked_quad_count) +
        " glyph quad packet(s) blocked";
  }
  return snapshot;
}

[[nodiscard]] inline std::string render_text_renderer_draw_payload_status_name(
    const render_text_renderer_draw_payload_status status) {
  switch (status) {
    case render_text_renderer_draw_payload_status::payload_ready:
      return "payload_ready";
    case render_text_renderer_draw_payload_status::blocked_glyph_quad:
      return "blocked_glyph_quad";
    case render_text_renderer_draw_payload_status::blocked_missing_payload_id:
      return "blocked_missing_payload_id";
  }
  return "unknown";
}

[[nodiscard]] inline std::string render_text_renderer_draw_payload_id_for(
    const render_text_renderer_glyph_quad_packet_record& quad,
    const std::size_t payload_index) {
  if (!quad.quad_packet_id.empty()) {
    return "text-renderer-draw-payload:v1:quad=" + quad.quad_packet_id;
  }
  if (!quad.stable_packet_key.empty()) {
    return "text-renderer-draw-payload:v1:stable=" + quad.stable_packet_key;
  }
  return "text-renderer-draw-payload:v1:frame=" + quad.source_label +
         ":missing-quad-id:index=" + std::to_string(payload_index);
}

[[nodiscard]] inline render_text_renderer_draw_payload_status
render_text_renderer_draw_payload_status_for(
    const render_text_renderer_glyph_quad_packet_record& quad) {
  if (quad.quad_packet_id.empty() && quad.stable_packet_key.empty()) {
    return render_text_renderer_draw_payload_status::blocked_missing_payload_id;
  }
  if (!quad.drawable()) {
    return render_text_renderer_draw_payload_status::blocked_glyph_quad;
  }
  return render_text_renderer_draw_payload_status::payload_ready;
}

[[nodiscard]] inline std::string render_text_renderer_draw_payload_blocker_summary_for(
    const render_text_renderer_glyph_quad_packet_record& quad,
    const render_text_renderer_draw_payload_status status) {
  switch (status) {
    case render_text_renderer_draw_payload_status::payload_ready:
      return {};
    case render_text_renderer_draw_payload_status::blocked_missing_payload_id:
      return "renderer draw payload is missing glyph quad packet identity";
    case render_text_renderer_draw_payload_status::blocked_glyph_quad:
      if (!quad.blocker_summary.empty()) {
        return quad.blocker_summary;
      }
      return "glyph quad packet is not drawable";
  }
  return "unknown renderer draw payload blocker";
}

[[nodiscard]] inline render_text_renderer_draw_payload_record
make_render_text_renderer_draw_payload(
    const render_text_renderer_glyph_quad_packet_record& quad,
    const std::size_t payload_index) {
  const render_text_renderer_draw_payload_status status =
      render_text_renderer_draw_payload_status_for(quad);
  const bool ready =
      status == render_text_renderer_draw_payload_status::payload_ready;
  const std::string blocker =
      render_text_renderer_draw_payload_blocker_summary_for(quad, status);
  return {
      .payload_id = render_text_renderer_draw_payload_id_for(quad, payload_index),
      .quad_packet_id = quad.quad_packet_id,
      .resource_packet_id = quad.resource_packet_id,
      .stable_packet_key = quad.stable_packet_key,
      .source_label = quad.source_label,
      .source_node_id_hint = quad.source_node_id_hint,
      .draw_packet_id = quad.draw_packet_id,
      .upload_handoff_id = quad.upload_handoff_id,
      .upload_operation_id = quad.upload_operation_id,
      .upload_request_id = quad.upload_request_id,
      .stable_page_id = quad.stable_page_id,
      .sampler_key = quad.sampler_key,
      .sampler_summary = quad.sampler_summary,
      .payload_index = payload_index,
      .packet_index = quad.packet_index,
      .materialization_index = quad.materialization_index,
      .run_index = quad.run_index,
      .cluster_byte_offset = quad.cluster_byte_offset,
      .cluster_byte_count = quad.cluster_byte_count,
      .cache_key = quad.cache_key,
      .resolved_glyph_id = quad.resolved_glyph_id,
      .resolved_face_id = quad.resolved_face_id,
      .page_id = quad.page_id,
      .page_revision = quad.page_revision,
      .page_width = quad.page_width,
      .page_height = quad.page_height,
      .quad_bounds = quad.layout_bounds,
      .atlas_bounds = quad.atlas_bounds,
      .uv_bounds = quad.uv_bounds,
      .quad_status = quad.status,
      .status = status,
      .ready = ready,
      .blocked = !ready,
      .renderer_boundary_ready = ready,
      .uploaded = quad.uploaded,
      .clean_reuse = quad.clean_reuse,
      .used_deterministic_fallback = quad.used_deterministic_fallback,
      .used_real_backend = quad.used_real_backend,
      .glyph_supported = quad.glyph_supported,
      .upload_consumed = quad.upload_consumed,
      .upload_rgba_bytes = quad.upload_rgba_bytes,
      .atlas_consumption = quad.atlas_consumption,
      .blocker_summary = blocker,
      .diagnostic = ready
          ? "renderer text draw payload is ready"
          : blocker,
  };
}

inline void append_render_text_renderer_draw_payload(
    render_text_renderer_draw_payload_snapshot& snapshot,
    render_text_renderer_draw_payload_record payload) {
  auto& policy = snapshot.policy;
  ++policy.payload_count;
  if (payload.ready) {
    ++policy.ready_payload_count;
    snapshot.ready_payload_ids.push_back(payload.payload_id);
  } else {
    ++policy.blocked_payload_count;
    snapshot.blocker_payload_ids.push_back(payload.payload_id);
    policy.has_blockers = true;
  }
  if (payload.status ==
      render_text_renderer_draw_payload_status::blocked_missing_payload_id) {
    ++policy.missing_payload_id_count;
    detail::append_unique_string(snapshot.missing_payload_ids,
                                 payload.payload_id);
  }
  if (payload.status ==
      render_text_renderer_draw_payload_status::blocked_glyph_quad) {
    ++policy.glyph_quad_blocked_count;
  }
  if (payload.uploaded) {
    ++policy.uploaded_payload_count;
  }
  if (payload.clean_reuse) {
    ++policy.clean_reuse_payload_count;
  }
  if (payload.atlas_consumption.missing_glyph) {
    ++policy.missing_glyph_count;
  }
  if (payload.atlas_consumption.used_fallback_glyph_id) {
    ++policy.fallback_glyph_count;
  }
  if (payload.used_deterministic_fallback) {
    ++policy.deterministic_fallback_count;
    policy.used_deterministic_fallback = true;
  }
  if (payload.used_real_backend) {
    ++policy.real_backend_count;
    policy.used_real_backend = true;
  }
  if (payload.upload_consumed) {
    ++policy.consumed_upload_count;
  }
  policy.total_upload_rgba_bytes += payload.upload_rgba_bytes;
  snapshot.payloads.push_back(std::move(payload));
}

[[nodiscard]] inline render_text_renderer_draw_payload_snapshot
make_render_text_renderer_draw_payloads(
    render_text_renderer_draw_payload_request request) {
  render_text_renderer_draw_payload_snapshot snapshot{
      .frame_id = request.glyph_quads.frame_id,
      .source_label = request.glyph_quads.source_label,
      .frame_ready_for_renderer = request.glyph_quads.frame_ready_for_renderer,
      .diagnostic =
          "renderer text draw payloads built from glyph quad packet evidence",
  };
  snapshot.policy.glyph_quad_packet_count = request.glyph_quads.packets.size();
  snapshot.policy.frame_ready_for_renderer =
      request.glyph_quads.frame_ready_for_renderer;
  snapshot.payloads.reserve(request.glyph_quads.packets.size());
  for (std::size_t index = 0; index < request.glyph_quads.packets.size();
       ++index) {
    append_render_text_renderer_draw_payload(
        snapshot,
        make_render_text_renderer_draw_payload(request.glyph_quads.packets[index],
                                               index));
  }

  if (!snapshot.frame_ready_for_renderer && snapshot.payloads.empty()) {
    snapshot.policy.has_blockers = true;
    snapshot.blocker_summary =
        "glyph quad packets are not ready for renderer text draw payloads";
  } else if (snapshot.policy.has_blockers) {
    snapshot.blocker_summary =
        std::to_string(snapshot.policy.blocked_payload_count) +
        " renderer text draw payload(s) blocked";
  }
  return snapshot;
}

[[nodiscard]] inline std::ptrdiff_t render_text_renderer_glyph_quad_packet_count_delta(
    const std::size_t before,
    const std::size_t after) {
  return static_cast<std::ptrdiff_t>(after) -
         static_cast<std::ptrdiff_t>(before);
}

[[nodiscard]] inline bool render_text_renderer_glyph_quad_packet_identity_duplicate(
    const std::vector<render_text_renderer_glyph_quad_packet_record>& packets,
    const std::string& quad_packet_id) {
  if (quad_packet_id.empty()) {
    return false;
  }
  return std::count_if(
             packets.begin(), packets.end(),
             [&](const render_text_renderer_glyph_quad_packet_record& packet) {
               return packet.quad_packet_id == quad_packet_id;
             }) > 1;
}

[[nodiscard]] inline const render_text_renderer_glyph_quad_packet_record*
find_unmatched_render_text_renderer_glyph_quad_packet(
    const std::vector<render_text_renderer_glyph_quad_packet_record>& packets,
    const std::vector<bool>& matched,
    const std::string& quad_packet_id) {
  if (quad_packet_id.empty()) {
    return nullptr;
  }
  for (std::size_t index = 0; index < packets.size(); ++index) {
    if (index < matched.size() && matched[index]) {
      continue;
    }
    if (packets[index].quad_packet_id == quad_packet_id) {
      return &packets[index];
    }
  }
  return nullptr;
}

[[nodiscard]] inline std::size_t render_text_renderer_glyph_quad_packet_index(
    const std::vector<render_text_renderer_glyph_quad_packet_record>& packets,
    const render_text_renderer_glyph_quad_packet_record* packet) {
  if (packet == nullptr || packets.empty()) {
    return packets.size();
  }
  const auto index = static_cast<std::size_t>(packet - packets.data());
  return index < packets.size() ? index : packets.size();
}

[[nodiscard]] inline render_text_renderer_glyph_quad_packet_diff_record
diff_render_text_renderer_glyph_quad_packet_records(
    const render_text_renderer_glyph_quad_packet_record* before,
    const render_text_renderer_glyph_quad_packet_record* after,
    const bool duplicate_identity) {
  const bool added = before == nullptr && after != nullptr;
  const bool removed = before != nullptr && after == nullptr;
  const auto* identity_source = after == nullptr ? before : after;
  const bool missing_identity =
      identity_source == nullptr || identity_source->quad_packet_id.empty();
  const bool layout_bounds_changed =
      before != nullptr && after != nullptr &&
      !render_text_frame_snapshot_rects_equal(before->layout_bounds,
                                              after->layout_bounds);
  const bool atlas_bounds_changed =
      before != nullptr && after != nullptr &&
      !render_text_frame_snapshot_rects_equal(before->atlas_bounds,
                                              after->atlas_bounds);
  const bool uv_bounds_changed =
      before != nullptr && after != nullptr &&
      !render_text_frame_draw_uv_rects_equal(before->uv_bounds, after->uv_bounds);
  const render_text_atlas_packet_consumption_evidence_diff
      atlas_consumption_diff =
          before != nullptr && after != nullptr
              ? diff_render_text_atlas_packet_consumption_evidence(
                    before->atlas_consumption, after->atlas_consumption)
              : render_text_atlas_packet_consumption_evidence_diff{};
  const bool page_revision_changed =
      before != nullptr && after != nullptr &&
      before->page_revision != after->page_revision;
  const bool sampler_key_changed =
      before != nullptr && after != nullptr &&
      before->sampler_key != after->sampler_key;
  const bool upload_request_id_changed =
      before != nullptr && after != nullptr &&
      before->upload_request_id != after->upload_request_id;
  const bool upload_operation_id_changed =
      before != nullptr && after != nullptr &&
      before->upload_operation_id != after->upload_operation_id;
  const bool uploaded_byte_count_changed =
      before != nullptr && after != nullptr &&
      before->upload_rgba_bytes != after->upload_rgba_bytes;
  const bool readiness_changed =
      before != nullptr && after != nullptr && before->ready != after->ready;
  const bool readiness_regressed =
      before != nullptr && after != nullptr && before->ready && after->blocked;
  const bool readiness_recovered =
      before != nullptr && after != nullptr && before->blocked && after->ready;
  const bool status_changed =
      before != nullptr && after != nullptr && before->status != after->status;
  const bool changed =
      !added && !removed &&
      (layout_bounds_changed || atlas_bounds_changed || uv_bounds_changed ||
       atlas_consumption_diff.has_changes() || page_revision_changed ||
       sampler_key_changed ||
       upload_request_id_changed || upload_operation_id_changed ||
       uploaded_byte_count_changed || readiness_changed ||
       status_changed || duplicate_identity || missing_identity);
  const bool unchanged = !added && !removed && !changed;

  return {
      .quad_packet_id =
          identity_source == nullptr ? std::string{} : identity_source->quad_packet_id,
      .previous_resource_packet_id =
          before == nullptr ? std::string{} : before->resource_packet_id,
      .current_resource_packet_id =
          after == nullptr ? std::string{} : after->resource_packet_id,
      .previous_sampler_key = before == nullptr ? std::string{} : before->sampler_key,
      .current_sampler_key = after == nullptr ? std::string{} : after->sampler_key,
      .previous_upload_request_id =
          before == nullptr ? std::string{} : before->upload_request_id,
      .current_upload_request_id =
          after == nullptr ? std::string{} : after->upload_request_id,
      .previous_upload_operation_id =
          before == nullptr ? std::string{} : before->upload_operation_id,
      .current_upload_operation_id =
          after == nullptr ? std::string{} : after->upload_operation_id,
      .added = added,
      .removed = removed,
      .changed = changed,
      .unchanged = unchanged,
      .missing_identity = missing_identity,
      .duplicate_identity = duplicate_identity,
      .layout_bounds_changed = layout_bounds_changed,
      .atlas_bounds_changed = atlas_bounds_changed,
      .uv_bounds_changed = uv_bounds_changed,
      .atlas_consumption_diff = atlas_consumption_diff,
      .page_revision_changed = page_revision_changed,
      .sampler_key_changed = sampler_key_changed,
      .upload_request_id_changed = upload_request_id_changed,
      .upload_operation_id_changed = upload_operation_id_changed,
      .uploaded_byte_count_changed = uploaded_byte_count_changed,
      .readiness_changed = readiness_changed,
      .readiness_regressed = readiness_regressed,
      .readiness_recovered = readiness_recovered,
      .status_changed = status_changed,
      .upload_rgba_bytes_delta =
          render_text_renderer_glyph_quad_packet_count_delta(
              before == nullptr ? 0U : before->upload_rgba_bytes,
              after == nullptr ? 0U : after->upload_rgba_bytes),
      .previous_ready = before != nullptr && before->ready,
      .current_ready = after != nullptr && after->ready,
      .previous_page_id = before == nullptr ? 0U : before->page_id,
      .current_page_id = after == nullptr ? 0U : after->page_id,
      .previous_page_revision = before == nullptr ? 0U : before->page_revision,
      .current_page_revision = after == nullptr ? 0U : after->page_revision,
      .previous_status =
          before == nullptr
              ? render_text_renderer_glyph_quad_packet_status::blocked_resource_packet
              : before->status,
      .current_status =
          after == nullptr
              ? render_text_renderer_glyph_quad_packet_status::blocked_resource_packet
              : after->status,
  };
}

inline void append_render_text_renderer_glyph_quad_packet_diff(
    render_text_renderer_glyph_quad_packet_diff_snapshot& diff,
    render_text_renderer_glyph_quad_packet_diff_record packet_diff) {
  auto& policy = diff.policy;
  if (packet_diff.added) {
    ++policy.added_packet_count;
    detail::append_unique_string(diff.added_quad_packet_ids,
                                 packet_diff.quad_packet_id);
  }
  if (packet_diff.removed) {
    ++policy.removed_packet_count;
    detail::append_unique_string(diff.removed_quad_packet_ids,
                                 packet_diff.quad_packet_id);
  }
  if (packet_diff.changed) {
    ++policy.changed_packet_count;
    detail::append_unique_string(diff.changed_quad_packet_ids,
                                 packet_diff.quad_packet_id);
  }
  if (packet_diff.unchanged) {
    ++policy.unchanged_packet_count;
    detail::append_unique_string(diff.unchanged_quad_packet_ids,
                                 packet_diff.quad_packet_id);
  }
  if (packet_diff.readiness_regressed) {
    ++policy.readiness_regression_count;
    detail::append_unique_string(diff.readiness_regressed_quad_packet_ids,
                                 packet_diff.quad_packet_id);
  }
  if (packet_diff.readiness_recovered) {
    ++policy.readiness_recovery_count;
    detail::append_unique_string(diff.readiness_recovered_quad_packet_ids,
                                 packet_diff.quad_packet_id);
  }
  if (packet_diff.layout_bounds_changed) {
    ++policy.layout_bounds_changed_count;
  }
  if (packet_diff.atlas_bounds_changed) {
    ++policy.atlas_bounds_changed_count;
  }
  if (packet_diff.uv_bounds_changed) {
    ++policy.uv_bounds_changed_count;
  }
  count_render_text_atlas_packet_consumption_evidence_diff(
      policy.atlas_consumption_diff, packet_diff.atlas_consumption_diff);
  if (packet_diff.page_revision_changed) {
    ++policy.page_revision_changed_count;
  }
  if (packet_diff.sampler_key_changed) {
    ++policy.sampler_key_changed_count;
  }
  if (packet_diff.upload_request_id_changed) {
    ++policy.upload_request_id_changed_count;
  }
  if (packet_diff.upload_operation_id_changed) {
    ++policy.upload_operation_id_changed_count;
  }
  if (packet_diff.uploaded_byte_count_changed) {
    ++policy.uploaded_byte_count_changed_count;
  }
  if (packet_diff.readiness_changed) {
    ++policy.readiness_changed_count;
  }
  if (packet_diff.status_changed) {
    ++policy.status_changed_count;
  }
  if (packet_diff.duplicate_identity) {
    ++policy.duplicate_identity_count;
    detail::append_unique_string(diff.duplicate_identity_quad_packet_ids,
                                 packet_diff.quad_packet_id);
  }
  if (packet_diff.missing_identity) {
    ++policy.missing_identity_count;
    detail::append_unique_string(diff.missing_identity_quad_packet_ids,
                                 packet_diff.quad_packet_id);
  }
  diff.packet_diffs.push_back(std::move(packet_diff));
}

[[nodiscard]] inline render_text_renderer_glyph_quad_packet_diff_snapshot
diff_render_text_renderer_glyph_quad_packet_snapshots(
    const render_text_renderer_glyph_quad_packet_snapshot& before,
    const render_text_renderer_glyph_quad_packet_snapshot& after) {
  render_text_renderer_glyph_quad_packet_diff_snapshot diff{
      .previous_frame_id = before.frame_id,
      .current_frame_id = after.frame_id,
      .previous_ready_for_renderer = before.frame_ready_for_renderer,
      .current_ready_for_renderer = after.frame_ready_for_renderer,
  };
  diff.policy = {
      .quad_packet_count_delta =
          render_text_renderer_glyph_quad_packet_count_delta(
              before.policy.quad_packet_count, after.policy.quad_packet_count),
      .ready_quad_count_delta =
          render_text_renderer_glyph_quad_packet_count_delta(
              before.policy.ready_quad_count, after.policy.ready_quad_count),
      .blocked_quad_count_delta =
          render_text_renderer_glyph_quad_packet_count_delta(
              before.policy.blocked_quad_count, after.policy.blocked_quad_count),
      .uploaded_quad_count_delta =
          render_text_renderer_glyph_quad_packet_count_delta(
              before.policy.uploaded_quad_count, after.policy.uploaded_quad_count),
      .clean_reuse_quad_count_delta =
          render_text_renderer_glyph_quad_packet_count_delta(
              before.policy.clean_reuse_quad_count,
              after.policy.clean_reuse_quad_count),
      .total_upload_rgba_bytes_delta =
          render_text_renderer_glyph_quad_packet_count_delta(
              before.policy.total_upload_rgba_bytes,
              after.policy.total_upload_rgba_bytes),
      .frame_id_changed = before.frame_id != after.frame_id,
      .frame_ready_changed =
          before.frame_ready_for_renderer != after.frame_ready_for_renderer,
      .blockers_changed = before.policy.has_blockers != after.policy.has_blockers,
  };

  std::vector<bool> matched_after(after.packets.size(), false);
  diff.packet_diffs.reserve(before.packets.size() + after.packets.size());
  for (const auto& previous_packet : before.packets) {
    const auto* current_packet = find_unmatched_render_text_renderer_glyph_quad_packet(
        after.packets, matched_after, previous_packet.quad_packet_id);
    if (current_packet != nullptr) {
      const std::size_t current_index =
          render_text_renderer_glyph_quad_packet_index(after.packets, current_packet);
      if (current_index < matched_after.size()) {
        matched_after[current_index] = true;
      }
    }
    const bool duplicate_identity =
        render_text_renderer_glyph_quad_packet_identity_duplicate(
            before.packets, previous_packet.quad_packet_id) ||
        render_text_renderer_glyph_quad_packet_identity_duplicate(
            after.packets, previous_packet.quad_packet_id);
    append_render_text_renderer_glyph_quad_packet_diff(
        diff,
        diff_render_text_renderer_glyph_quad_packet_records(
            &previous_packet, current_packet, duplicate_identity));
  }

  for (std::size_t index = 0; index < after.packets.size(); ++index) {
    if (matched_after[index]) {
      continue;
    }
    const auto& current_packet = after.packets[index];
    const bool duplicate_identity =
        render_text_renderer_glyph_quad_packet_identity_duplicate(
            after.packets, current_packet.quad_packet_id);
    append_render_text_renderer_glyph_quad_packet_diff(
        diff,
        diff_render_text_renderer_glyph_quad_packet_records(
            nullptr, &current_packet, duplicate_identity));
  }

  diff.policy.stable_no_change =
      !diff.policy.frame_id_changed && !diff.policy.frame_ready_changed &&
      !diff.policy.blockers_changed && diff.policy.added_packet_count == 0U &&
      diff.policy.removed_packet_count == 0U &&
      diff.policy.changed_packet_count == 0U &&
      diff.policy.duplicate_identity_count == 0U &&
      diff.policy.missing_identity_count == 0U;
  diff.summary = diff.policy.stable_no_change
      ? "renderer glyph quad packet diff is stable with no changes"
      : std::to_string(diff.policy.added_packet_count) + " added, " +
            std::to_string(diff.policy.removed_packet_count) + " removed, " +
            std::to_string(diff.policy.changed_packet_count) +
            " changed renderer glyph quad packet(s)";
  return diff;
}

}  // namespace quiz_vulkan::render
