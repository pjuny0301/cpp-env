#pragma once

#include "render/image/image_renderer_texture_quad_draw_payload.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

enum class render_image_renderer_resource_packet_descriptor_status {
    ready,
    placeholder_backed,
    blocked_source_payload,
    blocked_missing_payload_identity,
    blocked_duplicate_packet_identity,
};

inline std::string render_image_renderer_resource_packet_descriptor_status_name(
    render_image_renderer_resource_packet_descriptor_status status)
{
    switch (status) {
    case render_image_renderer_resource_packet_descriptor_status::ready:
        return "ready";
    case render_image_renderer_resource_packet_descriptor_status::placeholder_backed:
        return "placeholder_backed";
    case render_image_renderer_resource_packet_descriptor_status::blocked_source_payload:
        return "blocked_source_payload";
    case render_image_renderer_resource_packet_descriptor_status::blocked_missing_payload_identity:
        return "blocked_missing_payload_identity";
    case render_image_renderer_resource_packet_descriptor_status::blocked_duplicate_packet_identity:
        return "blocked_duplicate_packet_identity";
    }

    return "unknown";
}

enum class render_image_renderer_resource_packet_bridge_status {
    empty,
    ready,
    placeholder_backed,
    blocked,
};

inline std::string render_image_renderer_resource_packet_bridge_status_name(
    render_image_renderer_resource_packet_bridge_status status)
{
    switch (status) {
    case render_image_renderer_resource_packet_bridge_status::empty:
        return "empty";
    case render_image_renderer_resource_packet_bridge_status::ready:
        return "ready";
    case render_image_renderer_resource_packet_bridge_status::placeholder_backed:
        return "placeholder_backed";
    case render_image_renderer_resource_packet_bridge_status::blocked:
        return "blocked";
    }

    return "unknown";
}

struct render_image_renderer_resource_packet_descriptor {
    std::size_t descriptor_index = 0;
    std::size_t source_payload_index = 0;
    std::size_t source_packet_index = 0;
    std::string frame_label;
    std::size_t draw_command_index = 0;
    std::size_t image_command_index = 0;
    std::size_t texture_request_index = 0;
    render_node_id node_id;
    render_node_id parent_node_id;
    render_rect bounds;
    render_rect content_bounds;
    std::string uri;
    std::string alt_text;
    float aspect_ratio = 0.0f;
    std::string stable_draw_command_identity;
    std::string stable_quad_packet_identity;
    render_image_texture_key texture_key;
    render_image_texture_key placeholder_key;
    std::string stable_payload_identity;
    std::string stable_resource_packet_identity;
    std::string stable_texture_cache_key;
    std::string upload_cache_stable_cache_key;
    std::string upload_cache_requested_stable_cache_key;
    render_image_sampler_policy sampler;
    std::string sampler_key;
    render_image_texture_id texture_id = 0;
    render_image_revision texture_revision = 0;
    std::size_t texture_width = 0;
    std::size_t texture_height = 0;
    std::uint64_t upload_request_id = 0;
    std::uint64_t upload_generation_id = 0;
    std::size_t uploaded_byte_count = 0;
    std::uint64_t decoded_payload_hash = 0;
    std::size_t decoded_byte_count = 0;
    std::size_t upload_layout_byte_count = 0;
    std::size_t upload_layout_row_stride_byte_count = 0;
    std::size_t staging_payload_byte_count = 0;
    std::size_t staging_row_copy_count = 0;
    std::size_t upload_cache_decoded_byte_count = 0;
    std::size_t upload_cache_staging_byte_count = 0;
    std::size_t upload_cache_uploaded_byte_count = 0;
    std::uint64_t upload_cache_decoded_payload_hash = 0;
    fake_image_texture_placeholder_reason upload_cache_placeholder_reason =
        fake_image_texture_placeholder_reason::none;
    std::string upload_cache_placeholder_reason_name = fake_image_texture_placeholder_reason_name(
        fake_image_texture_placeholder_reason::none);
    render_image_renderer_texture_quad_packet_status source_packet_status =
        render_image_renderer_texture_quad_packet_status::blocked_handoff;
    std::string source_packet_status_name;
    render_image_texture_frame_resource_packet_status resource_packet_status =
        render_image_texture_frame_resource_packet_status::blocked;
    std::string resource_packet_status_name;
    render_image_renderer_texture_quad_draw_payload_status source_payload_status =
        render_image_renderer_texture_quad_draw_payload_status::blocked;
    std::string source_payload_status_name;
    render_image_texture_pipeline_upload_cache_payload_status upload_cache_payload_status =
        render_image_texture_pipeline_upload_cache_payload_status::blocked;
    std::string upload_cache_payload_status_name = render_image_texture_pipeline_upload_cache_payload_status_name(
        render_image_texture_pipeline_upload_cache_payload_status::blocked);
    render_image_renderer_resource_packet_descriptor_status status =
        render_image_renderer_resource_packet_descriptor_status::blocked_source_payload;
    std::string status_name = render_image_renderer_resource_packet_descriptor_status_name(
        render_image_renderer_resource_packet_descriptor_status::blocked_source_payload);
    bool renderer_ready = false;
    bool real_texture = false;
    bool placeholder_backed = false;
    bool fallback_placeholder = false;
    bool blocked = true;
    bool missing_payload_identity = false;
    bool duplicate_packet_identity = false;
    bool decoded_resource_evidence_present = false;
    bool decoded_payload_valid = false;
    bool upload_payload_layout_ready = false;
    bool staging_payload_ready = false;
    bool decoded_resource_ready = false;
    bool decoded_resource_blocked = false;
    bool upload_cache_payload_evidence_present = false;
    bool upload_cache_payload_identity_matched = false;
    bool upload_cache_payload_ready = false;
    bool upload_cache_placeholder_backed = false;
    bool upload_cache_payload_blocked = false;
    std::string blocker_summary;
    std::string decoded_resource_summary;
    std::string decoded_resource_blocker_summary;
    std::string upload_cache_payload_summary;
    std::string upload_cache_payload_blocker_summary;
    std::string diagnostic;

    bool ok() const
    {
        return renderer_ready && !blocked;
    }
};

struct render_image_renderer_resource_packet_bridge {
    render_image_renderer_resource_packet_bridge_status status =
        render_image_renderer_resource_packet_bridge_status::empty;
    std::string status_name = render_image_renderer_resource_packet_bridge_status_name(
        render_image_renderer_resource_packet_bridge_status::empty);
    std::string frame_label;
    std::size_t source_packet_count = 0;
    std::size_t source_payload_count = 0;
    std::size_t packet_count = 0;
    std::size_t ready_packet_count = 0;
    std::size_t placeholder_packet_count = 0;
    std::size_t fallback_placeholder_packet_count = 0;
    std::size_t blocked_packet_count = 0;
    std::size_t missing_payload_identity_count = 0;
    std::size_t duplicate_packet_identity_count = 0;
    std::size_t decoded_resource_evidence_packet_count = 0;
    std::size_t decoded_resource_ready_packet_count = 0;
    std::size_t decoded_resource_blocked_packet_count = 0;
    std::size_t upload_cache_payload_evidence_packet_count = 0;
    std::size_t upload_cache_payload_ready_packet_count = 0;
    std::size_t upload_cache_placeholder_packet_count = 0;
    std::size_t upload_cache_payload_blocked_packet_count = 0;
    std::size_t decoded_payload_hash_count = 0;
    std::size_t upload_cache_payload_hash_count = 0;
    std::size_t decoded_byte_count = 0;
    std::size_t staging_payload_byte_count = 0;
    std::size_t uploaded_byte_count = 0;
    std::size_t upload_cache_decoded_byte_count = 0;
    std::size_t upload_cache_staging_byte_count = 0;
    std::size_t upload_cache_uploaded_byte_count = 0;
    bool renderer_resource_packets_ready = false;
    bool has_placeholders = false;
    bool has_fallback_placeholders = false;
    bool has_blockers = false;
    bool has_duplicate_packet_identities = false;
    std::vector<render_image_renderer_resource_packet_descriptor> packets;
    std::string resource_packet_identity_summary;
    std::string texture_cache_key_summary;
    std::string sampler_summary;
    std::string blocker_summary;
    std::string decoded_resource_summary;
    std::string decoded_resource_blocker_summary;
    std::string upload_cache_payload_summary;
    std::string upload_cache_payload_blocker_summary;
    std::string diagnostic;

    bool ok() const
    {
        return renderer_resource_packets_ready && !has_blockers && packet_count != 0;
    }
};

inline std::string render_image_renderer_resource_packet_identity_for(
    const render_image_renderer_texture_quad_draw_payload& payload)
{
    if (payload.stable_payload_identity.empty()) {
        return {};
    }

    std::string texture_identity = payload.texture_key.source_key;
    if (texture_identity.empty()) {
        texture_identity = payload.stable_texture_cache_key;
    }
    if (texture_identity.empty()) {
        texture_identity = payload.upload_cache_stable_cache_key;
    }
    if (texture_identity.empty()) {
        texture_identity = make_render_image_texture_key_diagnostic(
            payload.texture_key).stable_cache_key;
    }
    if (texture_identity.empty()) {
        texture_identity = payload.uri;
    }

    return payload.stable_payload_identity
        + "|resource_packet=" + texture_identity
        + "|sampler=" + payload.sampler_key;
}

inline render_image_renderer_resource_packet_descriptor_status
render_image_renderer_resource_packet_descriptor_status_for(
    const render_image_renderer_texture_quad_draw_payload& payload,
    const std::string& stable_resource_packet_identity,
    bool duplicate_packet_identity)
{
    if (stable_resource_packet_identity.empty()) {
        return render_image_renderer_resource_packet_descriptor_status::
            blocked_missing_payload_identity;
    }
    if (duplicate_packet_identity) {
        return render_image_renderer_resource_packet_descriptor_status::
            blocked_duplicate_packet_identity;
    }
    if (payload.draw_ready) {
        return render_image_renderer_resource_packet_descriptor_status::ready;
    }
    if (payload.placeholder_backed) {
        return render_image_renderer_resource_packet_descriptor_status::placeholder_backed;
    }
    return render_image_renderer_resource_packet_descriptor_status::blocked_source_payload;
}

inline render_image_renderer_resource_packet_descriptor
make_render_image_renderer_resource_packet_descriptor(
    const render_image_renderer_texture_quad_draw_payload& payload,
    std::size_t descriptor_index,
    bool duplicate_packet_identity)
{
    const std::string stable_resource_packet_identity =
        render_image_renderer_resource_packet_identity_for(payload);
    const render_image_renderer_resource_packet_descriptor_status status =
        render_image_renderer_resource_packet_descriptor_status_for(
            payload,
            stable_resource_packet_identity,
            duplicate_packet_identity);
    const bool blocked =
        status == render_image_renderer_resource_packet_descriptor_status::
                blocked_source_payload
        || status
            == render_image_renderer_resource_packet_descriptor_status::
                blocked_missing_payload_identity
        || status
            == render_image_renderer_resource_packet_descriptor_status::
                blocked_duplicate_packet_identity;

    render_image_renderer_resource_packet_descriptor descriptor{
        .descriptor_index = descriptor_index,
        .source_payload_index = payload.payload_index,
        .source_packet_index = payload.source_packet_index,
        .frame_label = payload.frame_label,
        .draw_command_index = payload.draw_command_index,
        .image_command_index = payload.image_command_index,
        .texture_request_index = payload.texture_request_index,
        .node_id = payload.node_id,
        .parent_node_id = payload.parent_node_id,
        .bounds = payload.bounds,
        .content_bounds = payload.content_bounds,
        .uri = payload.uri,
        .alt_text = payload.alt_text,
        .aspect_ratio = payload.aspect_ratio,
        .stable_draw_command_identity = payload.stable_draw_command_identity,
        .stable_quad_packet_identity = payload.stable_quad_packet_identity,
        .texture_key = payload.texture_key,
        .placeholder_key = payload.placeholder_key,
        .stable_payload_identity = payload.stable_payload_identity,
        .stable_resource_packet_identity = stable_resource_packet_identity,
        .stable_texture_cache_key = payload.stable_texture_cache_key,
        .upload_cache_stable_cache_key = payload.upload_cache_stable_cache_key,
        .upload_cache_requested_stable_cache_key =
            payload.upload_cache_requested_stable_cache_key,
        .sampler = payload.texture_key.sampler,
        .sampler_key = payload.sampler_key,
        .texture_id = payload.texture_id,
        .texture_revision = payload.texture_revision,
        .texture_width = payload.texture_width,
        .texture_height = payload.texture_height,
        .upload_request_id = payload.upload_request_id,
        .upload_generation_id = payload.upload_generation_id,
        .uploaded_byte_count = payload.uploaded_byte_count,
        .decoded_payload_hash = payload.decoded_payload_hash,
        .decoded_byte_count = payload.decoded_byte_count,
        .upload_layout_byte_count = payload.upload_layout_byte_count,
        .upload_layout_row_stride_byte_count =
            payload.upload_layout_row_stride_byte_count,
        .staging_payload_byte_count = payload.staging_payload_byte_count,
        .staging_row_copy_count = payload.staging_row_copy_count,
        .upload_cache_decoded_byte_count =
            payload.upload_cache_decoded_byte_count,
        .upload_cache_staging_byte_count =
            payload.upload_cache_staging_byte_count,
        .upload_cache_uploaded_byte_count =
            payload.upload_cache_uploaded_byte_count,
        .upload_cache_decoded_payload_hash =
            payload.upload_cache_decoded_payload_hash,
        .upload_cache_placeholder_reason =
            payload.upload_cache_placeholder_reason,
        .upload_cache_placeholder_reason_name =
            payload.upload_cache_placeholder_reason_name,
        .source_packet_status = payload.source_packet_status,
        .source_packet_status_name = payload.source_packet_status_name,
        .resource_packet_status = payload.resource_packet_status,
        .resource_packet_status_name = payload.resource_packet_status_name,
        .source_payload_status = payload.status,
        .source_payload_status_name = payload.status_name,
        .upload_cache_payload_status = payload.upload_cache_payload_status,
        .upload_cache_payload_status_name =
            payload.upload_cache_payload_status_name,
        .status = status,
        .status_name = render_image_renderer_resource_packet_descriptor_status_name(status),
        .renderer_ready = !blocked,
        .real_texture =
            status == render_image_renderer_resource_packet_descriptor_status::ready,
        .placeholder_backed =
            status == render_image_renderer_resource_packet_descriptor_status::
                placeholder_backed,
        .fallback_placeholder = payload.fallback_placeholder,
        .blocked = blocked,
        .missing_payload_identity =
            status
            == render_image_renderer_resource_packet_descriptor_status::
                blocked_missing_payload_identity,
        .duplicate_packet_identity =
            status
            == render_image_renderer_resource_packet_descriptor_status::
                blocked_duplicate_packet_identity,
        .decoded_resource_evidence_present =
            payload.decoded_resource_evidence_present,
        .decoded_payload_valid = payload.decoded_payload_valid,
        .upload_payload_layout_ready = payload.upload_payload_layout_ready,
        .staging_payload_ready = payload.staging_payload_ready,
        .decoded_resource_ready = payload.decoded_resource_ready,
        .decoded_resource_blocked = payload.decoded_resource_blocked,
        .upload_cache_payload_evidence_present =
            payload.upload_cache_payload_evidence_present,
        .upload_cache_payload_identity_matched =
            payload.upload_cache_payload_identity_matched,
        .upload_cache_payload_ready = payload.upload_cache_payload_ready,
        .upload_cache_placeholder_backed =
            payload.upload_cache_placeholder_backed,
        .upload_cache_payload_blocked = payload.upload_cache_payload_blocked,
        .blocker_summary = payload.blocker_summary,
        .decoded_resource_summary = payload.decoded_resource_summary,
        .decoded_resource_blocker_summary =
            payload.decoded_resource_blocker_summary,
        .upload_cache_payload_summary = payload.upload_cache_payload_summary,
        .upload_cache_payload_blocker_summary =
            payload.upload_cache_payload_blocker_summary,
    };

    switch (status) {
    case render_image_renderer_resource_packet_descriptor_status::ready:
        descriptor.blocker_summary.clear();
        descriptor.diagnostic = "image renderer resource packet descriptor is ready";
        break;
    case render_image_renderer_resource_packet_descriptor_status::placeholder_backed:
        descriptor.blocker_summary.clear();
        descriptor.diagnostic =
            "image renderer resource packet descriptor is placeholder-backed";
        break;
    case render_image_renderer_resource_packet_descriptor_status::blocked_source_payload:
        if (descriptor.blocker_summary.empty()) {
            descriptor.blocker_summary = payload.diagnostic;
        }
        descriptor.diagnostic =
            "image renderer resource packet descriptor is blocked by draw payload";
        break;
    case render_image_renderer_resource_packet_descriptor_status::blocked_missing_payload_identity:
        descriptor.blocker_summary =
            "renderer resource packet descriptor is missing payload identity";
        descriptor.diagnostic =
            "image renderer resource packet descriptor is blocked by missing payload identity";
        break;
    case render_image_renderer_resource_packet_descriptor_status::blocked_duplicate_packet_identity:
        descriptor.blocker_summary =
            "renderer resource packet descriptor has duplicate stable identity";
        descriptor.diagnostic =
            "image renderer resource packet descriptor is blocked by duplicate identity";
        break;
    }

    return descriptor;
}

inline void count_render_image_renderer_resource_packet_descriptor(
    render_image_renderer_resource_packet_bridge& bridge,
    const render_image_renderer_resource_packet_descriptor& descriptor)
{
    switch (descriptor.status) {
    case render_image_renderer_resource_packet_descriptor_status::ready:
        ++bridge.ready_packet_count;
        break;
    case render_image_renderer_resource_packet_descriptor_status::placeholder_backed:
        ++bridge.placeholder_packet_count;
        bridge.has_placeholders = true;
        if (descriptor.fallback_placeholder) {
            ++bridge.fallback_placeholder_packet_count;
            bridge.has_fallback_placeholders = true;
        }
        break;
    case render_image_renderer_resource_packet_descriptor_status::blocked_source_payload:
        ++bridge.blocked_packet_count;
        bridge.has_blockers = true;
        append_render_image_texture_frame_upload_handoff_summary_fragment(
            bridge.blocker_summary,
            descriptor.blocker_summary);
        break;
    case render_image_renderer_resource_packet_descriptor_status::blocked_missing_payload_identity:
        ++bridge.blocked_packet_count;
        ++bridge.missing_payload_identity_count;
        bridge.has_blockers = true;
        append_render_image_texture_frame_upload_handoff_summary_fragment(
            bridge.blocker_summary,
            descriptor.blocker_summary);
        break;
    case render_image_renderer_resource_packet_descriptor_status::blocked_duplicate_packet_identity:
        ++bridge.blocked_packet_count;
        ++bridge.duplicate_packet_identity_count;
        bridge.has_blockers = true;
        bridge.has_duplicate_packet_identities = true;
        append_render_image_texture_frame_upload_handoff_summary_fragment(
            bridge.blocker_summary,
            descriptor.blocker_summary);
        break;
    }

    bridge.uploaded_byte_count += descriptor.uploaded_byte_count;
    if (descriptor.decoded_resource_evidence_present) {
        ++bridge.decoded_resource_evidence_packet_count;
        if (descriptor.decoded_resource_ready) {
            ++bridge.decoded_resource_ready_packet_count;
            bridge.decoded_byte_count += descriptor.decoded_byte_count;
            bridge.staging_payload_byte_count += descriptor.staging_payload_byte_count;
        } else {
            ++bridge.decoded_resource_blocked_packet_count;
            append_render_image_texture_frame_upload_handoff_summary_fragment(
                bridge.decoded_resource_blocker_summary,
                descriptor.decoded_resource_blocker_summary);
        }
    }
    if (descriptor.upload_cache_payload_evidence_present) {
        ++bridge.upload_cache_payload_evidence_packet_count;
        if (descriptor.upload_cache_payload_ready) {
            ++bridge.upload_cache_payload_ready_packet_count;
            bridge.upload_cache_decoded_byte_count +=
                descriptor.upload_cache_decoded_byte_count;
            bridge.upload_cache_staging_byte_count +=
                descriptor.upload_cache_staging_byte_count;
            bridge.upload_cache_uploaded_byte_count +=
                descriptor.upload_cache_uploaded_byte_count;
        }
        if (descriptor.upload_cache_placeholder_backed) {
            ++bridge.upload_cache_placeholder_packet_count;
        }
        if (descriptor.upload_cache_payload_blocked) {
            ++bridge.upload_cache_payload_blocked_packet_count;
            append_render_image_texture_frame_upload_handoff_summary_fragment(
                bridge.upload_cache_payload_blocker_summary,
                descriptor.upload_cache_payload_blocker_summary);
        }
    }
}

inline void finalize_render_image_renderer_resource_packet_bridge(
    render_image_renderer_resource_packet_bridge& bridge)
{
    bridge.packet_count = bridge.packets.size();
    bridge.renderer_resource_packets_ready =
        bridge.packet_count != 0 && !bridge.has_blockers;
    if (bridge.resource_packet_identity_summary.empty()) {
        bridge.resource_packet_identity_summary =
            "no image renderer resource packet identities";
    }
    if (bridge.texture_cache_key_summary.empty()) {
        bridge.texture_cache_key_summary =
            "no image renderer resource packet texture cache keys";
    }
    if (bridge.sampler_summary.empty()) {
        bridge.sampler_summary = "no image renderer resource packet samplers";
    }
    if (bridge.blocker_summary.empty()) {
        bridge.blocker_summary = "no image renderer resource packet blockers";
    }
    if (bridge.decoded_resource_blocker_summary.empty()) {
        bridge.decoded_resource_blocker_summary =
            "no image renderer resource packet decoded resource blockers";
    }
    if (bridge.upload_cache_payload_blocker_summary.empty()) {
        bridge.upload_cache_payload_blocker_summary =
            "no image renderer resource packet upload cache blockers";
    }
    bridge.decoded_resource_summary =
        "decoded_resources=" + std::to_string(bridge.decoded_resource_ready_packet_count)
        + "; payload_hashes=" + std::to_string(bridge.decoded_payload_hash_count)
        + "; decoded_bytes=" + std::to_string(bridge.decoded_byte_count)
        + "; staging_bytes=" + std::to_string(bridge.staging_payload_byte_count);
    bridge.upload_cache_payload_summary =
        "upload_cache_payloads="
        + std::to_string(bridge.upload_cache_payload_evidence_packet_count)
        + "; ready=" + std::to_string(bridge.upload_cache_payload_ready_packet_count)
        + "; placeholders=" + std::to_string(bridge.upload_cache_placeholder_packet_count)
        + "; blocked=" + std::to_string(bridge.upload_cache_payload_blocked_packet_count)
        + "; payload_hashes=" + std::to_string(bridge.upload_cache_payload_hash_count)
        + "; decoded_bytes=" + std::to_string(bridge.upload_cache_decoded_byte_count)
        + "; staging_bytes=" + std::to_string(bridge.upload_cache_staging_byte_count)
        + "; uploaded_bytes=" + std::to_string(bridge.upload_cache_uploaded_byte_count);

    bridge.status = bridge.packet_count == 0
        ? render_image_renderer_resource_packet_bridge_status::empty
        : (bridge.has_blockers
            ? render_image_renderer_resource_packet_bridge_status::blocked
            : (bridge.has_placeholders
                ? render_image_renderer_resource_packet_bridge_status::placeholder_backed
                : render_image_renderer_resource_packet_bridge_status::ready));
    bridge.status_name = render_image_renderer_resource_packet_bridge_status_name(bridge.status);

    switch (bridge.status) {
    case render_image_renderer_resource_packet_bridge_status::empty:
        bridge.diagnostic = "image renderer resource packet bridge has no packets";
        break;
    case render_image_renderer_resource_packet_bridge_status::ready:
        bridge.diagnostic = "image renderer resource packet bridge is ready";
        break;
    case render_image_renderer_resource_packet_bridge_status::placeholder_backed:
        bridge.diagnostic = "image renderer resource packet bridge is placeholder-backed";
        break;
    case render_image_renderer_resource_packet_bridge_status::blocked:
        bridge.diagnostic = "image renderer resource packet bridge has blocked packets";
        break;
    }
}

inline render_image_renderer_resource_packet_bridge
make_render_image_renderer_resource_packet_bridge(
    const render_image_renderer_texture_quad_draw_payload_frame& frame)
{
    render_image_renderer_resource_packet_bridge bridge{
        .frame_label = frame.frame_label,
        .source_packet_count = frame.source_packet_count,
        .source_payload_count = frame.payloads.size(),
    };

    std::map<std::string, std::size_t> identity_counts;
    for (const render_image_renderer_texture_quad_draw_payload& payload : frame.payloads) {
        const std::string identity =
            render_image_renderer_resource_packet_identity_for(payload);
        if (!identity.empty()) {
            ++identity_counts[identity];
        }
    }

    std::map<std::string, bool> unique_resource_packet_identities;
    std::map<std::string, bool> unique_texture_cache_keys;
    std::map<std::string, bool> unique_sampler_keys;
    std::map<std::uint64_t, bool> decoded_payload_hashes;
    std::map<std::uint64_t, bool> upload_cache_payload_hashes;
    for (const render_image_renderer_texture_quad_draw_payload& payload : frame.payloads) {
        const std::string identity =
            render_image_renderer_resource_packet_identity_for(payload);
        render_image_renderer_resource_packet_descriptor descriptor =
            make_render_image_renderer_resource_packet_descriptor(
                payload,
                bridge.packets.size(),
                !identity.empty() && identity_counts[identity] > 1);

        append_unique_render_image_texture_frame_upload_handoff_summary_fragment(
            unique_resource_packet_identities,
            bridge.resource_packet_identity_summary,
            descriptor.stable_resource_packet_identity);
        append_unique_render_image_texture_frame_upload_handoff_summary_fragment(
            unique_texture_cache_keys,
            bridge.texture_cache_key_summary,
            descriptor.stable_texture_cache_key);
        append_unique_render_image_texture_frame_upload_handoff_summary_fragment(
            unique_sampler_keys,
            bridge.sampler_summary,
            descriptor.sampler_key);
        if (descriptor.decoded_resource_evidence_present
            && descriptor.decoded_payload_hash != 0) {
            decoded_payload_hashes.emplace(descriptor.decoded_payload_hash, true);
        }
        if (descriptor.upload_cache_payload_evidence_present
            && descriptor.upload_cache_decoded_payload_hash != 0) {
            upload_cache_payload_hashes.emplace(
                descriptor.upload_cache_decoded_payload_hash,
                true);
        }

        count_render_image_renderer_resource_packet_descriptor(bridge, descriptor);
        bridge.packets.push_back(std::move(descriptor));
    }

    bridge.decoded_payload_hash_count = decoded_payload_hashes.size();
    bridge.upload_cache_payload_hash_count = upload_cache_payload_hashes.size();
    finalize_render_image_renderer_resource_packet_bridge(bridge);
    return bridge;
}

} // namespace quiz_vulkan::render
