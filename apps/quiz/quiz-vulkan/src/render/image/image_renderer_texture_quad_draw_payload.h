#pragma once

#include "render/image/image_renderer_texture_quad_packet_plan.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

enum class render_image_renderer_texture_quad_draw_payload_status {
    draw_ready,
    placeholder_backed,
    blocked,
};

inline std::string render_image_renderer_texture_quad_draw_payload_status_name(
    render_image_renderer_texture_quad_draw_payload_status status)
{
    switch (status) {
    case render_image_renderer_texture_quad_draw_payload_status::draw_ready:
        return "draw_ready";
    case render_image_renderer_texture_quad_draw_payload_status::placeholder_backed:
        return "placeholder_backed";
    case render_image_renderer_texture_quad_draw_payload_status::blocked:
        return "blocked";
    }

    return "unknown";
}

enum class render_image_renderer_texture_quad_draw_payload_frame_status {
    empty,
    draw_ready,
    placeholder_backed,
    blocked,
};

inline std::string render_image_renderer_texture_quad_draw_payload_frame_status_name(
    render_image_renderer_texture_quad_draw_payload_frame_status status)
{
    switch (status) {
    case render_image_renderer_texture_quad_draw_payload_frame_status::empty:
        return "empty";
    case render_image_renderer_texture_quad_draw_payload_frame_status::draw_ready:
        return "draw_ready";
    case render_image_renderer_texture_quad_draw_payload_frame_status::placeholder_backed:
        return "placeholder_backed";
    case render_image_renderer_texture_quad_draw_payload_frame_status::blocked:
        return "blocked";
    }

    return "unknown";
}

struct render_image_renderer_texture_quad_draw_payload_options {
    fake_image_texture_placeholder_policy placeholder_policy;
    fake_image_texture_placeholder_reason placeholder_reason =
        fake_image_texture_placeholder_reason::upload_failed;
};

struct render_image_renderer_texture_quad_draw_payload {
    std::size_t payload_index = 0;
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
    std::string stable_payload_identity;
    render_image_texture_key texture_key;
    render_image_texture_key placeholder_key;
    std::string stable_texture_cache_key;
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
    render_image_renderer_texture_quad_packet_status source_packet_status =
        render_image_renderer_texture_quad_packet_status::blocked_handoff;
    std::string source_packet_status_name;
    render_image_texture_frame_resource_packet_status resource_packet_status =
        render_image_texture_frame_resource_packet_status::blocked;
    std::string resource_packet_status_name;
    render_image_renderer_texture_quad_draw_payload_status status =
        render_image_renderer_texture_quad_draw_payload_status::blocked;
    std::string status_name = render_image_renderer_texture_quad_draw_payload_status_name(
        render_image_renderer_texture_quad_draw_payload_status::blocked);
    bool draw_ready = false;
    bool placeholder_backed = false;
    bool placeholder_policy_enabled = false;
    bool fallback_placeholder = false;
    bool decoded_resource_evidence_present = false;
    bool decoded_payload_valid = false;
    bool upload_payload_layout_ready = false;
    bool staging_payload_ready = false;
    bool decoded_resource_ready = false;
    bool decoded_resource_blocked = false;
    bool blocked = true;
    std::string blocker_summary;
    std::string decoded_resource_summary;
    std::string decoded_resource_blocker_summary;
    std::string diagnostic;

    bool ok() const
    {
        return draw_ready || placeholder_backed;
    }
};

struct render_image_renderer_texture_quad_draw_payload_frame {
    render_image_renderer_texture_quad_draw_payload_frame_status status =
        render_image_renderer_texture_quad_draw_payload_frame_status::empty;
    std::string status_name = render_image_renderer_texture_quad_draw_payload_frame_status_name(
        render_image_renderer_texture_quad_draw_payload_frame_status::empty);
    std::string frame_label;
    std::size_t source_packet_count = 0;
    std::size_t payload_count = 0;
    std::size_t draw_ready_payload_count = 0;
    std::size_t placeholder_payload_count = 0;
    std::size_t fallback_placeholder_payload_count = 0;
    std::size_t blocked_payload_count = 0;
    std::size_t decoded_resource_evidence_payload_count = 0;
    std::size_t decoded_resource_ready_payload_count = 0;
    std::size_t decoded_resource_blocked_payload_count = 0;
    std::size_t decoded_payload_hash_count = 0;
    std::size_t decoded_payload_byte_count = 0;
    std::size_t upload_layout_byte_count = 0;
    std::size_t staging_payload_byte_count = 0;
    bool placeholder_policy_enabled = false;
    bool draw_payloads_ready = false;
    bool has_placeholders = false;
    bool has_fallback_placeholders = false;
    bool has_blockers = false;
    bool has_decoded_resource_blockers = false;
    std::vector<render_image_renderer_texture_quad_draw_payload> payloads;
    std::string payload_identity_summary;
    std::string blocker_summary;
    std::string decoded_resource_summary;
    std::string decoded_resource_blocker_summary;
    std::string diagnostic;

    bool ok() const
    {
        return !has_blockers && payload_count != 0;
    }
};

inline render_image_texture_key render_image_renderer_texture_quad_draw_payload_requested_key_for(
    const render_image_renderer_texture_quad_packet& packet)
{
    render_image_texture_key key = packet.texture_key;
    if (key.source_key.empty()) {
        if (!packet.stable_texture_cache_key.empty()) {
            key.source_key = packet.stable_texture_cache_key;
        } else if (!packet.uri.empty()) {
            key.source_key = packet.uri;
        } else {
            key.source_key = packet.stable_draw_command_identity;
        }
    }
    key.sampler = packet.sampler;
    return key;
}

inline render_image_renderer_texture_quad_draw_payload_status
render_image_renderer_texture_quad_draw_payload_status_for(
    const render_image_renderer_texture_quad_packet& packet,
    const render_image_renderer_texture_quad_draw_payload_options& options)
{
    if (packet.ok()
        && packet.resource_packet_status == render_image_texture_frame_resource_packet_status::placeholder_backed) {
        return render_image_renderer_texture_quad_draw_payload_status::placeholder_backed;
    }
    if (packet.ok()) {
        return render_image_renderer_texture_quad_draw_payload_status::draw_ready;
    }
    if (options.placeholder_policy.enabled) {
        return render_image_renderer_texture_quad_draw_payload_status::placeholder_backed;
    }
    return render_image_renderer_texture_quad_draw_payload_status::blocked;
}

inline std::string render_image_renderer_texture_quad_draw_payload_identity_for(
    const render_image_renderer_texture_quad_packet& packet,
    render_image_renderer_texture_quad_draw_payload_status status,
    const render_image_texture_key& texture_key)
{
    std::string identity = !packet.stable_quad_packet_identity.empty()
        ? packet.stable_quad_packet_identity
        : packet.stable_draw_command_identity;
    if (identity.empty()) {
        identity = "frame=" + packet.frame_label
            + "|draw=" + std::to_string(packet.draw_command_index)
            + "|image=" + std::to_string(packet.image_command_index)
            + "|uri=" + packet.uri;
    }

    switch (status) {
    case render_image_renderer_texture_quad_draw_payload_status::draw_ready:
        return identity + "|payload=texture:"
            + std::to_string(packet.texture_id)
            + ":" + std::to_string(packet.texture_revision);
    case render_image_renderer_texture_quad_draw_payload_status::placeholder_backed:
        return identity + "|payload=placeholder:"
            + make_render_image_texture_key_diagnostic(texture_key).stable_cache_key;
    case render_image_renderer_texture_quad_draw_payload_status::blocked:
        return identity + "|payload=blocked:" + packet.status_name;
    }

    return identity;
}

inline render_image_renderer_texture_quad_draw_payload make_render_image_renderer_texture_quad_draw_payload(
    const render_image_renderer_texture_quad_packet& packet,
    const render_image_renderer_texture_quad_draw_payload_options& options,
    std::size_t payload_index)
{
    const render_image_renderer_texture_quad_draw_payload_status status =
        render_image_renderer_texture_quad_draw_payload_status_for(packet, options);
    const bool fallback_placeholder =
        status == render_image_renderer_texture_quad_draw_payload_status::placeholder_backed
        && !packet.ok();
    const render_image_texture_key requested_key =
        render_image_renderer_texture_quad_draw_payload_requested_key_for(packet);
    const render_image_texture_key placeholder_key = fallback_placeholder
        ? make_fake_image_texture_placeholder_key(
            options.placeholder_policy,
            options.placeholder_reason,
            requested_key)
        : render_image_texture_key{};
    const render_image_texture_key payload_key = fallback_placeholder
        ? placeholder_key
        : requested_key;
    const render_image_texture_key_diagnostic payload_key_diagnostic =
        make_render_image_texture_key_diagnostic(payload_key);

    render_image_renderer_texture_quad_draw_payload payload{
        .payload_index = payload_index,
        .source_packet_index = packet.packet_index,
        .frame_label = packet.frame_label,
        .draw_command_index = packet.draw_command_index,
        .image_command_index = packet.image_command_index,
        .texture_request_index = packet.texture_request_index,
        .node_id = packet.node_id,
        .parent_node_id = packet.parent_node_id,
        .bounds = packet.bounds,
        .content_bounds = packet.content_bounds,
        .uri = packet.uri,
        .alt_text = packet.alt_text,
        .aspect_ratio = packet.aspect_ratio,
        .stable_draw_command_identity = packet.stable_draw_command_identity,
        .stable_quad_packet_identity = packet.stable_quad_packet_identity,
        .texture_key = payload_key,
        .placeholder_key = placeholder_key,
        .stable_texture_cache_key = fallback_placeholder
            ? payload_key_diagnostic.stable_cache_key
            : packet.stable_texture_cache_key,
        .sampler_key = packet.sampler_key,
        .texture_id = fallback_placeholder ? 0 : packet.texture_id,
        .texture_revision = fallback_placeholder ? 0 : packet.texture_revision,
        .texture_width = fallback_placeholder ? options.placeholder_policy.width : packet.texture_width,
        .texture_height = fallback_placeholder ? options.placeholder_policy.height : packet.texture_height,
        .upload_request_id = fallback_placeholder ? 0 : packet.upload_request_id,
        .upload_generation_id = fallback_placeholder ? 0 : packet.upload_generation_id,
        .uploaded_byte_count = fallback_placeholder ? 0 : packet.uploaded_byte_count,
        .decoded_payload_hash = packet.decoded_payload_hash,
        .decoded_byte_count = packet.decoded_byte_count,
        .upload_layout_byte_count = packet.upload_layout_byte_count,
        .upload_layout_row_stride_byte_count = packet.upload_layout_row_stride_byte_count,
        .staging_payload_byte_count = packet.staging_payload_byte_count,
        .staging_row_copy_count = packet.staging_row_copy_count,
        .source_packet_status = packet.status,
        .source_packet_status_name = packet.status_name,
        .resource_packet_status = packet.resource_packet_status,
        .resource_packet_status_name = packet.resource_packet_status_name,
        .status = status,
        .status_name = render_image_renderer_texture_quad_draw_payload_status_name(status),
        .draw_ready = status == render_image_renderer_texture_quad_draw_payload_status::draw_ready,
        .placeholder_backed =
            status == render_image_renderer_texture_quad_draw_payload_status::placeholder_backed,
        .placeholder_policy_enabled = options.placeholder_policy.enabled,
        .fallback_placeholder = fallback_placeholder,
        .decoded_resource_evidence_present = packet.decoded_resource_evidence_present,
        .decoded_payload_valid = packet.decoded_payload_valid,
        .upload_payload_layout_ready = packet.upload_payload_layout_ready,
        .staging_payload_ready = packet.staging_payload_ready,
        .decoded_resource_ready = packet.decoded_resource_ready,
        .decoded_resource_blocked = packet.decoded_resource_blocked,
        .blocked = status == render_image_renderer_texture_quad_draw_payload_status::blocked,
        .blocker_summary = packet.blocker_summary,
        .decoded_resource_summary = packet.decoded_resource_summary,
        .decoded_resource_blocker_summary = packet.decoded_resource_blocker_summary,
    };
    payload.stable_payload_identity =
        render_image_renderer_texture_quad_draw_payload_identity_for(packet, status, payload_key);

    switch (payload.status) {
    case render_image_renderer_texture_quad_draw_payload_status::draw_ready:
        payload.blocker_summary.clear();
        payload.diagnostic = "image renderer texture quad draw payload is ready";
        break;
    case render_image_renderer_texture_quad_draw_payload_status::placeholder_backed:
        if (payload.fallback_placeholder) {
            payload.blocker_summary = packet.blocker_summary.empty()
                ? packet.diagnostic
                : packet.blocker_summary;
            payload.diagnostic = make_fake_image_texture_placeholder_diagnostic(
                options.placeholder_reason,
                payload.blocker_summary);
        } else {
            payload.blocker_summary.clear();
            payload.diagnostic = "image renderer texture quad draw payload uses placeholder texture";
        }
        break;
    case render_image_renderer_texture_quad_draw_payload_status::blocked:
        if (payload.blocker_summary.empty()) {
            payload.blocker_summary = packet.diagnostic;
        }
        payload.diagnostic = "image renderer texture quad draw payload is blocked: "
            + payload.blocker_summary;
        break;
    }

    if (payload.decoded_resource_blocked && payload.decoded_resource_blocker_summary.empty()) {
        payload.decoded_resource_blocker_summary = payload.blocker_summary;
    }
    if (payload.decoded_resource_evidence_present && payload.decoded_resource_summary.empty()) {
        payload.decoded_resource_summary =
            "decoded_bytes=" + std::to_string(payload.decoded_byte_count)
            + "; staging_bytes=" + std::to_string(payload.staging_payload_byte_count)
            + "; payload_hash=" + std::to_string(payload.decoded_payload_hash);
    }

    return payload;
}

inline void count_render_image_renderer_texture_quad_draw_payload(
    render_image_renderer_texture_quad_draw_payload_frame& frame,
    const render_image_renderer_texture_quad_draw_payload& payload)
{
    switch (payload.status) {
    case render_image_renderer_texture_quad_draw_payload_status::draw_ready:
        ++frame.draw_ready_payload_count;
        break;
    case render_image_renderer_texture_quad_draw_payload_status::placeholder_backed:
        ++frame.placeholder_payload_count;
        frame.has_placeholders = true;
        if (payload.fallback_placeholder) {
            ++frame.fallback_placeholder_payload_count;
            frame.has_fallback_placeholders = true;
        }
        break;
    case render_image_renderer_texture_quad_draw_payload_status::blocked:
        ++frame.blocked_payload_count;
        frame.has_blockers = true;
        append_render_image_texture_frame_upload_handoff_summary_fragment(
            frame.blocker_summary,
            payload.blocker_summary);
        break;
    }

    if (payload.decoded_resource_evidence_present) {
        ++frame.decoded_resource_evidence_payload_count;
        if (payload.decoded_resource_ready) {
            ++frame.decoded_resource_ready_payload_count;
            frame.decoded_payload_byte_count += payload.decoded_byte_count;
            frame.upload_layout_byte_count += payload.upload_layout_byte_count;
            frame.staging_payload_byte_count += payload.staging_payload_byte_count;
        } else {
            ++frame.decoded_resource_blocked_payload_count;
            frame.has_decoded_resource_blockers = true;
            append_render_image_texture_frame_upload_handoff_summary_fragment(
                frame.decoded_resource_blocker_summary,
                payload.decoded_resource_blocker_summary.empty()
                    ? payload.blocker_summary
                    : payload.decoded_resource_blocker_summary);
        }
    }
}

inline render_image_renderer_texture_quad_draw_payload_frame
make_render_image_renderer_texture_quad_draw_payload_frame(
    const render_image_renderer_texture_quad_packet_summary& summary,
    const render_image_renderer_texture_quad_draw_payload_options& options =
        render_image_renderer_texture_quad_draw_payload_options{})
{
    render_image_renderer_texture_quad_draw_payload_frame frame{
        .frame_label = summary.frame_label,
        .source_packet_count = summary.packets.size(),
        .placeholder_policy_enabled = options.placeholder_policy.enabled,
    };

    std::map<std::string, bool> unique_payload_identities;
    std::map<std::uint64_t, bool> decoded_payload_hashes;
    for (const render_image_renderer_texture_quad_packet& packet : summary.packets) {
        render_image_renderer_texture_quad_draw_payload payload =
            make_render_image_renderer_texture_quad_draw_payload(
                packet,
                options,
                frame.payloads.size());
        count_render_image_renderer_texture_quad_draw_payload(frame, payload);
        if (payload.decoded_resource_evidence_present && payload.decoded_payload_hash != 0) {
            decoded_payload_hashes.emplace(payload.decoded_payload_hash, true);
        }
        append_unique_render_image_texture_frame_upload_handoff_summary_fragment(
            unique_payload_identities,
            frame.payload_identity_summary,
            payload.stable_payload_identity);
        frame.payloads.push_back(std::move(payload));
    }

    frame.payload_count = frame.payloads.size();
    frame.decoded_payload_hash_count = decoded_payload_hashes.size();
    frame.draw_payloads_ready = frame.payload_count != 0 && !frame.has_blockers;
    if (frame.payload_identity_summary.empty()) {
        frame.payload_identity_summary = "no renderer texture quad draw payload identities";
    }
    if (frame.blocker_summary.empty()) {
        frame.blocker_summary = "no renderer texture quad draw payload blockers";
    }
    frame.decoded_resource_summary =
        "decoded_payloads=" + std::to_string(frame.decoded_resource_ready_payload_count)
        + "; payload_hashes=" + std::to_string(frame.decoded_payload_hash_count)
        + "; decoded_bytes=" + std::to_string(frame.decoded_payload_byte_count)
        + "; staging_bytes=" + std::to_string(frame.staging_payload_byte_count);
    if (frame.decoded_resource_blocker_summary.empty()) {
        frame.decoded_resource_blocker_summary = "no decoded resource blockers";
    }

    frame.status = frame.payload_count == 0
        ? render_image_renderer_texture_quad_draw_payload_frame_status::empty
        : (frame.has_blockers
            ? render_image_renderer_texture_quad_draw_payload_frame_status::blocked
            : (frame.has_placeholders
                ? render_image_renderer_texture_quad_draw_payload_frame_status::placeholder_backed
                : render_image_renderer_texture_quad_draw_payload_frame_status::draw_ready));
    frame.status_name = render_image_renderer_texture_quad_draw_payload_frame_status_name(frame.status);

    switch (frame.status) {
    case render_image_renderer_texture_quad_draw_payload_frame_status::empty:
        frame.diagnostic = "image renderer texture quad draw payload frame has no payloads";
        break;
    case render_image_renderer_texture_quad_draw_payload_frame_status::draw_ready:
        frame.diagnostic = "image renderer texture quad draw payload frame is ready";
        break;
    case render_image_renderer_texture_quad_draw_payload_frame_status::placeholder_backed:
        frame.diagnostic = "image renderer texture quad draw payload frame is placeholder-backed";
        break;
    case render_image_renderer_texture_quad_draw_payload_frame_status::blocked:
        frame.diagnostic = "image renderer texture quad draw payload frame has blocked payloads";
        break;
    }

    return frame;
}

} // namespace quiz_vulkan::render
