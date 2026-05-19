#pragma once

#include "render/text/text_frame_resource_packet_materialization.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

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

}  // namespace quiz_vulkan::render
