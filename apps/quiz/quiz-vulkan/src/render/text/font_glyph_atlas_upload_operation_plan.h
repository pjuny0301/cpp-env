#pragma once

#include "render/text/font_glyph_atlas_page_plan.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

enum class render_text_glyph_atlas_upload_operation_status {
    upload_ready,
    clean_reuse,
    blocked_missing_cache_key,
    blocked_missing_page,
    blocked_missing_update_bounds,
    blocked_overflow,
    blocked_payload_byte_count_mismatch,
    blocked_non_uploadable_materialization,
};

inline std::string render_text_glyph_atlas_upload_operation_status_name(
    const render_text_glyph_atlas_upload_operation_status status)
{
    switch (status) {
    case render_text_glyph_atlas_upload_operation_status::upload_ready:
        return "upload_ready";
    case render_text_glyph_atlas_upload_operation_status::clean_reuse:
        return "clean_reuse";
    case render_text_glyph_atlas_upload_operation_status::blocked_missing_cache_key:
        return "blocked_missing_cache_key";
    case render_text_glyph_atlas_upload_operation_status::blocked_missing_page:
        return "blocked_missing_page";
    case render_text_glyph_atlas_upload_operation_status::blocked_missing_update_bounds:
        return "blocked_missing_update_bounds";
    case render_text_glyph_atlas_upload_operation_status::blocked_overflow:
        return "blocked_overflow";
    case render_text_glyph_atlas_upload_operation_status::blocked_payload_byte_count_mismatch:
        return "blocked_payload_byte_count_mismatch";
    case render_text_glyph_atlas_upload_operation_status::blocked_non_uploadable_materialization:
        return "blocked_non_uploadable_materialization";
    }

    return "unknown";
}

struct render_text_glyph_atlas_upload_operation_packet {
    std::string operation_id;
    std::string stable_page_id;
    std::size_t operation_index = 0;
    std::size_t page_plan_entry_index = 0;
    std::size_t materialization_index = 0;
    std::string materialization_id;
    std::size_t run_index = 0;
    std::size_t cluster_index = 0;
    std::size_t cluster_byte_offset = 0;
    std::size_t cluster_byte_count = 0;
    render_text_glyph_atlas_upload_operation_status status =
        render_text_glyph_atlas_upload_operation_status::blocked_non_uploadable_materialization;
    render_text_glyph_atlas_page_plan_status page_plan_status =
        render_text_glyph_atlas_page_plan_status::skipped_materialization;
    render_text_glyph_atlas_materialization_status materialization_status =
        render_text_glyph_atlas_materialization_status::skipped_missing_cache_key;
    glyph_atlas_key cache_key;
    bool has_cache_key = false;
    render_text_atlas_page page;
    render_text_atlas_page_id page_id = 0;
    render_rect atlas_bounds;
    bool has_atlas_bounds = false;
    render_rect update_bounds;
    bool has_update_bounds = false;
    std::size_t rgba_byte_count = 0;
    bool dirty_upload = false;
    bool clean_reuse = false;
    bool skipped = false;
    bool blocked = true;
    bool overflow = false;
    bool payload_byte_count_mismatch = false;
    std::string blocker_reason;
    std::string diagnostic;

    bool uploadable() const
    {
        return status == render_text_glyph_atlas_upload_operation_status::upload_ready;
    }
};

struct render_text_glyph_atlas_upload_operation_page_summary {
    std::string stable_page_id;
    render_text_atlas_page page;
    render_text_atlas_page_id page_id = 0;
    std::size_t operation_count = 0;
    std::size_t upload_ready_count = 0;
    std::size_t clean_reuse_count = 0;
    std::size_t blocked_count = 0;
    std::size_t overflow_count = 0;
    std::size_t upload_rgba_bytes = 0;
    bool dirty = false;
    bool clean_reuse = false;
    bool has_blockers = false;
    bool overflow = false;
};

struct render_text_glyph_atlas_upload_operation_policy_snapshot {
    std::size_t page_plan_entry_count = 0;
    std::size_t materialization_count = 0;
    std::size_t operation_count = 0;
    std::size_t upload_ready_count = 0;
    std::size_t clean_reuse_count = 0;
    std::size_t skipped_count = 0;
    std::size_t blocked_count = 0;
    std::size_t overflow_count = 0;
    std::size_t payload_byte_count_mismatch_count = 0;
    std::size_t dirty_page_count = 0;
    std::size_t clean_reuse_page_count = 0;
    std::size_t blocked_page_count = 0;
    std::size_t page_count = 0;
    std::size_t total_upload_rgba_bytes = 0;
    bool has_uploads = false;
    bool has_blockers = false;
    bool has_overflow = false;
    std::string diagnostic;
};

struct render_text_glyph_atlas_upload_operation_plan_request {
    render_text_glyph_atlas_page_plan_snapshot page_plan;
    std::vector<render_text_glyph_atlas_materialization_snapshot> materializations;
    bool emit_clean_reuse_operations = true;
    bool emit_blocked_operations = true;
};

struct render_text_glyph_atlas_upload_operation_plan_snapshot {
    std::vector<render_text_glyph_atlas_upload_operation_packet> operations;
    std::vector<render_text_glyph_atlas_upload_operation_page_summary> pages;
    render_text_glyph_atlas_upload_operation_policy_snapshot policy;

    bool ok() const
    {
        return !policy.has_blockers;
    }

    bool has_uploads() const
    {
        return policy.has_uploads;
    }

    bool has_blockers() const
    {
        return policy.has_blockers;
    }
};

inline std::string render_text_glyph_atlas_upload_operation_stable_page_id_for(
    const render_text_atlas_page& page)
{
    return "page:" + std::to_string(page.id);
}

inline std::string render_text_glyph_atlas_upload_operation_cache_key_id_for(
    const glyph_atlas_key& cache_key)
{
    return "cache:"
        + std::to_string(cache_key.face_id)
        + ":"
        + std::to_string(cache_key.glyph_id)
        + ":"
        + std::to_string(cache_key.pixel_size);
}

inline std::string render_text_glyph_atlas_upload_operation_stable_id_for(
    const render_text_glyph_atlas_page_plan_entry_snapshot& entry)
{
    const std::string page_id = render_text_glyph_atlas_upload_operation_stable_page_id_for(entry.page);
    const std::string glyph_id = entry.has_cache_key
        ? render_text_glyph_atlas_upload_operation_cache_key_id_for(entry.cache_key)
        : entry.materialization_id;
    return page_id
        + ":"
        + glyph_id
        + ":run:"
        + std::to_string(entry.run_index)
        + ":cluster:"
        + std::to_string(entry.cluster_index);
}

inline const render_text_glyph_atlas_materialization_snapshot*
render_text_glyph_atlas_upload_operation_materialization_for(
    const std::vector<render_text_glyph_atlas_materialization_snapshot>& materializations,
    const render_text_glyph_atlas_page_plan_entry_snapshot& entry)
{
    if (entry.materialization_index < materializations.size()) {
        return &materializations[entry.materialization_index];
    }
    return nullptr;
}

inline bool render_text_glyph_atlas_upload_operation_has_positive_rect(const render_rect& rect)
{
    return rect.width > 0.0f && rect.height > 0.0f;
}

inline render_text_glyph_atlas_upload_operation_status render_text_glyph_atlas_upload_operation_status_for(
    const render_text_glyph_atlas_page_plan_entry_snapshot& entry,
    const render_text_glyph_atlas_materialization_snapshot* materialization)
{
    if (entry.overflow || entry.status == render_text_glyph_atlas_page_plan_status::overflow) {
        return render_text_glyph_atlas_upload_operation_status::blocked_overflow;
    }
    if (!entry.has_cache_key) {
        return render_text_glyph_atlas_upload_operation_status::blocked_missing_cache_key;
    }
    if (materialization != nullptr
        && render_text_glyph_atlas_materialization_has_payload_byte_count_mismatch(*materialization)) {
        return render_text_glyph_atlas_upload_operation_status::blocked_payload_byte_count_mismatch;
    }
    if (entry.page.id == 0U) {
        return render_text_glyph_atlas_upload_operation_status::blocked_missing_page;
    }
    if (entry.clean_reuse) {
        return render_text_glyph_atlas_upload_operation_status::clean_reuse;
    }
    if (!entry.upload_ready) {
        return render_text_glyph_atlas_upload_operation_status::blocked_non_uploadable_materialization;
    }
    if (!entry.has_atlas_bounds && materialization != nullptr && !materialization->has_atlas_update) {
        return render_text_glyph_atlas_upload_operation_status::blocked_missing_update_bounds;
    }
    if (materialization != nullptr
        && materialization->has_atlas_update
        && !render_text_glyph_atlas_upload_operation_has_positive_rect(materialization->atlas_update_bounds)) {
        return render_text_glyph_atlas_upload_operation_status::blocked_missing_update_bounds;
    }
    if (materialization == nullptr && !entry.has_atlas_bounds) {
        return render_text_glyph_atlas_upload_operation_status::blocked_missing_update_bounds;
    }
    return render_text_glyph_atlas_upload_operation_status::upload_ready;
}

inline std::string render_text_glyph_atlas_upload_operation_blocker_reason_for(
    const render_text_glyph_atlas_upload_operation_status status)
{
    switch (status) {
    case render_text_glyph_atlas_upload_operation_status::upload_ready:
        return {};
    case render_text_glyph_atlas_upload_operation_status::clean_reuse:
        return {};
    case render_text_glyph_atlas_upload_operation_status::blocked_missing_cache_key:
        return "missing cache key";
    case render_text_glyph_atlas_upload_operation_status::blocked_missing_page:
        return "missing atlas page";
    case render_text_glyph_atlas_upload_operation_status::blocked_missing_update_bounds:
        return "missing atlas update bounds";
    case render_text_glyph_atlas_upload_operation_status::blocked_overflow:
        return "atlas page overflow";
    case render_text_glyph_atlas_upload_operation_status::blocked_payload_byte_count_mismatch:
        return "payload byte count mismatch";
    case render_text_glyph_atlas_upload_operation_status::blocked_non_uploadable_materialization:
        return "materialization is not uploadable";
    }

    return "unknown blocker";
}

inline render_text_glyph_atlas_upload_operation_packet make_render_text_glyph_atlas_upload_operation(
    const render_text_glyph_atlas_page_plan_entry_snapshot& entry,
    const render_text_glyph_atlas_materialization_snapshot* materialization,
    const std::size_t operation_index,
    const std::size_t page_plan_entry_index)
{
    const render_text_glyph_atlas_upload_operation_status status =
        render_text_glyph_atlas_upload_operation_status_for(entry, materialization);
    const bool upload_ready =
        status == render_text_glyph_atlas_upload_operation_status::upload_ready;
    const bool clean_reuse =
        status == render_text_glyph_atlas_upload_operation_status::clean_reuse;
    const render_rect update_bounds =
        materialization != nullptr && materialization->has_atlas_update
            ? materialization->atlas_update_bounds
            : entry.atlas_bounds;
    const bool has_update_bounds =
        upload_ready
        && render_text_glyph_atlas_upload_operation_has_positive_rect(update_bounds);
    const std::size_t rgba_bytes =
        upload_ready && materialization != nullptr
            ? materialization->atlas_update_rgba_bytes
            : 0U;

    return render_text_glyph_atlas_upload_operation_packet{
        .operation_id = render_text_glyph_atlas_upload_operation_stable_id_for(entry),
        .stable_page_id = render_text_glyph_atlas_upload_operation_stable_page_id_for(entry.page),
        .operation_index = operation_index,
        .page_plan_entry_index = page_plan_entry_index,
        .materialization_index = entry.materialization_index,
        .materialization_id = entry.materialization_id,
        .run_index = entry.run_index,
        .cluster_index = entry.cluster_index,
        .cluster_byte_offset = entry.cluster_byte_offset,
        .cluster_byte_count = entry.cluster_byte_count,
        .status = status,
        .page_plan_status = entry.status,
        .materialization_status = entry.materialization_status,
        .cache_key = entry.cache_key,
        .has_cache_key = entry.has_cache_key,
        .page = entry.page,
        .page_id = entry.page.id,
        .atlas_bounds = entry.atlas_bounds,
        .has_atlas_bounds = entry.has_atlas_bounds,
        .update_bounds = update_bounds,
        .has_update_bounds = has_update_bounds,
        .rgba_byte_count = rgba_bytes,
        .dirty_upload = upload_ready,
        .clean_reuse = clean_reuse,
        .skipped = entry.skipped,
        .blocked = !upload_ready && !clean_reuse,
        .overflow = status == render_text_glyph_atlas_upload_operation_status::blocked_overflow,
        .payload_byte_count_mismatch =
            status == render_text_glyph_atlas_upload_operation_status::blocked_payload_byte_count_mismatch,
        .blocker_reason = render_text_glyph_atlas_upload_operation_blocker_reason_for(status),
        .diagnostic = upload_ready
            ? "glyph atlas upload operation is ready for renderer handoff"
            : (clean_reuse
                ? "glyph atlas upload operation reuses a clean atlas placement"
                : "glyph atlas upload operation is blocked: "
                    + render_text_glyph_atlas_upload_operation_blocker_reason_for(status)),
    };
}

inline render_text_glyph_atlas_upload_operation_page_summary*
render_text_glyph_atlas_upload_operation_find_page_summary(
    std::vector<render_text_glyph_atlas_upload_operation_page_summary>& pages,
    const render_text_atlas_page_id page_id)
{
    const auto match = std::find_if(
        pages.begin(),
        pages.end(),
        [&](const render_text_glyph_atlas_upload_operation_page_summary& page) {
            return page.page_id == page_id;
        });
    return match == pages.end() ? nullptr : &*match;
}

inline void append_render_text_glyph_atlas_upload_operation(
    render_text_glyph_atlas_upload_operation_plan_snapshot& plan,
    render_text_glyph_atlas_upload_operation_packet packet)
{
    ++plan.policy.operation_count;
    if (packet.dirty_upload) {
        ++plan.policy.upload_ready_count;
        plan.policy.total_upload_rgba_bytes += packet.rgba_byte_count;
        plan.policy.has_uploads = true;
    }
    if (packet.clean_reuse) {
        ++plan.policy.clean_reuse_count;
    }
    if (packet.skipped) {
        ++plan.policy.skipped_count;
    }
    if (packet.blocked) {
        ++plan.policy.blocked_count;
        plan.policy.has_blockers = true;
    }
    if (packet.overflow) {
        ++plan.policy.overflow_count;
        plan.policy.has_overflow = true;
    }
    if (packet.payload_byte_count_mismatch) {
        ++plan.policy.payload_byte_count_mismatch_count;
    }

    if (packet.page_id != 0U) {
        render_text_glyph_atlas_upload_operation_page_summary* page =
            render_text_glyph_atlas_upload_operation_find_page_summary(plan.pages, packet.page_id);
        if (page == nullptr) {
            plan.pages.push_back(render_text_glyph_atlas_upload_operation_page_summary{
                .stable_page_id = packet.stable_page_id,
                .page = packet.page,
                .page_id = packet.page_id,
            });
            page = &plan.pages.back();
        }
        ++page->operation_count;
        if (packet.dirty_upload) {
            ++page->upload_ready_count;
            page->upload_rgba_bytes += packet.rgba_byte_count;
            page->dirty = true;
        }
        if (packet.clean_reuse) {
            ++page->clean_reuse_count;
            page->clean_reuse = true;
        }
        if (packet.blocked) {
            ++page->blocked_count;
            page->has_blockers = true;
        }
        if (packet.overflow) {
            ++page->overflow_count;
            page->overflow = true;
        }
    }

    plan.operations.push_back(std::move(packet));
}

inline void summarize_render_text_glyph_atlas_upload_operation_pages(
    render_text_glyph_atlas_upload_operation_plan_snapshot& plan)
{
    plan.policy.page_count = plan.pages.size();
    plan.policy.dirty_page_count = 0;
    plan.policy.clean_reuse_page_count = 0;
    plan.policy.blocked_page_count = 0;
    for (const render_text_glyph_atlas_upload_operation_page_summary& page : plan.pages) {
        if (page.dirty) {
            ++plan.policy.dirty_page_count;
        }
        if (page.clean_reuse) {
            ++plan.policy.clean_reuse_page_count;
        }
        if (page.has_blockers) {
            ++plan.policy.blocked_page_count;
        }
    }
    plan.policy.diagnostic = "glyph atlas upload operation plan: "
        + std::to_string(plan.policy.upload_ready_count)
        + " uploads, "
        + std::to_string(plan.policy.clean_reuse_count)
        + " clean reuse, "
        + std::to_string(plan.policy.blocked_count)
        + " blocked";
}

inline render_text_glyph_atlas_upload_operation_plan_snapshot
plan_render_text_glyph_atlas_upload_operations(
    render_text_glyph_atlas_upload_operation_plan_request request)
{
    render_text_glyph_atlas_upload_operation_plan_snapshot plan;
    plan.policy.page_plan_entry_count = request.page_plan.entries.size();
    plan.policy.materialization_count = request.materializations.size();
    plan.operations.reserve(request.page_plan.entries.size());
    plan.pages.reserve(request.page_plan.pages.size());
    for (const render_text_glyph_atlas_page_plan_page_snapshot& page : request.page_plan.pages) {
        plan.pages.push_back(render_text_glyph_atlas_upload_operation_page_summary{
            .stable_page_id = render_text_glyph_atlas_upload_operation_stable_page_id_for(page.page),
            .page = page.page,
            .page_id = page.page.id,
            .has_blockers = page.overflow,
            .overflow = page.overflow,
        });
    }

    for (std::size_t index = 0; index < request.page_plan.entries.size(); ++index) {
        const render_text_glyph_atlas_page_plan_entry_snapshot& entry =
            request.page_plan.entries[index];
        const render_text_glyph_atlas_materialization_snapshot* materialization =
            render_text_glyph_atlas_upload_operation_materialization_for(
                request.materializations,
                entry);
        render_text_glyph_atlas_upload_operation_packet operation =
            make_render_text_glyph_atlas_upload_operation(
                entry,
                materialization,
                plan.operations.size(),
                index);
        if (operation.clean_reuse && !request.emit_clean_reuse_operations) {
            continue;
        }
        if (operation.blocked && !request.emit_blocked_operations) {
            continue;
        }
        append_render_text_glyph_atlas_upload_operation(plan, std::move(operation));
    }

    summarize_render_text_glyph_atlas_upload_operation_pages(plan);
    return plan;
}

inline render_text_glyph_atlas_upload_operation_plan_snapshot
plan_render_text_glyph_atlas_upload_operations(
    render_text_glyph_atlas_page_plan_snapshot page_plan,
    std::vector<render_text_glyph_atlas_materialization_snapshot> materializations)
{
    return plan_render_text_glyph_atlas_upload_operations(
        render_text_glyph_atlas_upload_operation_plan_request{
            .page_plan = std::move(page_plan),
            .materializations = std::move(materializations),
        });
}

} // namespace quiz_vulkan::render
