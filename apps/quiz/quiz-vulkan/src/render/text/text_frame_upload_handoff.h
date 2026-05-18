#pragma once

#include "font_glyph_atlas_upload_result.h"
#include "text_frame_draw_plan.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

enum class render_text_frame_upload_handoff_packet_status {
  ready_uploaded,
  ready_clean_reuse,
  blocked_draw_packet,
  blocked_missing_upload_result,
  blocked_missing_draw_packet,
  blocked_upload_rejected,
};

struct render_text_frame_upload_handoff_packet_snapshot {
  std::string handoff_id{};
  std::string stable_packet_key{};
  std::string frame_id{};
  std::string source_label{};
  std::string draw_packet_id{};
  std::string upload_operation_id{};
  std::string upload_request_id{};
  std::string stable_page_id{};
  std::size_t materialization_index{};
  std::size_t run_index{};
  std::size_t cluster_byte_offset{};
  std::size_t cluster_byte_count{};
  glyph_atlas_key cache_key{};
  std::uint32_t resolved_glyph_id{};
  std::uint32_t resolved_face_id{};
  render_text_atlas_page_id page_id{};
  render_text_revision page_revision{};
  render_rect layout_bounds{};
  render_rect atlas_bounds{};
  render_rect update_bounds{};
  render_text_frame_draw_packet_status draw_status{
      render_text_frame_draw_packet_status::frame_not_ready};
  render_text_glyph_atlas_upload_result_status upload_result_status{
      render_text_glyph_atlas_upload_result_status::rejected_blocked_packet};
  render_text_frame_upload_handoff_packet_status handoff_status{
      render_text_frame_upload_handoff_packet_status::blocked_missing_upload_result};
  bool has_upload_result{};
  bool requested{};
  bool ready{};
  bool blocked{true};
  bool drawable{};
  bool uploaded{};
  bool clean_reuse{};
  bool missing_upload_result{};
  bool missing_draw_packet{};
  bool missing_glyph{};
  bool missing_materialization{};
  bool fallback_incomplete{};
  bool used_deterministic_fallback{};
  bool used_real_backend{};
  bool glyph_supported{};
  bool upload_consumed{};
  std::size_t upload_rgba_bytes{};
  render_text_atlas_packet_consumption_evidence atlas_consumption{};
  std::string blocker_reason{};
  std::string diagnostic{};

  [[nodiscard]] constexpr bool upload_ready() const noexcept {
    return ready && !blocked;
  }
};

struct render_text_frame_upload_handoff_page_snapshot {
  std::string stable_page_id{};
  render_text_atlas_page_id page_id{};
  render_text_revision page_revision{};
  std::size_t packet_count{};
  std::size_t ready_packet_count{};
  std::size_t blocked_packet_count{};
  std::size_t uploaded_glyph_count{};
  std::size_t clean_reuse_glyph_count{};
  std::size_t missing_glyph_count{};
  std::size_t missing_draw_packet_count{};
  std::size_t missing_materialization_count{};
  std::size_t upload_rgba_bytes{};
  bool has_uploads{};
  bool has_blockers{};
};

struct render_text_frame_upload_handoff_policy_snapshot {
  std::size_t requested_glyph_packet_count{};
  std::size_t ready_glyph_packet_count{};
  std::size_t blocked_glyph_packet_count{};
  std::size_t uploaded_page_count{};
  std::size_t upload_result_missing_count{};
  std::size_t draw_packet_missing_count{};
  std::size_t upload_result_rejected_count{};
  std::size_t uploaded_glyph_count{};
  std::size_t clean_reuse_glyph_count{};
  std::size_t missing_glyph_count{};
  std::size_t missing_materialization_count{};
  std::size_t fallback_incomplete_count{};
  std::size_t deterministic_fallback_count{};
  std::size_t real_backend_count{};
  std::size_t consumed_upload_count{};
  std::size_t total_upload_rgba_bytes{};
  bool frame_ready_for_renderer{};
  bool has_blockers{};
  bool has_uploads{};
  bool used_deterministic_fallback{};
  bool used_real_backend{};
};

struct render_text_frame_upload_handoff_snapshot {
  std::string frame_id{};
  std::string source_label{};
  render_text_frame_snapshot_status frame_status{
      render_text_frame_snapshot_status::atlas_upload_incomplete};
  render_text_frame_upload_handoff_policy_snapshot policy{};
  std::vector<render_text_frame_upload_handoff_packet_snapshot> packets{};
  std::vector<render_text_frame_upload_handoff_page_snapshot> pages{};
  std::vector<std::string> uploaded_page_ids{};
  std::vector<std::string> ready_packet_ids{};
  std::vector<std::string> blocker_packet_ids{};
  std::string diagnostic{};

  [[nodiscard]] constexpr bool ok() const noexcept {
    return policy.frame_ready_for_renderer && !policy.has_blockers;
  }

  [[nodiscard]] constexpr bool has_blockers() const noexcept {
    return policy.has_blockers;
  }
};

struct render_text_frame_upload_handoff_request {
  render_text_frame_snapshot frame{};
  render_text_frame_draw_plan_snapshot draw_plan{};
  render_text_glyph_atlas_upload_result_snapshot upload_result{};
};

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

struct render_text_frame_resource_packet_consumption_diff_policy {
  std::ptrdiff_t resource_packet_count_delta{};
  std::ptrdiff_t ready_packet_count_delta{};
  std::ptrdiff_t blocked_packet_count_delta{};
  std::ptrdiff_t uploaded_packet_count_delta{};
  std::ptrdiff_t clean_reuse_packet_count_delta{};
  std::ptrdiff_t page_count_delta{};
  std::ptrdiff_t sampler_count_delta{};
  std::ptrdiff_t deterministic_fallback_count_delta{};
  std::ptrdiff_t real_backend_count_delta{};
  std::ptrdiff_t total_upload_rgba_bytes_delta{};
  std::size_t added_packet_count{};
  std::size_t removed_packet_count{};
  std::size_t changed_packet_count{};
  std::size_t unchanged_packet_count{};
  std::size_t added_page_count{};
  std::size_t removed_page_count{};
  std::size_t changed_page_count{};
  std::size_t unchanged_page_count{};
  std::size_t readiness_regression_count{};
  std::size_t readiness_improvement_count{};
  std::size_t stable_packet_key_changed_count{};
  std::size_t page_id_changed_count{};
  std::size_t page_revision_changed_count{};
  std::size_t page_extent_changed_count{};
  std::size_t sampler_key_changed_count{};
  std::size_t uv_bounds_changed_count{};
  std::size_t layout_bounds_changed_count{};
  std::size_t upload_request_id_changed_count{};
  std::size_t upload_operation_id_changed_count{};
  std::size_t uploaded_byte_count_changed_count{};
  std::size_t fallback_flag_changed_count{};
  std::size_t backend_flag_changed_count{};
  std::size_t readiness_or_blocker_status_changed_count{};
  std::size_t duplicate_identity_count{};
  std::size_t missing_identity_count{};
  bool frame_id_changed{};
  bool frame_ready_changed{};
  bool blockers_changed{};
  bool stable_no_change{};
};

struct render_text_frame_resource_packet_consumption_packet_diff {
  std::string resource_packet_id{};
  std::string previous_stable_packet_key{};
  std::string current_stable_packet_key{};
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
  bool stable_packet_key_changed{};
  bool page_id_changed{};
  bool page_revision_changed{};
  bool page_extent_changed{};
  bool sampler_key_changed{};
  bool uv_bounds_changed{};
  bool layout_bounds_changed{};
  bool upload_request_id_changed{};
  bool upload_operation_id_changed{};
  bool uploaded_byte_count_changed{};
  bool fallback_flag_changed{};
  bool backend_flag_changed{};
  bool readiness_changed{};
  bool readiness_regressed{};
  bool readiness_improved{};
  bool blocker_status_changed{};
  bool status_changed{};
  std::ptrdiff_t upload_rgba_bytes_delta{};
  render_text_atlas_page_id previous_page_id{};
  render_text_atlas_page_id current_page_id{};
  render_text_revision previous_page_revision{};
  render_text_revision current_page_revision{};
  std::size_t previous_page_width{};
  std::size_t previous_page_height{};
  std::size_t current_page_width{};
  std::size_t current_page_height{};
  render_text_frame_resource_packet_materialization_status previous_status{
      render_text_frame_resource_packet_materialization_status::
          blocked_missing_upload_handoff};
  render_text_frame_resource_packet_materialization_status current_status{
      render_text_frame_resource_packet_materialization_status::
          blocked_missing_upload_handoff};
};

struct render_text_frame_resource_packet_consumption_page_diff {
  std::string stable_page_id{};
  std::string previous_sampler_key{};
  std::string current_sampler_key{};
  bool added{};
  bool removed{};
  bool changed{};
  bool unchanged{};
  bool missing_identity{};
  bool page_id_changed{};
  bool page_revision_changed{};
  bool page_extent_changed{};
  bool sampler_key_changed{};
  bool readiness_changed{};
  bool blocker_changed{};
  std::ptrdiff_t packet_count_delta{};
  std::ptrdiff_t ready_packet_count_delta{};
  std::ptrdiff_t blocked_packet_count_delta{};
  std::ptrdiff_t upload_rgba_bytes_delta{};
  render_text_atlas_page_id previous_page_id{};
  render_text_atlas_page_id current_page_id{};
  render_text_revision previous_page_revision{};
  render_text_revision current_page_revision{};
  std::size_t previous_page_width{};
  std::size_t previous_page_height{};
  std::size_t current_page_width{};
  std::size_t current_page_height{};
};

struct render_text_frame_resource_packet_consumption_diff_snapshot {
  std::string previous_frame_id{};
  std::string current_frame_id{};
  bool previous_ready_for_renderer{};
  bool current_ready_for_renderer{};
  render_text_frame_resource_packet_consumption_diff_policy policy{};
  std::vector<render_text_frame_resource_packet_consumption_packet_diff>
      packet_diffs{};
  std::vector<render_text_frame_resource_packet_consumption_page_diff>
      page_diffs{};
  std::vector<std::string> added_resource_packet_ids{};
  std::vector<std::string> removed_resource_packet_ids{};
  std::vector<std::string> changed_resource_packet_ids{};
  std::vector<std::string> unchanged_resource_packet_ids{};
  std::vector<std::string> readiness_regressed_resource_packet_ids{};
  std::vector<std::string> readiness_improved_resource_packet_ids{};
  std::vector<std::string> duplicate_identity_resource_packet_ids{};
  std::vector<std::string> missing_identity_resource_packet_ids{};
  std::vector<std::string> changed_page_ids{};
  std::string summary{};

  [[nodiscard]] constexpr bool has_changes() const noexcept {
    return !policy.stable_no_change;
  }

  [[nodiscard]] constexpr bool stable_no_change() const noexcept {
    return policy.stable_no_change;
  }
};

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

namespace detail {

[[nodiscard]] inline std::string text_frame_upload_handoff_stable_key_for(
    const render_text_frame_draw_packet_snapshot& packet) {
  return "text-frame-upload-handoff:v1:item=" +
         std::to_string(packet.item_index) + ":mat=" +
         std::to_string(packet.materialization_index) + ":run=" +
         std::to_string(packet.run_index) + ":cluster=" +
         std::to_string(packet.cluster_byte_offset) + "+" +
         std::to_string(packet.cluster_byte_count) + ":face=" +
         std::to_string(packet.resolved_face_id) + ":glyph=" +
         std::to_string(packet.resolved_glyph_id) + ":cache-face=" +
         std::to_string(packet.cache_key.face_id) + ":cache-glyph=" +
         std::to_string(packet.cache_key.glyph_id) + ":cache-px=" +
         std::to_string(packet.cache_key.pixel_size);
}

[[nodiscard]] inline std::string text_frame_upload_handoff_packet_id_for(
    const render_text_frame_draw_packet_snapshot& packet) {
  return "text-frame-upload-handoff:v1:frame=" + packet.frame_id + ":draw=" +
         packet.packet_id;
}

[[nodiscard]] inline const render_text_glyph_atlas_upload_result_packet_snapshot*
find_matching_upload_result_packet(
    const render_text_glyph_atlas_upload_result_snapshot& result,
    const render_text_frame_draw_packet_snapshot& draw_packet) {
  if (!draw_packet.atlas_upload_request_id.empty()) {
    const auto by_request_id = std::find_if(
        result.packets.begin(), result.packets.end(),
        [&](const render_text_glyph_atlas_upload_result_packet_snapshot& packet) {
          return packet.upload_request_id == draw_packet.atlas_upload_request_id;
        });
    if (by_request_id != result.packets.end()) {
      return &*by_request_id;
    }
  }

  const auto by_materialization = std::find_if(
      result.packets.begin(), result.packets.end(),
      [&](const render_text_glyph_atlas_upload_result_packet_snapshot& packet) {
        return packet.materialization_index == draw_packet.materialization_index &&
               packet.cache_key == draw_packet.cache_key &&
               packet.page_id == draw_packet.page_id;
      });
  if (by_materialization != result.packets.end()) {
    return &*by_materialization;
  }

  const auto by_materialization_index = std::find_if(
      result.packets.begin(), result.packets.end(),
      [&](const render_text_glyph_atlas_upload_result_packet_snapshot& packet) {
        return packet.materialization_index == draw_packet.materialization_index;
      });
  if (by_materialization_index != result.packets.end()) {
    return &*by_materialization_index;
  }

  return nullptr;
}

[[nodiscard]] inline render_text_frame_upload_handoff_packet_status
handoff_status_for(
    const render_text_frame_draw_packet_snapshot& draw_packet,
    const render_text_glyph_atlas_upload_result_packet_snapshot* upload_packet) {
  if (!draw_packet.drawable()) {
    return render_text_frame_upload_handoff_packet_status::blocked_draw_packet;
  }
  if (upload_packet == nullptr) {
    return render_text_frame_upload_handoff_packet_status::
        blocked_missing_upload_result;
  }
  if (upload_packet->rejected) {
    return render_text_frame_upload_handoff_packet_status::blocked_upload_rejected;
  }
  if (upload_packet->clean_reuse_accepted) {
    return render_text_frame_upload_handoff_packet_status::ready_clean_reuse;
  }
  return render_text_frame_upload_handoff_packet_status::ready_uploaded;
}

[[nodiscard]] inline std::string handoff_blocker_reason_for(
    const render_text_frame_draw_packet_snapshot& draw_packet,
    const render_text_glyph_atlas_upload_result_packet_snapshot* upload_packet,
    render_text_frame_upload_handoff_packet_status status) {
  switch (status) {
    case render_text_frame_upload_handoff_packet_status::ready_uploaded:
    case render_text_frame_upload_handoff_packet_status::ready_clean_reuse:
      return {};
    case render_text_frame_upload_handoff_packet_status::blocked_draw_packet:
      return draw_packet.diagnostic.empty() ? "draw packet is not ready" :
                                             draw_packet.diagnostic;
    case render_text_frame_upload_handoff_packet_status::
        blocked_missing_upload_result:
      return "draw packet has no matching atlas upload result";
    case render_text_frame_upload_handoff_packet_status::
        blocked_missing_draw_packet:
      return "atlas upload result has no matching draw packet";
    case render_text_frame_upload_handoff_packet_status::blocked_upload_rejected:
      if (upload_packet != nullptr && !upload_packet->blocker_reason.empty()) {
        return upload_packet->blocker_reason;
      }
      return "atlas upload result rejected packet";
  }
  return "unknown atlas handoff blocker";
}

inline void append_unique_string(std::vector<std::string>& values,
                                 const std::string& value) {
  if (value.empty()) {
    return;
  }
  if (std::find(values.begin(), values.end(), value) == values.end()) {
    values.push_back(value);
  }
}

[[nodiscard]] inline render_text_frame_upload_handoff_page_snapshot*
find_handoff_page(std::vector<render_text_frame_upload_handoff_page_snapshot>& pages,
                  const std::string& stable_page_id) {
  const auto found = std::find_if(
      pages.begin(), pages.end(),
      [&](const render_text_frame_upload_handoff_page_snapshot& page) {
        return page.stable_page_id == stable_page_id;
      });
  if (found == pages.end()) {
    return nullptr;
  }
  return &*found;
}

inline void append_packet_to_handoff_policy(
    render_text_frame_upload_handoff_snapshot& snapshot,
    const render_text_frame_upload_handoff_packet_snapshot& packet) {
  auto& policy = snapshot.policy;
  if (packet.requested) {
    ++policy.requested_glyph_packet_count;
  }
  if (packet.ready) {
    ++policy.ready_glyph_packet_count;
    snapshot.ready_packet_ids.push_back(packet.handoff_id);
  }
  if (packet.blocked) {
    ++policy.blocked_glyph_packet_count;
    snapshot.blocker_packet_ids.push_back(packet.handoff_id);
  }
  if (packet.uploaded) {
    ++policy.uploaded_glyph_count;
  }
  if (packet.clean_reuse) {
    ++policy.clean_reuse_glyph_count;
  }
  if (packet.missing_upload_result) {
    ++policy.upload_result_missing_count;
  }
  if (packet.missing_draw_packet) {
    ++policy.draw_packet_missing_count;
  }
  if (packet.has_upload_result &&
      (packet.upload_result_status ==
           render_text_glyph_atlas_upload_result_status::rejected_blocked_packet ||
       packet.upload_result_status ==
           render_text_glyph_atlas_upload_result_status::
               rejected_missing_upload_request_id)) {
    ++policy.upload_result_rejected_count;
  }
  if (packet.missing_glyph) {
    ++policy.missing_glyph_count;
  }
  if (packet.missing_materialization) {
    ++policy.missing_materialization_count;
  }
  if (packet.fallback_incomplete) {
    ++policy.fallback_incomplete_count;
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
  policy.has_blockers = policy.has_blockers || packet.blocked;
  policy.has_uploads = policy.has_uploads || packet.uploaded;

  auto* page =
      detail::find_handoff_page(snapshot.pages, packet.stable_page_id);
  if (page == nullptr) {
    snapshot.pages.push_back(
        {.stable_page_id = packet.stable_page_id,
         .page_id = packet.page_id,
         .page_revision = packet.page_revision});
    page = &snapshot.pages.back();
  }
  ++page->packet_count;
  if (packet.ready) {
    ++page->ready_packet_count;
  }
  if (packet.blocked) {
    ++page->blocked_packet_count;
  }
  if (packet.uploaded) {
    ++page->uploaded_glyph_count;
    page->has_uploads = true;
    detail::append_unique_string(snapshot.uploaded_page_ids,
                                 packet.stable_page_id);
  }
  if (packet.clean_reuse) {
    ++page->clean_reuse_glyph_count;
  }
  if (packet.missing_glyph) {
    ++page->missing_glyph_count;
  }
  if (packet.missing_draw_packet) {
    ++page->missing_draw_packet_count;
  }
  if (packet.missing_materialization) {
    ++page->missing_materialization_count;
  }
  page->upload_rgba_bytes += packet.upload_rgba_bytes;
  page->has_blockers = page->has_blockers || packet.blocked;
}

}  // namespace detail

[[nodiscard]] inline render_text_frame_upload_handoff_packet_snapshot
make_render_text_frame_upload_handoff_packet(
    const render_text_frame_draw_packet_snapshot& draw_packet,
    const render_text_glyph_atlas_upload_result_packet_snapshot* upload_packet) {
  const auto status =
      detail::handoff_status_for(draw_packet, upload_packet);
  const bool ready =
      status == render_text_frame_upload_handoff_packet_status::ready_uploaded ||
      status ==
          render_text_frame_upload_handoff_packet_status::ready_clean_reuse;
  const bool blocked = !ready;
  const bool has_result = upload_packet != nullptr;
  const bool uploaded =
      has_result && upload_packet->upload_accepted &&
      status == render_text_frame_upload_handoff_packet_status::ready_uploaded;
  const bool clean_reuse =
      has_result && upload_packet->clean_reuse_accepted &&
      status ==
          render_text_frame_upload_handoff_packet_status::ready_clean_reuse;
  const bool missing_result =
      status == render_text_frame_upload_handoff_packet_status::
                    blocked_missing_upload_result;
  const bool missing_materialization =
      draw_packet.status ==
          render_text_frame_draw_packet_status::skipped_materialization ||
      missing_result;
  const bool missing_glyph =
      !draw_packet.glyph_supported ||
      draw_packet.status ==
          render_text_frame_draw_packet_status::missing_cache_key ||
      (has_result && upload_packet->missing_cache_key);
  const auto stable_key =
      detail::text_frame_upload_handoff_stable_key_for(draw_packet);
  const auto blocker =
      detail::handoff_blocker_reason_for(draw_packet, upload_packet, status);
  render_text_atlas_packet_consumption_evidence atlas_consumption =
      draw_packet.atlas_consumption;
  atlas_consumption.missing_glyph = missing_glyph;
  atlas_consumption.clean_reuse = clean_reuse;
  return {
      .handoff_id = detail::text_frame_upload_handoff_packet_id_for(draw_packet),
      .stable_packet_key = stable_key,
      .frame_id = draw_packet.frame_id,
      .source_label = draw_packet.source_label,
      .draw_packet_id = draw_packet.packet_id,
      .upload_operation_id = has_result ? upload_packet->operation_id : "",
      .upload_request_id = has_result ? upload_packet->upload_request_id
                                      : draw_packet.atlas_upload_request_id,
      .stable_page_id = has_result ? upload_packet->stable_page_id
                                   : ("page:" + std::to_string(draw_packet.page_id)),
      .materialization_index = draw_packet.materialization_index,
      .run_index = draw_packet.run_index,
      .cluster_byte_offset = draw_packet.cluster_byte_offset,
      .cluster_byte_count = draw_packet.cluster_byte_count,
      .cache_key = draw_packet.cache_key,
      .resolved_glyph_id = draw_packet.resolved_glyph_id,
      .resolved_face_id = draw_packet.resolved_face_id,
      .page_id = has_result ? upload_packet->page_id : draw_packet.page_id,
      .page_revision = draw_packet.page_revision,
      .layout_bounds = draw_packet.layout_bounds,
      .atlas_bounds = draw_packet.atlas_bounds,
      .update_bounds = has_result ? upload_packet->update_bounds
                                  : draw_packet.atlas_bounds,
      .draw_status = draw_packet.status,
      .upload_result_status =
          has_result ? upload_packet->result_status
                     : render_text_glyph_atlas_upload_result_status::
                           rejected_missing_upload_request_id,
      .handoff_status = status,
      .has_upload_result = has_result,
      .requested = true,
      .ready = ready,
      .blocked = blocked,
      .drawable = draw_packet.drawable(),
      .uploaded = uploaded,
      .clean_reuse = clean_reuse,
      .missing_upload_result = missing_result,
      .missing_glyph = missing_glyph,
      .missing_materialization = missing_materialization,
      .fallback_incomplete = draw_packet.fallback_incomplete,
      .used_deterministic_fallback = draw_packet.used_deterministic_fallback,
      .used_real_backend = draw_packet.used_real_backend,
      .glyph_supported = draw_packet.glyph_supported,
      .upload_consumed = draw_packet.upload_consumed,
      .upload_rgba_bytes = uploaded ? upload_packet->rgba_byte_count : 0U,
      .atlas_consumption = atlas_consumption,
      .blocker_reason = blocker,
      .diagnostic = ready ? "glyph packet is ready for frame upload handoff"
                          : blocker,
  };
}

[[nodiscard]] inline render_text_frame_upload_handoff_packet_snapshot
make_render_text_frame_upload_handoff_missing_draw_packet(
    const render_text_frame_snapshot& frame,
    const render_text_glyph_atlas_upload_result_packet_snapshot& upload_packet) {
  const std::string stable_key =
      "text-frame-upload-handoff:v1:item=orphan:mat=" +
      std::to_string(upload_packet.materialization_index) + ":run=" +
      std::to_string(upload_packet.run_index) + ":cluster=" +
      std::to_string(upload_packet.cluster_index) + ":face=" +
      std::to_string(upload_packet.cache_key.face_id) + ":glyph=" +
      std::to_string(upload_packet.cache_key.glyph_id) + ":cache-face=" +
      std::to_string(upload_packet.cache_key.face_id) + ":cache-glyph=" +
      std::to_string(upload_packet.cache_key.glyph_id) + ":cache-px=" +
      std::to_string(upload_packet.cache_key.pixel_size);
  return {
      .handoff_id = "text-frame-upload-handoff:v1:frame=" + frame.frame_id +
                    ":missing-draw:operation=" + upload_packet.operation_id,
      .stable_packet_key = stable_key,
      .frame_id = frame.frame_id,
      .source_label = frame.source_label,
      .upload_operation_id = upload_packet.operation_id,
      .upload_request_id = upload_packet.upload_request_id,
      .stable_page_id = upload_packet.stable_page_id,
      .materialization_index = upload_packet.materialization_index,
      .run_index = upload_packet.run_index,
      .cluster_byte_offset = upload_packet.cluster_index,
      .cache_key = upload_packet.cache_key,
      .resolved_glyph_id = upload_packet.cache_key.glyph_id,
      .resolved_face_id = upload_packet.cache_key.face_id,
      .page_id = upload_packet.page_id,
      .page_revision = upload_packet.page.revision,
      .update_bounds = upload_packet.update_bounds,
      .upload_result_status = upload_packet.result_status,
      .handoff_status =
          render_text_frame_upload_handoff_packet_status::blocked_missing_draw_packet,
      .has_upload_result = true,
      .blocked = true,
      .missing_draw_packet = true,
      .missing_glyph = upload_packet.missing_cache_key,
      .missing_materialization = upload_packet.blocked,
      .glyph_supported = !upload_packet.missing_cache_key,
      .atlas_consumption =
          render_text_atlas_packet_consumption_evidence{
              .cluster_index = upload_packet.cluster_index,
              .run_index = upload_packet.run_index,
              .cluster_byte_offset = upload_packet.cluster_index,
              .cache_key = upload_packet.cache_key,
              .resolved_glyph_id = upload_packet.cache_key.glyph_id,
              .resolved_face_id = upload_packet.cache_key.face_id,
              .upload_generation = upload_packet.page.revision,
              .missing_glyph = upload_packet.missing_cache_key,
          },
      .blocker_reason = "atlas upload result has no matching draw packet",
      .diagnostic = "atlas upload result has no matching draw packet",
  };
}

[[nodiscard]] inline render_text_frame_upload_handoff_snapshot
make_render_text_frame_upload_handoff(
    render_text_frame_upload_handoff_request request) {
  render_text_frame_upload_handoff_snapshot snapshot{
      .frame_id = request.frame.frame_id,
      .source_label = request.frame.source_label,
      .frame_status = request.frame.status,
      .policy =
          {.frame_ready_for_renderer = request.draw_plan.frame_ready_for_renderer},
      .diagnostic = "text frame upload handoff snapshot built from draw plan and "
                    "atlas upload result diagnostics",
  };

  snapshot.packets.reserve(request.draw_plan.packets.size());
  std::vector<bool> matched_upload_packets(request.upload_result.packets.size(), false);
  for (const auto& draw_packet : request.draw_plan.packets) {
    const auto* upload_packet = detail::find_matching_upload_result_packet(
        request.upload_result, draw_packet);
    if (upload_packet != nullptr && !request.upload_result.packets.empty()) {
      const auto matched_index = static_cast<std::size_t>(
          upload_packet - request.upload_result.packets.data());
      if (matched_index < matched_upload_packets.size()) {
        matched_upload_packets[matched_index] = true;
      }
    }
    auto handoff_packet =
        make_render_text_frame_upload_handoff_packet(draw_packet, upload_packet);
    detail::append_packet_to_handoff_policy(snapshot, handoff_packet);
    snapshot.packets.push_back(std::move(handoff_packet));
  }
  for (std::size_t index = 0; index < request.upload_result.packets.size(); ++index) {
    if (matched_upload_packets[index]) {
      continue;
    }
    auto handoff_packet = make_render_text_frame_upload_handoff_missing_draw_packet(
        request.frame, request.upload_result.packets[index]);
    detail::append_packet_to_handoff_policy(snapshot, handoff_packet);
    snapshot.packets.push_back(std::move(handoff_packet));
  }

  snapshot.policy.uploaded_page_count = snapshot.uploaded_page_ids.size();
  if (!snapshot.policy.frame_ready_for_renderer &&
      !snapshot.policy.has_blockers) {
    snapshot.policy.has_blockers = true;
  }
  return snapshot;
}

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

namespace detail {

[[nodiscard]] inline std::ptrdiff_t resource_packet_consumption_delta(
    const std::size_t before,
    const std::size_t after) {
  return static_cast<std::ptrdiff_t>(after) -
         static_cast<std::ptrdiff_t>(before);
}

[[nodiscard]] inline bool resource_packet_identity_duplicate(
    const std::vector<render_text_frame_resource_packet_materialization_entry>&
        entries,
    const std::string& resource_packet_id) {
  if (resource_packet_id.empty()) {
    return false;
  }
  return std::count_if(
             entries.begin(), entries.end(),
             [&](const render_text_frame_resource_packet_materialization_entry&
                     entry) {
               return entry.resource_packet_id == resource_packet_id;
             }) > 1;
}

[[nodiscard]] inline const render_text_frame_resource_packet_materialization_entry*
find_unmatched_resource_packet_entry(
    const std::vector<render_text_frame_resource_packet_materialization_entry>&
        entries,
    const std::vector<bool>& matched,
    const std::string& resource_packet_id) {
  if (resource_packet_id.empty()) {
    return nullptr;
  }
  for (std::size_t index = 0; index < entries.size(); ++index) {
    if (index < matched.size() && matched[index]) {
      continue;
    }
    if (entries[index].resource_packet_id == resource_packet_id) {
      return &entries[index];
    }
  }
  return nullptr;
}

[[nodiscard]] inline std::size_t resource_packet_entry_index(
    const std::vector<render_text_frame_resource_packet_materialization_entry>&
        entries,
    const render_text_frame_resource_packet_materialization_entry* entry) {
  if (entry == nullptr || entries.empty()) {
    return entries.size();
  }
  const auto index = static_cast<std::size_t>(entry - entries.data());
  return index < entries.size() ? index : entries.size();
}

[[nodiscard]] inline const render_text_frame_resource_packet_materialization_page_snapshot*
find_resource_materialization_page(
    const std::vector<render_text_frame_resource_packet_materialization_page_snapshot>&
        pages,
    const std::string& stable_page_id) {
  if (stable_page_id.empty()) {
    return nullptr;
  }
  const auto found = std::find_if(
      pages.begin(), pages.end(),
      [&](const render_text_frame_resource_packet_materialization_page_snapshot&
              page) { return page.stable_page_id == stable_page_id; });
  return found == pages.end() ? nullptr : &*found;
}

[[nodiscard]] inline render_text_frame_resource_packet_consumption_packet_diff
diff_resource_packet_consumption_entries(
    const render_text_frame_resource_packet_materialization_entry* before,
    const render_text_frame_resource_packet_materialization_entry* after,
    const bool duplicate_identity) {
  const bool added = before == nullptr && after != nullptr;
  const bool removed = before != nullptr && after == nullptr;
  const auto* identity_source = after == nullptr ? before : after;
  const bool missing_identity =
      identity_source == nullptr || identity_source->resource_packet_id.empty();
  const bool stable_packet_key_changed =
      before != nullptr && after != nullptr &&
      before->stable_packet_key != after->stable_packet_key;
  const bool page_id_changed =
      before != nullptr && after != nullptr && before->page_id != after->page_id;
  const bool page_revision_changed =
      before != nullptr && after != nullptr &&
      before->page_revision != after->page_revision;
  const bool page_extent_changed =
      before != nullptr && after != nullptr &&
      (before->page_width != after->page_width ||
       before->page_height != after->page_height);
  const bool sampler_key_changed =
      before != nullptr && after != nullptr &&
      before->sampler_key != after->sampler_key;
  const bool uv_bounds_changed =
      before != nullptr && after != nullptr &&
      !render_text_frame_draw_uv_rects_equal(before->uv_bounds, after->uv_bounds);
  const bool layout_bounds_changed =
      before != nullptr && after != nullptr &&
      !render_text_frame_snapshot_rects_equal(before->layout_bounds,
                                              after->layout_bounds);
  const bool upload_request_id_changed =
      before != nullptr && after != nullptr &&
      before->upload_request_id != after->upload_request_id;
  const bool upload_operation_id_changed =
      before != nullptr && after != nullptr &&
      before->upload_operation_id != after->upload_operation_id;
  const bool uploaded_byte_count_changed =
      before != nullptr && after != nullptr &&
      before->upload_rgba_bytes != after->upload_rgba_bytes;
  const bool fallback_flag_changed =
      before != nullptr && after != nullptr &&
      before->used_deterministic_fallback != after->used_deterministic_fallback;
  const bool backend_flag_changed =
      before != nullptr && after != nullptr &&
      before->used_real_backend != after->used_real_backend;
  const bool readiness_changed =
      before != nullptr && after != nullptr && before->ready != after->ready;
  const bool readiness_regressed =
      before != nullptr && after != nullptr && before->ready && after->blocked;
  const bool readiness_improved =
      before != nullptr && after != nullptr && before->blocked && after->ready;
  const bool status_changed =
      before != nullptr && after != nullptr && before->status != after->status;
  const bool blocker_status_changed =
      before != nullptr && after != nullptr &&
      (before->blocked != after->blocked ||
       before->blocker_summary != after->blocker_summary || status_changed);
  const bool changed =
      !added && !removed &&
      (stable_packet_key_changed || page_id_changed || page_revision_changed ||
       page_extent_changed || sampler_key_changed || uv_bounds_changed ||
       layout_bounds_changed || upload_request_id_changed ||
       upload_operation_id_changed || uploaded_byte_count_changed ||
       fallback_flag_changed || backend_flag_changed || readiness_changed ||
       blocker_status_changed || duplicate_identity || missing_identity);
  const bool unchanged = !added && !removed && !changed;

  return {
      .resource_packet_id =
          identity_source == nullptr ? std::string{} : identity_source->resource_packet_id,
      .previous_stable_packet_key =
          before == nullptr ? std::string{} : before->stable_packet_key,
      .current_stable_packet_key =
          after == nullptr ? std::string{} : after->stable_packet_key,
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
      .stable_packet_key_changed = stable_packet_key_changed,
      .page_id_changed = page_id_changed,
      .page_revision_changed = page_revision_changed,
      .page_extent_changed = page_extent_changed,
      .sampler_key_changed = sampler_key_changed,
      .uv_bounds_changed = uv_bounds_changed,
      .layout_bounds_changed = layout_bounds_changed,
      .upload_request_id_changed = upload_request_id_changed,
      .upload_operation_id_changed = upload_operation_id_changed,
      .uploaded_byte_count_changed = uploaded_byte_count_changed,
      .fallback_flag_changed = fallback_flag_changed,
      .backend_flag_changed = backend_flag_changed,
      .readiness_changed = readiness_changed,
      .readiness_regressed = readiness_regressed,
      .readiness_improved = readiness_improved,
      .blocker_status_changed = blocker_status_changed,
      .status_changed = status_changed,
      .upload_rgba_bytes_delta =
          resource_packet_consumption_delta(
              before == nullptr ? 0U : before->upload_rgba_bytes,
              after == nullptr ? 0U : after->upload_rgba_bytes),
      .previous_page_id = before == nullptr ? 0U : before->page_id,
      .current_page_id = after == nullptr ? 0U : after->page_id,
      .previous_page_revision = before == nullptr ? 0U : before->page_revision,
      .current_page_revision = after == nullptr ? 0U : after->page_revision,
      .previous_page_width = before == nullptr ? 0U : before->page_width,
      .previous_page_height = before == nullptr ? 0U : before->page_height,
      .current_page_width = after == nullptr ? 0U : after->page_width,
      .current_page_height = after == nullptr ? 0U : after->page_height,
      .previous_status =
          before == nullptr
              ? render_text_frame_resource_packet_materialization_status::
                    blocked_missing_upload_handoff
              : before->status,
      .current_status =
          after == nullptr
              ? render_text_frame_resource_packet_materialization_status::
                    blocked_missing_upload_handoff
              : after->status,
  };
}

[[nodiscard]] inline render_text_frame_resource_packet_consumption_page_diff
diff_resource_packet_consumption_pages(
    const render_text_frame_resource_packet_materialization_page_snapshot* before,
    const render_text_frame_resource_packet_materialization_page_snapshot* after) {
  const bool added = before == nullptr && after != nullptr;
  const bool removed = before != nullptr && after == nullptr;
  const auto* identity_source = after == nullptr ? before : after;
  const bool missing_identity =
      identity_source == nullptr || identity_source->stable_page_id.empty();
  const bool page_id_changed =
      before != nullptr && after != nullptr && before->page_id != after->page_id;
  const bool page_revision_changed =
      before != nullptr && after != nullptr &&
      before->page_revision != after->page_revision;
  const bool page_extent_changed =
      before != nullptr && after != nullptr &&
      (before->page_width != after->page_width ||
       before->page_height != after->page_height);
  const bool sampler_key_changed =
      before != nullptr && after != nullptr &&
      before->sampler_key != after->sampler_key;
  const bool readiness_changed =
      before != nullptr && after != nullptr &&
      before->ready_packet_count != after->ready_packet_count;
  const bool blocker_changed =
      before != nullptr && after != nullptr &&
      before->blocked_packet_count != after->blocked_packet_count;
  const bool changed =
      !added && !removed &&
      (page_id_changed || page_revision_changed || page_extent_changed ||
       sampler_key_changed || readiness_changed || blocker_changed ||
       before->packet_count != after->packet_count ||
       before->upload_rgba_bytes != after->upload_rgba_bytes ||
       missing_identity);
  const bool unchanged = !added && !removed && !changed;
  return {
      .stable_page_id =
          identity_source == nullptr ? std::string{} : identity_source->stable_page_id,
      .previous_sampler_key = before == nullptr ? std::string{} : before->sampler_key,
      .current_sampler_key = after == nullptr ? std::string{} : after->sampler_key,
      .added = added,
      .removed = removed,
      .changed = changed,
      .unchanged = unchanged,
      .missing_identity = missing_identity,
      .page_id_changed = page_id_changed,
      .page_revision_changed = page_revision_changed,
      .page_extent_changed = page_extent_changed,
      .sampler_key_changed = sampler_key_changed,
      .readiness_changed = readiness_changed,
      .blocker_changed = blocker_changed,
      .packet_count_delta = resource_packet_consumption_delta(
          before == nullptr ? 0U : before->packet_count,
          after == nullptr ? 0U : after->packet_count),
      .ready_packet_count_delta = resource_packet_consumption_delta(
          before == nullptr ? 0U : before->ready_packet_count,
          after == nullptr ? 0U : after->ready_packet_count),
      .blocked_packet_count_delta = resource_packet_consumption_delta(
          before == nullptr ? 0U : before->blocked_packet_count,
          after == nullptr ? 0U : after->blocked_packet_count),
      .upload_rgba_bytes_delta = resource_packet_consumption_delta(
          before == nullptr ? 0U : before->upload_rgba_bytes,
          after == nullptr ? 0U : after->upload_rgba_bytes),
      .previous_page_id = before == nullptr ? 0U : before->page_id,
      .current_page_id = after == nullptr ? 0U : after->page_id,
      .previous_page_revision = before == nullptr ? 0U : before->page_revision,
      .current_page_revision = after == nullptr ? 0U : after->page_revision,
      .previous_page_width = before == nullptr ? 0U : before->page_width,
      .previous_page_height = before == nullptr ? 0U : before->page_height,
      .current_page_width = after == nullptr ? 0U : after->page_width,
      .current_page_height = after == nullptr ? 0U : after->page_height,
  };
}

inline void append_resource_packet_consumption_packet_diff(
    render_text_frame_resource_packet_consumption_diff_snapshot& diff,
    render_text_frame_resource_packet_consumption_packet_diff packet_diff) {
  auto& policy = diff.policy;
  if (packet_diff.added) {
    ++policy.added_packet_count;
    detail::append_unique_string(diff.added_resource_packet_ids,
                                 packet_diff.resource_packet_id);
  }
  if (packet_diff.removed) {
    ++policy.removed_packet_count;
    detail::append_unique_string(diff.removed_resource_packet_ids,
                                 packet_diff.resource_packet_id);
  }
  if (packet_diff.changed) {
    ++policy.changed_packet_count;
    detail::append_unique_string(diff.changed_resource_packet_ids,
                                 packet_diff.resource_packet_id);
  }
  if (packet_diff.unchanged) {
    ++policy.unchanged_packet_count;
    detail::append_unique_string(diff.unchanged_resource_packet_ids,
                                 packet_diff.resource_packet_id);
  }
  if (packet_diff.readiness_regressed) {
    ++policy.readiness_regression_count;
    detail::append_unique_string(diff.readiness_regressed_resource_packet_ids,
                                 packet_diff.resource_packet_id);
  }
  if (packet_diff.readiness_improved) {
    ++policy.readiness_improvement_count;
    detail::append_unique_string(diff.readiness_improved_resource_packet_ids,
                                 packet_diff.resource_packet_id);
  }
  if (packet_diff.stable_packet_key_changed) {
    ++policy.stable_packet_key_changed_count;
  }
  if (packet_diff.page_id_changed) {
    ++policy.page_id_changed_count;
  }
  if (packet_diff.page_revision_changed) {
    ++policy.page_revision_changed_count;
  }
  if (packet_diff.page_extent_changed) {
    ++policy.page_extent_changed_count;
  }
  if (packet_diff.sampler_key_changed) {
    ++policy.sampler_key_changed_count;
  }
  if (packet_diff.uv_bounds_changed) {
    ++policy.uv_bounds_changed_count;
  }
  if (packet_diff.layout_bounds_changed) {
    ++policy.layout_bounds_changed_count;
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
  if (packet_diff.fallback_flag_changed) {
    ++policy.fallback_flag_changed_count;
  }
  if (packet_diff.backend_flag_changed) {
    ++policy.backend_flag_changed_count;
  }
  if (packet_diff.readiness_changed || packet_diff.blocker_status_changed ||
      packet_diff.status_changed) {
    ++policy.readiness_or_blocker_status_changed_count;
  }
  if (packet_diff.duplicate_identity) {
    ++policy.duplicate_identity_count;
    detail::append_unique_string(diff.duplicate_identity_resource_packet_ids,
                                 packet_diff.resource_packet_id);
  }
  if (packet_diff.missing_identity) {
    ++policy.missing_identity_count;
    detail::append_unique_string(diff.missing_identity_resource_packet_ids,
                                 packet_diff.resource_packet_id);
  }
  diff.packet_diffs.push_back(std::move(packet_diff));
}

inline void append_resource_packet_consumption_page_diff(
    render_text_frame_resource_packet_consumption_diff_snapshot& diff,
    render_text_frame_resource_packet_consumption_page_diff page_diff) {
  auto& policy = diff.policy;
  if (page_diff.added) {
    ++policy.added_page_count;
  }
  if (page_diff.removed) {
    ++policy.removed_page_count;
  }
  if (page_diff.changed) {
    ++policy.changed_page_count;
    detail::append_unique_string(diff.changed_page_ids,
                                 page_diff.stable_page_id);
  }
  if (page_diff.unchanged) {
    ++policy.unchanged_page_count;
  }
  diff.page_diffs.push_back(std::move(page_diff));
}

}  // namespace detail

[[nodiscard]] inline render_text_frame_resource_packet_consumption_diff_snapshot
diff_render_text_frame_resource_packet_materializations(
    const render_text_frame_resource_packet_materialization& before,
    const render_text_frame_resource_packet_materialization& after) {
  render_text_frame_resource_packet_consumption_diff_snapshot diff{
      .previous_frame_id = before.frame_id,
      .current_frame_id = after.frame_id,
      .previous_ready_for_renderer = before.frame_ready_for_renderer,
      .current_ready_for_renderer = after.frame_ready_for_renderer,
      .policy =
          {.resource_packet_count_delta =
               detail::resource_packet_consumption_delta(
                   before.policy.resource_packet_count,
                   after.policy.resource_packet_count),
           .ready_packet_count_delta =
               detail::resource_packet_consumption_delta(
                   before.policy.ready_packet_count,
                   after.policy.ready_packet_count),
           .blocked_packet_count_delta =
               detail::resource_packet_consumption_delta(
                   before.policy.blocked_packet_count,
                   after.policy.blocked_packet_count),
           .uploaded_packet_count_delta =
               detail::resource_packet_consumption_delta(
                   before.policy.uploaded_packet_count,
                   after.policy.uploaded_packet_count),
           .clean_reuse_packet_count_delta =
               detail::resource_packet_consumption_delta(
                   before.policy.clean_reuse_packet_count,
                   after.policy.clean_reuse_packet_count),
           .page_count_delta =
               detail::resource_packet_consumption_delta(
                   before.policy.page_count, after.policy.page_count),
           .sampler_count_delta =
               detail::resource_packet_consumption_delta(
                   before.policy.sampler_count, after.policy.sampler_count),
           .deterministic_fallback_count_delta =
               detail::resource_packet_consumption_delta(
                   before.policy.deterministic_fallback_count,
                   after.policy.deterministic_fallback_count),
           .real_backend_count_delta =
               detail::resource_packet_consumption_delta(
                   before.policy.real_backend_count,
                   after.policy.real_backend_count),
           .total_upload_rgba_bytes_delta =
               detail::resource_packet_consumption_delta(
                   before.policy.total_upload_rgba_bytes,
                   after.policy.total_upload_rgba_bytes),
           .frame_id_changed = before.frame_id != after.frame_id,
           .frame_ready_changed =
               before.frame_ready_for_renderer != after.frame_ready_for_renderer,
           .blockers_changed =
               before.policy.has_blockers != after.policy.has_blockers},
  };

  std::vector<bool> matched_after_entries(after.entries.size(), false);
  diff.packet_diffs.reserve(before.entries.size() + after.entries.size());
  for (const auto& before_entry : before.entries) {
    const auto* after_entry = detail::find_unmatched_resource_packet_entry(
        after.entries, matched_after_entries, before_entry.resource_packet_id);
    const std::size_t after_index =
        detail::resource_packet_entry_index(after.entries, after_entry);
    if (after_index < matched_after_entries.size()) {
      matched_after_entries[after_index] = true;
    }
    const bool duplicate_identity =
        detail::resource_packet_identity_duplicate(
            before.entries, before_entry.resource_packet_id) ||
        detail::resource_packet_identity_duplicate(
            after.entries, before_entry.resource_packet_id);
    detail::append_resource_packet_consumption_packet_diff(
        diff,
        detail::diff_resource_packet_consumption_entries(
            &before_entry, after_entry, duplicate_identity));
  }

  for (std::size_t index = 0; index < after.entries.size(); ++index) {
    if (matched_after_entries[index]) {
      continue;
    }
    const auto& after_entry = after.entries[index];
    const bool duplicate_identity = detail::resource_packet_identity_duplicate(
        after.entries, after_entry.resource_packet_id);
    detail::append_resource_packet_consumption_packet_diff(
        diff,
        detail::diff_resource_packet_consumption_entries(
            nullptr, &after_entry, duplicate_identity));
  }

  std::vector<bool> matched_after_pages(after.pages.size(), false);
  diff.page_diffs.reserve(before.pages.size() + after.pages.size());
  for (const auto& before_page : before.pages) {
    const auto* after_page = detail::find_resource_materialization_page(
        after.pages, before_page.stable_page_id);
    if (after_page != nullptr && !after.pages.empty()) {
      const auto after_index =
          static_cast<std::size_t>(after_page - after.pages.data());
      if (after_index < matched_after_pages.size()) {
        matched_after_pages[after_index] = true;
      }
    }
    detail::append_resource_packet_consumption_page_diff(
        diff,
        detail::diff_resource_packet_consumption_pages(&before_page, after_page));
  }
  for (std::size_t index = 0; index < after.pages.size(); ++index) {
    if (matched_after_pages[index]) {
      continue;
    }
    detail::append_resource_packet_consumption_page_diff(
        diff,
        detail::diff_resource_packet_consumption_pages(nullptr,
                                                       &after.pages[index]));
  }

  const bool aggregate_deltas_stable =
      diff.policy.resource_packet_count_delta == 0 &&
      diff.policy.ready_packet_count_delta == 0 &&
      diff.policy.blocked_packet_count_delta == 0 &&
      diff.policy.uploaded_packet_count_delta == 0 &&
      diff.policy.clean_reuse_packet_count_delta == 0 &&
      diff.policy.page_count_delta == 0 &&
      diff.policy.sampler_count_delta == 0 &&
      diff.policy.deterministic_fallback_count_delta == 0 &&
      diff.policy.real_backend_count_delta == 0 &&
      diff.policy.total_upload_rgba_bytes_delta == 0;
  diff.policy.stable_no_change =
      diff.policy.added_packet_count == 0U &&
      diff.policy.removed_packet_count == 0U &&
      diff.policy.changed_packet_count == 0U &&
      diff.policy.added_page_count == 0U &&
      diff.policy.removed_page_count == 0U &&
      diff.policy.changed_page_count == 0U &&
      !diff.policy.frame_id_changed && !diff.policy.frame_ready_changed &&
      !diff.policy.blockers_changed &&
      diff.policy.duplicate_identity_count == 0U &&
      diff.policy.missing_identity_count == 0U && aggregate_deltas_stable;
  diff.summary =
      diff.policy.stable_no_change
          ? "text resource packet consumption identities are stable"
          : std::to_string(diff.policy.added_packet_count) + " added, " +
                std::to_string(diff.policy.removed_packet_count) +
                " removed, " +
                std::to_string(diff.policy.changed_packet_count) +
                " changed text resource packet(s)";
  return diff;
}

struct render_text_frame_upload_handoff_diff_policy {
  std::ptrdiff_t requested_glyph_packet_count_delta{};
  std::ptrdiff_t ready_glyph_packet_count_delta{};
  std::ptrdiff_t blocked_glyph_packet_count_delta{};
  std::ptrdiff_t uploaded_page_count_delta{};
  std::ptrdiff_t upload_result_missing_count_delta{};
  std::ptrdiff_t draw_packet_missing_count_delta{};
  std::ptrdiff_t upload_result_rejected_count_delta{};
  std::ptrdiff_t uploaded_glyph_count_delta{};
  std::ptrdiff_t clean_reuse_glyph_count_delta{};
  std::ptrdiff_t missing_glyph_count_delta{};
  std::ptrdiff_t missing_materialization_count_delta{};
  std::ptrdiff_t fallback_incomplete_count_delta{};
  std::ptrdiff_t deterministic_fallback_count_delta{};
  std::ptrdiff_t real_backend_count_delta{};
  std::ptrdiff_t consumed_upload_count_delta{};
  std::ptrdiff_t total_upload_rgba_bytes_delta{};
  bool frame_ready_changed{};
  bool blockers_changed{};
  bool uploads_changed{};
  bool deterministic_fallback_changed{};
  bool real_backend_changed{};
};

struct render_text_frame_upload_handoff_packet_diff {
  std::string stable_packet_key{};
  std::string previous_handoff_id{};
  std::string current_handoff_id{};
  bool added{};
  bool removed{};
  bool changed{};
  bool readiness_changed{};
  bool blocked_changed{};
  bool status_changed{};
  bool page_changed{};
  bool fallback_changed{};
  bool backend_changed{};
  std::ptrdiff_t upload_rgba_bytes_delta{};
  render_text_frame_upload_handoff_packet_status previous_status{
      render_text_frame_upload_handoff_packet_status::blocked_missing_upload_result};
  render_text_frame_upload_handoff_packet_status current_status{
      render_text_frame_upload_handoff_packet_status::blocked_missing_upload_result};
};

struct render_text_frame_upload_handoff_page_diff {
  std::string stable_page_id{};
  bool added{};
  bool removed{};
  bool changed{};
  bool blocker_changed{};
  bool upload_changed{};
  std::ptrdiff_t ready_packet_count_delta{};
  std::ptrdiff_t blocked_packet_count_delta{};
  std::ptrdiff_t upload_rgba_bytes_delta{};
  render_text_revision previous_page_revision{};
  render_text_revision current_page_revision{};
  bool page_revision_changed{};
};

struct render_text_frame_upload_handoff_diff_snapshot {
  std::string previous_frame_id{};
  std::string current_frame_id{};
  render_text_frame_upload_handoff_diff_policy policy{};
  std::vector<render_text_frame_upload_handoff_packet_diff> packet_diffs{};
  std::vector<render_text_frame_upload_handoff_page_diff> page_diffs{};
  std::vector<std::string> added_packet_keys{};
  std::vector<std::string> removed_packet_keys{};
  std::vector<std::string> changed_packet_keys{};
  std::vector<std::string> added_page_ids{};
  std::vector<std::string> removed_page_ids{};
  std::vector<std::string> changed_page_ids{};
  std::size_t added_packet_count{};
  std::size_t removed_packet_count{};
  std::size_t changed_packet_count{};
  std::size_t unchanged_packet_count{};
  std::size_t added_page_count{};
  std::size_t removed_page_count{};
  std::size_t changed_page_count{};
  std::size_t unchanged_page_count{};
  std::string summary{};

  [[nodiscard]] constexpr bool has_changes() const noexcept {
    return added_packet_count != 0U || removed_packet_count != 0U ||
           changed_packet_count != 0U || added_page_count != 0U ||
           removed_page_count != 0U || changed_page_count != 0U ||
           policy.frame_ready_changed || policy.blockers_changed ||
           policy.uploads_changed || policy.deterministic_fallback_changed ||
           policy.real_backend_changed;
  }
};

namespace detail {

[[nodiscard]] inline std::ptrdiff_t text_frame_upload_handoff_delta(
    std::size_t before, std::size_t after) {
  return static_cast<std::ptrdiff_t>(after) -
         static_cast<std::ptrdiff_t>(before);
}

[[nodiscard]] inline const render_text_frame_upload_handoff_packet_snapshot*
find_handoff_packet_by_key(
    const std::vector<render_text_frame_upload_handoff_packet_snapshot>& packets,
    const std::string& stable_packet_key) {
  const auto found =
      std::find_if(packets.begin(), packets.end(),
                   [&](const render_text_frame_upload_handoff_packet_snapshot& packet) {
                     return packet.stable_packet_key == stable_packet_key;
                   });
  if (found == packets.end()) {
    return nullptr;
  }
  return &*found;
}

[[nodiscard]] inline const render_text_frame_upload_handoff_page_snapshot*
find_handoff_page_by_id(
    const std::vector<render_text_frame_upload_handoff_page_snapshot>& pages,
    const std::string& stable_page_id) {
  const auto found =
      std::find_if(pages.begin(), pages.end(),
                   [&](const render_text_frame_upload_handoff_page_snapshot& page) {
                     return page.stable_page_id == stable_page_id;
                   });
  if (found == pages.end()) {
    return nullptr;
  }
  return &*found;
}

[[nodiscard]] inline render_text_frame_upload_handoff_diff_policy
diff_handoff_policy(const render_text_frame_upload_handoff_policy_snapshot& before,
                    const render_text_frame_upload_handoff_policy_snapshot& after) {
  return {
      .requested_glyph_packet_count_delta =
          text_frame_upload_handoff_delta(before.requested_glyph_packet_count,
                                          after.requested_glyph_packet_count),
      .ready_glyph_packet_count_delta =
          text_frame_upload_handoff_delta(before.ready_glyph_packet_count,
                                          after.ready_glyph_packet_count),
      .blocked_glyph_packet_count_delta =
          text_frame_upload_handoff_delta(before.blocked_glyph_packet_count,
                                          after.blocked_glyph_packet_count),
      .uploaded_page_count_delta =
          text_frame_upload_handoff_delta(before.uploaded_page_count,
                                          after.uploaded_page_count),
      .upload_result_missing_count_delta =
          text_frame_upload_handoff_delta(before.upload_result_missing_count,
                                          after.upload_result_missing_count),
      .draw_packet_missing_count_delta =
          text_frame_upload_handoff_delta(before.draw_packet_missing_count,
                                          after.draw_packet_missing_count),
      .upload_result_rejected_count_delta =
          text_frame_upload_handoff_delta(before.upload_result_rejected_count,
                                          after.upload_result_rejected_count),
      .uploaded_glyph_count_delta =
          text_frame_upload_handoff_delta(before.uploaded_glyph_count,
                                          after.uploaded_glyph_count),
      .clean_reuse_glyph_count_delta =
          text_frame_upload_handoff_delta(before.clean_reuse_glyph_count,
                                          after.clean_reuse_glyph_count),
      .missing_glyph_count_delta =
          text_frame_upload_handoff_delta(before.missing_glyph_count,
                                          after.missing_glyph_count),
      .missing_materialization_count_delta =
          text_frame_upload_handoff_delta(before.missing_materialization_count,
                                          after.missing_materialization_count),
      .fallback_incomplete_count_delta =
          text_frame_upload_handoff_delta(before.fallback_incomplete_count,
                                          after.fallback_incomplete_count),
      .deterministic_fallback_count_delta =
          text_frame_upload_handoff_delta(before.deterministic_fallback_count,
                                          after.deterministic_fallback_count),
      .real_backend_count_delta =
          text_frame_upload_handoff_delta(before.real_backend_count,
                                          after.real_backend_count),
      .consumed_upload_count_delta =
          text_frame_upload_handoff_delta(before.consumed_upload_count,
                                          after.consumed_upload_count),
      .total_upload_rgba_bytes_delta =
          text_frame_upload_handoff_delta(before.total_upload_rgba_bytes,
                                          after.total_upload_rgba_bytes),
      .frame_ready_changed =
          before.frame_ready_for_renderer != after.frame_ready_for_renderer,
      .blockers_changed = before.has_blockers != after.has_blockers,
      .uploads_changed = before.has_uploads != after.has_uploads,
      .deterministic_fallback_changed =
          before.used_deterministic_fallback != after.used_deterministic_fallback,
      .real_backend_changed = before.used_real_backend != after.used_real_backend,
  };
}

[[nodiscard]] inline render_text_frame_upload_handoff_packet_diff
diff_handoff_packets(
    const render_text_frame_upload_handoff_packet_snapshot* before,
    const render_text_frame_upload_handoff_packet_snapshot* after,
    const std::string& stable_packet_key) {
  if (before == nullptr && after != nullptr) {
    return {.stable_packet_key = stable_packet_key,
            .current_handoff_id = after->handoff_id,
            .added = true,
            .changed = true,
            .current_status = after->handoff_status};
  }
  if (before != nullptr && after == nullptr) {
    return {.stable_packet_key = stable_packet_key,
            .previous_handoff_id = before->handoff_id,
            .removed = true,
            .changed = true,
            .previous_status = before->handoff_status};
  }
  if (before == nullptr || after == nullptr) {
    return {.stable_packet_key = stable_packet_key};
  }

  const bool readiness_changed = before->ready != after->ready;
  const bool blocked_changed = before->blocked != after->blocked;
  const bool status_changed = before->handoff_status != after->handoff_status;
  const bool page_changed = before->stable_page_id != after->stable_page_id ||
                            before->page_revision != after->page_revision;
  const bool fallback_changed =
      before->used_deterministic_fallback != after->used_deterministic_fallback ||
      before->fallback_incomplete != after->fallback_incomplete;
  const bool backend_changed = before->used_real_backend != after->used_real_backend;
  const auto byte_delta =
      text_frame_upload_handoff_delta(before->upload_rgba_bytes,
                                      after->upload_rgba_bytes);
  return {
      .stable_packet_key = stable_packet_key,
      .previous_handoff_id = before->handoff_id,
      .current_handoff_id = after->handoff_id,
      .changed = readiness_changed || blocked_changed || status_changed ||
                 page_changed || fallback_changed || backend_changed ||
                 byte_delta != 0,
      .readiness_changed = readiness_changed,
      .blocked_changed = blocked_changed,
      .status_changed = status_changed,
      .page_changed = page_changed,
      .fallback_changed = fallback_changed,
      .backend_changed = backend_changed,
      .upload_rgba_bytes_delta = byte_delta,
      .previous_status = before->handoff_status,
      .current_status = after->handoff_status,
  };
}

[[nodiscard]] inline render_text_frame_upload_handoff_page_diff diff_handoff_pages(
    const render_text_frame_upload_handoff_page_snapshot* before,
    const render_text_frame_upload_handoff_page_snapshot* after,
    const std::string& stable_page_id) {
  if (before == nullptr && after != nullptr) {
    return {.stable_page_id = stable_page_id,
            .added = true,
            .changed = true,
            .ready_packet_count_delta =
                static_cast<std::ptrdiff_t>(after->ready_packet_count),
            .blocked_packet_count_delta =
                static_cast<std::ptrdiff_t>(after->blocked_packet_count),
            .upload_rgba_bytes_delta =
                static_cast<std::ptrdiff_t>(after->upload_rgba_bytes),
            .current_page_revision = after->page_revision};
  }
  if (before != nullptr && after == nullptr) {
    return {.stable_page_id = stable_page_id,
            .removed = true,
            .changed = true,
            .ready_packet_count_delta =
                -static_cast<std::ptrdiff_t>(before->ready_packet_count),
            .blocked_packet_count_delta =
                -static_cast<std::ptrdiff_t>(before->blocked_packet_count),
            .upload_rgba_bytes_delta =
                -static_cast<std::ptrdiff_t>(before->upload_rgba_bytes),
            .previous_page_revision = before->page_revision};
  }
  if (before == nullptr || after == nullptr) {
    return {.stable_page_id = stable_page_id};
  }

  const auto ready_delta =
      text_frame_upload_handoff_delta(before->ready_packet_count,
                                      after->ready_packet_count);
  const auto blocked_delta =
      text_frame_upload_handoff_delta(before->blocked_packet_count,
                                      after->blocked_packet_count);
  const auto byte_delta =
      text_frame_upload_handoff_delta(before->upload_rgba_bytes,
                                      after->upload_rgba_bytes);
  const bool blocker_changed = before->has_blockers != after->has_blockers;
  const bool upload_changed = before->has_uploads != after->has_uploads;
  const bool revision_changed = before->page_revision != after->page_revision;
  return {
      .stable_page_id = stable_page_id,
      .changed = ready_delta != 0 || blocked_delta != 0 || byte_delta != 0 ||
                 blocker_changed || upload_changed || revision_changed,
      .blocker_changed = blocker_changed,
      .upload_changed = upload_changed,
      .ready_packet_count_delta = ready_delta,
      .blocked_packet_count_delta = blocked_delta,
      .upload_rgba_bytes_delta = byte_delta,
      .previous_page_revision = before->page_revision,
      .current_page_revision = after->page_revision,
      .page_revision_changed = revision_changed,
  };
}

inline void append_handoff_diff_key(std::vector<std::string>& keys,
                                    const std::string& key) {
  if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
    keys.push_back(key);
  }
}

}  // namespace detail

[[nodiscard]] inline render_text_frame_upload_handoff_diff_snapshot
diff_render_text_frame_upload_handoffs(
    const render_text_frame_upload_handoff_snapshot& before,
    const render_text_frame_upload_handoff_snapshot& after) {
  render_text_frame_upload_handoff_diff_snapshot diff{
      .previous_frame_id = before.frame_id,
      .current_frame_id = after.frame_id,
      .policy = detail::diff_handoff_policy(before.policy, after.policy),
  };

  std::vector<std::string> packet_keys{};
  for (const auto& packet : before.packets) {
    detail::append_handoff_diff_key(packet_keys, packet.stable_packet_key);
  }
  for (const auto& packet : after.packets) {
    detail::append_handoff_diff_key(packet_keys, packet.stable_packet_key);
  }
  for (const auto& key : packet_keys) {
    const auto* before_packet =
        detail::find_handoff_packet_by_key(before.packets, key);
    const auto* after_packet =
        detail::find_handoff_packet_by_key(after.packets, key);
    auto packet_diff =
        detail::diff_handoff_packets(before_packet, after_packet, key);
    if (packet_diff.added) {
      ++diff.added_packet_count;
      diff.added_packet_keys.push_back(key);
    } else if (packet_diff.removed) {
      ++diff.removed_packet_count;
      diff.removed_packet_keys.push_back(key);
    } else if (packet_diff.changed) {
      ++diff.changed_packet_count;
      diff.changed_packet_keys.push_back(key);
    } else {
      ++diff.unchanged_packet_count;
    }
    diff.packet_diffs.push_back(std::move(packet_diff));
  }

  std::vector<std::string> page_ids{};
  for (const auto& page : before.pages) {
    detail::append_handoff_diff_key(page_ids, page.stable_page_id);
  }
  for (const auto& page : after.pages) {
    detail::append_handoff_diff_key(page_ids, page.stable_page_id);
  }
  for (const auto& id : page_ids) {
    const auto* before_page = detail::find_handoff_page_by_id(before.pages, id);
    const auto* after_page = detail::find_handoff_page_by_id(after.pages, id);
    auto page_diff = detail::diff_handoff_pages(before_page, after_page, id);
    if (page_diff.added) {
      ++diff.added_page_count;
      diff.added_page_ids.push_back(id);
    } else if (page_diff.removed) {
      ++diff.removed_page_count;
      diff.removed_page_ids.push_back(id);
    } else if (page_diff.changed) {
      ++diff.changed_page_count;
      diff.changed_page_ids.push_back(id);
    } else {
      ++diff.unchanged_page_count;
    }
    diff.page_diffs.push_back(std::move(page_diff));
  }

  diff.summary =
      diff.has_changes()
          ? "text frame upload handoff changed between snapshots"
          : "text frame upload handoff snapshots match";
  return diff;
}

}  // namespace quiz_vulkan::render
