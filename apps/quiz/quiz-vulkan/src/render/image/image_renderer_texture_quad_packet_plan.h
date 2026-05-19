#pragma once

#include "render/image/image_draw_list_texture_frame_composition.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

enum class render_image_renderer_texture_quad_packet_status {
    ready,
    blocked_missing_stable_identity,
    blocked_duplicate_stable_identity,
    blocked_handoff,
    blocked_missing_batch_request,
    blocked_batch,
    blocked_missing_frame_entry,
    blocked_frame,
    blocked_missing_resource_packet,
    blocked_resource_packet,
};

inline std::string render_image_renderer_texture_quad_packet_status_name(
    render_image_renderer_texture_quad_packet_status status)
{
    switch (status) {
    case render_image_renderer_texture_quad_packet_status::ready:
        return "ready";
    case render_image_renderer_texture_quad_packet_status::blocked_missing_stable_identity:
        return "blocked_missing_stable_identity";
    case render_image_renderer_texture_quad_packet_status::blocked_duplicate_stable_identity:
        return "blocked_duplicate_stable_identity";
    case render_image_renderer_texture_quad_packet_status::blocked_handoff:
        return "blocked_handoff";
    case render_image_renderer_texture_quad_packet_status::blocked_missing_batch_request:
        return "blocked_missing_batch_request";
    case render_image_renderer_texture_quad_packet_status::blocked_batch:
        return "blocked_batch";
    case render_image_renderer_texture_quad_packet_status::blocked_missing_frame_entry:
        return "blocked_missing_frame_entry";
    case render_image_renderer_texture_quad_packet_status::blocked_frame:
        return "blocked_frame";
    case render_image_renderer_texture_quad_packet_status::blocked_missing_resource_packet:
        return "blocked_missing_resource_packet";
    case render_image_renderer_texture_quad_packet_status::blocked_resource_packet:
        return "blocked_resource_packet";
    }

    return "unknown";
}

inline bool render_image_renderer_texture_quad_packet_status_is_blocked(
    render_image_renderer_texture_quad_packet_status status)
{
    return status != render_image_renderer_texture_quad_packet_status::ready;
}

enum class render_image_renderer_texture_quad_packet_summary_status {
    empty,
    ready,
    blocked,
};

inline std::string render_image_renderer_texture_quad_packet_summary_status_name(
    render_image_renderer_texture_quad_packet_summary_status status)
{
    switch (status) {
    case render_image_renderer_texture_quad_packet_summary_status::empty:
        return "empty";
    case render_image_renderer_texture_quad_packet_summary_status::ready:
        return "ready";
    case render_image_renderer_texture_quad_packet_summary_status::blocked:
        return "blocked";
    }

    return "unknown";
}


struct render_image_renderer_texture_quad_packet {
    std::size_t packet_index = 0;
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
    std::string sampler_key;
    render_image_texture_key texture_key;
    render_image_texture_key_diagnostic texture_key_diagnostic;
    std::string stable_draw_command_identity;
    std::string stable_texture_cache_key;
    std::string stable_quad_packet_identity;
    render_image_draw_list_frame_handoff_entry_status handoff_status =
        render_image_draw_list_frame_handoff_entry_status::blocked_empty_uri;
    std::string handoff_status_name = render_image_draw_list_frame_handoff_entry_status_name(
        render_image_draw_list_frame_handoff_entry_status::blocked_empty_uri);
    render_image_draw_list_texture_frame_composition_entry_status composition_status =
        render_image_draw_list_texture_frame_composition_entry_status::handoff_blocked;
    std::string composition_status_name = render_image_draw_list_texture_frame_composition_entry_status_name(
        render_image_draw_list_texture_frame_composition_entry_status::handoff_blocked);
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
    std::uint64_t decoded_payload_hash = 0;
    std::size_t decoded_byte_count = 0;
    std::size_t upload_layout_byte_count = 0;
    std::size_t upload_layout_row_stride_byte_count = 0;
    std::size_t staging_payload_byte_count = 0;
    std::size_t staging_row_copy_count = 0;
    bool entered_texture_batch = false;
    bool frame_entry_present = false;
    bool resource_packet_present = false;
    bool resource_packet_ready = false;
    bool renderer_handoff_ready = false;
    bool missing_stable_identity = false;
    bool duplicate_stable_identity = false;
    bool decoded_resource_evidence_present = false;
    bool decoded_payload_valid = false;
    bool upload_payload_layout_ready = false;
    bool staging_payload_ready = false;
    bool decoded_resource_ready = false;
    bool decoded_resource_blocked = false;
    bool ready = false;
    bool blocked = true;
    render_image_renderer_texture_quad_packet_status status =
        render_image_renderer_texture_quad_packet_status::blocked_handoff;
    std::string status_name = render_image_renderer_texture_quad_packet_status_name(
        render_image_renderer_texture_quad_packet_status::blocked_handoff);
    std::string blocker_summary;
    std::string decoded_resource_summary;
    std::string decoded_resource_blocker_summary;
    std::string diagnostic;

    bool ok() const
    {
        return status == render_image_renderer_texture_quad_packet_status::ready;
    }
};

struct render_image_renderer_texture_quad_packet_summary {
    render_image_renderer_texture_quad_packet_summary_status status =
        render_image_renderer_texture_quad_packet_summary_status::empty;
    std::string status_name = render_image_renderer_texture_quad_packet_summary_status_name(
        render_image_renderer_texture_quad_packet_summary_status::empty);
    std::string frame_label;
    std::size_t draw_command_count = 0;
    std::size_t non_image_command_count = 0;
    std::size_t image_command_count = 0;
    std::size_t composition_entry_count = 0;
    std::size_t packet_count = 0;
    std::size_t ready_packet_count = 0;
    std::size_t blocked_packet_count = 0;
    std::size_t missing_stable_identity_count = 0;
    std::size_t duplicate_stable_identity_count = 0;
    std::size_t handoff_blocked_packet_count = 0;
    std::size_t missing_batch_request_packet_count = 0;
    std::size_t batch_blocked_packet_count = 0;
    std::size_t missing_frame_entry_packet_count = 0;
    std::size_t frame_blocked_packet_count = 0;
    std::size_t missing_resource_packet_count = 0;
    std::size_t resource_packet_blocked_count = 0;
    std::size_t unique_stable_quad_packet_identity_count = 0;
    std::size_t unique_texture_cache_key_count = 0;
    std::size_t unique_sampler_key_count = 0;
    std::size_t uploaded_byte_count = 0;
    std::size_t decoded_resource_evidence_count = 0;
    std::size_t decoded_resource_ready_count = 0;
    std::size_t decoded_resource_blocked_count = 0;
    std::size_t decoded_payload_hash_count = 0;
    std::size_t decoded_payload_byte_count = 0;
    std::size_t upload_layout_byte_count = 0;
    std::size_t staging_payload_byte_count = 0;
    bool has_non_image_commands = false;
    bool has_blockers = false;
    bool has_decoded_resource_blockers = false;
    bool renderer_quad_packets_ready = false;
    bool has_missing_stable_identities = false;
    bool has_duplicate_stable_identities = false;
    std::vector<render_image_renderer_texture_quad_packet> packets;
    std::string skipped_command_summary;
    std::string stable_identity_summary;
    std::string texture_cache_key_summary;
    std::string sampler_summary;
    std::string blocker_summary;
    std::string decoded_resource_summary;
    std::string decoded_resource_blocker_summary;
    std::string diagnostic;

    bool ok() const
    {
        return status == render_image_renderer_texture_quad_packet_summary_status::ready;
    }
};


inline std::string render_image_renderer_texture_quad_packet_identity_for(
    const render_image_draw_list_texture_frame_composition_entry& composition_entry)
{
    if (composition_entry.stable_draw_command_identity.empty()
        || composition_entry.stable_texture_cache_key.empty()) {
        return {};
    }
    return composition_entry.stable_draw_command_identity + "|texture="
        + composition_entry.stable_texture_cache_key;
}

inline render_image_renderer_texture_quad_packet_status
render_image_renderer_texture_quad_packet_status_for(
    const render_image_renderer_texture_quad_packet& packet)
{
    if (packet.missing_stable_identity) {
        return render_image_renderer_texture_quad_packet_status::blocked_missing_stable_identity;
    }
    if (packet.duplicate_stable_identity) {
        return render_image_renderer_texture_quad_packet_status::blocked_duplicate_stable_identity;
    }
    if (packet.decoded_resource_evidence_present && packet.decoded_resource_blocked) {
        return render_image_renderer_texture_quad_packet_status::blocked_resource_packet;
    }

    switch (packet.composition_status) {
    case render_image_draw_list_texture_frame_composition_entry_status::ready:
        return render_image_renderer_texture_quad_packet_status::ready;
    case render_image_draw_list_texture_frame_composition_entry_status::handoff_blocked:
        return render_image_renderer_texture_quad_packet_status::blocked_handoff;
    case render_image_draw_list_texture_frame_composition_entry_status::missing_batch_request:
        return render_image_renderer_texture_quad_packet_status::blocked_missing_batch_request;
    case render_image_draw_list_texture_frame_composition_entry_status::batch_blocked:
        return render_image_renderer_texture_quad_packet_status::blocked_batch;
    case render_image_draw_list_texture_frame_composition_entry_status::missing_frame_entry:
        return render_image_renderer_texture_quad_packet_status::blocked_missing_frame_entry;
    case render_image_draw_list_texture_frame_composition_entry_status::frame_blocked:
        return render_image_renderer_texture_quad_packet_status::blocked_frame;
    case render_image_draw_list_texture_frame_composition_entry_status::missing_resource_packet:
        return render_image_renderer_texture_quad_packet_status::blocked_missing_resource_packet;
    case render_image_draw_list_texture_frame_composition_entry_status::resource_packet_blocked:
        return render_image_renderer_texture_quad_packet_status::blocked_resource_packet;
    }

    return render_image_renderer_texture_quad_packet_status::blocked_handoff;
}

inline void finalize_render_image_renderer_texture_quad_packet(
    render_image_renderer_texture_quad_packet& packet)
{
    packet.status = render_image_renderer_texture_quad_packet_status_for(packet);
    packet.status_name = render_image_renderer_texture_quad_packet_status_name(packet.status);
    packet.ready = packet.status == render_image_renderer_texture_quad_packet_status::ready;
    packet.blocked = !packet.ready;

    switch (packet.status) {
    case render_image_renderer_texture_quad_packet_status::ready:
        packet.blocker_summary.clear();
        packet.diagnostic = "image renderer texture quad packet is ready";
        break;
    case render_image_renderer_texture_quad_packet_status::blocked_missing_stable_identity:
        packet.blocker_summary = "renderer texture quad packet is missing stable identity";
        packet.diagnostic = "image renderer texture quad packet is blocked by missing stable identity";
        break;
    case render_image_renderer_texture_quad_packet_status::blocked_duplicate_stable_identity:
        packet.blocker_summary = "renderer texture quad packet has duplicate stable identity";
        packet.diagnostic = "image renderer texture quad packet is blocked by duplicate stable identity";
        break;
    case render_image_renderer_texture_quad_packet_status::blocked_handoff:
        if (packet.blocker_summary.empty()) {
            packet.blocker_summary = "draw-list handoff blocked renderer texture quad packet";
        }
        packet.diagnostic = "image renderer texture quad packet is blocked by draw-list handoff";
        break;
    case render_image_renderer_texture_quad_packet_status::blocked_missing_batch_request:
        packet.blocker_summary = "renderer texture quad packet is missing texture batch request";
        packet.diagnostic = "image renderer texture quad packet is blocked by missing batch request";
        break;
    case render_image_renderer_texture_quad_packet_status::blocked_batch:
        packet.blocker_summary = "texture batch plan blocked renderer texture quad packet";
        packet.diagnostic = "image renderer texture quad packet is blocked by texture batch plan";
        break;
    case render_image_renderer_texture_quad_packet_status::blocked_missing_frame_entry:
        packet.blocker_summary = "renderer texture quad packet is missing texture frame entry";
        packet.diagnostic = "image renderer texture quad packet is blocked by missing frame entry";
        break;
    case render_image_renderer_texture_quad_packet_status::blocked_frame:
        packet.blocker_summary = "texture frame entry blocked renderer texture quad packet";
        packet.diagnostic = "image renderer texture quad packet is blocked by frame entry";
        break;
    case render_image_renderer_texture_quad_packet_status::blocked_missing_resource_packet:
        packet.blocker_summary = "renderer texture quad packet is missing resource packet";
        packet.diagnostic = "image renderer texture quad packet is blocked by missing resource packet";
        break;
    case render_image_renderer_texture_quad_packet_status::blocked_resource_packet:
        if (packet.blocker_summary.empty()) {
            packet.blocker_summary = "resource packet blocked renderer texture quad packet";
        }
        packet.diagnostic = "image renderer texture quad packet is blocked by resource packet";
        break;
    }
}

inline render_image_renderer_texture_quad_packet make_render_image_renderer_texture_quad_packet(
    const render_image_draw_list_texture_frame_composition_entry& composition_entry,
    std::size_t packet_index)
{
    const std::string sampler_key =
        render_image_sampler_policy_stable_fragment(composition_entry.sampler);
    const std::string blocker_summary = composition_entry.decoded_resource_blocker_summary.empty()
        ? composition_entry.handoff_blocker_summary
        : composition_entry.decoded_resource_blocker_summary;
    render_image_renderer_texture_quad_packet packet{
        .packet_index = packet_index,
        .frame_label = composition_entry.frame_label,
        .draw_command_index = composition_entry.draw_command_index,
        .image_command_index = composition_entry.image_command_index,
        .texture_request_index = composition_entry.texture_request_index,
        .node_id = composition_entry.node_id,
        .parent_node_id = composition_entry.parent_node_id,
        .bounds = composition_entry.bounds,
        .content_bounds = composition_entry.content_bounds,
        .image = composition_entry.image,
        .uri = composition_entry.uri,
        .alt_text = composition_entry.alt_text,
        .aspect_ratio = composition_entry.aspect_ratio,
        .sampler = composition_entry.sampler,
        .sampler_policy = composition_entry.sampler_policy,
        .sampler_key = sampler_key,
        .texture_key = composition_entry.texture_key,
        .texture_key_diagnostic = composition_entry.texture_key_diagnostic,
        .stable_draw_command_identity = composition_entry.stable_draw_command_identity,
        .stable_texture_cache_key = composition_entry.stable_texture_cache_key,
        .stable_quad_packet_identity =
            render_image_renderer_texture_quad_packet_identity_for(composition_entry),
        .handoff_status = composition_entry.handoff_status,
        .handoff_status_name = composition_entry.handoff_status_name,
        .composition_status = composition_entry.status,
        .composition_status_name = composition_entry.status_name,
        .resource_packet_status = composition_entry.resource_packet_status,
        .resource_packet_status_name = composition_entry.resource_packet_status_name,
        .texture_id = composition_entry.texture_id,
        .texture_revision = composition_entry.texture_revision,
        .texture_width = composition_entry.texture_width,
        .texture_height = composition_entry.texture_height,
        .upload_request_id = composition_entry.upload_request_id,
        .upload_generation_id = composition_entry.upload_generation_id,
        .uploaded_byte_count = composition_entry.uploaded_byte_count,
        .decoded_payload_hash = composition_entry.decoded_payload_hash,
        .decoded_byte_count = composition_entry.decoded_byte_count,
        .upload_layout_byte_count = composition_entry.upload_layout_byte_count,
        .upload_layout_row_stride_byte_count = composition_entry.upload_layout_row_stride_byte_count,
        .staging_payload_byte_count = composition_entry.staging_payload_byte_count,
        .staging_row_copy_count = composition_entry.staging_row_copy_count,
        .entered_texture_batch = composition_entry.entered_texture_batch,
        .frame_entry_present = composition_entry.frame_entry_present,
        .resource_packet_present = composition_entry.resource_packet_present,
        .resource_packet_ready = composition_entry.resource_packet_ready,
        .renderer_handoff_ready = composition_entry.frame_renderer_handoff_ready,
        .missing_stable_identity =
            composition_entry.stable_draw_command_identity.empty()
            || composition_entry.stable_texture_cache_key.empty()
            || composition_entry.handoff_status
                == render_image_draw_list_frame_handoff_entry_status::blocked_missing_stable_identity,
        .duplicate_stable_identity =
            composition_entry.handoff_status
            == render_image_draw_list_frame_handoff_entry_status::blocked_duplicate_stable_identity,
        .decoded_resource_evidence_present = composition_entry.decoded_resource_evidence_present,
        .decoded_payload_valid = composition_entry.decoded_payload_valid,
        .upload_payload_layout_ready = composition_entry.upload_payload_layout_ready,
        .staging_payload_ready = composition_entry.staging_payload_ready,
        .decoded_resource_ready = composition_entry.decoded_resource_ready,
        .decoded_resource_blocked = composition_entry.decoded_resource_blocked,
        .blocker_summary = blocker_summary,
        .decoded_resource_summary = composition_entry.decoded_resource_summary,
        .decoded_resource_blocker_summary = composition_entry.decoded_resource_blocker_summary,
    };

    finalize_render_image_renderer_texture_quad_packet(packet);
    return packet;
}

inline void count_render_image_renderer_texture_quad_packet(
    render_image_renderer_texture_quad_packet_summary& summary,
    const render_image_renderer_texture_quad_packet& packet)
{
    if (packet.ready) {
        ++summary.ready_packet_count;
    } else {
        ++summary.blocked_packet_count;
        summary.has_blockers = true;
    }

    summary.uploaded_byte_count += packet.uploaded_byte_count;
    if (packet.decoded_resource_evidence_present) {
        ++summary.decoded_resource_evidence_count;
        if (packet.decoded_resource_ready) {
            ++summary.decoded_resource_ready_count;
            summary.decoded_payload_byte_count += packet.decoded_byte_count;
            summary.upload_layout_byte_count += packet.upload_layout_byte_count;
            summary.staging_payload_byte_count += packet.staging_payload_byte_count;
        } else {
            ++summary.decoded_resource_blocked_count;
            summary.has_decoded_resource_blockers = true;
            append_render_image_texture_frame_upload_handoff_summary_fragment(
                summary.decoded_resource_blocker_summary,
                packet.decoded_resource_blocker_summary.empty()
                    ? packet.blocker_summary
                    : packet.decoded_resource_blocker_summary);
        }
    }
    if (packet.missing_stable_identity) {
        ++summary.missing_stable_identity_count;
        summary.has_missing_stable_identities = true;
    }
    if (packet.duplicate_stable_identity) {
        ++summary.duplicate_stable_identity_count;
        summary.has_duplicate_stable_identities = true;
    }

    switch (packet.status) {
    case render_image_renderer_texture_quad_packet_status::ready:
        break;
    case render_image_renderer_texture_quad_packet_status::blocked_missing_stable_identity:
        break;
    case render_image_renderer_texture_quad_packet_status::blocked_duplicate_stable_identity:
        break;
    case render_image_renderer_texture_quad_packet_status::blocked_handoff:
        ++summary.handoff_blocked_packet_count;
        break;
    case render_image_renderer_texture_quad_packet_status::blocked_missing_batch_request:
        ++summary.missing_batch_request_packet_count;
        break;
    case render_image_renderer_texture_quad_packet_status::blocked_batch:
        ++summary.batch_blocked_packet_count;
        break;
    case render_image_renderer_texture_quad_packet_status::blocked_missing_frame_entry:
        ++summary.missing_frame_entry_packet_count;
        break;
    case render_image_renderer_texture_quad_packet_status::blocked_frame:
        ++summary.frame_blocked_packet_count;
        break;
    case render_image_renderer_texture_quad_packet_status::blocked_missing_resource_packet:
        ++summary.missing_resource_packet_count;
        break;
    case render_image_renderer_texture_quad_packet_status::blocked_resource_packet:
        ++summary.resource_packet_blocked_count;
        break;
    }

    if (!packet.blocker_summary.empty()) {
        append_render_image_texture_frame_upload_handoff_summary_fragment(
            summary.blocker_summary,
            packet.blocker_summary);
    }
}

inline render_image_renderer_texture_quad_packet_summary
make_render_image_renderer_texture_quad_packet_summary(
    const render_image_draw_list_texture_frame_composition& composition)
{
    render_image_renderer_texture_quad_packet_summary summary{
        .frame_label = composition.frame_label,
        .draw_command_count = composition.draw_command_count,
        .non_image_command_count = composition.non_image_command_count,
        .image_command_count = composition.image_command_count,
        .composition_entry_count = composition.entries.size(),
        .has_non_image_commands = composition.has_non_image_commands,
        .skipped_command_summary = composition.skipped_command_summary,
    };

    std::map<std::string, std::size_t> stable_draw_command_identity_counts;
    std::map<std::string, std::size_t> stable_quad_packet_identity_counts;
    for (const render_image_draw_list_texture_frame_composition_entry& entry : composition.entries) {
        if (!entry.stable_draw_command_identity.empty()) {
            ++stable_draw_command_identity_counts[entry.stable_draw_command_identity];
        }
        const std::string stable_quad_identity =
            render_image_renderer_texture_quad_packet_identity_for(entry);
        if (!stable_quad_identity.empty()) {
            ++stable_quad_packet_identity_counts[stable_quad_identity];
        }
    }

    std::map<std::string, bool> unique_quad_packet_identities;
    std::map<std::string, bool> unique_texture_cache_keys;
    std::map<std::string, bool> unique_sampler_keys;
    std::map<std::uint64_t, bool> decoded_payload_hashes;
    for (const render_image_draw_list_texture_frame_composition_entry& entry : composition.entries) {
        render_image_renderer_texture_quad_packet packet =
            make_render_image_renderer_texture_quad_packet(entry, summary.packets.size());
        if (!packet.stable_draw_command_identity.empty()
            && stable_draw_command_identity_counts[packet.stable_draw_command_identity] > 1) {
            packet.duplicate_stable_identity = true;
        }
        if (!packet.stable_quad_packet_identity.empty()
            && stable_quad_packet_identity_counts[packet.stable_quad_packet_identity] > 1) {
            packet.duplicate_stable_identity = true;
        }
        finalize_render_image_renderer_texture_quad_packet(packet);

        append_unique_render_image_texture_frame_upload_handoff_summary_fragment(
            unique_quad_packet_identities,
            summary.stable_identity_summary,
            packet.stable_quad_packet_identity);
        append_unique_render_image_texture_frame_upload_handoff_summary_fragment(
            unique_texture_cache_keys,
            summary.texture_cache_key_summary,
            packet.stable_texture_cache_key);
        append_unique_render_image_texture_frame_upload_handoff_summary_fragment(
            unique_sampler_keys,
            summary.sampler_summary,
            packet.sampler_key);
        if (packet.decoded_resource_evidence_present && packet.decoded_payload_hash != 0) {
            decoded_payload_hashes.emplace(packet.decoded_payload_hash, true);
        }

        count_render_image_renderer_texture_quad_packet(summary, packet);
        summary.packets.push_back(std::move(packet));
    }

    summary.packet_count = summary.packets.size();
    summary.unique_stable_quad_packet_identity_count = unique_quad_packet_identities.size();
    summary.unique_texture_cache_key_count = unique_texture_cache_keys.size();
    summary.unique_sampler_key_count = unique_sampler_keys.size();
    summary.decoded_payload_hash_count = decoded_payload_hashes.size();
    summary.renderer_quad_packets_ready = summary.packet_count != 0 && !summary.has_blockers;

    if (summary.stable_identity_summary.empty()) {
        summary.stable_identity_summary = "no renderer texture quad packet stable identities";
    }
    if (summary.texture_cache_key_summary.empty()) {
        summary.texture_cache_key_summary = "no renderer texture quad packet cache keys";
    }
    if (summary.sampler_summary.empty()) {
        summary.sampler_summary = "no renderer texture quad packet sampler keys";
    }
    if (summary.blocker_summary.empty()) {
        summary.blocker_summary = "no renderer texture quad packet blockers";
    }
    summary.decoded_resource_summary =
        "decoded_resources=" + std::to_string(summary.decoded_resource_ready_count)
        + "; payload_hashes=" + std::to_string(summary.decoded_payload_hash_count)
        + "; decoded_bytes=" + std::to_string(summary.decoded_payload_byte_count)
        + "; staging_bytes=" + std::to_string(summary.staging_payload_byte_count);
    if (summary.decoded_resource_blocker_summary.empty()) {
        summary.decoded_resource_blocker_summary = "no decoded resource blockers";
    }

    summary.status = summary.packet_count == 0
        ? render_image_renderer_texture_quad_packet_summary_status::empty
        : (summary.has_blockers
            ? render_image_renderer_texture_quad_packet_summary_status::blocked
            : render_image_renderer_texture_quad_packet_summary_status::ready);
    summary.status_name = render_image_renderer_texture_quad_packet_summary_status_name(summary.status);

    switch (summary.status) {
    case render_image_renderer_texture_quad_packet_summary_status::empty:
        summary.diagnostic = "image renderer texture quad packet summary has no image packets";
        break;
    case render_image_renderer_texture_quad_packet_summary_status::ready:
        summary.diagnostic = "image renderer texture quad packet summary is ready";
        break;
    case render_image_renderer_texture_quad_packet_summary_status::blocked:
        summary.diagnostic = "image renderer texture quad packet summary has blocked image packets";
        break;
    }

    return summary;
}

} // namespace quiz_vulkan::render
