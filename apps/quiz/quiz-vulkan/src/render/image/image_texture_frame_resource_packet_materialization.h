#pragma once

#include "render/image/image_texture_frame_resource_packet_plan.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

enum class render_image_texture_frame_resource_packet_materialization_status {
    materialized,
    materialized_placeholder,
    blocked_missing_upload_result,
    blocked_missing_frame_binding,
    blocked_retry_backoff,
    blocked,
    removed,
};

inline std::string render_image_texture_frame_resource_packet_materialization_status_name(
    render_image_texture_frame_resource_packet_materialization_status status)
{
    switch (status) {
    case render_image_texture_frame_resource_packet_materialization_status::materialized:
        return "materialized";
    case render_image_texture_frame_resource_packet_materialization_status::materialized_placeholder:
        return "materialized_placeholder";
    case render_image_texture_frame_resource_packet_materialization_status::blocked_missing_upload_result:
        return "blocked_missing_upload_result";
    case render_image_texture_frame_resource_packet_materialization_status::blocked_missing_frame_binding:
        return "blocked_missing_frame_binding";
    case render_image_texture_frame_resource_packet_materialization_status::blocked_retry_backoff:
        return "blocked_retry_backoff";
    case render_image_texture_frame_resource_packet_materialization_status::blocked:
        return "blocked";
    case render_image_texture_frame_resource_packet_materialization_status::removed:
        return "removed";
    }

    return "unknown";
}

inline bool render_image_texture_frame_resource_packet_materialization_status_is_blocked(
    render_image_texture_frame_resource_packet_materialization_status status)
{
    return status
        == render_image_texture_frame_resource_packet_materialization_status::blocked_missing_upload_result
        || status
            == render_image_texture_frame_resource_packet_materialization_status::blocked_missing_frame_binding
        || status == render_image_texture_frame_resource_packet_materialization_status::blocked_retry_backoff
        || status == render_image_texture_frame_resource_packet_materialization_status::blocked;
}

inline bool render_image_texture_frame_resource_packet_materialization_status_is_materialized(
    render_image_texture_frame_resource_packet_materialization_status status)
{
    return status == render_image_texture_frame_resource_packet_materialization_status::materialized
        || status == render_image_texture_frame_resource_packet_materialization_status::materialized_placeholder;
}

inline render_image_texture_frame_resource_packet_materialization_status
render_image_texture_frame_resource_packet_materialization_status_for(
    render_image_texture_frame_resource_packet_status status)
{
    switch (status) {
    case render_image_texture_frame_resource_packet_status::resource_packet_ready:
        return render_image_texture_frame_resource_packet_materialization_status::materialized;
    case render_image_texture_frame_resource_packet_status::placeholder_backed:
        return render_image_texture_frame_resource_packet_materialization_status::materialized_placeholder;
    case render_image_texture_frame_resource_packet_status::blocked_missing_upload_result:
        return render_image_texture_frame_resource_packet_materialization_status::blocked_missing_upload_result;
    case render_image_texture_frame_resource_packet_status::blocked_missing_frame_binding:
        return render_image_texture_frame_resource_packet_materialization_status::blocked_missing_frame_binding;
    case render_image_texture_frame_resource_packet_status::blocked_retry_backoff:
        return render_image_texture_frame_resource_packet_materialization_status::blocked_retry_backoff;
    case render_image_texture_frame_resource_packet_status::blocked:
        return render_image_texture_frame_resource_packet_materialization_status::blocked;
    case render_image_texture_frame_resource_packet_status::removed:
        return render_image_texture_frame_resource_packet_materialization_status::removed;
    }

    return render_image_texture_frame_resource_packet_materialization_status::blocked;
}

struct render_image_texture_frame_resource_cache_handoff_record {
    std::size_t materialization_index = 0;
    std::size_t request_index = 0;
    std::string render_image_uri;
    std::string normalized_uri;
    render_image_cache_key cache_key;
    render_image_texture_key texture_key;
    std::string stable_texture_cache_key;
    render_image_texture_id texture_id = 0;
    render_image_revision texture_revision = 0;
    std::size_t texture_width = 0;
    std::size_t texture_height = 0;
    bool placeholder_backed = false;
    bool cache_reused = false;
    bool expected_cache_reuse = false;
    bool renderer_boundary_ready = false;
    std::string diagnostic;

    bool ok() const
    {
        return renderer_boundary_ready;
    }
};

struct render_image_texture_frame_resource_upload_handoff_record {
    std::size_t materialization_index = 0;
    std::size_t request_index = 0;
    std::uint64_t upload_request_id = 0;
    std::uint64_t upload_generation_id = 0;
    std::size_t mip_level_count = 0;
    std::size_t uploaded_byte_count = 0;
    render_image_decoded_payload_evidence decoded_payload;
    render_image_texture_upload_payload_layout_evidence payload_layout;
    render_image_texture_staging_payload_plan staging_payload_plan;
    bool upload_result_present = false;
    bool placeholder_backed = false;
    bool renderer_boundary_ready = false;
    std::string diagnostic;

    bool ok() const
    {
        return renderer_boundary_ready;
    }
};

struct render_image_texture_frame_resource_sampler_handoff_record {
    std::size_t materialization_index = 0;
    std::size_t request_index = 0;
    render_image_sampler_policy sampler;
    std::string sampler_key;
    std::string sampler_summary;
    bool placeholder_backed = false;
    bool renderer_boundary_ready = false;
    std::string diagnostic;

    bool ok() const
    {
        return renderer_boundary_ready;
    }
};

struct render_image_texture_frame_resource_packet_materialization_entry {
    std::size_t materialization_index = 0;
    std::size_t request_index = 0;
    render_image_texture_frame_resource_packet_materialization_status status =
        render_image_texture_frame_resource_packet_materialization_status::blocked;
    std::string status_name = render_image_texture_frame_resource_packet_materialization_status_name(
        render_image_texture_frame_resource_packet_materialization_status::blocked);
    render_image_texture_frame_resource_packet_status packet_status =
        render_image_texture_frame_resource_packet_status::blocked;
    std::string packet_status_name;
    bool materialized = false;
    bool placeholder_backed = false;
    bool blocked = true;
    bool removed = false;
    bool missing_upload_result = false;
    bool missing_frame_binding = false;
    bool retry_backoff_blocked = false;
    bool cache_record_present = false;
    bool upload_record_present = false;
    bool sampler_record_present = false;
    render_image_texture_frame_resource_cache_handoff_record cache_record;
    render_image_texture_frame_resource_upload_handoff_record upload_record;
    render_image_texture_frame_resource_sampler_handoff_record sampler_record;
    std::string blocker_summary;
    std::string diagnostic;

    bool ok() const
    {
        return materialized && !blocked;
    }
};

struct render_image_texture_frame_resource_packet_materialization {
    std::size_t frame_request_count = 0;
    std::size_t planned_packet_count = 0;
    std::size_t materialized_packet_count = 0;
    std::size_t placeholder_packet_count = 0;
    std::size_t blocked_packet_count = 0;
    std::size_t removed_packet_count = 0;
    std::size_t missing_upload_result_count = 0;
    std::size_t missing_frame_binding_count = 0;
    std::size_t retry_backoff_blocked_count = 0;
    std::size_t cache_record_count = 0;
    std::size_t upload_record_count = 0;
    std::size_t sampler_record_count = 0;
    bool renderer_boundary_ready = false;
    bool has_placeholders = false;
    bool has_blockers = false;
    bool has_retry_backoff = false;
    std::vector<render_image_texture_frame_resource_packet_materialization_entry> entries;
    std::vector<render_image_texture_frame_resource_cache_handoff_record> cache_handoff_records;
    std::vector<render_image_texture_frame_resource_upload_handoff_record> upload_handoff_records;
    std::vector<render_image_texture_frame_resource_sampler_handoff_record> sampler_handoff_records;
    std::string cache_handoff_summary;
    std::string upload_handoff_summary;
    std::string sampler_handoff_summary;
    std::string blocker_summary;
    std::string diagnostic;

    bool ok() const
    {
        return renderer_boundary_ready && !has_blockers;
    }
};

inline std::vector<const render_image_texture_frame_resource_packet_plan_entry*>
render_image_texture_frame_resource_packet_plan_entries_in_materialization_order(
    const render_image_texture_frame_resource_packet_plan& plan)
{
    std::vector<const render_image_texture_frame_resource_packet_plan_entry*> entries;
    entries.reserve(plan.entries.size());
    for (const render_image_texture_frame_resource_packet_plan_entry& entry : plan.entries) {
        entries.push_back(&entry);
    }

    std::sort(
        entries.begin(),
        entries.end(),
        [](const render_image_texture_frame_resource_packet_plan_entry* lhs,
           const render_image_texture_frame_resource_packet_plan_entry* rhs) {
            return std::tie(
                       lhs->request_index,
                       lhs->stable_texture_cache_key,
                       lhs->sampler_summary,
                       lhs->upload_request_id)
                < std::tie(
                    rhs->request_index,
                    rhs->stable_texture_cache_key,
                    rhs->sampler_summary,
                    rhs->upload_request_id);
        });
    return entries;
}

inline render_image_texture_frame_resource_cache_handoff_record
make_render_image_texture_frame_resource_cache_handoff_record(
    const render_image_texture_frame_resource_packet_plan_entry& packet,
    std::size_t materialization_index)
{
    return render_image_texture_frame_resource_cache_handoff_record{
        .materialization_index = materialization_index,
        .request_index = packet.request_index,
        .render_image_uri = packet.render_image_uri,
        .normalized_uri = packet.normalized_uri,
        .cache_key = packet.cache_key,
        .texture_key = packet.texture_key,
        .stable_texture_cache_key = packet.stable_texture_cache_key,
        .texture_id = packet.texture_id,
        .texture_revision = packet.texture_revision,
        .texture_width = packet.texture_width,
        .texture_height = packet.texture_height,
        .placeholder_backed = packet.placeholder_backed,
        .cache_reused = packet.cache_reused,
        .expected_cache_reuse = packet.expected_cache_reuse,
        .renderer_boundary_ready = packet.bindable,
        .diagnostic = packet.placeholder_backed
            ? "image frame resource cache handoff uses placeholder texture"
            : "image frame resource cache handoff is ready",
    };
}

inline render_image_texture_frame_resource_upload_handoff_record
make_render_image_texture_frame_resource_upload_handoff_record(
    const render_image_texture_frame_resource_packet_plan_entry& packet,
    std::size_t materialization_index)
{
    return render_image_texture_frame_resource_upload_handoff_record{
        .materialization_index = materialization_index,
        .request_index = packet.request_index,
        .upload_request_id = packet.upload_request_id,
        .upload_generation_id = packet.upload_generation_id,
        .mip_level_count = packet.mip_level_count,
        .uploaded_byte_count = packet.uploaded_byte_count,
        .decoded_payload = packet.decoded_payload,
        .payload_layout = packet.payload_layout,
        .staging_payload_plan = packet.staging_payload_plan,
        .upload_result_present = packet.upload_request_id != 0 || packet.upload_generation_id != 0,
        .placeholder_backed = packet.placeholder_backed,
        .renderer_boundary_ready = packet.bindable,
        .diagnostic = packet.placeholder_backed
            ? "image frame resource upload handoff uses placeholder upload"
            : "image frame resource upload handoff is ready",
    };
}

inline render_image_texture_frame_resource_sampler_handoff_record
make_render_image_texture_frame_resource_sampler_handoff_record(
    const render_image_texture_frame_resource_packet_plan_entry& packet,
    std::size_t materialization_index)
{
    return render_image_texture_frame_resource_sampler_handoff_record{
        .materialization_index = materialization_index,
        .request_index = packet.request_index,
        .sampler = packet.sampler,
        .sampler_key = packet.sampler_key,
        .sampler_summary = packet.sampler_summary,
        .placeholder_backed = packet.placeholder_backed,
        .renderer_boundary_ready = packet.bindable,
        .diagnostic = packet.placeholder_backed
            ? "image frame resource sampler handoff uses placeholder texture"
            : "image frame resource sampler handoff is ready",
    };
}

inline render_image_texture_frame_resource_packet_materialization_entry
make_render_image_texture_frame_resource_packet_materialization_entry(
    const render_image_texture_frame_resource_packet_plan_entry& packet,
    std::size_t materialization_index)
{
    const render_image_texture_frame_resource_packet_materialization_status status =
        render_image_texture_frame_resource_packet_materialization_status_for(packet.status);
    const bool materialized =
        render_image_texture_frame_resource_packet_materialization_status_is_materialized(status);
    const bool blocked =
        render_image_texture_frame_resource_packet_materialization_status_is_blocked(status);

    render_image_texture_frame_resource_packet_materialization_entry entry{
        .materialization_index = materialization_index,
        .request_index = packet.request_index,
        .status = status,
        .status_name = render_image_texture_frame_resource_packet_materialization_status_name(status),
        .packet_status = packet.status,
        .packet_status_name = packet.status_name,
        .materialized = materialized,
        .placeholder_backed =
            status == render_image_texture_frame_resource_packet_materialization_status::materialized_placeholder,
        .blocked = blocked,
        .removed = status == render_image_texture_frame_resource_packet_materialization_status::removed,
        .missing_upload_result =
            status
            == render_image_texture_frame_resource_packet_materialization_status::blocked_missing_upload_result,
        .missing_frame_binding =
            status
            == render_image_texture_frame_resource_packet_materialization_status::blocked_missing_frame_binding,
        .retry_backoff_blocked =
            status == render_image_texture_frame_resource_packet_materialization_status::blocked_retry_backoff,
        .blocker_summary = packet.blocker_summary,
    };

    if (materialized) {
        entry.cache_record = make_render_image_texture_frame_resource_cache_handoff_record(
            packet,
            materialization_index);
        entry.upload_record = make_render_image_texture_frame_resource_upload_handoff_record(
            packet,
            materialization_index);
        entry.sampler_record = make_render_image_texture_frame_resource_sampler_handoff_record(
            packet,
            materialization_index);
        entry.cache_record_present = true;
        entry.upload_record_present = true;
        entry.sampler_record_present = true;
    }

    switch (entry.status) {
    case render_image_texture_frame_resource_packet_materialization_status::materialized:
        entry.diagnostic = "image frame resource packet materialized";
        break;
    case render_image_texture_frame_resource_packet_materialization_status::materialized_placeholder:
        entry.diagnostic = "image frame resource packet materialized with placeholder";
        break;
    case render_image_texture_frame_resource_packet_materialization_status::blocked_missing_upload_result:
        entry.diagnostic = "image frame resource packet materialization blocked by missing upload result";
        break;
    case render_image_texture_frame_resource_packet_materialization_status::blocked_missing_frame_binding:
        entry.diagnostic = "image frame resource packet materialization blocked by missing frame binding";
        break;
    case render_image_texture_frame_resource_packet_materialization_status::blocked_retry_backoff:
        entry.diagnostic = "image frame resource packet materialization blocked by retry backoff";
        break;
    case render_image_texture_frame_resource_packet_materialization_status::blocked:
        entry.diagnostic = "image frame resource packet materialization blocked";
        break;
    case render_image_texture_frame_resource_packet_materialization_status::removed:
        entry.diagnostic = "image frame resource packet materialization skipped removed packet";
        break;
    }

    return entry;
}

inline void count_render_image_texture_frame_resource_packet_materialization_entry(
    render_image_texture_frame_resource_packet_materialization& materialization,
    const render_image_texture_frame_resource_packet_materialization_entry& entry)
{
    switch (entry.status) {
    case render_image_texture_frame_resource_packet_materialization_status::materialized:
        ++materialization.materialized_packet_count;
        break;
    case render_image_texture_frame_resource_packet_materialization_status::materialized_placeholder:
        ++materialization.materialized_packet_count;
        ++materialization.placeholder_packet_count;
        materialization.has_placeholders = true;
        break;
    case render_image_texture_frame_resource_packet_materialization_status::blocked_missing_upload_result:
        ++materialization.missing_upload_result_count;
        ++materialization.blocked_packet_count;
        materialization.has_blockers = true;
        break;
    case render_image_texture_frame_resource_packet_materialization_status::blocked_missing_frame_binding:
        ++materialization.missing_frame_binding_count;
        ++materialization.blocked_packet_count;
        materialization.has_blockers = true;
        break;
    case render_image_texture_frame_resource_packet_materialization_status::blocked_retry_backoff:
        ++materialization.retry_backoff_blocked_count;
        ++materialization.blocked_packet_count;
        materialization.has_blockers = true;
        materialization.has_retry_backoff = true;
        append_render_image_texture_frame_upload_handoff_summary_fragment(
            materialization.blocker_summary,
            entry.blocker_summary);
        break;
    case render_image_texture_frame_resource_packet_materialization_status::blocked:
        ++materialization.blocked_packet_count;
        materialization.has_blockers = true;
        append_render_image_texture_frame_upload_handoff_summary_fragment(
            materialization.blocker_summary,
            entry.blocker_summary);
        break;
    case render_image_texture_frame_resource_packet_materialization_status::removed:
        ++materialization.removed_packet_count;
        break;
    }
}

inline void finalize_render_image_texture_frame_resource_packet_materialization(
    render_image_texture_frame_resource_packet_materialization& materialization,
    const render_image_texture_frame_resource_packet_plan& plan)
{
    materialization.cache_record_count = materialization.cache_handoff_records.size();
    materialization.upload_record_count = materialization.upload_handoff_records.size();
    materialization.sampler_record_count = materialization.sampler_handoff_records.size();
    materialization.renderer_boundary_ready = plan.ok() && !materialization.has_blockers;

    materialization.cache_handoff_summary = "cache_records="
        + std::to_string(materialization.cache_record_count)
        + "; stable_cache_keys=" + std::to_string(plan.unique_texture_cache_key_count);
    materialization.upload_handoff_summary = "upload_records="
        + std::to_string(materialization.upload_record_count)
        + "; uploaded_bytes=" + std::to_string(plan.uploaded_byte_count);
    materialization.sampler_handoff_summary = "sampler_records="
        + std::to_string(materialization.sampler_record_count)
        + "; sampler_keys=" + std::to_string(plan.unique_sampler_key_count);
    if (materialization.blocker_summary.empty()) {
        materialization.blocker_summary = "no materialization blockers";
    }

    if (materialization.planned_packet_count == 0) {
        materialization.diagnostic = "image frame resource packet materialization has no packets";
    } else if (materialization.missing_upload_result_count != 0) {
        materialization.diagnostic = "image frame resource packet materialization has missing upload results";
    } else if (materialization.missing_frame_binding_count != 0) {
        materialization.diagnostic = "image frame resource packet materialization has missing frame bindings";
    } else if (materialization.retry_backoff_blocked_count != 0) {
        materialization.diagnostic = "image frame resource packet materialization is blocked by retry backoff";
    } else if (materialization.blocked_packet_count != 0) {
        materialization.diagnostic = "image frame resource packet materialization has blocked packets";
    } else if (materialization.placeholder_packet_count != 0) {
        materialization.diagnostic = "image frame resource packet materialization is placeholder-backed";
    } else {
        materialization.diagnostic = "image frame resource packet materialization is ready";
    }
}

inline render_image_texture_frame_resource_packet_materialization
materialize_render_image_texture_frame_resource_packets(
    const render_image_texture_frame_resource_packet_plan& plan)
{
    render_image_texture_frame_resource_packet_materialization materialization{
        .frame_request_count = plan.frame_request_count,
        .planned_packet_count = plan.entries.size(),
    };

    const std::vector<const render_image_texture_frame_resource_packet_plan_entry*> ordered_entries =
        render_image_texture_frame_resource_packet_plan_entries_in_materialization_order(plan);

    for (const render_image_texture_frame_resource_packet_plan_entry* packet : ordered_entries) {
        render_image_texture_frame_resource_packet_materialization_entry entry =
            make_render_image_texture_frame_resource_packet_materialization_entry(
                *packet,
                materialization.entries.size());
        count_render_image_texture_frame_resource_packet_materialization_entry(materialization, entry);
        if (entry.cache_record_present) {
            materialization.cache_handoff_records.push_back(entry.cache_record);
        }
        if (entry.upload_record_present) {
            materialization.upload_handoff_records.push_back(entry.upload_record);
        }
        if (entry.sampler_record_present) {
            materialization.sampler_handoff_records.push_back(entry.sampler_record);
        }
        materialization.entries.push_back(std::move(entry));
    }

    finalize_render_image_texture_frame_resource_packet_materialization(materialization, plan);
    return materialization;
}

inline render_image_texture_frame_resource_packet_materialization
materialize_render_image_texture_frame_resource_packets(
    const render_image_texture_frame_upload_handoff_summary& handoff)
{
    return materialize_render_image_texture_frame_resource_packets(
        make_render_image_texture_frame_resource_packet_plan(handoff));
}

inline render_image_texture_frame_resource_packet_materialization
materialize_render_image_texture_frame_resource_packets(
    const render_image_texture_frame_snapshot& frame,
    const render_image_texture_frame_binding_plan& binding_plan,
    const render_image_texture_upload_result_snapshot& upload_result)
{
    return materialize_render_image_texture_frame_resource_packets(
        make_render_image_texture_frame_resource_packet_plan(frame, binding_plan, upload_result));
}

inline render_image_texture_frame_resource_packet_materialization
materialize_render_image_texture_frame_resource_packets(
    const render_image_texture_frame_snapshot& frame,
    const render_image_texture_upload_result_snapshot& upload_result)
{
    return materialize_render_image_texture_frame_resource_packets(
        make_render_image_texture_frame_resource_packet_plan(frame, upload_result));
}

enum class render_image_texture_frame_resource_packet_consumption_diff_entry_status {
    unchanged,
    added,
    removed,
    changed,
};

inline std::string render_image_texture_frame_resource_packet_consumption_diff_entry_status_name(
    render_image_texture_frame_resource_packet_consumption_diff_entry_status status)
{
    switch (status) {
    case render_image_texture_frame_resource_packet_consumption_diff_entry_status::unchanged:
        return "unchanged";
    case render_image_texture_frame_resource_packet_consumption_diff_entry_status::added:
        return "added";
    case render_image_texture_frame_resource_packet_consumption_diff_entry_status::removed:
        return "removed";
    case render_image_texture_frame_resource_packet_consumption_diff_entry_status::changed:
        return "changed";
    }

    return "unknown";
}

struct render_image_texture_frame_resource_packet_consumption_entry {
    std::size_t materialization_index = 0;
    std::size_t request_index = 0;
    std::string stable_packet_identity;
    bool stable_packet_identity_present = false;
    bool duplicate_stable_packet_identity = false;
    render_image_texture_frame_resource_packet_materialization_status materialization_status =
        render_image_texture_frame_resource_packet_materialization_status::blocked;
    std::string materialization_status_name = render_image_texture_frame_resource_packet_materialization_status_name(
        render_image_texture_frame_resource_packet_materialization_status::blocked);
    render_image_texture_frame_resource_packet_status packet_status =
        render_image_texture_frame_resource_packet_status::blocked;
    std::string packet_status_name;
    std::string stable_texture_cache_key;
    render_image_texture_id texture_id = 0;
    render_image_revision texture_revision = 0;
    std::size_t texture_width = 0;
    std::size_t texture_height = 0;
    std::string sampler_key;
    std::uint64_t upload_request_id = 0;
    std::uint64_t upload_generation_id = 0;
    std::size_t uploaded_byte_count = 0;
    std::uint64_t decoded_payload_hash = 0;
    std::size_t decoded_byte_count = 0;
    std::size_t upload_layout_byte_count = 0;
    std::size_t upload_layout_row_stride_byte_count = 0;
    std::size_t staging_payload_byte_count = 0;
    std::size_t staging_row_copy_count = 0;
    bool materialized = false;
    bool placeholder_backed = false;
    bool blocked = true;
    bool removed = false;
    bool renderer_boundary_ready = false;
    bool decoded_payload_valid = false;
    bool upload_payload_layout_ready = false;
    bool staging_payload_ready = false;
    bool decoded_resource_ready = false;
    bool decoded_resource_blocked = false;
    std::string blocker_summary;
    std::string decoded_resource_summary;
    std::string decoded_resource_blocker_summary;
    std::string diagnostic;

    bool ok() const
    {
        return materialized && !blocked && decoded_resource_ready && !decoded_resource_blocked;
    }
};

struct render_image_texture_frame_resource_packet_consumption_summary {
    std::size_t frame_request_count = 0;
    std::size_t packet_count = 0;
    std::size_t materialized_packet_count = 0;
    std::size_t placeholder_packet_count = 0;
    std::size_t blocked_packet_count = 0;
    std::size_t removed_packet_count = 0;
    std::size_t stable_packet_identity_count = 0;
    std::size_t duplicate_stable_packet_identity_count = 0;
    std::size_t missing_stable_packet_identity_count = 0;
    std::size_t decoded_resource_ready_count = 0;
    std::size_t decoded_resource_blocked_count = 0;
    std::size_t decoded_payload_hash_count = 0;
    std::size_t decoded_payload_byte_count = 0;
    std::size_t staging_payload_byte_count = 0;
    bool renderer_boundary_ready = false;
    bool has_placeholders = false;
    bool has_blockers = false;
    bool has_duplicate_stable_packet_identity = false;
    bool has_missing_stable_packet_identity = false;
    bool has_decoded_resource_blockers = false;
    std::vector<render_image_texture_frame_resource_packet_consumption_entry> entries;
    std::string identity_summary;
    std::string decoded_resource_summary;
    std::string decoded_resource_blocker_summary;
    std::string diagnostic;

    bool ok() const
    {
        return renderer_boundary_ready && !has_blockers && !has_decoded_resource_blockers;
    }
};

struct render_image_texture_frame_resource_packet_consumption_entry_diff {
    std::size_t request_index = 0;
    render_image_texture_frame_resource_packet_consumption_diff_entry_status status =
        render_image_texture_frame_resource_packet_consumption_diff_entry_status::unchanged;
    std::string status_name = render_image_texture_frame_resource_packet_consumption_diff_entry_status_name(
        render_image_texture_frame_resource_packet_consumption_diff_entry_status::unchanged);
    bool before_present = false;
    bool after_present = false;
    std::string before_stable_packet_identity;
    std::string after_stable_packet_identity;
    render_image_texture_frame_resource_packet_materialization_status before_materialization_status =
        render_image_texture_frame_resource_packet_materialization_status::blocked;
    render_image_texture_frame_resource_packet_materialization_status after_materialization_status =
        render_image_texture_frame_resource_packet_materialization_status::blocked;
    std::string before_materialization_status_name;
    std::string after_materialization_status_name;
    std::string before_stable_texture_cache_key;
    std::string after_stable_texture_cache_key;
    render_image_texture_id before_texture_id = 0;
    render_image_texture_id after_texture_id = 0;
    render_image_revision before_texture_revision = 0;
    render_image_revision after_texture_revision = 0;
    std::size_t before_texture_width = 0;
    std::size_t after_texture_width = 0;
    std::size_t before_texture_height = 0;
    std::size_t after_texture_height = 0;
    std::string before_sampler_key;
    std::string after_sampler_key;
    std::uint64_t before_upload_request_id = 0;
    std::uint64_t after_upload_request_id = 0;
    std::uint64_t before_upload_generation_id = 0;
    std::uint64_t after_upload_generation_id = 0;
    std::size_t before_uploaded_byte_count = 0;
    std::size_t after_uploaded_byte_count = 0;
    std::int64_t uploaded_byte_delta = 0;
    bool before_ready = false;
    bool after_ready = false;
    bool before_placeholder_backed = false;
    bool after_placeholder_backed = false;
    bool before_blocked = false;
    bool after_blocked = false;
    bool before_removed = false;
    bool after_removed = false;
    bool before_stable_packet_identity_present = false;
    bool after_stable_packet_identity_present = false;
    bool before_duplicate_stable_packet_identity = false;
    bool after_duplicate_stable_packet_identity = false;
    std::string before_blocker_summary;
    std::string after_blocker_summary;
    bool stable_packet_identity_changed = false;
    bool stable_texture_cache_key_changed = false;
    bool texture_handle_changed = false;
    bool texture_extent_changed = false;
    bool sampler_key_changed = false;
    bool upload_request_changed = false;
    bool upload_generation_changed = false;
    bool uploaded_byte_count_changed = false;
    bool placeholder_changed = false;
    bool readiness_changed = false;
    bool blocker_changed = false;
    bool stable_packet_identity_quality_changed = false;
    bool ready_regressed = false;
    bool ready_recovered = false;
    bool regression = false;
    bool improvement = false;
    std::string diagnostic;

    bool changed() const
    {
        return status != render_image_texture_frame_resource_packet_consumption_diff_entry_status::unchanged;
    }

    bool ok() const
    {
        return !regression;
    }
};

struct render_image_texture_frame_resource_packet_consumption_diff {
    std::size_t before_frame_request_count = 0;
    std::size_t after_frame_request_count = 0;
    std::int64_t frame_request_delta = 0;
    std::size_t before_packet_count = 0;
    std::size_t after_packet_count = 0;
    std::int64_t packet_delta = 0;
    std::size_t before_materialized_packet_count = 0;
    std::size_t after_materialized_packet_count = 0;
    std::int64_t materialized_packet_delta = 0;
    std::size_t before_blocked_packet_count = 0;
    std::size_t after_blocked_packet_count = 0;
    std::int64_t blocked_packet_delta = 0;
    std::size_t before_placeholder_packet_count = 0;
    std::size_t after_placeholder_packet_count = 0;
    std::int64_t placeholder_packet_delta = 0;
    std::size_t before_removed_packet_count = 0;
    std::size_t after_removed_packet_count = 0;
    std::int64_t removed_packet_delta = 0;
    std::size_t before_stable_packet_identity_count = 0;
    std::size_t after_stable_packet_identity_count = 0;
    std::int64_t stable_packet_identity_delta = 0;
    std::size_t before_duplicate_stable_packet_identity_count = 0;
    std::size_t after_duplicate_stable_packet_identity_count = 0;
    std::int64_t duplicate_stable_packet_identity_delta = 0;
    std::size_t before_missing_stable_packet_identity_count = 0;
    std::size_t after_missing_stable_packet_identity_count = 0;
    std::int64_t missing_stable_packet_identity_delta = 0;
    std::size_t unchanged_packet_count = 0;
    std::size_t added_packet_count = 0;
    std::size_t removed_packet_count = 0;
    std::size_t changed_packet_count = 0;
    std::size_t stable_packet_identity_changed_count = 0;
    std::size_t stable_texture_cache_key_changed_count = 0;
    std::size_t texture_handle_changed_count = 0;
    std::size_t texture_extent_changed_count = 0;
    std::size_t sampler_key_changed_count = 0;
    std::size_t upload_request_changed_count = 0;
    std::size_t upload_generation_changed_count = 0;
    std::size_t uploaded_byte_count_changed_count = 0;
    std::size_t placeholder_changed_count = 0;
    std::size_t readiness_changed_count = 0;
    std::size_t blocker_changed_count = 0;
    std::size_t stable_packet_identity_quality_changed_count = 0;
    std::size_t ready_regression_count = 0;
    std::size_t ready_recovery_count = 0;
    bool before_renderer_boundary_ready = false;
    bool after_renderer_boundary_ready = false;
    bool renderer_boundary_regressed = false;
    bool renderer_boundary_recovered = false;
    bool stable_no_change_frame = false;
    bool has_changes = false;
    bool has_regression = false;
    bool has_improvement = false;
    bool has_duplicate_or_missing_identity_change = false;
    std::string changed_summary;
    std::string identity_summary;
    std::string regression_summary;
    std::string improvement_summary;
    std::vector<render_image_texture_frame_resource_packet_consumption_entry_diff> entries;
    std::string diagnostic;

    bool ok() const
    {
        return !has_regression;
    }
};

inline std::string render_image_texture_frame_resource_packet_consumption_stable_identity_for(
    const render_image_texture_frame_resource_packet_materialization_entry& entry)
{
    if (!entry.cache_record_present || !entry.upload_record_present || !entry.sampler_record_present) {
        return {};
    }

    return entry.cache_record.stable_texture_cache_key
        + "|sampler=" + entry.sampler_record.sampler_key
        + "|texture=" + std::to_string(entry.cache_record.texture_id)
        + ":" + std::to_string(entry.cache_record.texture_revision)
        + "|upload=" + std::to_string(entry.upload_record.upload_request_id)
        + ":" + std::to_string(entry.upload_record.upload_generation_id);
}

inline render_image_texture_frame_resource_packet_consumption_entry
make_render_image_texture_frame_resource_packet_consumption_entry(
    const render_image_texture_frame_resource_packet_materialization_entry& entry)
{
    render_image_texture_frame_resource_packet_consumption_entry consumption{
        .materialization_index = entry.materialization_index,
        .request_index = entry.request_index,
        .stable_packet_identity =
            render_image_texture_frame_resource_packet_consumption_stable_identity_for(entry),
        .materialization_status = entry.status,
        .materialization_status_name = entry.status_name,
        .packet_status = entry.packet_status,
        .packet_status_name = entry.packet_status_name,
        .materialized = entry.materialized,
        .placeholder_backed = entry.placeholder_backed,
        .blocked = entry.blocked,
        .removed = entry.removed,
        .renderer_boundary_ready = entry.ok(),
        .blocker_summary = entry.blocker_summary,
    };
    consumption.stable_packet_identity_present = !consumption.stable_packet_identity.empty();

    if (entry.cache_record_present) {
        consumption.stable_texture_cache_key = entry.cache_record.stable_texture_cache_key;
        consumption.texture_id = entry.cache_record.texture_id;
        consumption.texture_revision = entry.cache_record.texture_revision;
        consumption.texture_width = entry.cache_record.texture_width;
        consumption.texture_height = entry.cache_record.texture_height;
    }
    if (entry.upload_record_present) {
        consumption.upload_request_id = entry.upload_record.upload_request_id;
        consumption.upload_generation_id = entry.upload_record.upload_generation_id;
        consumption.uploaded_byte_count = entry.upload_record.uploaded_byte_count;
        consumption.decoded_payload_hash = entry.upload_record.decoded_payload.stable_byte_hash;
        consumption.decoded_byte_count = entry.upload_record.decoded_payload.decoded_byte_count;
        consumption.upload_layout_byte_count = entry.upload_record.payload_layout.decoded_byte_count;
        consumption.upload_layout_row_stride_byte_count =
            entry.upload_record.payload_layout.row_stride_byte_count;
        consumption.staging_payload_byte_count =
            entry.upload_record.staging_payload_plan.total_staging_byte_count;
        consumption.staging_row_copy_count = entry.upload_record.staging_payload_plan.row_copy_count;
        consumption.decoded_payload_valid = entry.upload_record.decoded_payload.payload_valid;
        consumption.upload_payload_layout_ready = entry.upload_record.payload_layout.ok();
        consumption.staging_payload_ready = entry.upload_record.staging_payload_plan.ok();
    }
    if (entry.sampler_record_present) {
        consumption.sampler_key = entry.sampler_record.sampler_key;
    }
    consumption.decoded_resource_ready = consumption.materialized
        && consumption.decoded_payload_valid
        && consumption.upload_payload_layout_ready
        && consumption.staging_payload_ready;
    consumption.decoded_resource_blocked = consumption.materialized && !consumption.decoded_resource_ready;
    if (consumption.materialized && !consumption.decoded_payload_valid) {
        consumption.decoded_resource_blocker_summary = entry.upload_record.decoded_payload.diagnostic.empty()
            ? "decoded payload bytes are missing or invalid"
            : entry.upload_record.decoded_payload.diagnostic;
    } else if (consumption.materialized && !consumption.upload_payload_layout_ready) {
        consumption.decoded_resource_blocker_summary = entry.upload_record.payload_layout.diagnostic.empty()
            ? "upload payload layout is not ready"
            : entry.upload_record.payload_layout.diagnostic;
    } else if (consumption.materialized && !consumption.staging_payload_ready) {
        consumption.decoded_resource_blocker_summary =
            entry.upload_record.staging_payload_plan.blocker_summary.empty()
            ? entry.upload_record.staging_payload_plan.diagnostic
            : entry.upload_record.staging_payload_plan.blocker_summary;
    } else if (!consumption.materialized && consumption.blocked) {
        consumption.decoded_resource_blocker_summary = consumption.blocker_summary;
    }
    consumption.decoded_resource_summary = "decoded_bytes=" + std::to_string(consumption.decoded_byte_count)
        + "; staging_bytes=" + std::to_string(consumption.staging_payload_byte_count)
        + "; payload_hash=" + std::to_string(consumption.decoded_payload_hash);

    if (!consumption.stable_packet_identity_present) {
        consumption.diagnostic = "image frame resource packet consumption identity is missing";
    } else if (consumption.decoded_resource_blocked) {
        consumption.diagnostic = "image frame resource packet consumption decoded resource is blocked";
    } else if (consumption.placeholder_backed) {
        consumption.diagnostic = "image frame resource packet consumption identity uses placeholder";
    } else if (consumption.ok()) {
        consumption.diagnostic = "image frame resource packet consumption identity is ready";
    } else {
        consumption.diagnostic = "image frame resource packet consumption identity is blocked";
    }
    return consumption;
}

inline render_image_texture_frame_resource_packet_consumption_summary
make_render_image_texture_frame_resource_packet_consumption_summary(
    const render_image_texture_frame_resource_packet_materialization& materialization)
{
    render_image_texture_frame_resource_packet_consumption_summary summary{
        .frame_request_count = materialization.frame_request_count,
        .packet_count = materialization.entries.size(),
        .materialized_packet_count = materialization.materialized_packet_count,
        .placeholder_packet_count = materialization.placeholder_packet_count,
        .blocked_packet_count = materialization.blocked_packet_count,
        .removed_packet_count = materialization.removed_packet_count,
        .renderer_boundary_ready = materialization.renderer_boundary_ready,
        .has_placeholders = materialization.has_placeholders,
        .has_blockers = materialization.has_blockers,
    };

    std::map<std::string, std::size_t> identity_counts;
    std::map<std::uint64_t, bool> decoded_payload_hashes;
    for (const render_image_texture_frame_resource_packet_materialization_entry& entry : materialization.entries) {
        render_image_texture_frame_resource_packet_consumption_entry consumption =
            make_render_image_texture_frame_resource_packet_consumption_entry(entry);
        if (consumption.stable_packet_identity_present) {
            ++identity_counts[consumption.stable_packet_identity];
        } else {
            ++summary.missing_stable_packet_identity_count;
            summary.has_missing_stable_packet_identity = true;
        }
        if (consumption.decoded_resource_ready) {
            ++summary.decoded_resource_ready_count;
            summary.decoded_payload_byte_count += consumption.decoded_byte_count;
            summary.staging_payload_byte_count += consumption.staging_payload_byte_count;
            if (consumption.decoded_payload_hash != 0) {
                decoded_payload_hashes.emplace(consumption.decoded_payload_hash, true);
            }
        } else if (consumption.decoded_resource_blocked || consumption.blocked) {
            ++summary.decoded_resource_blocked_count;
            summary.has_decoded_resource_blockers = true;
            append_render_image_texture_frame_upload_handoff_summary_fragment(
                summary.decoded_resource_blocker_summary,
                consumption.decoded_resource_blocker_summary.empty()
                    ? consumption.blocker_summary
                    : consumption.decoded_resource_blocker_summary);
        }
        summary.entries.push_back(std::move(consumption));
    }

    summary.stable_packet_identity_count = identity_counts.size();
    summary.decoded_payload_hash_count = decoded_payload_hashes.size();
    for (render_image_texture_frame_resource_packet_consumption_entry& entry : summary.entries) {
        const auto count = identity_counts.find(entry.stable_packet_identity);
        if (entry.stable_packet_identity_present && count != identity_counts.end() && count->second > 1) {
            entry.duplicate_stable_packet_identity = true;
            ++summary.duplicate_stable_packet_identity_count;
            summary.has_duplicate_stable_packet_identity = true;
            entry.diagnostic = "image frame resource packet consumption identity is duplicated";
        }
    }

    summary.identity_summary = "packets=" + std::to_string(summary.packet_count)
        + "; identities=" + std::to_string(summary.stable_packet_identity_count)
        + "; duplicate_identities=" + std::to_string(summary.duplicate_stable_packet_identity_count)
        + "; missing_identities=" + std::to_string(summary.missing_stable_packet_identity_count);
    summary.decoded_resource_summary = "decoded_resources=" + std::to_string(summary.decoded_resource_ready_count)
        + "; payload_hashes=" + std::to_string(summary.decoded_payload_hash_count)
        + "; decoded_bytes=" + std::to_string(summary.decoded_payload_byte_count)
        + "; staging_bytes=" + std::to_string(summary.staging_payload_byte_count);
    if (summary.decoded_resource_blocker_summary.empty()) {
        summary.decoded_resource_blocker_summary = "no decoded resource blockers";
    }
    if (summary.packet_count == 0) {
        summary.diagnostic = "image frame resource packet consumption summary has no packets";
    } else if (summary.has_missing_stable_packet_identity) {
        summary.diagnostic = "image frame resource packet consumption summary has missing packet identities";
    } else if (summary.has_duplicate_stable_packet_identity) {
        summary.diagnostic = "image frame resource packet consumption summary has duplicate packet identities";
    } else if (summary.has_decoded_resource_blockers) {
        summary.diagnostic = "image frame resource packet consumption summary has decoded resource blockers";
    } else if (summary.has_blockers) {
        summary.diagnostic = "image frame resource packet consumption summary has blocked packets";
    } else if (summary.has_placeholders) {
        summary.diagnostic = "image frame resource packet consumption summary is placeholder-backed";
    } else {
        summary.diagnostic = "image frame resource packet consumption summary is ready";
    }
    return summary;
}

inline const render_image_texture_frame_resource_packet_consumption_entry*
render_image_texture_frame_resource_packet_consumption_entry_for_request_index(
    const render_image_texture_frame_resource_packet_consumption_summary& summary,
    std::size_t request_index)
{
    for (const render_image_texture_frame_resource_packet_consumption_entry& entry : summary.entries) {
        if (entry.request_index == request_index) {
            return &entry;
        }
    }
    return nullptr;
}

inline void apply_render_image_texture_frame_resource_packet_consumption_to_renderer_texture_quad_packet(
    render_image_renderer_texture_quad_packet& packet,
    const render_image_texture_frame_resource_packet_consumption_entry& consumption)
{
    packet.decoded_resource_evidence_present = true;
    packet.decoded_payload_hash = consumption.decoded_payload_hash;
    packet.decoded_byte_count = consumption.decoded_byte_count;
    packet.upload_layout_byte_count = consumption.upload_layout_byte_count;
    packet.upload_layout_row_stride_byte_count = consumption.upload_layout_row_stride_byte_count;
    packet.staging_payload_byte_count = consumption.staging_payload_byte_count;
    packet.staging_row_copy_count = consumption.staging_row_copy_count;
    packet.decoded_payload_valid = consumption.decoded_payload_valid;
    packet.upload_payload_layout_ready = consumption.upload_payload_layout_ready;
    packet.staging_payload_ready = consumption.staging_payload_ready;
    packet.decoded_resource_ready = consumption.decoded_resource_ready;
    packet.decoded_resource_blocked = consumption.decoded_resource_blocked || consumption.blocked;
    packet.decoded_resource_summary = consumption.decoded_resource_summary;
    packet.decoded_resource_blocker_summary = consumption.decoded_resource_blocker_summary;
    if (packet.decoded_resource_blocked && !packet.decoded_resource_blocker_summary.empty()) {
        packet.blocker_summary = packet.decoded_resource_blocker_summary;
    }
    finalize_render_image_renderer_texture_quad_packet(packet);
}

inline render_image_renderer_texture_quad_packet_summary
make_render_image_renderer_texture_quad_packet_summary_with_resource_consumption(
    const render_image_renderer_texture_quad_packet_summary& summary,
    const render_image_texture_frame_resource_packet_consumption_summary& consumption)
{
    render_image_renderer_texture_quad_packet_summary enriched{
        .frame_label = summary.frame_label,
        .draw_command_count = summary.draw_command_count,
        .non_image_command_count = summary.non_image_command_count,
        .image_command_count = summary.image_command_count,
        .composition_entry_count = summary.composition_entry_count,
        .has_non_image_commands = summary.has_non_image_commands,
        .skipped_command_summary = summary.skipped_command_summary,
    };

    std::map<std::string, bool> unique_quad_packet_identities;
    std::map<std::string, bool> unique_texture_cache_keys;
    std::map<std::string, bool> unique_sampler_keys;
    std::map<std::uint64_t, bool> decoded_payload_hashes;
    for (const render_image_renderer_texture_quad_packet& source_packet : summary.packets) {
        render_image_renderer_texture_quad_packet packet = source_packet;
        packet.packet_index = enriched.packets.size();
        if (const render_image_texture_frame_resource_packet_consumption_entry* consumption_entry =
                render_image_texture_frame_resource_packet_consumption_entry_for_request_index(
                    consumption,
                    packet.texture_request_index);
            consumption_entry != nullptr) {
            apply_render_image_texture_frame_resource_packet_consumption_to_renderer_texture_quad_packet(
                packet,
                *consumption_entry);
        }

        append_unique_render_image_texture_frame_upload_handoff_summary_fragment(
            unique_quad_packet_identities,
            enriched.stable_identity_summary,
            packet.stable_quad_packet_identity);
        append_unique_render_image_texture_frame_upload_handoff_summary_fragment(
            unique_texture_cache_keys,
            enriched.texture_cache_key_summary,
            packet.stable_texture_cache_key);
        append_unique_render_image_texture_frame_upload_handoff_summary_fragment(
            unique_sampler_keys,
            enriched.sampler_summary,
            packet.sampler_key);
        if (packet.decoded_resource_evidence_present && packet.decoded_payload_hash != 0) {
            decoded_payload_hashes.emplace(packet.decoded_payload_hash, true);
        }

        count_render_image_renderer_texture_quad_packet(enriched, packet);
        enriched.packets.push_back(std::move(packet));
    }

    enriched.packet_count = enriched.packets.size();
    enriched.unique_stable_quad_packet_identity_count = unique_quad_packet_identities.size();
    enriched.unique_texture_cache_key_count = unique_texture_cache_keys.size();
    enriched.unique_sampler_key_count = unique_sampler_keys.size();
    enriched.decoded_payload_hash_count = decoded_payload_hashes.size();
    enriched.renderer_quad_packets_ready = enriched.packet_count != 0 && !enriched.has_blockers;
    if (enriched.stable_identity_summary.empty()) {
        enriched.stable_identity_summary = "no renderer texture quad packet stable identities";
    }
    if (enriched.texture_cache_key_summary.empty()) {
        enriched.texture_cache_key_summary = "no renderer texture quad packet cache keys";
    }
    if (enriched.sampler_summary.empty()) {
        enriched.sampler_summary = "no renderer texture quad packet sampler keys";
    }
    if (enriched.blocker_summary.empty()) {
        enriched.blocker_summary = "no renderer texture quad packet blockers";
    }
    enriched.decoded_resource_summary =
        "decoded_resources=" + std::to_string(enriched.decoded_resource_ready_count)
        + "; payload_hashes=" + std::to_string(enriched.decoded_payload_hash_count)
        + "; decoded_bytes=" + std::to_string(enriched.decoded_payload_byte_count)
        + "; staging_bytes=" + std::to_string(enriched.staging_payload_byte_count);
    if (enriched.decoded_resource_blocker_summary.empty()) {
        enriched.decoded_resource_blocker_summary = "no decoded resource blockers";
    }

    enriched.status = enriched.packet_count == 0
        ? render_image_renderer_texture_quad_packet_summary_status::empty
        : (enriched.has_blockers
            ? render_image_renderer_texture_quad_packet_summary_status::blocked
            : render_image_renderer_texture_quad_packet_summary_status::ready);
    enriched.status_name = render_image_renderer_texture_quad_packet_summary_status_name(enriched.status);
    switch (enriched.status) {
    case render_image_renderer_texture_quad_packet_summary_status::empty:
        enriched.diagnostic = "image renderer texture quad packet summary has no image packets";
        break;
    case render_image_renderer_texture_quad_packet_summary_status::ready:
        enriched.diagnostic = "image renderer texture quad packet summary is ready";
        break;
    case render_image_renderer_texture_quad_packet_summary_status::blocked:
        enriched.diagnostic = "image renderer texture quad packet summary has blocked image packets";
        break;
    }
    return enriched;
}

inline void append_render_image_texture_frame_resource_packet_consumption_request_indexes(
    std::map<std::size_t, bool>& request_indexes,
    const render_image_texture_frame_resource_packet_consumption_summary& summary)
{
    for (const render_image_texture_frame_resource_packet_consumption_entry& entry : summary.entries) {
        request_indexes.emplace(entry.request_index, true);
    }
}

inline bool render_image_texture_frame_resource_packet_consumption_entry_equal(
    const render_image_texture_frame_resource_packet_consumption_entry& before,
    const render_image_texture_frame_resource_packet_consumption_entry& after)
{
    return before.request_index == after.request_index
        && before.stable_packet_identity == after.stable_packet_identity
        && before.stable_packet_identity_present == after.stable_packet_identity_present
        && before.duplicate_stable_packet_identity == after.duplicate_stable_packet_identity
        && before.materialization_status == after.materialization_status
        && before.packet_status == after.packet_status
        && before.stable_texture_cache_key == after.stable_texture_cache_key
        && before.texture_id == after.texture_id
        && before.texture_revision == after.texture_revision
        && before.texture_width == after.texture_width
        && before.texture_height == after.texture_height
        && before.sampler_key == after.sampler_key
        && before.upload_request_id == after.upload_request_id
        && before.upload_generation_id == after.upload_generation_id
        && before.uploaded_byte_count == after.uploaded_byte_count
        && before.decoded_payload_hash == after.decoded_payload_hash
        && before.decoded_byte_count == after.decoded_byte_count
        && before.upload_layout_byte_count == after.upload_layout_byte_count
        && before.upload_layout_row_stride_byte_count == after.upload_layout_row_stride_byte_count
        && before.staging_payload_byte_count == after.staging_payload_byte_count
        && before.staging_row_copy_count == after.staging_row_copy_count
        && before.materialized == after.materialized
        && before.placeholder_backed == after.placeholder_backed
        && before.blocked == after.blocked
        && before.removed == after.removed
        && before.renderer_boundary_ready == after.renderer_boundary_ready
        && before.decoded_payload_valid == after.decoded_payload_valid
        && before.upload_payload_layout_ready == after.upload_payload_layout_ready
        && before.staging_payload_ready == after.staging_payload_ready
        && before.decoded_resource_ready == after.decoded_resource_ready
        && before.decoded_resource_blocked == after.decoded_resource_blocked
        && before.blocker_summary == after.blocker_summary
        && before.decoded_resource_summary == after.decoded_resource_summary
        && before.decoded_resource_blocker_summary == after.decoded_resource_blocker_summary;
}

inline render_image_texture_frame_resource_packet_consumption_entry_diff
make_render_image_texture_frame_resource_packet_consumption_entry_diff(
    const render_image_texture_frame_resource_packet_consumption_entry* before,
    const render_image_texture_frame_resource_packet_consumption_entry* after,
    std::size_t request_index)
{
    render_image_texture_frame_resource_packet_consumption_entry_diff diff{
        .request_index = request_index,
        .before_present = before != nullptr,
        .after_present = after != nullptr,
    };

    if (before != nullptr) {
        diff.before_stable_packet_identity = before->stable_packet_identity;
        diff.before_materialization_status = before->materialization_status;
        diff.before_materialization_status_name = before->materialization_status_name;
        diff.before_stable_texture_cache_key = before->stable_texture_cache_key;
        diff.before_texture_id = before->texture_id;
        diff.before_texture_revision = before->texture_revision;
        diff.before_texture_width = before->texture_width;
        diff.before_texture_height = before->texture_height;
        diff.before_sampler_key = before->sampler_key;
        diff.before_upload_request_id = before->upload_request_id;
        diff.before_upload_generation_id = before->upload_generation_id;
        diff.before_uploaded_byte_count = before->uploaded_byte_count;
        diff.before_ready = before->ok();
        diff.before_placeholder_backed = before->placeholder_backed;
        diff.before_blocked = before->blocked;
        diff.before_removed = before->removed;
        diff.before_stable_packet_identity_present = before->stable_packet_identity_present;
        diff.before_duplicate_stable_packet_identity = before->duplicate_stable_packet_identity;
        diff.before_blocker_summary = before->blocker_summary;
    }
    if (after != nullptr) {
        diff.after_stable_packet_identity = after->stable_packet_identity;
        diff.after_materialization_status = after->materialization_status;
        diff.after_materialization_status_name = after->materialization_status_name;
        diff.after_stable_texture_cache_key = after->stable_texture_cache_key;
        diff.after_texture_id = after->texture_id;
        diff.after_texture_revision = after->texture_revision;
        diff.after_texture_width = after->texture_width;
        diff.after_texture_height = after->texture_height;
        diff.after_sampler_key = after->sampler_key;
        diff.after_upload_request_id = after->upload_request_id;
        diff.after_upload_generation_id = after->upload_generation_id;
        diff.after_uploaded_byte_count = after->uploaded_byte_count;
        diff.after_ready = after->ok();
        diff.after_placeholder_backed = after->placeholder_backed;
        diff.after_blocked = after->blocked;
        diff.after_removed = after->removed;
        diff.after_stable_packet_identity_present = after->stable_packet_identity_present;
        diff.after_duplicate_stable_packet_identity = after->duplicate_stable_packet_identity;
        diff.after_blocker_summary = after->blocker_summary;
    }

    diff.uploaded_byte_delta = render_image_texture_upload_result_size_delta(
        diff.before_uploaded_byte_count,
        diff.after_uploaded_byte_count);

    const bool both_present = before != nullptr && after != nullptr;
    diff.stable_packet_identity_changed = both_present
        && diff.before_stable_packet_identity != diff.after_stable_packet_identity;
    diff.stable_texture_cache_key_changed = both_present
        && diff.before_stable_texture_cache_key != diff.after_stable_texture_cache_key;
    diff.texture_handle_changed = both_present
        && (diff.before_texture_id != diff.after_texture_id
            || diff.before_texture_revision != diff.after_texture_revision);
    diff.texture_extent_changed = both_present
        && (diff.before_texture_width != diff.after_texture_width
            || diff.before_texture_height != diff.after_texture_height);
    diff.sampler_key_changed = both_present && diff.before_sampler_key != diff.after_sampler_key;
    diff.upload_request_changed = both_present
        && diff.before_upload_request_id != diff.after_upload_request_id;
    diff.upload_generation_changed = both_present
        && diff.before_upload_generation_id != diff.after_upload_generation_id;
    diff.uploaded_byte_count_changed = both_present
        && diff.before_uploaded_byte_count != diff.after_uploaded_byte_count;
    diff.placeholder_changed = both_present
        && diff.before_placeholder_backed != diff.after_placeholder_backed;
    diff.readiness_changed = both_present && diff.before_ready != diff.after_ready;
    diff.blocker_changed = both_present
        && (diff.before_blocked != diff.after_blocked
            || diff.before_blocker_summary != diff.after_blocker_summary);
    diff.stable_packet_identity_quality_changed = both_present
        && (diff.before_stable_packet_identity_present != diff.after_stable_packet_identity_present
            || diff.before_duplicate_stable_packet_identity != diff.after_duplicate_stable_packet_identity);
    diff.ready_regressed = both_present && diff.before_ready && !diff.after_ready;
    diff.ready_recovered = both_present && !diff.before_ready && diff.after_ready;
    diff.regression = diff.ready_regressed;
    diff.improvement = diff.ready_recovered;

    if (before == nullptr && after != nullptr) {
        diff.status = render_image_texture_frame_resource_packet_consumption_diff_entry_status::added;
    } else if (before != nullptr && after == nullptr) {
        diff.status = render_image_texture_frame_resource_packet_consumption_diff_entry_status::removed;
    } else if (before != nullptr && after != nullptr
        && !render_image_texture_frame_resource_packet_consumption_entry_equal(*before, *after)) {
        diff.status = render_image_texture_frame_resource_packet_consumption_diff_entry_status::changed;
    } else {
        diff.status = render_image_texture_frame_resource_packet_consumption_diff_entry_status::unchanged;
    }
    diff.status_name = render_image_texture_frame_resource_packet_consumption_diff_entry_status_name(diff.status);

    if (diff.status == render_image_texture_frame_resource_packet_consumption_diff_entry_status::unchanged) {
        diff.diagnostic = "image frame resource packet consumption diff entry unchanged";
    } else if (diff.regression) {
        diff.diagnostic = "image frame resource packet consumption diff entry changed with regression";
    } else if (diff.improvement) {
        diff.diagnostic = "image frame resource packet consumption diff entry changed with improvement";
    } else if (diff.stable_packet_identity_changed || diff.texture_handle_changed) {
        diff.diagnostic = "image frame resource packet consumption diff entry changed identity";
    } else {
        diff.diagnostic = "image frame resource packet consumption diff entry changed";
    }
    return diff;
}

inline void count_render_image_texture_frame_resource_packet_consumption_diff_entry(
    render_image_texture_frame_resource_packet_consumption_diff& diff,
    const render_image_texture_frame_resource_packet_consumption_entry_diff& entry)
{
    switch (entry.status) {
    case render_image_texture_frame_resource_packet_consumption_diff_entry_status::unchanged:
        ++diff.unchanged_packet_count;
        break;
    case render_image_texture_frame_resource_packet_consumption_diff_entry_status::added:
        ++diff.added_packet_count;
        break;
    case render_image_texture_frame_resource_packet_consumption_diff_entry_status::removed:
        ++diff.removed_packet_count;
        break;
    case render_image_texture_frame_resource_packet_consumption_diff_entry_status::changed:
        ++diff.changed_packet_count;
        break;
    }

    if (entry.stable_packet_identity_changed) {
        ++diff.stable_packet_identity_changed_count;
    }
    if (entry.stable_texture_cache_key_changed) {
        ++diff.stable_texture_cache_key_changed_count;
    }
    if (entry.texture_handle_changed) {
        ++diff.texture_handle_changed_count;
    }
    if (entry.texture_extent_changed) {
        ++diff.texture_extent_changed_count;
    }
    if (entry.sampler_key_changed) {
        ++diff.sampler_key_changed_count;
    }
    if (entry.upload_request_changed) {
        ++diff.upload_request_changed_count;
    }
    if (entry.upload_generation_changed) {
        ++diff.upload_generation_changed_count;
    }
    if (entry.uploaded_byte_count_changed) {
        ++diff.uploaded_byte_count_changed_count;
    }
    if (entry.placeholder_changed) {
        ++diff.placeholder_changed_count;
    }
    if (entry.readiness_changed) {
        ++diff.readiness_changed_count;
    }
    if (entry.blocker_changed) {
        ++diff.blocker_changed_count;
    }
    if (entry.stable_packet_identity_quality_changed) {
        ++diff.stable_packet_identity_quality_changed_count;
    }
    if (entry.ready_regressed) {
        ++diff.ready_regression_count;
        append_render_image_texture_frame_upload_handoff_summary_fragment(
            diff.regression_summary,
            "request=" + std::to_string(entry.request_index) + ":ready->blocked");
    }
    if (entry.ready_recovered) {
        ++diff.ready_recovery_count;
        append_render_image_texture_frame_upload_handoff_summary_fragment(
            diff.improvement_summary,
            "request=" + std::to_string(entry.request_index) + ":blocked->ready");
    }
    if (entry.changed()) {
        append_render_image_texture_frame_upload_handoff_summary_fragment(
            diff.changed_summary,
            "request=" + std::to_string(entry.request_index) + ":" + entry.status_name);
    }

    diff.has_changes = diff.has_changes || entry.changed();
    diff.has_regression = diff.has_regression || entry.regression;
    diff.has_improvement = diff.has_improvement || entry.improvement;
}

inline render_image_texture_frame_resource_packet_consumption_diff
diff_render_image_texture_frame_resource_packet_consumption_summaries(
    const render_image_texture_frame_resource_packet_consumption_summary& before,
    const render_image_texture_frame_resource_packet_consumption_summary& after)
{
    render_image_texture_frame_resource_packet_consumption_diff diff{
        .before_frame_request_count = before.frame_request_count,
        .after_frame_request_count = after.frame_request_count,
        .frame_request_delta = render_image_texture_upload_result_size_delta(
            before.frame_request_count,
            after.frame_request_count),
        .before_packet_count = before.packet_count,
        .after_packet_count = after.packet_count,
        .packet_delta = render_image_texture_upload_result_size_delta(before.packet_count, after.packet_count),
        .before_materialized_packet_count = before.materialized_packet_count,
        .after_materialized_packet_count = after.materialized_packet_count,
        .materialized_packet_delta = render_image_texture_upload_result_size_delta(
            before.materialized_packet_count,
            after.materialized_packet_count),
        .before_blocked_packet_count = before.blocked_packet_count,
        .after_blocked_packet_count = after.blocked_packet_count,
        .blocked_packet_delta = render_image_texture_upload_result_size_delta(
            before.blocked_packet_count,
            after.blocked_packet_count),
        .before_placeholder_packet_count = before.placeholder_packet_count,
        .after_placeholder_packet_count = after.placeholder_packet_count,
        .placeholder_packet_delta = render_image_texture_upload_result_size_delta(
            before.placeholder_packet_count,
            after.placeholder_packet_count),
        .before_removed_packet_count = before.removed_packet_count,
        .after_removed_packet_count = after.removed_packet_count,
        .removed_packet_delta = render_image_texture_upload_result_size_delta(
            before.removed_packet_count,
            after.removed_packet_count),
        .before_stable_packet_identity_count = before.stable_packet_identity_count,
        .after_stable_packet_identity_count = after.stable_packet_identity_count,
        .stable_packet_identity_delta = render_image_texture_upload_result_size_delta(
            before.stable_packet_identity_count,
            after.stable_packet_identity_count),
        .before_duplicate_stable_packet_identity_count = before.duplicate_stable_packet_identity_count,
        .after_duplicate_stable_packet_identity_count = after.duplicate_stable_packet_identity_count,
        .duplicate_stable_packet_identity_delta = render_image_texture_upload_result_size_delta(
            before.duplicate_stable_packet_identity_count,
            after.duplicate_stable_packet_identity_count),
        .before_missing_stable_packet_identity_count = before.missing_stable_packet_identity_count,
        .after_missing_stable_packet_identity_count = after.missing_stable_packet_identity_count,
        .missing_stable_packet_identity_delta = render_image_texture_upload_result_size_delta(
            before.missing_stable_packet_identity_count,
            after.missing_stable_packet_identity_count),
        .before_renderer_boundary_ready = before.renderer_boundary_ready,
        .after_renderer_boundary_ready = after.renderer_boundary_ready,
    };

    diff.renderer_boundary_regressed = before.renderer_boundary_ready && !after.renderer_boundary_ready;
    diff.renderer_boundary_recovered = !before.renderer_boundary_ready && after.renderer_boundary_ready;
    diff.has_duplicate_or_missing_identity_change =
        before.duplicate_stable_packet_identity_count != after.duplicate_stable_packet_identity_count
        || before.missing_stable_packet_identity_count != after.missing_stable_packet_identity_count;

    std::map<std::size_t, bool> request_indexes;
    append_render_image_texture_frame_resource_packet_consumption_request_indexes(request_indexes, before);
    append_render_image_texture_frame_resource_packet_consumption_request_indexes(request_indexes, after);

    for (const auto& [request_index, _] : request_indexes) {
        render_image_texture_frame_resource_packet_consumption_entry_diff entry =
            make_render_image_texture_frame_resource_packet_consumption_entry_diff(
                render_image_texture_frame_resource_packet_consumption_entry_for_request_index(before, request_index),
                render_image_texture_frame_resource_packet_consumption_entry_for_request_index(after, request_index),
                request_index);
        count_render_image_texture_frame_resource_packet_consumption_diff_entry(diff, entry);
        diff.entries.push_back(std::move(entry));
    }

    diff.has_changes = diff.has_changes
        || before.frame_request_count != after.frame_request_count
        || before.packet_count != after.packet_count
        || before.materialized_packet_count != after.materialized_packet_count
        || before.blocked_packet_count != after.blocked_packet_count
        || before.placeholder_packet_count != after.placeholder_packet_count
        || before.removed_packet_count != after.removed_packet_count
        || before.stable_packet_identity_count != after.stable_packet_identity_count
        || diff.has_duplicate_or_missing_identity_change
        || diff.renderer_boundary_regressed
        || diff.renderer_boundary_recovered;
    diff.has_regression = diff.has_regression || diff.renderer_boundary_regressed;
    diff.has_improvement = diff.has_improvement || diff.renderer_boundary_recovered;
    diff.stable_no_change_frame = !diff.has_changes;

    diff.identity_summary = "identities=" + std::to_string(diff.before_stable_packet_identity_count)
        + "->" + std::to_string(diff.after_stable_packet_identity_count)
        + "; duplicate_identities=" + std::to_string(diff.before_duplicate_stable_packet_identity_count)
        + "->" + std::to_string(diff.after_duplicate_stable_packet_identity_count)
        + "; missing_identities=" + std::to_string(diff.before_missing_stable_packet_identity_count)
        + "->" + std::to_string(diff.after_missing_stable_packet_identity_count);
    if (diff.changed_summary.empty()) {
        diff.changed_summary = "no resource packet consumption changes";
    }
    if (diff.regression_summary.empty()) {
        diff.regression_summary = diff.renderer_boundary_regressed
            ? "renderer boundary readiness regressed"
            : "no resource packet consumption regressions";
    }
    if (diff.improvement_summary.empty()) {
        diff.improvement_summary = diff.renderer_boundary_recovered
            ? "renderer boundary readiness recovered"
            : "no resource packet consumption improvements";
    }
    if (diff.stable_no_change_frame) {
        diff.diagnostic = "image frame resource packet consumption diff is unchanged";
    } else if (diff.has_regression) {
        diff.diagnostic = "image frame resource packet consumption diff has regressions";
    } else if (diff.has_improvement) {
        diff.diagnostic = "image frame resource packet consumption diff has improvements";
    } else {
        diff.diagnostic = "image frame resource packet consumption diff changed";
    }
    return diff;
}

inline render_image_texture_frame_resource_packet_consumption_diff
diff_render_image_texture_frame_resource_packet_consumption(
    const render_image_texture_frame_resource_packet_materialization& before,
    const render_image_texture_frame_resource_packet_materialization& after)
{
    return diff_render_image_texture_frame_resource_packet_consumption_summaries(
        make_render_image_texture_frame_resource_packet_consumption_summary(before),
        make_render_image_texture_frame_resource_packet_consumption_summary(after));
}

} // namespace quiz_vulkan::render
