#pragma once

#include "render/image/image_texture_frame_resource_packet_plan_core.h"
#include "render/image/image_texture_pipeline.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

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
    std::uint64_t decoded_payload_hash = 0;
    std::size_t decoded_byte_count = 0;
    std::size_t upload_layout_byte_count = 0;
    std::size_t upload_layout_row_stride_byte_count = 0;
    std::size_t staging_payload_byte_count = 0;
    std::size_t staging_row_copy_count = 0;
    bool handoff_blocked = true;
    bool batch_entry_present = false;
    bool batch_request_planned = false;
    bool frame_entry_present = false;
    bool frame_renderer_handoff_ready = false;
    bool resource_packet_present = false;
    bool resource_packet_ready = false;
    bool resource_packet_blocked = false;
    bool entered_texture_batch = false;
    bool decoded_resource_evidence_present = false;
    bool decoded_payload_valid = false;
    bool upload_payload_layout_ready = false;
    bool staging_payload_ready = false;
    bool decoded_resource_ready = false;
    bool decoded_resource_blocked = false;
    bool ready = false;
    bool blocked = true;
    render_image_draw_list_texture_frame_composition_entry_status status =
        render_image_draw_list_texture_frame_composition_entry_status::handoff_blocked;
    std::string status_name = render_image_draw_list_texture_frame_composition_entry_status_name(
        render_image_draw_list_texture_frame_composition_entry_status::handoff_blocked);
    std::string decoded_resource_summary;
    std::string decoded_resource_blocker_summary;
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
    std::size_t decoded_resource_ready_count = 0;
    std::size_t decoded_resource_blocked_count = 0;
    std::size_t decoded_payload_hash_count = 0;
    std::size_t decoded_payload_byte_count = 0;
    std::size_t staging_payload_byte_count = 0;
    bool has_non_image_commands = false;
    bool has_blockers = false;
    bool has_decoded_resource_blockers = false;
    bool renderer_handoff_ready = false;
    bool resource_packet_ready = false;
    std::vector<render_image_draw_list_texture_frame_composition_entry> entries;
    std::string skipped_command_summary;
    std::string decoded_resource_summary;
    std::string decoded_resource_blocker_summary;
    std::string diagnostic;

    bool ok() const
    {
        return status == render_image_draw_list_texture_frame_composition_status::ready;
    }
};

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
    if (entry.decoded_resource_evidence_present && entry.decoded_resource_blocked) {
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
        if (!entry.decoded_resource_blocker_summary.empty()) {
            entry.diagnostic += ": " + entry.decoded_resource_blocker_summary;
        }
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

    if (entry.decoded_resource_evidence_present) {
        if (entry.decoded_resource_ready) {
            ++composition.decoded_resource_ready_count;
            composition.decoded_payload_byte_count += entry.decoded_byte_count;
            composition.staging_payload_byte_count += entry.staging_payload_byte_count;
        } else {
            ++composition.decoded_resource_blocked_count;
            composition.has_decoded_resource_blockers = true;
            append_render_image_texture_frame_upload_handoff_summary_fragment(
                composition.decoded_resource_blocker_summary,
                entry.decoded_resource_blocker_summary);
        }
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
            entry.decoded_payload_hash = resource_packet->decoded_payload.stable_byte_hash;
            entry.decoded_byte_count = resource_packet->decoded_payload.decoded_byte_count;
            entry.upload_layout_byte_count = resource_packet->payload_layout.decoded_byte_count;
            entry.upload_layout_row_stride_byte_count =
                resource_packet->payload_layout.row_stride_byte_count;
            entry.staging_payload_byte_count =
                resource_packet->staging_payload_plan.total_staging_byte_count;
            entry.staging_row_copy_count = resource_packet->staging_payload_plan.row_copy_count;
            entry.decoded_payload_valid = resource_packet->decoded_payload.payload_valid;
            entry.upload_payload_layout_ready = resource_packet->payload_layout.ok();
            entry.staging_payload_ready = resource_packet->staging_payload_plan.ok();
            entry.decoded_resource_evidence_present =
                entry.decoded_payload_valid
                || entry.decoded_payload_hash != 0
                || entry.decoded_byte_count != 0
                || !resource_packet->decoded_payload.diagnostic.empty()
                || entry.upload_layout_byte_count != 0
                || entry.upload_layout_row_stride_byte_count != 0
                || !resource_packet->payload_layout.diagnostic.empty()
                || entry.staging_payload_byte_count != 0
                || entry.staging_row_copy_count != 0
                || !resource_packet->staging_payload_plan.blocker_summary.empty()
                || !resource_packet->staging_payload_plan.diagnostic.empty();
            entry.decoded_resource_ready =
                entry.decoded_resource_evidence_present
                && resource_packet->bindable
                && entry.decoded_payload_valid
                && entry.upload_payload_layout_ready
                && entry.staging_payload_ready;
            entry.decoded_resource_blocked =
                entry.decoded_resource_evidence_present && !entry.decoded_resource_ready;
            if (entry.decoded_resource_evidence_present && !entry.decoded_payload_valid) {
                entry.decoded_resource_blocker_summary = resource_packet->decoded_payload.diagnostic.empty()
                    ? "decoded payload bytes are missing or invalid"
                    : resource_packet->decoded_payload.diagnostic;
            } else if (entry.decoded_resource_evidence_present && !entry.upload_payload_layout_ready) {
                entry.decoded_resource_blocker_summary = resource_packet->payload_layout.diagnostic.empty()
                    ? "upload payload layout is not ready"
                    : resource_packet->payload_layout.diagnostic;
            } else if (entry.decoded_resource_evidence_present && !entry.staging_payload_ready) {
                entry.decoded_resource_blocker_summary =
                    resource_packet->staging_payload_plan.blocker_summary.empty()
                    ? resource_packet->staging_payload_plan.diagnostic
                    : resource_packet->staging_payload_plan.blocker_summary;
            } else if (entry.decoded_resource_evidence_present && entry.resource_packet_blocked) {
                entry.decoded_resource_blocker_summary = resource_packet->blocker_summary;
            }
            if (entry.decoded_resource_evidence_present) {
                entry.decoded_resource_summary =
                    "decoded_bytes=" + std::to_string(entry.decoded_byte_count)
                    + "; staging_bytes=" + std::to_string(entry.staging_payload_byte_count)
                    + "; payload_hash=" + std::to_string(entry.decoded_payload_hash);
            }
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

    std::map<std::uint64_t, bool> decoded_payload_hashes;
    for (const render_image_draw_list_frame_handoff_entry& handoff_entry : handoff.entries) {
        render_image_draw_list_texture_frame_composition_entry entry =
            make_render_image_draw_list_texture_frame_composition_entry(
                handoff_entry,
                batch_plan,
                frame,
                resource_packets);
        count_render_image_draw_list_texture_frame_composition_entry(composition, entry);
        if (entry.decoded_resource_evidence_present && entry.decoded_payload_hash != 0) {
            decoded_payload_hashes.emplace(entry.decoded_payload_hash, true);
        }
        composition.entries.push_back(std::move(entry));
    }

    composition.status = composition.handoff_entry_count == 0
        ? render_image_draw_list_texture_frame_composition_status::empty
        : (composition.has_blockers
            ? render_image_draw_list_texture_frame_composition_status::blocked
            : render_image_draw_list_texture_frame_composition_status::ready);
    composition.status_name = render_image_draw_list_texture_frame_composition_status_name(composition.status);
    composition.decoded_payload_hash_count = decoded_payload_hashes.size();
    composition.decoded_resource_summary =
        "decoded_resources=" + std::to_string(composition.decoded_resource_ready_count)
        + "; payload_hashes=" + std::to_string(composition.decoded_payload_hash_count)
        + "; decoded_bytes=" + std::to_string(composition.decoded_payload_byte_count)
        + "; staging_bytes=" + std::to_string(composition.staging_payload_byte_count);
    if (composition.decoded_resource_blocker_summary.empty()) {
        composition.decoded_resource_blocker_summary = "no decoded resource blockers";
    }

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
