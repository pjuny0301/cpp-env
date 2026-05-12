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
