#pragma once

#include "render/image/image_texture_frame_resource_packet_plan.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
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

} // namespace quiz_vulkan::render
