#pragma once

#include "render/text/text_frame_upload_handoff_core.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

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
