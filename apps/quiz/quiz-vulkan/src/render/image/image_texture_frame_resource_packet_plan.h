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

} // namespace quiz_vulkan::render
