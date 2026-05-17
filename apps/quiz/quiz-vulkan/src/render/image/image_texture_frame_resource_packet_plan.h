#pragma once

#include "render/image/image_texture_frame_binding_summary.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

enum class render_image_texture_frame_resource_packet_status {
    resource_packet_ready,
    placeholder_backed,
    blocked_missing_upload_result,
    blocked_missing_frame_binding,
    blocked_retry_backoff,
    blocked,
    removed,
};

inline std::string render_image_texture_frame_resource_packet_status_name(
    render_image_texture_frame_resource_packet_status status)
{
    switch (status) {
    case render_image_texture_frame_resource_packet_status::resource_packet_ready:
        return "resource_packet_ready";
    case render_image_texture_frame_resource_packet_status::placeholder_backed:
        return "placeholder_backed";
    case render_image_texture_frame_resource_packet_status::blocked_missing_upload_result:
        return "blocked_missing_upload_result";
    case render_image_texture_frame_resource_packet_status::blocked_missing_frame_binding:
        return "blocked_missing_frame_binding";
    case render_image_texture_frame_resource_packet_status::blocked_retry_backoff:
        return "blocked_retry_backoff";
    case render_image_texture_frame_resource_packet_status::blocked:
        return "blocked";
    case render_image_texture_frame_resource_packet_status::removed:
        return "removed";
    }

    return "unknown";
}

inline bool render_image_texture_frame_resource_packet_status_is_blocked(
    render_image_texture_frame_resource_packet_status status)
{
    return status == render_image_texture_frame_resource_packet_status::blocked_missing_upload_result
        || status == render_image_texture_frame_resource_packet_status::blocked_missing_frame_binding
        || status == render_image_texture_frame_resource_packet_status::blocked_retry_backoff
        || status == render_image_texture_frame_resource_packet_status::blocked;
}

inline bool render_image_texture_frame_resource_packet_status_is_bindable(
    render_image_texture_frame_resource_packet_status status)
{
    return status == render_image_texture_frame_resource_packet_status::resource_packet_ready
        || status == render_image_texture_frame_resource_packet_status::placeholder_backed;
}

inline render_image_texture_frame_resource_packet_status render_image_texture_frame_resource_packet_status_for(
    const render_image_texture_frame_upload_handoff_entry& entry)
{
    if (entry.removed || entry.status == render_image_texture_frame_upload_handoff_entry_status::removed) {
        return render_image_texture_frame_resource_packet_status::removed;
    }
    if (entry.missing_frame_binding
        || entry.status == render_image_texture_frame_upload_handoff_entry_status::missing_frame_binding) {
        return render_image_texture_frame_resource_packet_status::blocked_missing_frame_binding;
    }
    if (entry.missing_upload_result
        || entry.status == render_image_texture_frame_upload_handoff_entry_status::missing_upload_result) {
        return render_image_texture_frame_resource_packet_status::blocked_missing_upload_result;
    }
    if (entry.retryable_blocker) {
        return render_image_texture_frame_resource_packet_status::blocked_retry_backoff;
    }
    if (entry.blocked || entry.status == render_image_texture_frame_upload_handoff_entry_status::blocked) {
        return render_image_texture_frame_resource_packet_status::blocked;
    }
    if (entry.placeholder_texture
        || entry.status == render_image_texture_frame_upload_handoff_entry_status::placeholder) {
        return render_image_texture_frame_resource_packet_status::placeholder_backed;
    }
    if (entry.ready || entry.status == render_image_texture_frame_upload_handoff_entry_status::ready) {
        return render_image_texture_frame_resource_packet_status::resource_packet_ready;
    }
    return render_image_texture_frame_resource_packet_status::blocked;
}

struct render_image_texture_frame_resource_packet_plan_entry {
    std::size_t sequence = 0;
    std::size_t request_index = 0;
    render_image_texture_frame_resource_packet_status status =
        render_image_texture_frame_resource_packet_status::blocked;
    std::string status_name = render_image_texture_frame_resource_packet_status_name(
        render_image_texture_frame_resource_packet_status::blocked);
    render_image_texture_frame_upload_handoff_entry_status handoff_status =
        render_image_texture_frame_upload_handoff_entry_status::missing_upload_result;
    std::string handoff_status_name;
    std::string render_image_uri;
    std::string normalized_uri;
    render_image_cache_key cache_key;
    render_image_source_kind source_kind = render_image_source_kind::unsupported;
    render_image_sampler_policy sampler;
    std::string sampler_key;
    render_image_texture_key texture_key;
    std::string stable_texture_cache_key;
    render_image_texture_id texture_id = 0;
    render_image_revision texture_revision = 0;
    std::size_t texture_width = 0;
    std::size_t texture_height = 0;
    std::uint64_t upload_request_id = 0;
    std::uint64_t upload_generation_id = 0;
    std::size_t mip_level_count = 0;
    std::size_t uploaded_byte_count = 0;
    render_image_decoded_payload_evidence decoded_payload;
    render_image_texture_upload_payload_layout_evidence payload_layout;
    render_image_texture_staging_payload_plan staging_payload_plan;
    bool requested = false;
    bool bindable = false;
    bool resource_packet_ready = false;
    bool placeholder_backed = false;
    bool blocked = true;
    bool missing_upload_result = false;
    bool missing_frame_binding = false;
    bool retry_backoff_blocked = false;
    bool removed = false;
    bool renderer_handoff_ready = false;
    bool cache_reused = false;
    bool expected_cache_reuse = false;
    std::size_t retry_after_queue_sequence_delta = 0;
    std::size_t next_retry_sequence = 0;
    std::string cache_key_summary;
    std::string sampler_summary;
    std::string retry_backoff_summary;
    std::string blocker_summary;
    std::string diagnostic;

    bool ok() const
    {
        return bindable && !blocked;
    }
};

struct render_image_texture_frame_resource_packet_plan {
    render_image_texture_frame_binding_summary_status binding_status =
        render_image_texture_frame_binding_summary_status::empty;
    std::string binding_status_name = render_image_texture_frame_binding_summary_status_name(
        render_image_texture_frame_binding_summary_status::empty);
    std::size_t frame_request_count = 0;
    std::size_t handoff_entry_count = 0;
    std::size_t requested_texture_count = 0;
    std::size_t bindable_packet_count = 0;
    std::size_t resource_packet_count = 0;
    std::size_t placeholder_backed_packet_count = 0;
    std::size_t blocked_packet_count = 0;
    std::size_t missing_upload_result_packet_count = 0;
    std::size_t missing_frame_binding_packet_count = 0;
    std::size_t retry_backoff_blocked_packet_count = 0;
    std::size_t removed_packet_count = 0;
    std::size_t cache_reused_count = 0;
    std::size_t unique_texture_cache_key_count = 0;
    std::size_t unique_sampler_key_count = 0;
    std::size_t uploaded_byte_count = 0;
    std::size_t mip_level_count = 0;
    bool resource_binding_ready = false;
    bool fully_resource_backed = false;
    bool placeholder_backed = false;
    bool has_blockers = false;
    bool has_retry_backoff = false;
    bool renderer_handoff_ready = false;
    bool cache_reused = false;
    std::vector<render_image_texture_frame_resource_packet_plan_entry> entries;
    std::string cache_key_summary;
    std::string sampler_summary;
    std::string retry_backoff_summary;
    std::string diagnostic;

    bool ok() const
    {
        return resource_binding_ready && !has_blockers;
    }
};

enum class render_image_draw_list_texture_frame_composition_status {
    empty,
    ready,
    blocked,
};

inline std::string render_image_draw_list_texture_frame_composition_status_name(
    render_image_draw_list_texture_frame_composition_status status)
{
    switch (status) {
    case render_image_draw_list_texture_frame_composition_status::empty:
        return "empty";
    case render_image_draw_list_texture_frame_composition_status::ready:
        return "ready";
    case render_image_draw_list_texture_frame_composition_status::blocked:
        return "blocked";
    }

    return "unknown";
}

enum class render_image_draw_list_texture_frame_composition_entry_status {
    ready,
    handoff_blocked,
    missing_batch_request,
    batch_blocked,
    missing_frame_entry,
    frame_blocked,
    missing_resource_packet,
    resource_packet_blocked,
};

inline std::string render_image_draw_list_texture_frame_composition_entry_status_name(
    render_image_draw_list_texture_frame_composition_entry_status status)
{
    switch (status) {
    case render_image_draw_list_texture_frame_composition_entry_status::ready:
        return "ready";
    case render_image_draw_list_texture_frame_composition_entry_status::handoff_blocked:
        return "handoff_blocked";
    case render_image_draw_list_texture_frame_composition_entry_status::missing_batch_request:
        return "missing_batch_request";
    case render_image_draw_list_texture_frame_composition_entry_status::batch_blocked:
        return "batch_blocked";
    case render_image_draw_list_texture_frame_composition_entry_status::missing_frame_entry:
        return "missing_frame_entry";
    case render_image_draw_list_texture_frame_composition_entry_status::frame_blocked:
        return "frame_blocked";
    case render_image_draw_list_texture_frame_composition_entry_status::missing_resource_packet:
        return "missing_resource_packet";
    case render_image_draw_list_texture_frame_composition_entry_status::resource_packet_blocked:
        return "resource_packet_blocked";
    }

    return "unknown";
}

struct render_image_draw_list_texture_frame_composition_entry {
    std::string frame_label;
    std::size_t draw_command_index = 0;
    std::size_t image_command_index = 0;
    std::size_t texture_request_index = 0;
    render_node_id node_id;
    render_node_id parent_node_id;
    render_rect bounds;
    render_rect content_bounds;
    render_image_ref image;
    std::string uri;
    std::string alt_text;
    float aspect_ratio = 0.0f;
    render_image_sampler_policy sampler;
    render_image_sampler_policy_diagnostic sampler_policy;
    std::string stable_draw_command_identity;
    std::string stable_texture_cache_key;
    render_image_draw_list_frame_handoff_entry_status handoff_status =
        render_image_draw_list_frame_handoff_entry_status::blocked_empty_uri;
    std::string handoff_status_name = render_image_draw_list_frame_handoff_entry_status_name(
        render_image_draw_list_frame_handoff_entry_status::blocked_empty_uri);
    std::string handoff_blocker_summary;
    render_image_texture_batch_plan_entry_status batch_status =
        render_image_texture_batch_plan_entry_status::resolve_failed;
    std::string batch_status_name = render_image_texture_batch_plan_entry_status_name(
        render_image_texture_batch_plan_entry_status::resolve_failed);
    render_image_texture_pipeline_request texture_request;
    render_image_cache_key normalized_source_key;
    render_image_texture_key texture_key;
    render_image_texture_key_diagnostic texture_key_diagnostic;
    render_image_texture_batch_execution_entry_status frame_execution_status =
        render_image_texture_batch_execution_entry_status::skipped_invalid_request;
    render_image_texture_pipeline_status frame_pipeline_status =
        render_image_texture_pipeline_status::resolve_failed;
    render_image_texture_status frame_texture_status = render_image_texture_status::missing_source;
    render_image_texture_handle_map_entry_status frame_handle_status =
        render_image_texture_handle_map_entry_status::missing_execution;
    render_image_texture_frame_resource_packet_status resource_packet_status =
        render_image_texture_frame_resource_packet_status::blocked;
    std::string resource_packet_status_name = render_image_texture_frame_resource_packet_status_name(
        render_image_texture_frame_resource_packet_status::blocked);
    render_image_texture_id texture_id = 0;
    render_image_revision texture_revision = 0;
    std::size_t texture_width = 0;
    std::size_t texture_height = 0;
    std::uint64_t upload_request_id = 0;
    std::uint64_t upload_generation_id = 0;
    std::size_t uploaded_byte_count = 0;
    bool handoff_blocked = true;
    bool batch_entry_present = false;
    bool batch_request_planned = false;
    bool frame_entry_present = false;
    bool frame_renderer_handoff_ready = false;
    bool resource_packet_present = false;
    bool resource_packet_ready = false;
    bool resource_packet_blocked = false;
    bool entered_texture_batch = false;
    bool ready = false;
    bool blocked = true;
    render_image_draw_list_texture_frame_composition_entry_status status =
        render_image_draw_list_texture_frame_composition_entry_status::handoff_blocked;
    std::string status_name = render_image_draw_list_texture_frame_composition_entry_status_name(
        render_image_draw_list_texture_frame_composition_entry_status::handoff_blocked);
    std::string diagnostic;

    bool ok() const
    {
        return status == render_image_draw_list_texture_frame_composition_entry_status::ready;
    }
};

struct render_image_draw_list_texture_frame_composition {
    render_image_draw_list_texture_frame_composition_status status =
        render_image_draw_list_texture_frame_composition_status::empty;
    std::string status_name = render_image_draw_list_texture_frame_composition_status_name(
        render_image_draw_list_texture_frame_composition_status::empty);
    std::string frame_label;
    std::size_t draw_command_count = 0;
    std::size_t non_image_command_count = 0;
    std::size_t image_command_count = 0;
    std::size_t handoff_entry_count = 0;
    std::size_t texture_batch_request_count = 0;
    std::size_t batch_planned_request_count = 0;
    std::size_t frame_entry_count = 0;
    std::size_t resource_packet_count = 0;
    std::size_t ready_entry_count = 0;
    std::size_t blocked_entry_count = 0;
    std::size_t handoff_blocked_count = 0;
    std::size_t missing_batch_request_count = 0;
    std::size_t batch_blocked_count = 0;
    std::size_t missing_frame_entry_count = 0;
    std::size_t frame_blocked_count = 0;
    std::size_t missing_resource_packet_count = 0;
    std::size_t resource_packet_blocked_count = 0;
    bool has_non_image_commands = false;
    bool has_blockers = false;
    bool renderer_handoff_ready = false;
    bool resource_packet_ready = false;
    std::vector<render_image_draw_list_texture_frame_composition_entry> entries;
    std::string skipped_command_summary;
    std::string diagnostic;

    bool ok() const
    {
        return status == render_image_draw_list_texture_frame_composition_status::ready;
    }
};

inline render_image_texture_frame_resource_packet_plan_entry
make_render_image_texture_frame_resource_packet_plan_entry(
    const render_image_texture_frame_upload_handoff_entry& handoff_entry)
{
    const render_image_texture_frame_resource_packet_status status =
        render_image_texture_frame_resource_packet_status_for(handoff_entry);
    const bool blocked = render_image_texture_frame_resource_packet_status_is_blocked(status);
    const bool bindable = render_image_texture_frame_resource_packet_status_is_bindable(status);

    render_image_texture_frame_resource_packet_plan_entry entry{
        .sequence = handoff_entry.sequence,
        .request_index = handoff_entry.request_index,
        .status = status,
        .status_name = render_image_texture_frame_resource_packet_status_name(status),
        .handoff_status = handoff_entry.status,
        .handoff_status_name = handoff_entry.status_name,
        .render_image_uri = handoff_entry.render_image_uri,
        .normalized_uri = handoff_entry.normalized_uri,
        .cache_key = handoff_entry.cache_key,
        .source_kind = handoff_entry.source_kind,
        .sampler = handoff_entry.sampler,
        .sampler_key = handoff_entry.sampler_key,
        .texture_key = handoff_entry.texture_key,
        .stable_texture_cache_key = handoff_entry.stable_texture_cache_key,
        .texture_id = handoff_entry.texture_id,
        .texture_revision = handoff_entry.texture_revision,
        .texture_width = handoff_entry.texture_width,
        .texture_height = handoff_entry.texture_height,
        .upload_request_id = handoff_entry.upload_request_id,
        .upload_generation_id = handoff_entry.upload_generation_id,
        .mip_level_count = handoff_entry.mip_level_count,
        .uploaded_byte_count = handoff_entry.uploaded_byte_count,
        .decoded_payload = handoff_entry.decoded_payload,
        .payload_layout = handoff_entry.payload_layout,
        .staging_payload_plan = handoff_entry.staging_payload_plan,
        .requested = handoff_entry.requested,
        .bindable = bindable,
        .resource_packet_ready =
            status == render_image_texture_frame_resource_packet_status::resource_packet_ready,
        .placeholder_backed =
            status == render_image_texture_frame_resource_packet_status::placeholder_backed,
        .blocked = blocked,
        .missing_upload_result =
            status == render_image_texture_frame_resource_packet_status::blocked_missing_upload_result,
        .missing_frame_binding =
            status == render_image_texture_frame_resource_packet_status::blocked_missing_frame_binding,
        .retry_backoff_blocked =
            status == render_image_texture_frame_resource_packet_status::blocked_retry_backoff,
        .removed = status == render_image_texture_frame_resource_packet_status::removed,
        .renderer_handoff_ready = handoff_entry.renderer_handoff_ready,
        .cache_reused = handoff_entry.cache_reused,
        .expected_cache_reuse = handoff_entry.expected_cache_reuse,
        .retry_after_queue_sequence_delta = handoff_entry.retry_after_queue_sequence_delta,
        .next_retry_sequence = handoff_entry.next_retry_sequence,
        .cache_key_summary = handoff_entry.stable_texture_cache_key,
        .sampler_summary = handoff_entry.sampler_summary,
        .retry_backoff_summary = handoff_entry.retry_eligibility_name,
        .blocker_summary = handoff_entry.blocker_summary,
    };

    if (entry.sampler_summary.empty()) {
        entry.sampler_summary = handoff_entry.sampler_key;
    }
    if (entry.retry_backoff_blocked && entry.retry_backoff_summary.empty()) {
        entry.retry_backoff_summary = handoff_entry.blocker_summary;
    }

    switch (entry.status) {
    case render_image_texture_frame_resource_packet_status::resource_packet_ready:
        entry.diagnostic = "image frame resource packet is ready";
        break;
    case render_image_texture_frame_resource_packet_status::placeholder_backed:
        entry.diagnostic = "image frame resource packet stays placeholder-backed";
        break;
    case render_image_texture_frame_resource_packet_status::blocked_missing_upload_result:
        entry.diagnostic = "image frame resource packet is blocked by missing upload result";
        break;
    case render_image_texture_frame_resource_packet_status::blocked_missing_frame_binding:
        entry.diagnostic = "image frame resource packet is blocked by missing frame binding";
        break;
    case render_image_texture_frame_resource_packet_status::blocked_retry_backoff:
        entry.diagnostic = "image frame resource packet is blocked by retry backoff";
        break;
    case render_image_texture_frame_resource_packet_status::blocked:
        entry.diagnostic = "image frame resource packet is blocked";
        break;
    case render_image_texture_frame_resource_packet_status::removed:
        entry.diagnostic = "image frame resource packet was removed from frame";
        break;
    }

    return entry;
}

inline void count_render_image_texture_frame_resource_packet_plan_entry(
    render_image_texture_frame_resource_packet_plan& plan,
    const render_image_texture_frame_resource_packet_plan_entry& entry)
{
    switch (entry.status) {
    case render_image_texture_frame_resource_packet_status::resource_packet_ready:
        ++plan.resource_packet_count;
        ++plan.bindable_packet_count;
        break;
    case render_image_texture_frame_resource_packet_status::placeholder_backed:
        ++plan.placeholder_backed_packet_count;
        ++plan.bindable_packet_count;
        break;
    case render_image_texture_frame_resource_packet_status::blocked_missing_upload_result:
        ++plan.missing_upload_result_packet_count;
        ++plan.blocked_packet_count;
        plan.has_blockers = true;
        break;
    case render_image_texture_frame_resource_packet_status::blocked_missing_frame_binding:
        ++plan.missing_frame_binding_packet_count;
        ++plan.blocked_packet_count;
        plan.has_blockers = true;
        break;
    case render_image_texture_frame_resource_packet_status::blocked_retry_backoff:
        ++plan.retry_backoff_blocked_packet_count;
        ++plan.blocked_packet_count;
        plan.has_blockers = true;
        plan.has_retry_backoff = true;
        append_render_image_texture_frame_upload_handoff_summary_fragment(
            plan.retry_backoff_summary,
            entry.blocker_summary);
        break;
    case render_image_texture_frame_resource_packet_status::blocked:
        ++plan.blocked_packet_count;
        plan.has_blockers = true;
        break;
    case render_image_texture_frame_resource_packet_status::removed:
        ++plan.removed_packet_count;
        break;
    }
}

inline void finalize_render_image_texture_frame_resource_packet_plan(
    render_image_texture_frame_resource_packet_plan& plan,
    const render_image_texture_frame_binding_summary& binding_summary)
{
    std::map<std::string, bool> unique_texture_cache_keys;
    std::map<std::string, bool> unique_sampler_keys;
    for (const render_image_texture_frame_resource_packet_plan_entry& entry : plan.entries) {
        if (entry.removed) {
            continue;
        }
        append_unique_render_image_texture_frame_upload_handoff_summary_fragment(
            unique_texture_cache_keys,
            plan.cache_key_summary,
            entry.stable_texture_cache_key);
        append_unique_render_image_texture_frame_upload_handoff_summary_fragment(
            unique_sampler_keys,
            plan.sampler_summary,
            entry.sampler_summary);
    }

    plan.unique_texture_cache_key_count = unique_texture_cache_keys.size();
    plan.unique_sampler_key_count = unique_sampler_keys.size();
    plan.resource_binding_ready = binding_summary.ok() && !plan.has_blockers;
    plan.fully_resource_backed =
        binding_summary.status == render_image_texture_frame_binding_summary_status::fully_upload_backed;
    plan.placeholder_backed = plan.placeholder_backed_packet_count != 0;
    plan.renderer_handoff_ready = binding_summary.renderer_handoff_ready && plan.resource_binding_ready;
    plan.cache_reused = binding_summary.cache_reused;

    if (plan.cache_key_summary.empty()) {
        plan.cache_key_summary = "no frame resource packet cache keys";
    }
    if (plan.sampler_summary.empty()) {
        plan.sampler_summary = "no frame resource packet sampler keys";
    }
    if (plan.retry_backoff_summary.empty()) {
        plan.retry_backoff_summary = "no retry backoff blockers";
    }

    if (plan.handoff_entry_count == 0) {
        plan.diagnostic = "image frame resource packet plan has no handoff entries";
    } else if (plan.missing_upload_result_packet_count != 0) {
        plan.diagnostic = "image frame resource packet plan has missing upload results";
    } else if (plan.missing_frame_binding_packet_count != 0) {
        plan.diagnostic = "image frame resource packet plan has upload results without frame bindings";
    } else if (plan.retry_backoff_blocked_packet_count != 0) {
        plan.diagnostic = "image frame resource packet plan is blocked by retry backoff";
    } else if (plan.blocked_packet_count != 0) {
        plan.diagnostic = "image frame resource packet plan has blocked image refs";
    } else if (plan.placeholder_backed_packet_count != 0) {
        plan.diagnostic = "image frame resource packet plan is placeholder-backed";
    } else {
        plan.diagnostic = "image frame resource packet plan is ready";
    }
}

inline render_image_texture_frame_resource_packet_plan make_render_image_texture_frame_resource_packet_plan(
    const render_image_texture_frame_upload_handoff_summary& handoff)
{
    const render_image_texture_frame_binding_summary binding_summary =
        make_render_image_texture_frame_binding_summary(handoff);
    render_image_texture_frame_resource_packet_plan plan{
        .binding_status = binding_summary.status,
        .binding_status_name = binding_summary.status_name,
        .frame_request_count = binding_summary.frame_request_count,
        .handoff_entry_count = binding_summary.handoff_entry_count,
        .requested_texture_count = binding_summary.requested_texture_count,
        .cache_reused_count = binding_summary.cache_reused_count,
        .uploaded_byte_count = binding_summary.uploaded_byte_count,
        .mip_level_count = binding_summary.mip_level_count,
    };

    for (const render_image_texture_frame_upload_handoff_entry& handoff_entry : handoff.entries) {
        render_image_texture_frame_resource_packet_plan_entry entry =
            make_render_image_texture_frame_resource_packet_plan_entry(handoff_entry);
        count_render_image_texture_frame_resource_packet_plan_entry(plan, entry);
        plan.entries.push_back(std::move(entry));
    }

    finalize_render_image_texture_frame_resource_packet_plan(plan, binding_summary);
    return plan;
}

inline render_image_texture_frame_resource_packet_plan make_render_image_texture_frame_resource_packet_plan(
    const render_image_texture_frame_snapshot& frame,
    const render_image_texture_frame_binding_plan& binding_plan,
    const render_image_texture_upload_result_snapshot& upload_result)
{
    return make_render_image_texture_frame_resource_packet_plan(
        make_render_image_texture_frame_upload_handoff_summary(frame, binding_plan, upload_result));
}

inline render_image_texture_frame_resource_packet_plan make_render_image_texture_frame_resource_packet_plan(
    const render_image_texture_frame_snapshot& frame,
    const render_image_texture_upload_result_snapshot& upload_result)
{
    return make_render_image_texture_frame_resource_packet_plan(
        make_render_image_texture_frame_upload_handoff_summary(frame, upload_result));
}

inline const render_image_texture_frame_entry_snapshot*
render_image_draw_list_texture_frame_composition_frame_entry_for_request_index(
    const render_image_texture_frame_snapshot& frame,
    std::size_t request_index)
{
    for (const render_image_texture_frame_entry_snapshot& entry : frame.entries) {
        if (entry.request_index == request_index) {
            return &entry;
        }
    }
    return nullptr;
}

inline const render_image_texture_frame_resource_packet_plan_entry*
render_image_draw_list_texture_frame_composition_resource_packet_for_request_index(
    const render_image_texture_frame_resource_packet_plan& resource_packets,
    std::size_t request_index)
{
    for (const render_image_texture_frame_resource_packet_plan_entry& entry : resource_packets.entries) {
        if (entry.request_index == request_index) {
            return &entry;
        }
    }
    return nullptr;
}

inline render_image_draw_list_texture_frame_composition_entry_status
render_image_draw_list_texture_frame_composition_entry_status_for(
    const render_image_draw_list_texture_frame_composition_entry& entry)
{
    if (entry.handoff_blocked) {
        return render_image_draw_list_texture_frame_composition_entry_status::handoff_blocked;
    }
    if (!entry.batch_entry_present) {
        return render_image_draw_list_texture_frame_composition_entry_status::missing_batch_request;
    }
    if (!entry.batch_request_planned) {
        return render_image_draw_list_texture_frame_composition_entry_status::batch_blocked;
    }
    if (!entry.frame_entry_present) {
        return render_image_draw_list_texture_frame_composition_entry_status::missing_frame_entry;
    }
    if (!entry.frame_renderer_handoff_ready) {
        return render_image_draw_list_texture_frame_composition_entry_status::frame_blocked;
    }
    if (!entry.resource_packet_present) {
        return render_image_draw_list_texture_frame_composition_entry_status::missing_resource_packet;
    }
    if (!entry.resource_packet_ready) {
        return render_image_draw_list_texture_frame_composition_entry_status::resource_packet_blocked;
    }
    return render_image_draw_list_texture_frame_composition_entry_status::ready;
}

inline void finalize_render_image_draw_list_texture_frame_composition_entry(
    render_image_draw_list_texture_frame_composition_entry& entry)
{
    entry.status = render_image_draw_list_texture_frame_composition_entry_status_for(entry);
    entry.status_name = render_image_draw_list_texture_frame_composition_entry_status_name(entry.status);
    entry.ready = entry.status == render_image_draw_list_texture_frame_composition_entry_status::ready;
    entry.blocked = !entry.ready;

    switch (entry.status) {
    case render_image_draw_list_texture_frame_composition_entry_status::ready:
        entry.diagnostic = "image draw command texture frame composition is ready";
        break;
    case render_image_draw_list_texture_frame_composition_entry_status::handoff_blocked:
        entry.diagnostic = "image draw command texture frame composition blocked by draw-list handoff: "
            + entry.handoff_blocker_summary;
        break;
    case render_image_draw_list_texture_frame_composition_entry_status::missing_batch_request:
        entry.diagnostic = "image draw command texture frame composition is missing batch request";
        break;
    case render_image_draw_list_texture_frame_composition_entry_status::batch_blocked:
        entry.diagnostic = "image draw command texture frame composition blocked by texture batch plan";
        break;
    case render_image_draw_list_texture_frame_composition_entry_status::missing_frame_entry:
        entry.diagnostic = "image draw command texture frame composition is missing frame snapshot entry";
        break;
    case render_image_draw_list_texture_frame_composition_entry_status::frame_blocked:
        entry.diagnostic = "image draw command texture frame composition blocked by frame snapshot entry";
        break;
    case render_image_draw_list_texture_frame_composition_entry_status::missing_resource_packet:
        entry.diagnostic = "image draw command texture frame composition is missing resource packet";
        break;
    case render_image_draw_list_texture_frame_composition_entry_status::resource_packet_blocked:
        entry.diagnostic = "image draw command texture frame composition blocked by resource packet";
        break;
    }
}

inline void count_render_image_draw_list_texture_frame_composition_entry(
    render_image_draw_list_texture_frame_composition& composition,
    const render_image_draw_list_texture_frame_composition_entry& entry)
{
    if (entry.ready) {
        ++composition.ready_entry_count;
    } else {
        ++composition.blocked_entry_count;
        composition.has_blockers = true;
    }

    switch (entry.status) {
    case render_image_draw_list_texture_frame_composition_entry_status::ready:
        break;
    case render_image_draw_list_texture_frame_composition_entry_status::handoff_blocked:
        ++composition.handoff_blocked_count;
        break;
    case render_image_draw_list_texture_frame_composition_entry_status::missing_batch_request:
        ++composition.missing_batch_request_count;
        break;
    case render_image_draw_list_texture_frame_composition_entry_status::batch_blocked:
        ++composition.batch_blocked_count;
        break;
    case render_image_draw_list_texture_frame_composition_entry_status::missing_frame_entry:
        ++composition.missing_frame_entry_count;
        break;
    case render_image_draw_list_texture_frame_composition_entry_status::frame_blocked:
        ++composition.frame_blocked_count;
        break;
    case render_image_draw_list_texture_frame_composition_entry_status::missing_resource_packet:
        ++composition.missing_resource_packet_count;
        break;
    case render_image_draw_list_texture_frame_composition_entry_status::resource_packet_blocked:
        ++composition.resource_packet_blocked_count;
        break;
    }
}

inline render_image_draw_list_texture_frame_composition_entry
make_render_image_draw_list_texture_frame_composition_entry(
    const render_image_draw_list_frame_handoff_entry& handoff_entry,
    const render_image_texture_batch_plan& batch_plan,
    const render_image_texture_frame_snapshot& frame,
    const render_image_texture_frame_resource_packet_plan& resource_packets)
{
    render_image_draw_list_texture_frame_composition_entry entry{
        .frame_label = handoff_entry.frame_label,
        .draw_command_index = handoff_entry.draw_command_index,
        .image_command_index = handoff_entry.image_command_index,
        .texture_request_index = handoff_entry.texture_request_index,
        .node_id = handoff_entry.node_id,
        .parent_node_id = handoff_entry.parent_node_id,
        .bounds = handoff_entry.bounds,
        .content_bounds = handoff_entry.content_bounds,
        .image = handoff_entry.image,
        .uri = handoff_entry.uri,
        .alt_text = handoff_entry.alt_text,
        .aspect_ratio = handoff_entry.aspect_ratio,
        .sampler = handoff_entry.sampler,
        .sampler_policy = handoff_entry.sampler_policy,
        .stable_draw_command_identity = handoff_entry.stable_draw_command_identity,
        .stable_texture_cache_key = handoff_entry.stable_texture_cache_key,
        .handoff_status = handoff_entry.status,
        .handoff_status_name = handoff_entry.status_name,
        .handoff_blocker_summary = handoff_entry.blocker_summary,
        .texture_request = handoff_entry.pipeline_request,
        .normalized_source_key = handoff_entry.normalized_source_key,
        .texture_key = handoff_entry.texture_key,
        .texture_key_diagnostic = handoff_entry.texture_key_diagnostic,
        .handoff_blocked = handoff_entry.blocked,
        .entered_texture_batch = false,
    };

    if (!handoff_entry.ok()) {
        finalize_render_image_draw_list_texture_frame_composition_entry(entry);
        return entry;
    }

    const render_image_texture_batch_plan_entry* batch_entry =
        render_image_texture_batch_plan_entry_for_request_index(batch_plan, handoff_entry.texture_request_index);
    if (batch_entry != nullptr) {
        entry.batch_entry_present = true;
        entry.batch_status = batch_entry->status;
        entry.batch_status_name = render_image_texture_batch_plan_entry_status_name(batch_entry->status);
        entry.batch_request_planned = batch_entry->ok();
        entry.entered_texture_batch = batch_entry->ok();
        entry.texture_request = batch_entry->pipeline_request;
        entry.normalized_source_key = batch_entry->normalized_source_key;
        entry.texture_key = batch_entry->texture_key;
        entry.texture_key_diagnostic = batch_entry->texture_key_diagnostic;
        entry.stable_texture_cache_key = batch_entry->stable_texture_cache_key;
    }

    if (entry.batch_entry_present && entry.batch_request_planned) {
        const render_image_texture_frame_entry_snapshot* frame_entry =
            render_image_draw_list_texture_frame_composition_frame_entry_for_request_index(
                frame,
                handoff_entry.texture_request_index);
        if (frame_entry != nullptr) {
            entry.frame_entry_present = true;
            entry.frame_execution_status = frame_entry->execution_status;
            entry.frame_pipeline_status = frame_entry->pipeline_status;
            entry.frame_texture_status = frame_entry->texture_status;
            entry.frame_handle_status = frame_entry->handle_status;
            entry.frame_renderer_handoff_ready = frame_entry->renderer_handoff_ready;
            entry.texture_id = frame_entry->texture_id;
            entry.texture_revision = frame_entry->texture_revision;
            entry.texture_width = frame_entry->texture_width;
            entry.texture_height = frame_entry->texture_height;
        }
    }

    if (entry.frame_entry_present && entry.frame_renderer_handoff_ready) {
        const render_image_texture_frame_resource_packet_plan_entry* resource_packet =
            render_image_draw_list_texture_frame_composition_resource_packet_for_request_index(
                resource_packets,
                handoff_entry.texture_request_index);
        if (resource_packet != nullptr) {
            entry.resource_packet_present = true;
            entry.resource_packet_status = resource_packet->status;
            entry.resource_packet_status_name = resource_packet->status_name;
            entry.resource_packet_ready = resource_packet->ok();
            entry.resource_packet_blocked = resource_packet->blocked;
            entry.texture_id = resource_packet->texture_id;
            entry.texture_revision = resource_packet->texture_revision;
            entry.texture_width = resource_packet->texture_width;
            entry.texture_height = resource_packet->texture_height;
            entry.upload_request_id = resource_packet->upload_request_id;
            entry.upload_generation_id = resource_packet->upload_generation_id;
            entry.uploaded_byte_count = resource_packet->uploaded_byte_count;
        }
    }

    finalize_render_image_draw_list_texture_frame_composition_entry(entry);
    return entry;
}

inline render_image_draw_list_texture_frame_composition
make_render_image_draw_list_texture_frame_composition(
    const render_image_draw_list_frame_handoff_snapshot& handoff,
    const render_image_texture_batch_plan& batch_plan,
    const render_image_texture_frame_snapshot& frame,
    const render_image_texture_frame_resource_packet_plan& resource_packets)
{
    render_image_draw_list_texture_frame_composition composition{
        .frame_label = handoff.frame_label,
        .draw_command_count = handoff.draw_command_count,
        .non_image_command_count = handoff.non_image_command_count,
        .image_command_count = handoff.image_command_count,
        .handoff_entry_count = handoff.entries.size(),
        .texture_batch_request_count = batch_plan.request_count,
        .batch_planned_request_count = batch_plan.planned_request_count,
        .frame_entry_count = frame.entries.size(),
        .resource_packet_count = resource_packets.entries.size(),
        .has_non_image_commands = handoff.has_non_image_commands,
        .renderer_handoff_ready = frame.renderer_handoff_ready,
        .resource_packet_ready = resource_packets.renderer_handoff_ready,
        .skipped_command_summary = handoff.skipped_command_summary,
    };

    for (const render_image_draw_list_frame_handoff_entry& handoff_entry : handoff.entries) {
        render_image_draw_list_texture_frame_composition_entry entry =
            make_render_image_draw_list_texture_frame_composition_entry(
                handoff_entry,
                batch_plan,
                frame,
                resource_packets);
        count_render_image_draw_list_texture_frame_composition_entry(composition, entry);
        composition.entries.push_back(std::move(entry));
    }

    composition.status = composition.handoff_entry_count == 0
        ? render_image_draw_list_texture_frame_composition_status::empty
        : (composition.has_blockers
            ? render_image_draw_list_texture_frame_composition_status::blocked
            : render_image_draw_list_texture_frame_composition_status::ready);
    composition.status_name = render_image_draw_list_texture_frame_composition_status_name(composition.status);

    switch (composition.status) {
    case render_image_draw_list_texture_frame_composition_status::empty:
        composition.diagnostic = "image draw-list texture frame composition has no image commands";
        break;
    case render_image_draw_list_texture_frame_composition_status::ready:
        composition.diagnostic = "image draw-list texture frame composition ready";
        break;
    case render_image_draw_list_texture_frame_composition_status::blocked:
        composition.diagnostic = "image draw-list texture frame composition has blocked image commands";
        break;
    }

    return composition;
}

} // namespace quiz_vulkan::render
