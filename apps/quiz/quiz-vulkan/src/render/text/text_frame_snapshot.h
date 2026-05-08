#pragma once

#include "render/text/font_coverage_run_segmentation.h"
#include "render/text/font_shaped_atlas_update.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

enum class render_text_frame_snapshot_status {
    ready,
    pending_atlas_updates,
    font_fallback_incomplete,
    atlas_upload_incomplete,
    consumed_update_mismatch,
};

inline std::string render_text_frame_snapshot_status_name(
    const render_text_frame_snapshot_status status)
{
    switch (status) {
    case render_text_frame_snapshot_status::ready:
        return "ready";
    case render_text_frame_snapshot_status::pending_atlas_updates:
        return "pending_atlas_updates";
    case render_text_frame_snapshot_status::font_fallback_incomplete:
        return "font_fallback_incomplete";
    case render_text_frame_snapshot_status::atlas_upload_incomplete:
        return "atlas_upload_incomplete";
    case render_text_frame_snapshot_status::consumed_update_mismatch:
        return "consumed_update_mismatch";
    }

    return "unknown";
}

struct render_text_frame_atlas_upload_snapshot {
    std::string request_id;
    render_text_atlas_upload_request_status status =
        render_text_atlas_upload_request_status::skipped_materialization;
    std::size_t batch_atlas_request_index = 0;
    std::size_t item_index = 0;
    std::size_t materialization_index = 0;
    glyph_atlas_key cache_key;
    std::uint32_t resolved_glyph_id = 0;
    font_face_id resolved_face_id = 0;
    render_text_atlas_page page;
    render_rect updated_bounds;
    std::size_t upload_rgba_bytes = 0;
    bool has_upload_request = false;
    bool queued = false;
    bool consumed = false;
    bool stable_request_id = false;
    std::string diagnostic;
};

struct render_text_frame_snapshot_policy {
    std::size_t layout_request_count = 0;
    std::size_t fallback_chain_run_count = 0;
    std::size_t fallback_chain_missing_glyph_count = 0;
    std::size_t fallback_chain_invalid_utf8_count = 0;
    std::size_t selected_face_count = 0;
    std::size_t materialization_count = 0;
    std::size_t upload_request_count = 0;
    std::size_t queued_upload_request_id_count = 0;
    std::size_t consumed_upload_request_id_count = 0;
    std::size_t consumed_atlas_update_count = 0;
    std::size_t total_upload_rgba_bytes = 0;
    bool all_queued_uploads_consumed = true;
    bool deterministic_fallback_used = false;
};

struct render_text_frame_snapshot_request {
    std::string frame_id;
    std::string source_label;
    render_text_request_batch_plan_snapshot batch_plan;
    render_text_font_fallback_chain_plan_snapshot fallback_chain_plan;
    std::vector<render_text_glyph_atlas_materialization_snapshot> materializations;
    render_text_glyph_atlas_materialization_policy_snapshot materialization_policy;
    render_text_atlas_upload_request_bridge_snapshot atlas_upload_bridge;
    std::vector<std::string> queued_atlas_upload_request_ids;
    std::vector<std::string> consumed_atlas_upload_request_ids;
    std::size_t consumed_atlas_update_count = 0;
};

struct render_text_frame_snapshot {
    render_text_frame_snapshot_status status =
        render_text_frame_snapshot_status::pending_atlas_updates;
    std::string frame_id;
    std::string source_label;
    render_text_request_batch_plan_policy_snapshot batch_policy;
    render_text_font_fallback_chain_plan_policy_snapshot fallback_chain_policy;
    render_text_glyph_atlas_materialization_policy_snapshot materialization_policy;
    render_text_atlas_upload_request_policy_snapshot atlas_upload_policy;
    render_text_frame_snapshot_policy policy;
    std::vector<render_text_batch_layout_request_snapshot> layout_requests;
    std::vector<render_text_batch_normalized_style_key> style_keys;
    std::vector<font_face_id> selected_face_order;
    std::vector<render_text_font_fallback_chain_missing_glyph_snapshot> missing_glyphs;
    std::vector<render_text_frame_atlas_upload_snapshot> atlas_uploads;
    std::vector<std::string> queued_atlas_upload_request_ids;
    std::vector<std::string> consumed_atlas_upload_request_ids;
    std::string diagnostic;

    bool ok() const
    {
        return status == render_text_frame_snapshot_status::ready
            || status == render_text_frame_snapshot_status::pending_atlas_updates;
    }

    bool ready_for_renderer() const
    {
        return status == render_text_frame_snapshot_status::ready;
    }

    bool has_atlas_uploads() const
    {
        return !atlas_uploads.empty();
    }
};

inline std::vector<std::string> render_text_frame_upload_ready_request_ids(
    const render_text_atlas_upload_request_bridge_snapshot& bridge)
{
    std::vector<std::string> ids;
    ids.reserve(bridge.upload_requests.size());
    for (const render_text_atlas_upload_request_snapshot& request : bridge.requests) {
        if (request.has_upload_request) {
            ids.push_back(request.request_id);
        }
    }
    return ids;
}

inline bool render_text_frame_snapshot_ids_match(
    const std::vector<std::string>& expected,
    const std::vector<std::string>& actual)
{
    if (expected.size() != actual.size()) {
        return false;
    }

    return std::all_of(
        expected.begin(),
        expected.end(),
        [&](const std::string& id) {
            return render_text_atlas_upload_request_contains_id(actual, id);
        });
}

inline render_text_frame_snapshot_status render_text_frame_snapshot_status_for(
    const render_text_font_fallback_chain_plan_policy_snapshot& fallback_policy,
    const render_text_atlas_upload_request_bridge_snapshot& upload_bridge,
    const std::vector<std::string>& queued_ids,
    const std::vector<std::string>& consumed_ids,
    const std::size_t consumed_atlas_update_count)
{
    if (fallback_policy.missing_glyph_count > 0U || fallback_policy.invalid_utf8_count > 0U) {
        return render_text_frame_snapshot_status::font_fallback_incomplete;
    }
    if (!upload_bridge.ok()) {
        return render_text_frame_snapshot_status::atlas_upload_incomplete;
    }
    if (queued_ids.empty()) {
        if (!consumed_ids.empty() || consumed_atlas_update_count > 0U) {
            return render_text_frame_snapshot_status::consumed_update_mismatch;
        }
        return render_text_frame_snapshot_status::ready;
    }
    if (consumed_ids.empty() && consumed_atlas_update_count == 0U) {
        return render_text_frame_snapshot_status::pending_atlas_updates;
    }
    if (consumed_atlas_update_count != consumed_ids.size()) {
        return render_text_frame_snapshot_status::consumed_update_mismatch;
    }
    if (!render_text_frame_snapshot_ids_match(queued_ids, consumed_ids)) {
        return render_text_frame_snapshot_status::consumed_update_mismatch;
    }
    return render_text_frame_snapshot_status::ready;
}

inline render_text_frame_atlas_upload_snapshot make_render_text_frame_atlas_upload_snapshot(
    const render_text_atlas_upload_request_snapshot& request,
    const std::vector<std::string>& queued_ids,
    const std::vector<std::string>& consumed_ids)
{
    return render_text_frame_atlas_upload_snapshot{
        .request_id = request.request_id,
        .status = request.status,
        .batch_atlas_request_index = request.batch_atlas_request_index,
        .item_index = request.item_index,
        .materialization_index = request.materialization_index,
        .cache_key = request.cache_key,
        .resolved_glyph_id = request.resolved_glyph_id,
        .resolved_face_id = request.resolved_face_id,
        .page = request.upload_request.page,
        .updated_bounds = request.upload_request.updated_bounds,
        .upload_rgba_bytes = request.actual_upload_rgba_bytes,
        .has_upload_request = request.has_upload_request,
        .queued = render_text_atlas_upload_request_contains_id(queued_ids, request.request_id),
        .consumed = render_text_atlas_upload_request_contains_id(consumed_ids, request.request_id),
        .stable_request_id = request.stable_request_id,
        .diagnostic = request.diagnostic,
    };
}

inline render_text_frame_snapshot make_render_text_frame_snapshot(
    render_text_frame_snapshot_request request)
{
    if (request.queued_atlas_upload_request_ids.empty()) {
        request.queued_atlas_upload_request_ids =
            render_text_frame_upload_ready_request_ids(request.atlas_upload_bridge);
    }

    render_text_frame_snapshot snapshot{
        .status = render_text_frame_snapshot_status_for(
            request.fallback_chain_plan.policy,
            request.atlas_upload_bridge,
            request.queued_atlas_upload_request_ids,
            request.consumed_atlas_upload_request_ids,
            request.consumed_atlas_update_count),
        .frame_id = std::move(request.frame_id),
        .source_label = std::move(request.source_label),
        .batch_policy = request.batch_plan.policy,
        .fallback_chain_policy = request.fallback_chain_plan.policy,
        .materialization_policy = request.materialization_policy,
        .atlas_upload_policy = request.atlas_upload_bridge.policy,
        .layout_requests = std::move(request.batch_plan.layout_requests),
        .style_keys = std::move(request.batch_plan.style_keys),
        .selected_face_order = std::move(request.fallback_chain_plan.deterministic_selected_face_order),
        .missing_glyphs = std::move(request.fallback_chain_plan.missing_glyphs),
        .queued_atlas_upload_request_ids = std::move(request.queued_atlas_upload_request_ids),
        .consumed_atlas_upload_request_ids = std::move(request.consumed_atlas_upload_request_ids),
    };

    snapshot.policy = render_text_frame_snapshot_policy{
        .layout_request_count = snapshot.batch_policy.layout_request_count,
        .fallback_chain_run_count = snapshot.fallback_chain_policy.run_count,
        .fallback_chain_missing_glyph_count = snapshot.fallback_chain_policy.missing_glyph_count,
        .fallback_chain_invalid_utf8_count = snapshot.fallback_chain_policy.invalid_utf8_count,
        .selected_face_count = snapshot.selected_face_order.size(),
        .materialization_count = request.materialization_policy.request_count,
        .upload_request_count = request.atlas_upload_bridge.policy.upload_request_count,
        .queued_upload_request_id_count = snapshot.queued_atlas_upload_request_ids.size(),
        .consumed_upload_request_id_count = snapshot.consumed_atlas_upload_request_ids.size(),
        .consumed_atlas_update_count = request.consumed_atlas_update_count,
        .total_upload_rgba_bytes = request.atlas_upload_bridge.policy.total_upload_rgba_bytes,
        .all_queued_uploads_consumed = render_text_frame_snapshot_ids_match(
            snapshot.queued_atlas_upload_request_ids,
            snapshot.consumed_atlas_upload_request_ids),
        .deterministic_fallback_used =
            snapshot.batch_policy.fallback_materialization_count > 0U
            || snapshot.fallback_chain_policy.deterministic_backend_selected,
    };

    snapshot.atlas_uploads.reserve(request.atlas_upload_bridge.requests.size());
    for (const render_text_atlas_upload_request_snapshot& upload_request :
         request.atlas_upload_bridge.requests) {
        snapshot.atlas_uploads.push_back(make_render_text_frame_atlas_upload_snapshot(
            upload_request,
            snapshot.queued_atlas_upload_request_ids,
            snapshot.consumed_atlas_upload_request_ids));
    }

    snapshot.diagnostic = snapshot.ok()
        ? "text frame snapshot captured renderer-agnostic layout and atlas handoff diagnostics"
        : "text frame snapshot captured incomplete layout or atlas handoff diagnostics";
    return snapshot;
}

inline render_text_frame_snapshot render_text_frame_snapshot_with_consumed_atlas_updates(
    render_text_frame_snapshot snapshot,
    std::vector<std::string> consumed_atlas_upload_request_ids,
    const std::size_t consumed_atlas_update_count)
{
    snapshot.consumed_atlas_upload_request_ids = std::move(consumed_atlas_upload_request_ids);
    snapshot.policy.consumed_upload_request_id_count =
        snapshot.consumed_atlas_upload_request_ids.size();
    snapshot.policy.consumed_atlas_update_count = consumed_atlas_update_count;
    snapshot.policy.all_queued_uploads_consumed = render_text_frame_snapshot_ids_match(
        snapshot.queued_atlas_upload_request_ids,
        snapshot.consumed_atlas_upload_request_ids);
    for (render_text_frame_atlas_upload_snapshot& upload : snapshot.atlas_uploads) {
        upload.consumed = render_text_atlas_upload_request_contains_id(
            snapshot.consumed_atlas_upload_request_ids,
            upload.request_id);
    }
    snapshot.status = render_text_frame_snapshot_status_for(
        snapshot.fallback_chain_policy,
        render_text_atlas_upload_request_bridge_snapshot{
            .policy = snapshot.atlas_upload_policy,
        },
        snapshot.queued_atlas_upload_request_ids,
        snapshot.consumed_atlas_upload_request_ids,
        consumed_atlas_update_count);
    snapshot.diagnostic = snapshot.ok()
        ? "text frame snapshot captured renderer-agnostic layout and atlas handoff diagnostics"
        : "text frame snapshot captured incomplete layout or atlas handoff diagnostics";
    return snapshot;
}

enum class render_text_frame_snapshot_regression_status {
    none,
    readiness_regressed,
    fallback_regressed,
    atlas_upload_regressed,
    consumption_regressed,
};

inline std::string render_text_frame_snapshot_regression_status_name(
    const render_text_frame_snapshot_regression_status status)
{
    switch (status) {
    case render_text_frame_snapshot_regression_status::none:
        return "none";
    case render_text_frame_snapshot_regression_status::readiness_regressed:
        return "readiness_regressed";
    case render_text_frame_snapshot_regression_status::fallback_regressed:
        return "fallback_regressed";
    case render_text_frame_snapshot_regression_status::atlas_upload_regressed:
        return "atlas_upload_regressed";
    case render_text_frame_snapshot_regression_status::consumption_regressed:
        return "consumption_regressed";
    }

    return "unknown";
}

struct render_text_frame_snapshot_regression_summary {
    render_text_frame_snapshot_regression_status status =
        render_text_frame_snapshot_regression_status::none;
    bool regressed = false;
    bool readiness_regressed = false;
    bool fallback_regressed = false;
    bool atlas_upload_regressed = false;
    bool consumption_regressed = false;
    std::size_t issue_count = 0;
    std::string diagnostic;
};

struct render_text_frame_snapshot_diff_policy {
    bool status_changed = false;
    bool readiness_changed = false;
    std::ptrdiff_t layout_request_count_delta = 0;
    std::ptrdiff_t fallback_chain_run_count_delta = 0;
    std::ptrdiff_t fallback_codepoint_count_delta = 0;
    std::ptrdiff_t missing_glyph_count_delta = 0;
    std::ptrdiff_t invalid_utf8_count_delta = 0;
    std::ptrdiff_t atlas_upload_request_count_delta = 0;
    std::ptrdiff_t queued_upload_request_id_count_delta = 0;
    std::ptrdiff_t consumed_upload_request_id_count_delta = 0;
    std::ptrdiff_t total_upload_rgba_bytes_delta = 0;
    std::size_t added_atlas_upload_request_id_count = 0;
    std::size_t removed_atlas_upload_request_id_count = 0;
    std::size_t changed_atlas_upload_request_id_count = 0;
    std::size_t added_queued_upload_request_id_count = 0;
    std::size_t removed_queued_upload_request_id_count = 0;
    std::size_t added_consumed_upload_request_id_count = 0;
    std::size_t removed_consumed_upload_request_id_count = 0;
};

struct render_text_frame_snapshot_diff {
    render_text_frame_snapshot_status previous_status =
        render_text_frame_snapshot_status::pending_atlas_updates;
    render_text_frame_snapshot_status current_status =
        render_text_frame_snapshot_status::pending_atlas_updates;
    bool previous_ready_for_renderer = false;
    bool current_ready_for_renderer = false;
    render_text_frame_snapshot_diff_policy policy;
    std::vector<std::string> added_atlas_upload_request_ids;
    std::vector<std::string> removed_atlas_upload_request_ids;
    std::vector<std::string> changed_atlas_upload_request_ids;
    std::vector<std::string> added_queued_atlas_upload_request_ids;
    std::vector<std::string> removed_queued_atlas_upload_request_ids;
    std::vector<std::string> added_consumed_atlas_upload_request_ids;
    std::vector<std::string> removed_consumed_atlas_upload_request_ids;
    render_text_frame_snapshot_regression_summary regression;
    std::string diagnostic;

    bool has_atlas_upload_id_changes() const
    {
        return !added_atlas_upload_request_ids.empty()
            || !removed_atlas_upload_request_ids.empty()
            || !changed_atlas_upload_request_ids.empty();
    }

    bool has_queue_or_consume_id_deltas() const
    {
        return !added_queued_atlas_upload_request_ids.empty()
            || !removed_queued_atlas_upload_request_ids.empty()
            || !added_consumed_atlas_upload_request_ids.empty()
            || !removed_consumed_atlas_upload_request_ids.empty();
    }

    bool has_regression() const
    {
        return regression.regressed;
    }

    bool ok() const
    {
        return !has_regression();
    }
};

inline std::ptrdiff_t render_text_frame_snapshot_count_delta(
    const std::size_t previous,
    const std::size_t current)
{
    if (current >= previous) {
        return static_cast<std::ptrdiff_t>(current - previous);
    }
    return -static_cast<std::ptrdiff_t>(previous - current);
}

inline std::vector<std::string> render_text_frame_snapshot_id_delta_added(
    const std::vector<std::string>& previous,
    const std::vector<std::string>& current)
{
    std::vector<std::string> added;
    added.reserve(current.size());
    for (const std::string& id : current) {
        if (!render_text_atlas_upload_request_contains_id(previous, id)) {
            render_text_atlas_upload_request_append_unique_id(added, id);
        }
    }
    return added;
}

inline std::vector<std::string> render_text_frame_snapshot_id_delta_removed(
    const std::vector<std::string>& previous,
    const std::vector<std::string>& current)
{
    return render_text_frame_snapshot_id_delta_added(current, previous);
}

inline const render_text_frame_atlas_upload_snapshot* render_text_frame_snapshot_find_atlas_upload(
    const std::vector<render_text_frame_atlas_upload_snapshot>& uploads,
    const std::string& request_id)
{
    const auto match = std::find_if(
        uploads.begin(),
        uploads.end(),
        [&](const render_text_frame_atlas_upload_snapshot& upload) {
            return upload.request_id == request_id;
        });
    return match == uploads.end() ? nullptr : &*match;
}

inline bool render_text_frame_snapshot_rects_equal(
    const render_rect& lhs,
    const render_rect& rhs)
{
    return lhs.x == rhs.x
        && lhs.y == rhs.y
        && lhs.width == rhs.width
        && lhs.height == rhs.height;
}

inline bool render_text_frame_snapshot_pages_equal(
    const render_text_atlas_page& lhs,
    const render_text_atlas_page& rhs)
{
    return lhs.id == rhs.id
        && lhs.revision == rhs.revision
        && lhs.width == rhs.width
        && lhs.height == rhs.height;
}

inline bool render_text_frame_atlas_upload_snapshots_equal(
    const render_text_frame_atlas_upload_snapshot& lhs,
    const render_text_frame_atlas_upload_snapshot& rhs)
{
    return lhs.status == rhs.status
        && lhs.cache_key == rhs.cache_key
        && lhs.resolved_glyph_id == rhs.resolved_glyph_id
        && lhs.resolved_face_id == rhs.resolved_face_id
        && render_text_frame_snapshot_pages_equal(lhs.page, rhs.page)
        && render_text_frame_snapshot_rects_equal(lhs.updated_bounds, rhs.updated_bounds)
        && lhs.upload_rgba_bytes == rhs.upload_rgba_bytes
        && lhs.has_upload_request == rhs.has_upload_request
        && lhs.queued == rhs.queued
        && lhs.consumed == rhs.consumed
        && lhs.stable_request_id == rhs.stable_request_id;
}

inline std::vector<std::string> render_text_frame_snapshot_changed_atlas_upload_request_ids(
    const render_text_frame_snapshot& previous,
    const render_text_frame_snapshot& current)
{
    std::vector<std::string> changed;
    changed.reserve(current.atlas_uploads.size());
    for (const render_text_frame_atlas_upload_snapshot& current_upload : current.atlas_uploads) {
        const render_text_frame_atlas_upload_snapshot* previous_upload =
            render_text_frame_snapshot_find_atlas_upload(previous.atlas_uploads, current_upload.request_id);
        if (previous_upload != nullptr
            && !render_text_frame_atlas_upload_snapshots_equal(*previous_upload, current_upload)) {
            render_text_atlas_upload_request_append_unique_id(changed, current_upload.request_id);
        }
    }
    return changed;
}

inline render_text_frame_snapshot_regression_summary make_render_text_frame_snapshot_regression_summary(
    const render_text_frame_snapshot& previous,
    const render_text_frame_snapshot& current)
{
    render_text_frame_snapshot_regression_summary summary;
    summary.readiness_regressed = previous.ready_for_renderer() && !current.ready_for_renderer();
    summary.fallback_regressed =
        current.fallback_chain_policy.missing_glyph_count
            > previous.fallback_chain_policy.missing_glyph_count
        || current.fallback_chain_policy.invalid_utf8_count
            > previous.fallback_chain_policy.invalid_utf8_count;
    summary.atlas_upload_regressed =
        current.atlas_upload_policy.skipped_materialization_count
            > previous.atlas_upload_policy.skipped_materialization_count
        || current.atlas_upload_policy.payload_byte_count_mismatch_count
            > previous.atlas_upload_policy.payload_byte_count_mismatch_count
        || (current.status == render_text_frame_snapshot_status::atlas_upload_incomplete
            && previous.status != render_text_frame_snapshot_status::atlas_upload_incomplete);
    summary.consumption_regressed =
        (previous.policy.all_queued_uploads_consumed && !current.policy.all_queued_uploads_consumed)
        || (current.status == render_text_frame_snapshot_status::consumed_update_mismatch
            && previous.status != render_text_frame_snapshot_status::consumed_update_mismatch);

    if (summary.readiness_regressed) {
        summary.status = render_text_frame_snapshot_regression_status::readiness_regressed;
        ++summary.issue_count;
    }
    if (summary.fallback_regressed) {
        if (summary.status == render_text_frame_snapshot_regression_status::none) {
            summary.status = render_text_frame_snapshot_regression_status::fallback_regressed;
        }
        ++summary.issue_count;
    }
    if (summary.atlas_upload_regressed) {
        if (summary.status == render_text_frame_snapshot_regression_status::none) {
            summary.status = render_text_frame_snapshot_regression_status::atlas_upload_regressed;
        }
        ++summary.issue_count;
    }
    if (summary.consumption_regressed) {
        if (summary.status == render_text_frame_snapshot_regression_status::none) {
            summary.status = render_text_frame_snapshot_regression_status::consumption_regressed;
        }
        ++summary.issue_count;
    }

    summary.regressed = summary.issue_count > 0U;
    summary.diagnostic = summary.regressed
        ? "text frame snapshot diff detected renderer handoff regression(s)"
        : "text frame snapshot diff has no renderer handoff regressions";
    return summary;
}

inline render_text_frame_snapshot_diff diff_render_text_frame_snapshots(
    const render_text_frame_snapshot& previous,
    const render_text_frame_snapshot& current)
{
    render_text_frame_snapshot_diff diff{
        .previous_status = previous.status,
        .current_status = current.status,
        .previous_ready_for_renderer = previous.ready_for_renderer(),
        .current_ready_for_renderer = current.ready_for_renderer(),
    };

    diff.added_atlas_upload_request_ids.reserve(current.atlas_uploads.size());
    for (const render_text_frame_atlas_upload_snapshot& upload : current.atlas_uploads) {
        if (render_text_frame_snapshot_find_atlas_upload(previous.atlas_uploads, upload.request_id) == nullptr) {
            render_text_atlas_upload_request_append_unique_id(diff.added_atlas_upload_request_ids, upload.request_id);
        }
    }

    diff.removed_atlas_upload_request_ids.reserve(previous.atlas_uploads.size());
    for (const render_text_frame_atlas_upload_snapshot& upload : previous.atlas_uploads) {
        if (render_text_frame_snapshot_find_atlas_upload(current.atlas_uploads, upload.request_id) == nullptr) {
            render_text_atlas_upload_request_append_unique_id(diff.removed_atlas_upload_request_ids, upload.request_id);
        }
    }
    diff.changed_atlas_upload_request_ids =
        render_text_frame_snapshot_changed_atlas_upload_request_ids(previous, current);

    diff.added_queued_atlas_upload_request_ids = render_text_frame_snapshot_id_delta_added(
        previous.queued_atlas_upload_request_ids,
        current.queued_atlas_upload_request_ids);
    diff.removed_queued_atlas_upload_request_ids = render_text_frame_snapshot_id_delta_removed(
        previous.queued_atlas_upload_request_ids,
        current.queued_atlas_upload_request_ids);
    diff.added_consumed_atlas_upload_request_ids = render_text_frame_snapshot_id_delta_added(
        previous.consumed_atlas_upload_request_ids,
        current.consumed_atlas_upload_request_ids);
    diff.removed_consumed_atlas_upload_request_ids = render_text_frame_snapshot_id_delta_removed(
        previous.consumed_atlas_upload_request_ids,
        current.consumed_atlas_upload_request_ids);

    diff.policy = render_text_frame_snapshot_diff_policy{
        .status_changed = previous.status != current.status,
        .readiness_changed = diff.previous_ready_for_renderer != diff.current_ready_for_renderer,
        .layout_request_count_delta = render_text_frame_snapshot_count_delta(
            previous.policy.layout_request_count,
            current.policy.layout_request_count),
        .fallback_chain_run_count_delta = render_text_frame_snapshot_count_delta(
            previous.policy.fallback_chain_run_count,
            current.policy.fallback_chain_run_count),
        .fallback_codepoint_count_delta = render_text_frame_snapshot_count_delta(
            previous.fallback_chain_policy.fallback_codepoint_count,
            current.fallback_chain_policy.fallback_codepoint_count),
        .missing_glyph_count_delta = render_text_frame_snapshot_count_delta(
            previous.policy.fallback_chain_missing_glyph_count,
            current.policy.fallback_chain_missing_glyph_count),
        .invalid_utf8_count_delta = render_text_frame_snapshot_count_delta(
            previous.policy.fallback_chain_invalid_utf8_count,
            current.policy.fallback_chain_invalid_utf8_count),
        .atlas_upload_request_count_delta = render_text_frame_snapshot_count_delta(
            previous.policy.upload_request_count,
            current.policy.upload_request_count),
        .queued_upload_request_id_count_delta = render_text_frame_snapshot_count_delta(
            previous.policy.queued_upload_request_id_count,
            current.policy.queued_upload_request_id_count),
        .consumed_upload_request_id_count_delta = render_text_frame_snapshot_count_delta(
            previous.policy.consumed_upload_request_id_count,
            current.policy.consumed_upload_request_id_count),
        .total_upload_rgba_bytes_delta = render_text_frame_snapshot_count_delta(
            previous.policy.total_upload_rgba_bytes,
            current.policy.total_upload_rgba_bytes),
        .added_atlas_upload_request_id_count = diff.added_atlas_upload_request_ids.size(),
        .removed_atlas_upload_request_id_count = diff.removed_atlas_upload_request_ids.size(),
        .changed_atlas_upload_request_id_count = diff.changed_atlas_upload_request_ids.size(),
        .added_queued_upload_request_id_count = diff.added_queued_atlas_upload_request_ids.size(),
        .removed_queued_upload_request_id_count = diff.removed_queued_atlas_upload_request_ids.size(),
        .added_consumed_upload_request_id_count = diff.added_consumed_atlas_upload_request_ids.size(),
        .removed_consumed_upload_request_id_count = diff.removed_consumed_atlas_upload_request_ids.size(),
    };

    diff.regression = make_render_text_frame_snapshot_regression_summary(previous, current);
    diff.diagnostic = diff.has_regression()
        ? "text frame snapshot diff found renderer handoff regression(s)"
        : "text frame snapshot diff is renderer-agnostic and regression-free";
    return diff;
}

} // namespace quiz_vulkan::render
