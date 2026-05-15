#pragma once

#include "render/text/font_glyph_atlas_upload_operation_plan.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

enum class render_text_glyph_atlas_upload_result_status {
    accepted_upload,
    accepted_clean_reuse,
    rejected_blocked_packet,
    rejected_missing_upload_request_id,
};

inline std::string render_text_glyph_atlas_upload_result_status_name(
    const render_text_glyph_atlas_upload_result_status status)
{
    switch (status) {
    case render_text_glyph_atlas_upload_result_status::accepted_upload:
        return "accepted_upload";
    case render_text_glyph_atlas_upload_result_status::accepted_clean_reuse:
        return "accepted_clean_reuse";
    case render_text_glyph_atlas_upload_result_status::rejected_blocked_packet:
        return "rejected_blocked_packet";
    case render_text_glyph_atlas_upload_result_status::rejected_missing_upload_request_id:
        return "rejected_missing_upload_request_id";
    }

    return "unknown";
}

struct render_text_glyph_atlas_upload_result_packet_snapshot {
    std::string operation_id;
    std::string upload_request_id;
    std::string stable_page_id;
    std::size_t operation_index = 0;
    std::size_t page_plan_entry_index = 0;
    std::size_t materialization_index = 0;
    std::string materialization_id;
    std::size_t run_index = 0;
    std::size_t cluster_index = 0;
    glyph_atlas_key cache_key;
    bool has_cache_key = false;
    render_text_atlas_page page;
    render_text_atlas_page_id page_id = 0;
    render_rect update_bounds;
    std::size_t rgba_byte_count = 0;
    render_text_glyph_atlas_upload_operation_status operation_status =
        render_text_glyph_atlas_upload_operation_status::blocked_non_uploadable_materialization;
    render_text_glyph_atlas_upload_result_status result_status =
        render_text_glyph_atlas_upload_result_status::rejected_blocked_packet;
    bool accepted = false;
    bool rejected = true;
    bool upload_accepted = false;
    bool clean_reuse_accepted = false;
    bool dirty_upload = false;
    bool clean_reuse = false;
    bool blocked = true;
    bool overflow = false;
    bool missing_cache_key = false;
    bool missing_upload_request_id = false;
    bool payload_byte_count_mismatch = false;
    std::string blocker_reason;
    std::string diagnostic;
};

struct render_text_glyph_atlas_upload_result_page_snapshot {
    std::string stable_page_id;
    render_text_atlas_page page;
    render_text_atlas_page_id page_id = 0;
    std::size_t packet_count = 0;
    std::size_t accepted_packet_count = 0;
    std::size_t rejected_packet_count = 0;
    std::size_t upload_request_count = 0;
    std::size_t materialized_glyph_count = 0;
    std::size_t reused_glyph_count = 0;
    std::size_t missing_glyph_count = 0;
    std::size_t blocker_count = 0;
    std::size_t overflow_count = 0;
    std::size_t upload_rgba_bytes = 0;
    bool has_uploads = false;
    bool has_rejections = false;
};

struct render_text_glyph_atlas_upload_result_policy_snapshot {
    std::size_t operation_count = 0;
    std::size_t accepted_packet_count = 0;
    std::size_t rejected_packet_count = 0;
    std::size_t accepted_upload_count = 0;
    std::size_t accepted_clean_reuse_count = 0;
    std::size_t rejected_blocked_packet_count = 0;
    std::size_t rejected_missing_upload_request_id_count = 0;
    std::size_t blocker_count = 0;
    std::size_t overflow_count = 0;
    std::size_t payload_byte_count_mismatch_count = 0;
    std::size_t upload_request_id_count = 0;
    std::size_t page_count = 0;
    std::size_t page_with_upload_count = 0;
    std::size_t page_with_rejection_count = 0;
    std::size_t materialized_glyph_count = 0;
    std::size_t reused_glyph_count = 0;
    std::size_t missing_glyph_count = 0;
    std::size_t total_upload_rgba_bytes = 0;
    bool has_uploads = false;
    bool has_rejections = false;
    std::string diagnostic;
};

struct render_text_glyph_atlas_upload_result_request {
    render_text_glyph_atlas_upload_operation_plan_snapshot operation_plan;
    std::vector<std::string> upload_request_ids;
    bool require_upload_request_ids = true;
};

struct render_text_glyph_atlas_upload_result_snapshot {
    std::vector<render_text_glyph_atlas_upload_result_packet_snapshot> packets;
    std::vector<render_text_glyph_atlas_upload_result_page_snapshot> pages;
    render_text_glyph_atlas_upload_result_policy_snapshot policy;

    bool ok() const
    {
        return !policy.has_rejections;
    }

    bool has_uploads() const
    {
        return policy.has_uploads;
    }

    bool has_rejections() const
    {
        return policy.has_rejections;
    }
};

inline render_text_glyph_atlas_upload_result_status render_text_glyph_atlas_upload_result_status_for(
    const render_text_glyph_atlas_upload_operation_packet& packet,
    const std::string& upload_request_id,
    const bool require_upload_request_ids)
{
    if (packet.uploadable()) {
        if (require_upload_request_ids && upload_request_id.empty()) {
            return render_text_glyph_atlas_upload_result_status::rejected_missing_upload_request_id;
        }
        return render_text_glyph_atlas_upload_result_status::accepted_upload;
    }
    if (packet.clean_reuse) {
        return render_text_glyph_atlas_upload_result_status::accepted_clean_reuse;
    }
    return render_text_glyph_atlas_upload_result_status::rejected_blocked_packet;
}

inline std::string render_text_glyph_atlas_upload_result_rejection_reason_for(
    const render_text_glyph_atlas_upload_result_status status,
    const render_text_glyph_atlas_upload_operation_packet& packet)
{
    switch (status) {
    case render_text_glyph_atlas_upload_result_status::accepted_upload:
    case render_text_glyph_atlas_upload_result_status::accepted_clean_reuse:
        return {};
    case render_text_glyph_atlas_upload_result_status::rejected_missing_upload_request_id:
        return "missing upload request id";
    case render_text_glyph_atlas_upload_result_status::rejected_blocked_packet:
        return packet.blocker_reason.empty() ? "blocked upload packet" : packet.blocker_reason;
    }

    return "unknown rejection";
}

inline render_text_glyph_atlas_upload_result_packet_snapshot make_render_text_glyph_atlas_upload_result_packet(
    const render_text_glyph_atlas_upload_operation_packet& packet,
    std::string upload_request_id,
    const bool require_upload_request_ids)
{
    const render_text_glyph_atlas_upload_result_status status =
        render_text_glyph_atlas_upload_result_status_for(
            packet,
            upload_request_id,
            require_upload_request_ids);
    const bool accepted = status == render_text_glyph_atlas_upload_result_status::accepted_upload
        || status == render_text_glyph_atlas_upload_result_status::accepted_clean_reuse;
    const bool upload_accepted = status == render_text_glyph_atlas_upload_result_status::accepted_upload;
    const bool clean_reuse_accepted =
        status == render_text_glyph_atlas_upload_result_status::accepted_clean_reuse;
    const bool missing_upload_request_id =
        status == render_text_glyph_atlas_upload_result_status::rejected_missing_upload_request_id;
    const std::string rejection_reason =
        render_text_glyph_atlas_upload_result_rejection_reason_for(status, packet);

    return render_text_glyph_atlas_upload_result_packet_snapshot{
        .operation_id = packet.operation_id,
        .upload_request_id = std::move(upload_request_id),
        .stable_page_id = packet.stable_page_id,
        .operation_index = packet.operation_index,
        .page_plan_entry_index = packet.page_plan_entry_index,
        .materialization_index = packet.materialization_index,
        .materialization_id = packet.materialization_id,
        .run_index = packet.run_index,
        .cluster_index = packet.cluster_index,
        .cache_key = packet.cache_key,
        .has_cache_key = packet.has_cache_key,
        .page = packet.page,
        .page_id = packet.page_id,
        .update_bounds = packet.update_bounds,
        .rgba_byte_count = upload_accepted ? packet.rgba_byte_count : 0U,
        .operation_status = packet.status,
        .result_status = status,
        .accepted = accepted,
        .rejected = !accepted,
        .upload_accepted = upload_accepted,
        .clean_reuse_accepted = clean_reuse_accepted,
        .dirty_upload = packet.dirty_upload && upload_accepted,
        .clean_reuse = packet.clean_reuse,
        .blocked = packet.blocked || missing_upload_request_id,
        .overflow = packet.overflow,
        .missing_cache_key = !packet.has_cache_key,
        .missing_upload_request_id = missing_upload_request_id,
        .payload_byte_count_mismatch = packet.payload_byte_count_mismatch,
        .blocker_reason = rejection_reason,
        .diagnostic = accepted
            ? "glyph atlas upload result accepted packet"
            : "glyph atlas upload result rejected packet: " + rejection_reason,
    };
}

inline render_text_glyph_atlas_upload_result_page_snapshot*
render_text_glyph_atlas_upload_result_find_page(
    std::vector<render_text_glyph_atlas_upload_result_page_snapshot>& pages,
    const std::string& stable_page_id)
{
    const auto match = std::find_if(
        pages.begin(),
        pages.end(),
        [&](const render_text_glyph_atlas_upload_result_page_snapshot& page) {
            return page.stable_page_id == stable_page_id;
        });
    return match == pages.end() ? nullptr : &*match;
}

inline void append_render_text_glyph_atlas_upload_result_packet(
    render_text_glyph_atlas_upload_result_snapshot& result,
    render_text_glyph_atlas_upload_result_packet_snapshot packet)
{
    ++result.policy.operation_count;
    if (!packet.upload_request_id.empty()) {
        ++result.policy.upload_request_id_count;
    }
    if (packet.accepted) {
        ++result.policy.accepted_packet_count;
    } else {
        ++result.policy.rejected_packet_count;
        result.policy.has_rejections = true;
    }
    if (packet.upload_accepted) {
        ++result.policy.accepted_upload_count;
        ++result.policy.materialized_glyph_count;
        result.policy.total_upload_rgba_bytes += packet.rgba_byte_count;
        result.policy.has_uploads = true;
    }
    if (packet.clean_reuse_accepted) {
        ++result.policy.accepted_clean_reuse_count;
        ++result.policy.reused_glyph_count;
    }
    if (packet.result_status == render_text_glyph_atlas_upload_result_status::rejected_blocked_packet) {
        ++result.policy.rejected_blocked_packet_count;
    }
    if (packet.result_status == render_text_glyph_atlas_upload_result_status::rejected_missing_upload_request_id) {
        ++result.policy.rejected_missing_upload_request_id_count;
    }
    if (packet.blocked) {
        ++result.policy.blocker_count;
    }
    if (packet.overflow) {
        ++result.policy.overflow_count;
    }
    if (packet.payload_byte_count_mismatch) {
        ++result.policy.payload_byte_count_mismatch_count;
    }
    if (packet.missing_cache_key) {
        ++result.policy.missing_glyph_count;
    }

    if (!packet.stable_page_id.empty() && packet.page_id != 0U) {
        render_text_glyph_atlas_upload_result_page_snapshot* page =
            render_text_glyph_atlas_upload_result_find_page(result.pages, packet.stable_page_id);
        if (page == nullptr) {
            result.pages.push_back(render_text_glyph_atlas_upload_result_page_snapshot{
                .stable_page_id = packet.stable_page_id,
                .page = packet.page,
                .page_id = packet.page_id,
            });
            page = &result.pages.back();
        }
        ++page->packet_count;
        if (packet.accepted) {
            ++page->accepted_packet_count;
        } else {
            ++page->rejected_packet_count;
            page->has_rejections = true;
        }
        if (!packet.upload_request_id.empty()) {
            ++page->upload_request_count;
        }
        if (packet.upload_accepted) {
            ++page->materialized_glyph_count;
            page->upload_rgba_bytes += packet.rgba_byte_count;
            page->has_uploads = true;
        }
        if (packet.clean_reuse_accepted) {
            ++page->reused_glyph_count;
        }
        if (packet.missing_cache_key) {
            ++page->missing_glyph_count;
        }
        if (packet.blocked) {
            ++page->blocker_count;
        }
        if (packet.overflow) {
            ++page->overflow_count;
        }
    }

    result.packets.push_back(std::move(packet));
}

inline void summarize_render_text_glyph_atlas_upload_result_pages(
    render_text_glyph_atlas_upload_result_snapshot& result)
{
    result.policy.page_count = result.pages.size();
    result.policy.page_with_upload_count = 0;
    result.policy.page_with_rejection_count = 0;
    for (const render_text_glyph_atlas_upload_result_page_snapshot& page : result.pages) {
        if (page.has_uploads) {
            ++result.policy.page_with_upload_count;
        }
        if (page.has_rejections) {
            ++result.policy.page_with_rejection_count;
        }
    }
    result.policy.diagnostic = "glyph atlas upload result: "
        + std::to_string(result.policy.accepted_packet_count)
        + " accepted, "
        + std::to_string(result.policy.rejected_packet_count)
        + " rejected, "
        + std::to_string(result.policy.total_upload_rgba_bytes)
        + " upload bytes";
}

inline render_text_glyph_atlas_upload_result_snapshot make_render_text_glyph_atlas_upload_result(
    render_text_glyph_atlas_upload_result_request request)
{
    render_text_glyph_atlas_upload_result_snapshot result;
    result.packets.reserve(request.operation_plan.operations.size());
    result.pages.reserve(request.operation_plan.pages.size());
    for (const render_text_glyph_atlas_upload_operation_page_summary& page : request.operation_plan.pages) {
        result.pages.push_back(render_text_glyph_atlas_upload_result_page_snapshot{
            .stable_page_id = page.stable_page_id,
            .page = page.page,
            .page_id = page.page_id,
            .has_rejections = page.has_blockers,
        });
    }

    std::size_t upload_request_id_index = 0;
    for (const render_text_glyph_atlas_upload_operation_packet& packet : request.operation_plan.operations) {
        std::string upload_request_id;
        if (packet.uploadable() && upload_request_id_index < request.upload_request_ids.size()) {
            upload_request_id = request.upload_request_ids[upload_request_id_index];
        }
        if (packet.uploadable()) {
            ++upload_request_id_index;
        }

        append_render_text_glyph_atlas_upload_result_packet(
            result,
            make_render_text_glyph_atlas_upload_result_packet(
                packet,
                std::move(upload_request_id),
                request.require_upload_request_ids));
    }
    summarize_render_text_glyph_atlas_upload_result_pages(result);
    return result;
}

enum class render_text_glyph_atlas_upload_result_diff_status {
    unchanged,
    added,
    removed,
    changed,
};

inline std::string render_text_glyph_atlas_upload_result_diff_status_name(
    const render_text_glyph_atlas_upload_result_diff_status status)
{
    switch (status) {
    case render_text_glyph_atlas_upload_result_diff_status::unchanged:
        return "unchanged";
    case render_text_glyph_atlas_upload_result_diff_status::added:
        return "added";
    case render_text_glyph_atlas_upload_result_diff_status::removed:
        return "removed";
    case render_text_glyph_atlas_upload_result_diff_status::changed:
        return "changed";
    }

    return "unknown";
}

struct render_text_glyph_atlas_upload_result_policy_diff_snapshot {
    render_text_glyph_atlas_upload_result_policy_snapshot before;
    render_text_glyph_atlas_upload_result_policy_snapshot after;
    std::ptrdiff_t operation_count_delta = 0;
    std::ptrdiff_t accepted_packet_count_delta = 0;
    std::ptrdiff_t rejected_packet_count_delta = 0;
    std::ptrdiff_t accepted_upload_count_delta = 0;
    std::ptrdiff_t accepted_clean_reuse_count_delta = 0;
    std::ptrdiff_t blocker_count_delta = 0;
    std::ptrdiff_t upload_request_id_count_delta = 0;
    std::ptrdiff_t page_count_delta = 0;
    std::ptrdiff_t materialized_glyph_count_delta = 0;
    std::ptrdiff_t reused_glyph_count_delta = 0;
    std::ptrdiff_t missing_glyph_count_delta = 0;
    std::ptrdiff_t total_upload_rgba_bytes_delta = 0;
    bool has_changes = false;
    std::string summary;
};

struct render_text_glyph_atlas_upload_result_packet_diff_snapshot {
    render_text_glyph_atlas_upload_result_diff_status status =
        render_text_glyph_atlas_upload_result_diff_status::unchanged;
    std::string operation_id;
    render_text_glyph_atlas_upload_result_packet_snapshot before;
    render_text_glyph_atlas_upload_result_packet_snapshot after;
    bool has_before = false;
    bool has_after = false;
    bool accepted_changed = false;
    bool rejected_changed = false;
    bool result_status_changed = false;
    bool page_changed = false;
    bool upload_request_id_changed = false;
    bool upload_byte_count_changed = false;
    bool blocker_changed = false;
    bool materialized_glyph_changed = false;
    bool reused_glyph_changed = false;
    bool missing_glyph_changed = false;
    std::ptrdiff_t upload_rgba_bytes_delta = 0;
    std::string summary;
};

struct render_text_glyph_atlas_upload_result_page_diff_snapshot {
    render_text_glyph_atlas_upload_result_diff_status status =
        render_text_glyph_atlas_upload_result_diff_status::unchanged;
    std::string stable_page_id;
    render_text_glyph_atlas_upload_result_page_snapshot before;
    render_text_glyph_atlas_upload_result_page_snapshot after;
    bool has_before = false;
    bool has_after = false;
    bool accepted_packet_count_changed = false;
    bool rejected_packet_count_changed = false;
    bool upload_byte_count_changed = false;
    bool blocker_count_changed = false;
    bool materialized_glyph_count_changed = false;
    bool reused_glyph_count_changed = false;
    bool missing_glyph_count_changed = false;
    std::ptrdiff_t accepted_packet_count_delta = 0;
    std::ptrdiff_t rejected_packet_count_delta = 0;
    std::ptrdiff_t upload_rgba_bytes_delta = 0;
    std::ptrdiff_t blocker_count_delta = 0;
    std::ptrdiff_t materialized_glyph_count_delta = 0;
    std::ptrdiff_t reused_glyph_count_delta = 0;
    std::ptrdiff_t missing_glyph_count_delta = 0;
    std::string summary;
};

struct render_text_glyph_atlas_upload_result_diff_snapshot {
    std::vector<render_text_glyph_atlas_upload_result_packet_diff_snapshot> packet_diffs;
    std::vector<render_text_glyph_atlas_upload_result_page_diff_snapshot> page_diffs;
    std::vector<std::string> changed_packet_ids;
    std::vector<std::string> changed_page_ids;
    render_text_glyph_atlas_upload_result_policy_diff_snapshot policy;
    std::size_t added_packet_count = 0;
    std::size_t removed_packet_count = 0;
    std::size_t changed_packet_count = 0;
    std::size_t unchanged_packet_count = 0;
    std::size_t added_page_count = 0;
    std::size_t removed_page_count = 0;
    std::size_t changed_page_count = 0;
    std::size_t unchanged_page_count = 0;
    std::string summary;

    bool has_changes() const
    {
        return added_packet_count > 0
            || removed_packet_count > 0
            || changed_packet_count > 0
            || added_page_count > 0
            || removed_page_count > 0
            || changed_page_count > 0
            || policy.has_changes;
    }
};

inline std::ptrdiff_t render_text_glyph_atlas_upload_result_delta(
    const std::size_t before,
    const std::size_t after)
{
    return static_cast<std::ptrdiff_t>(after) - static_cast<std::ptrdiff_t>(before);
}

inline bool render_text_glyph_atlas_upload_result_packet_equal(
    const render_text_glyph_atlas_upload_result_packet_snapshot& before,
    const render_text_glyph_atlas_upload_result_packet_snapshot& after)
{
    return before.result_status == after.result_status
        && before.upload_request_id == after.upload_request_id
        && before.stable_page_id == after.stable_page_id
        && before.rgba_byte_count == after.rgba_byte_count
        && before.blocker_reason == after.blocker_reason
        && before.materialization_id == after.materialization_id
        && before.cache_key == after.cache_key
        && before.has_cache_key == after.has_cache_key;
}

inline bool render_text_glyph_atlas_upload_result_page_equal(
    const render_text_glyph_atlas_upload_result_page_snapshot& before,
    const render_text_glyph_atlas_upload_result_page_snapshot& after)
{
    return before.packet_count == after.packet_count
        && before.accepted_packet_count == after.accepted_packet_count
        && before.rejected_packet_count == after.rejected_packet_count
        && before.upload_request_count == after.upload_request_count
        && before.materialized_glyph_count == after.materialized_glyph_count
        && before.reused_glyph_count == after.reused_glyph_count
        && before.missing_glyph_count == after.missing_glyph_count
        && before.blocker_count == after.blocker_count
        && before.overflow_count == after.overflow_count
        && before.upload_rgba_bytes == after.upload_rgba_bytes;
}

inline render_text_glyph_atlas_upload_result_policy_diff_snapshot
diff_render_text_glyph_atlas_upload_result_policies(
    const render_text_glyph_atlas_upload_result_policy_snapshot& before,
    const render_text_glyph_atlas_upload_result_policy_snapshot& after)
{
    render_text_glyph_atlas_upload_result_policy_diff_snapshot diff{
        .before = before,
        .after = after,
        .operation_count_delta = render_text_glyph_atlas_upload_result_delta(
            before.operation_count,
            after.operation_count),
        .accepted_packet_count_delta = render_text_glyph_atlas_upload_result_delta(
            before.accepted_packet_count,
            after.accepted_packet_count),
        .rejected_packet_count_delta = render_text_glyph_atlas_upload_result_delta(
            before.rejected_packet_count,
            after.rejected_packet_count),
        .accepted_upload_count_delta = render_text_glyph_atlas_upload_result_delta(
            before.accepted_upload_count,
            after.accepted_upload_count),
        .accepted_clean_reuse_count_delta = render_text_glyph_atlas_upload_result_delta(
            before.accepted_clean_reuse_count,
            after.accepted_clean_reuse_count),
        .blocker_count_delta = render_text_glyph_atlas_upload_result_delta(
            before.blocker_count,
            after.blocker_count),
        .upload_request_id_count_delta = render_text_glyph_atlas_upload_result_delta(
            before.upload_request_id_count,
            after.upload_request_id_count),
        .page_count_delta = render_text_glyph_atlas_upload_result_delta(
            before.page_count,
            after.page_count),
        .materialized_glyph_count_delta = render_text_glyph_atlas_upload_result_delta(
            before.materialized_glyph_count,
            after.materialized_glyph_count),
        .reused_glyph_count_delta = render_text_glyph_atlas_upload_result_delta(
            before.reused_glyph_count,
            after.reused_glyph_count),
        .missing_glyph_count_delta = render_text_glyph_atlas_upload_result_delta(
            before.missing_glyph_count,
            after.missing_glyph_count),
        .total_upload_rgba_bytes_delta = render_text_glyph_atlas_upload_result_delta(
            before.total_upload_rgba_bytes,
            after.total_upload_rgba_bytes),
    };
    diff.has_changes = diff.operation_count_delta != 0
        || diff.accepted_packet_count_delta != 0
        || diff.rejected_packet_count_delta != 0
        || diff.accepted_upload_count_delta != 0
        || diff.accepted_clean_reuse_count_delta != 0
        || diff.blocker_count_delta != 0
        || diff.upload_request_id_count_delta != 0
        || diff.page_count_delta != 0
        || diff.materialized_glyph_count_delta != 0
        || diff.reused_glyph_count_delta != 0
        || diff.missing_glyph_count_delta != 0
        || diff.total_upload_rgba_bytes_delta != 0;
    diff.summary = "glyph atlas upload result policy diff: "
        + std::to_string(before.accepted_packet_count)
        + " -> "
        + std::to_string(after.accepted_packet_count)
        + " accepted packets";
    return diff;
}

inline render_text_glyph_atlas_upload_result_packet_diff_snapshot
diff_render_text_glyph_atlas_upload_result_packets(
    const render_text_glyph_atlas_upload_result_packet_snapshot* before,
    const render_text_glyph_atlas_upload_result_packet_snapshot* after)
{
    render_text_glyph_atlas_upload_result_packet_diff_snapshot diff{
        .operation_id = before != nullptr ? before->operation_id : (after != nullptr ? after->operation_id : ""),
        .has_before = before != nullptr,
        .has_after = after != nullptr,
    };
    if (before != nullptr) {
        diff.before = *before;
    }
    if (after != nullptr) {
        diff.after = *after;
    }
    if (before == nullptr && after != nullptr) {
        diff.status = render_text_glyph_atlas_upload_result_diff_status::added;
    } else if (before != nullptr && after == nullptr) {
        diff.status = render_text_glyph_atlas_upload_result_diff_status::removed;
    } else if (before != nullptr && after != nullptr && !render_text_glyph_atlas_upload_result_packet_equal(
        *before,
        *after)) {
        diff.status = render_text_glyph_atlas_upload_result_diff_status::changed;
    }

    const render_text_glyph_atlas_upload_result_packet_snapshot& before_value = diff.before;
    const render_text_glyph_atlas_upload_result_packet_snapshot& after_value = diff.after;
    diff.accepted_changed = before_value.accepted != after_value.accepted;
    diff.rejected_changed = before_value.rejected != after_value.rejected;
    diff.result_status_changed = before_value.result_status != after_value.result_status;
    diff.page_changed = before_value.stable_page_id != after_value.stable_page_id;
    diff.upload_request_id_changed = before_value.upload_request_id != after_value.upload_request_id;
    diff.upload_byte_count_changed = before_value.rgba_byte_count != after_value.rgba_byte_count;
    diff.blocker_changed = before_value.blocker_reason != after_value.blocker_reason;
    diff.materialized_glyph_changed = before_value.upload_accepted != after_value.upload_accepted;
    diff.reused_glyph_changed = before_value.clean_reuse_accepted != after_value.clean_reuse_accepted;
    diff.missing_glyph_changed = before_value.missing_cache_key != after_value.missing_cache_key;
    diff.upload_rgba_bytes_delta = render_text_glyph_atlas_upload_result_delta(
        before_value.rgba_byte_count,
        after_value.rgba_byte_count);
    diff.summary = "glyph atlas upload result packet diff: "
        + diff.operation_id
        + " "
        + render_text_glyph_atlas_upload_result_diff_status_name(diff.status);
    return diff;
}

inline render_text_glyph_atlas_upload_result_page_diff_snapshot
diff_render_text_glyph_atlas_upload_result_pages(
    const render_text_glyph_atlas_upload_result_page_snapshot* before,
    const render_text_glyph_atlas_upload_result_page_snapshot* after)
{
    render_text_glyph_atlas_upload_result_page_diff_snapshot diff{
        .stable_page_id = before != nullptr ? before->stable_page_id : (after != nullptr ? after->stable_page_id : ""),
        .has_before = before != nullptr,
        .has_after = after != nullptr,
    };
    if (before != nullptr) {
        diff.before = *before;
    }
    if (after != nullptr) {
        diff.after = *after;
    }
    if (before == nullptr && after != nullptr) {
        diff.status = render_text_glyph_atlas_upload_result_diff_status::added;
    } else if (before != nullptr && after == nullptr) {
        diff.status = render_text_glyph_atlas_upload_result_diff_status::removed;
    } else if (before != nullptr && after != nullptr && !render_text_glyph_atlas_upload_result_page_equal(
        *before,
        *after)) {
        diff.status = render_text_glyph_atlas_upload_result_diff_status::changed;
    }

    diff.accepted_packet_count_delta = render_text_glyph_atlas_upload_result_delta(
        diff.before.accepted_packet_count,
        diff.after.accepted_packet_count);
    diff.rejected_packet_count_delta = render_text_glyph_atlas_upload_result_delta(
        diff.before.rejected_packet_count,
        diff.after.rejected_packet_count);
    diff.upload_rgba_bytes_delta = render_text_glyph_atlas_upload_result_delta(
        diff.before.upload_rgba_bytes,
        diff.after.upload_rgba_bytes);
    diff.blocker_count_delta = render_text_glyph_atlas_upload_result_delta(
        diff.before.blocker_count,
        diff.after.blocker_count);
    diff.materialized_glyph_count_delta = render_text_glyph_atlas_upload_result_delta(
        diff.before.materialized_glyph_count,
        diff.after.materialized_glyph_count);
    diff.reused_glyph_count_delta = render_text_glyph_atlas_upload_result_delta(
        diff.before.reused_glyph_count,
        diff.after.reused_glyph_count);
    diff.missing_glyph_count_delta = render_text_glyph_atlas_upload_result_delta(
        diff.before.missing_glyph_count,
        diff.after.missing_glyph_count);
    diff.accepted_packet_count_changed = diff.accepted_packet_count_delta != 0;
    diff.rejected_packet_count_changed = diff.rejected_packet_count_delta != 0;
    diff.upload_byte_count_changed = diff.upload_rgba_bytes_delta != 0;
    diff.blocker_count_changed = diff.blocker_count_delta != 0;
    diff.materialized_glyph_count_changed = diff.materialized_glyph_count_delta != 0;
    diff.reused_glyph_count_changed = diff.reused_glyph_count_delta != 0;
    diff.missing_glyph_count_changed = diff.missing_glyph_count_delta != 0;
    diff.summary = "glyph atlas upload result page diff: "
        + diff.stable_page_id
        + " "
        + render_text_glyph_atlas_upload_result_diff_status_name(diff.status);
    return diff;
}

inline const render_text_glyph_atlas_upload_result_packet_snapshot*
find_render_text_glyph_atlas_upload_result_packet(
    const std::vector<render_text_glyph_atlas_upload_result_packet_snapshot>& packets,
    const std::string& operation_id,
    const std::vector<bool>& used)
{
    for (std::size_t index = 0; index < packets.size(); ++index) {
        if (!used[index] && packets[index].operation_id == operation_id) {
            return &packets[index];
        }
    }
    return nullptr;
}

inline const render_text_glyph_atlas_upload_result_page_snapshot*
find_render_text_glyph_atlas_upload_result_page(
    const std::vector<render_text_glyph_atlas_upload_result_page_snapshot>& pages,
    const std::string& stable_page_id,
    const std::vector<bool>& used)
{
    for (std::size_t index = 0; index < pages.size(); ++index) {
        if (!used[index] && pages[index].stable_page_id == stable_page_id) {
            return &pages[index];
        }
    }
    return nullptr;
}

inline std::size_t render_text_glyph_atlas_upload_result_packet_index_for(
    const std::vector<render_text_glyph_atlas_upload_result_packet_snapshot>& packets,
    const render_text_glyph_atlas_upload_result_packet_snapshot* packet)
{
    return static_cast<std::size_t>(packet - packets.data());
}

inline std::size_t render_text_glyph_atlas_upload_result_page_index_for(
    const std::vector<render_text_glyph_atlas_upload_result_page_snapshot>& pages,
    const render_text_glyph_atlas_upload_result_page_snapshot* page)
{
    return static_cast<std::size_t>(page - pages.data());
}

inline render_text_glyph_atlas_upload_result_diff_snapshot diff_render_text_glyph_atlas_upload_results(
    const render_text_glyph_atlas_upload_result_snapshot& before,
    const render_text_glyph_atlas_upload_result_snapshot& after)
{
    render_text_glyph_atlas_upload_result_diff_snapshot diff{
        .policy = diff_render_text_glyph_atlas_upload_result_policies(before.policy, after.policy),
    };

    std::vector<bool> used_after_packets(after.packets.size(), false);
    for (const render_text_glyph_atlas_upload_result_packet_snapshot& before_packet : before.packets) {
        const render_text_glyph_atlas_upload_result_packet_snapshot* after_packet =
            find_render_text_glyph_atlas_upload_result_packet(
                after.packets,
                before_packet.operation_id,
                used_after_packets);
        if (after_packet != nullptr) {
            used_after_packets[render_text_glyph_atlas_upload_result_packet_index_for(after.packets, after_packet)] =
                true;
        }
        diff.packet_diffs.push_back(diff_render_text_glyph_atlas_upload_result_packets(
            &before_packet,
            after_packet));
    }
    for (std::size_t index = 0; index < after.packets.size(); ++index) {
        if (!used_after_packets[index]) {
            diff.packet_diffs.push_back(diff_render_text_glyph_atlas_upload_result_packets(nullptr, &after.packets[index]));
        }
    }

    std::vector<bool> used_after_pages(after.pages.size(), false);
    for (const render_text_glyph_atlas_upload_result_page_snapshot& before_page : before.pages) {
        const render_text_glyph_atlas_upload_result_page_snapshot* after_page =
            find_render_text_glyph_atlas_upload_result_page(
                after.pages,
                before_page.stable_page_id,
                used_after_pages);
        if (after_page != nullptr) {
            used_after_pages[render_text_glyph_atlas_upload_result_page_index_for(after.pages, after_page)] =
                true;
        }
        diff.page_diffs.push_back(diff_render_text_glyph_atlas_upload_result_pages(
            &before_page,
            after_page));
    }
    for (std::size_t index = 0; index < after.pages.size(); ++index) {
        if (!used_after_pages[index]) {
            diff.page_diffs.push_back(diff_render_text_glyph_atlas_upload_result_pages(nullptr, &after.pages[index]));
        }
    }

    for (const render_text_glyph_atlas_upload_result_packet_diff_snapshot& packet_diff : diff.packet_diffs) {
        switch (packet_diff.status) {
        case render_text_glyph_atlas_upload_result_diff_status::added:
            ++diff.added_packet_count;
            break;
        case render_text_glyph_atlas_upload_result_diff_status::removed:
            ++diff.removed_packet_count;
            break;
        case render_text_glyph_atlas_upload_result_diff_status::changed:
            ++diff.changed_packet_count;
            diff.changed_packet_ids.push_back(packet_diff.operation_id);
            break;
        case render_text_glyph_atlas_upload_result_diff_status::unchanged:
            ++diff.unchanged_packet_count;
            break;
        }
    }
    for (const render_text_glyph_atlas_upload_result_page_diff_snapshot& page_diff : diff.page_diffs) {
        switch (page_diff.status) {
        case render_text_glyph_atlas_upload_result_diff_status::added:
            ++diff.added_page_count;
            break;
        case render_text_glyph_atlas_upload_result_diff_status::removed:
            ++diff.removed_page_count;
            break;
        case render_text_glyph_atlas_upload_result_diff_status::changed:
            ++diff.changed_page_count;
            diff.changed_page_ids.push_back(page_diff.stable_page_id);
            break;
        case render_text_glyph_atlas_upload_result_diff_status::unchanged:
            ++diff.unchanged_page_count;
            break;
        }
    }
    diff.summary = "glyph atlas upload result diff: "
        + std::to_string(diff.added_packet_count)
        + " added packets, "
        + std::to_string(diff.removed_packet_count)
        + " removed packets, "
        + std::to_string(diff.changed_packet_count)
        + " changed packets";
    return diff;
}

} // namespace quiz_vulkan::render
