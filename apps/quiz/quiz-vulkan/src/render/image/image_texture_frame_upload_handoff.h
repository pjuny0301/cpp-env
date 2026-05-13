#pragma once

#include "render/image/image_texture_frame_snapshot.h"
#include "render/image/image_texture_upload_result_diagnostics.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

enum class render_image_texture_frame_upload_handoff_entry_status {
    ready,
    placeholder,
    blocked,
    missing_upload_result,
    missing_frame_binding,
    removed,
};

inline std::string render_image_texture_frame_upload_handoff_entry_status_name(
    render_image_texture_frame_upload_handoff_entry_status status)
{
    switch (status) {
    case render_image_texture_frame_upload_handoff_entry_status::ready:
        return "ready";
    case render_image_texture_frame_upload_handoff_entry_status::placeholder:
        return "placeholder";
    case render_image_texture_frame_upload_handoff_entry_status::blocked:
        return "blocked";
    case render_image_texture_frame_upload_handoff_entry_status::missing_upload_result:
        return "missing_upload_result";
    case render_image_texture_frame_upload_handoff_entry_status::missing_frame_binding:
        return "missing_frame_binding";
    case render_image_texture_frame_upload_handoff_entry_status::removed:
        return "removed";
    }

    return "unknown";
}

inline bool render_image_texture_frame_upload_handoff_entry_status_is_blocked(
    render_image_texture_frame_upload_handoff_entry_status status)
{
    return status == render_image_texture_frame_upload_handoff_entry_status::blocked
        || status == render_image_texture_frame_upload_handoff_entry_status::missing_upload_result;
}

struct render_image_texture_frame_upload_handoff_entry {
    std::size_t sequence = 0;
    std::size_t request_index = 0;
    render_image_texture_frame_upload_handoff_entry_status status =
        render_image_texture_frame_upload_handoff_entry_status::missing_upload_result;
    std::string status_name = render_image_texture_frame_upload_handoff_entry_status_name(
        render_image_texture_frame_upload_handoff_entry_status::missing_upload_result);
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
    render_image_texture_upload_result_packet_status upload_result_status =
        render_image_texture_upload_result_packet_status::rejected_missing_snapshot;
    std::string upload_result_status_name = render_image_texture_upload_result_packet_status_name(
        render_image_texture_upload_result_packet_status::rejected_missing_snapshot);
    render_image_texture_upload_status upload_status = render_image_texture_upload_status::invalid_image;
    std::string upload_status_name = render_image_texture_upload_operation_upload_status_name(
        render_image_texture_upload_status::invalid_image);
    std::size_t mip_level_count = 0;
    std::size_t accepted_mip_level_count = 0;
    std::size_t rejected_mip_level_count = 0;
    std::size_t uploaded_byte_count = 0;
    std::size_t planned_staging_byte_count = 0;
    std::size_t planned_mipmap_byte_count = 0;
    bool requested = false;
    bool upload_result_present = false;
    bool ready = false;
    bool placeholder_texture = false;
    bool blocked = true;
    bool retryable_blocker = false;
    bool nonretryable_blocker = false;
    bool missing_upload_result = true;
    bool missing_frame_binding = false;
    bool removed = false;
    bool renderer_handoff_ready = false;
    bool cache_reused = false;
    bool expected_cache_reuse = false;
    bool added_in_frame = false;
    bool changed_in_frame = false;
    bool removed_from_frame = false;
    bool readiness_changed = false;
    bool readiness_regressed = false;
    bool readiness_recovered = false;
    bool residency_budget_pressure = false;
    render_image_texture_residency_budget_pressure_status residency_pressure_status =
        render_image_texture_residency_budget_pressure_status::within_budget;
    std::string residency_pressure_status_name =
        render_image_texture_residency_budget_pressure_status_name(
            render_image_texture_residency_budget_pressure_status::within_budget);
    std::string cache_key_summary;
    std::string sampler_summary;
    std::string retry_eligibility_name;
    std::size_t attempt_count_for_key = 0;
    std::size_t failed_attempt_count_for_key = 0;
    std::size_t retry_after_queue_sequence_delta = 0;
    std::size_t next_retry_sequence = 0;
    std::string blocker_summary;
    std::string diagnostic;

    bool ok() const
    {
        return !blocked;
    }
};

struct render_image_texture_frame_upload_handoff_summary {
    std::size_t frame_request_count = 0;
    std::size_t binding_packet_count = 0;
    std::size_t current_binding_packet_count = 0;
    std::size_t upload_packet_count = 0;
    std::size_t requested_texture_count = 0;
    std::size_t ready_texture_count = 0;
    std::size_t placeholder_texture_count = 0;
    std::size_t blocked_texture_count = 0;
    std::size_t removed_texture_count = 0;
    std::size_t missing_upload_result_count = 0;
    std::size_t missing_frame_binding_count = 0;
    std::size_t retry_blocker_count = 0;
    std::size_t nonretryable_blocker_count = 0;
    std::size_t cache_reused_count = 0;
    std::size_t expected_cache_reuse_count = 0;
    std::size_t unique_texture_id_count = 0;
    std::size_t unique_texture_cache_key_count = 0;
    std::size_t added_in_frame_count = 0;
    std::size_t changed_in_frame_count = 0;
    std::size_t removed_from_frame_count = 0;
    std::size_t readiness_changed_count = 0;
    std::size_t readiness_regressed_count = 0;
    std::size_t readiness_recovered_count = 0;
    std::size_t uploaded_byte_count = 0;
    std::size_t planned_staging_byte_count = 0;
    std::size_t planned_mipmap_byte_count = 0;
    std::size_t total_mip_level_count = 0;
    std::size_t accepted_mip_level_count = 0;
    std::size_t rejected_mip_level_count = 0;
    bool renderer_handoff_ready = false;
    bool has_placeholders = false;
    bool has_blockers = false;
    bool has_retry_blockers = false;
    bool has_frame_delta = false;
    std::vector<render_image_texture_frame_upload_handoff_entry> entries;
    std::string cache_key_summary;
    std::string sampler_summary;
    std::string retry_blocker_summary;
    std::string mip_level_summary;
    std::string frame_delta_summary;
    std::string diagnostic;

    bool ok() const
    {
        return renderer_handoff_ready && !has_blockers;
    }
};

inline void append_render_image_texture_frame_upload_handoff_summary_fragment(
    std::string& summary,
    std::string_view fragment)
{
    if (fragment.empty()) {
        return;
    }
    if (!summary.empty()) {
        summary += "; ";
    }
    summary += fragment;
}

inline void append_unique_render_image_texture_frame_upload_handoff_summary_fragment(
    std::map<std::string, bool>& seen,
    std::string& summary,
    const std::string& fragment)
{
    if (fragment.empty() || seen.contains(fragment)) {
        return;
    }
    seen.emplace(fragment, true);
    append_render_image_texture_frame_upload_handoff_summary_fragment(summary, fragment);
}

inline const render_image_texture_upload_result_packet_snapshot*
render_image_texture_upload_result_packet_for_stable_cache_key(
    const render_image_texture_upload_result_snapshot& upload_result,
    const std::string& stable_cache_key)
{
    for (const render_image_texture_upload_result_packet_snapshot& packet : upload_result.packets) {
        if (packet.stable_cache_key == stable_cache_key) {
            return &packet;
        }
    }
    return nullptr;
}

inline const render_image_texture_upload_result_packet_snapshot*
render_image_texture_frame_upload_result_packet_for_binding_packet(
    const render_image_texture_upload_result_snapshot& upload_result,
    const render_image_texture_frame_binding_packet& binding_packet)
{
    if (!binding_packet.stable_texture_cache_key.empty()) {
        if (const render_image_texture_upload_result_packet_snapshot* packet =
                render_image_texture_upload_result_packet_for_stable_cache_key(
                    upload_result,
                    binding_packet.stable_texture_cache_key);
            packet != nullptr) {
            return packet;
        }
    }

    if (binding_packet.texture_id != 0) {
        for (const render_image_texture_upload_result_packet_snapshot& packet : upload_result.packets) {
            if (packet.texture_id == binding_packet.texture_id) {
                return &packet;
            }
        }
    }

    return nullptr;
}

inline render_image_texture_frame_upload_handoff_entry_status
render_image_texture_frame_upload_handoff_entry_status_for(
    const render_image_texture_frame_binding_packet& binding_packet,
    const render_image_texture_upload_result_packet_snapshot* upload_packet)
{
    if (binding_packet.removed) {
        return render_image_texture_frame_upload_handoff_entry_status::removed;
    }
    if (upload_packet == nullptr) {
        return render_image_texture_frame_upload_handoff_entry_status::missing_upload_result;
    }
    if (binding_packet.failed || upload_packet->blocked || upload_packet->rejected) {
        return render_image_texture_frame_upload_handoff_entry_status::blocked;
    }
    if (binding_packet.placeholder_texture || upload_packet->placeholder_texture) {
        return render_image_texture_frame_upload_handoff_entry_status::placeholder;
    }
    return render_image_texture_frame_upload_handoff_entry_status::ready;
}

inline render_image_texture_frame_upload_handoff_entry make_render_image_texture_frame_upload_handoff_entry(
    const render_image_texture_frame_binding_packet& binding_packet,
    const render_image_texture_upload_result_packet_snapshot* upload_packet)
{
    const render_image_texture_frame_upload_handoff_entry_status status =
        render_image_texture_frame_upload_handoff_entry_status_for(binding_packet, upload_packet);
    render_image_texture_frame_upload_handoff_entry entry{
        .sequence = binding_packet.sequence,
        .request_index = binding_packet.request_index,
        .status = status,
        .status_name = render_image_texture_frame_upload_handoff_entry_status_name(status),
        .render_image_uri = binding_packet.render_image_uri,
        .normalized_uri = binding_packet.normalized_uri,
        .cache_key = binding_packet.cache_key,
        .source_kind = binding_packet.source_kind,
        .sampler = binding_packet.sampler,
        .sampler_key = binding_packet.sampler_key,
        .texture_key = binding_packet.texture_key,
        .stable_texture_cache_key = binding_packet.stable_texture_cache_key,
        .texture_id = binding_packet.texture_id,
        .texture_revision = binding_packet.texture_revision,
        .texture_width = binding_packet.texture_width,
        .texture_height = binding_packet.texture_height,
        .requested = !binding_packet.removed,
        .upload_result_present = upload_packet != nullptr,
        .ready = status == render_image_texture_frame_upload_handoff_entry_status::ready,
        .placeholder_texture = binding_packet.placeholder_texture,
        .blocked = render_image_texture_frame_upload_handoff_entry_status_is_blocked(status),
        .missing_upload_result =
            status == render_image_texture_frame_upload_handoff_entry_status::missing_upload_result,
        .missing_frame_binding = false,
        .removed = binding_packet.removed,
        .renderer_handoff_ready = binding_packet.renderer_handoff_ready,
        .cache_reused = binding_packet.cache_reused,
        .expected_cache_reuse = binding_packet.expected_cache_reuse,
        .added_in_frame = binding_packet.added_in_frame,
        .changed_in_frame = binding_packet.changed_in_frame,
        .removed_from_frame = binding_packet.removed_from_frame,
        .readiness_changed = binding_packet.readiness_changed,
        .readiness_regressed = binding_packet.readiness_regressed,
        .readiness_recovered = binding_packet.readiness_recovered,
        .residency_budget_pressure = binding_packet.residency_budget_pressure,
        .residency_pressure_status = binding_packet.residency_pressure_status,
        .residency_pressure_status_name = binding_packet.residency_pressure_status_name,
        .cache_key_summary = binding_packet.stable_texture_cache_key,
        .sampler_summary = binding_packet.sampler_key,
    };

    if (upload_packet != nullptr) {
        entry.upload_request_id = upload_packet->request_id;
        entry.upload_generation_id = upload_packet->generation_id;
        entry.upload_result_status = upload_packet->status;
        entry.upload_result_status_name = upload_packet->status_name;
        entry.upload_status = upload_packet->upload_status;
        entry.upload_status_name = upload_packet->upload_status_name;
        entry.mip_level_count = upload_packet->mip_level_count;
        entry.accepted_mip_level_count = upload_packet->accepted_mip_level_count;
        entry.rejected_mip_level_count = upload_packet->rejected_mip_level_count;
        entry.uploaded_byte_count = upload_packet->uploaded_byte_count;
        entry.planned_staging_byte_count = upload_packet->planned_staging_byte_count;
        entry.planned_mipmap_byte_count = upload_packet->planned_mipmap_byte_count;
        entry.placeholder_texture = entry.placeholder_texture || upload_packet->placeholder_texture;
        entry.retryable_blocker = upload_packet->retryable && upload_packet->rejected;
        entry.nonretryable_blocker = upload_packet->nonretryable_failure && upload_packet->rejected;
        entry.retry_eligibility_name = upload_packet->retry_eligibility_name;
        entry.attempt_count_for_key = upload_packet->attempt_count_for_key;
        entry.failed_attempt_count_for_key = upload_packet->failed_attempt_count_for_key;
        entry.retry_after_queue_sequence_delta = upload_packet->retry_after_queue_sequence_delta;
        entry.next_retry_sequence = upload_packet->next_retry_sequence;
        entry.blocker_summary = upload_packet->blocker_summary;
    } else {
        entry.blocker_summary = binding_packet.failed
            ? binding_packet.diagnostic
            : "upload result packet is missing";
    }

    if (entry.status == render_image_texture_frame_upload_handoff_entry_status::removed) {
        entry.blocked = false;
        entry.missing_upload_result = false;
        entry.diagnostic = "image frame upload handoff entry was removed from frame";
    } else if (entry.status == render_image_texture_frame_upload_handoff_entry_status::ready) {
        entry.diagnostic = "image frame upload handoff entry is ready";
    } else if (entry.status == render_image_texture_frame_upload_handoff_entry_status::placeholder) {
        entry.diagnostic = "image frame upload handoff entry uses placeholder texture";
    } else if (entry.status == render_image_texture_frame_upload_handoff_entry_status::missing_upload_result) {
        entry.diagnostic = "image frame upload handoff entry is missing upload result";
    } else if (entry.status == render_image_texture_frame_upload_handoff_entry_status::missing_frame_binding) {
        entry.diagnostic = "image frame upload handoff entry is missing frame binding";
    } else {
        entry.diagnostic = "image frame upload handoff entry is blocked: " + entry.blocker_summary;
    }

    return entry;
}

inline render_image_texture_frame_upload_handoff_entry make_render_image_texture_frame_upload_handoff_missing_binding_entry(
    const render_image_texture_upload_result_packet_snapshot& upload_packet,
    std::size_t request_index)
{
    return render_image_texture_frame_upload_handoff_entry{
        .request_index = request_index,
        .status = render_image_texture_frame_upload_handoff_entry_status::missing_frame_binding,
        .status_name = render_image_texture_frame_upload_handoff_entry_status_name(
            render_image_texture_frame_upload_handoff_entry_status::missing_frame_binding),
        .sampler = upload_packet.sampler,
        .sampler_key = upload_packet.sampler_summary,
        .texture_key = upload_packet.texture_key,
        .stable_texture_cache_key = upload_packet.stable_cache_key,
        .texture_id = upload_packet.texture_id,
        .texture_revision = upload_packet.texture_revision,
        .texture_width = upload_packet.texture_width,
        .texture_height = upload_packet.texture_height,
        .upload_request_id = upload_packet.request_id,
        .upload_generation_id = upload_packet.generation_id,
        .upload_result_status = upload_packet.status,
        .upload_result_status_name = upload_packet.status_name,
        .upload_status = upload_packet.upload_status,
        .upload_status_name = upload_packet.upload_status_name,
        .mip_level_count = upload_packet.mip_level_count,
        .accepted_mip_level_count = upload_packet.accepted_mip_level_count,
        .rejected_mip_level_count = upload_packet.rejected_mip_level_count,
        .uploaded_byte_count = upload_packet.uploaded_byte_count,
        .planned_staging_byte_count = upload_packet.planned_staging_byte_count,
        .planned_mipmap_byte_count = upload_packet.planned_mipmap_byte_count,
        .requested = false,
        .upload_result_present = true,
        .ready = false,
        .placeholder_texture = upload_packet.placeholder_texture,
        .blocked = false,
        .retryable_blocker = upload_packet.retryable && upload_packet.rejected,
        .nonretryable_blocker = upload_packet.nonretryable_failure && upload_packet.rejected,
        .missing_upload_result = false,
        .missing_frame_binding = true,
        .renderer_handoff_ready = false,
        .cache_key_summary = upload_packet.stable_cache_key,
        .sampler_summary = upload_packet.sampler_summary,
        .retry_eligibility_name = upload_packet.retry_eligibility_name,
        .attempt_count_for_key = upload_packet.attempt_count_for_key,
        .failed_attempt_count_for_key = upload_packet.failed_attempt_count_for_key,
        .retry_after_queue_sequence_delta = upload_packet.retry_after_queue_sequence_delta,
        .next_retry_sequence = upload_packet.next_retry_sequence,
        .blocker_summary = upload_packet.blocker_summary,
        .diagnostic = "image frame upload handoff entry is missing frame binding",
    };
}

inline void count_render_image_texture_frame_upload_handoff_entry(
    render_image_texture_frame_upload_handoff_summary& summary,
    const render_image_texture_frame_upload_handoff_entry& entry)
{
    if (entry.removed) {
        ++summary.removed_texture_count;
    } else if (entry.requested) {
        ++summary.requested_texture_count;
    }

    switch (entry.status) {
    case render_image_texture_frame_upload_handoff_entry_status::ready:
        ++summary.ready_texture_count;
        break;
    case render_image_texture_frame_upload_handoff_entry_status::placeholder:
        ++summary.placeholder_texture_count;
        summary.has_placeholders = true;
        break;
    case render_image_texture_frame_upload_handoff_entry_status::blocked:
        ++summary.blocked_texture_count;
        summary.has_blockers = true;
        if (entry.placeholder_texture) {
            ++summary.placeholder_texture_count;
            summary.has_placeholders = true;
        }
        break;
    case render_image_texture_frame_upload_handoff_entry_status::missing_upload_result:
        ++summary.blocked_texture_count;
        ++summary.missing_upload_result_count;
        summary.has_blockers = true;
        break;
    case render_image_texture_frame_upload_handoff_entry_status::missing_frame_binding:
        ++summary.missing_frame_binding_count;
        if (entry.placeholder_texture) {
            ++summary.placeholder_texture_count;
            summary.has_placeholders = true;
        }
        break;
    case render_image_texture_frame_upload_handoff_entry_status::removed:
        break;
    }

    if (entry.retryable_blocker) {
        ++summary.retry_blocker_count;
        summary.has_retry_blockers = true;
        append_render_image_texture_frame_upload_handoff_summary_fragment(
            summary.retry_blocker_summary,
            entry.blocker_summary);
    }
    if (entry.nonretryable_blocker) {
        ++summary.nonretryable_blocker_count;
    }
    if (entry.cache_reused) {
        ++summary.cache_reused_count;
    }
    if (entry.expected_cache_reuse) {
        ++summary.expected_cache_reuse_count;
    }
    if (entry.added_in_frame) {
        ++summary.added_in_frame_count;
    }
    if (entry.changed_in_frame) {
        ++summary.changed_in_frame_count;
    }
    if (entry.removed_from_frame) {
        ++summary.removed_from_frame_count;
    }
    if (entry.readiness_changed) {
        ++summary.readiness_changed_count;
    }
    if (entry.readiness_regressed) {
        ++summary.readiness_regressed_count;
    }
    if (entry.readiness_recovered) {
        ++summary.readiness_recovered_count;
    }
}

inline void finalize_render_image_texture_frame_upload_handoff_summary(
    render_image_texture_frame_upload_handoff_summary& summary,
    const render_image_texture_frame_binding_plan& binding_plan,
    const render_image_texture_upload_result_snapshot& upload_result)
{
    summary.uploaded_byte_count = upload_result.total_uploaded_byte_count;
    summary.planned_staging_byte_count = upload_result.total_planned_staging_byte_count;
    summary.planned_mipmap_byte_count = upload_result.total_planned_mipmap_byte_count;
    summary.total_mip_level_count = upload_result.total_mip_level_count;
    summary.accepted_mip_level_count = upload_result.accepted_mip_level_count;
    summary.rejected_mip_level_count = upload_result.rejected_mip_level_count;
    summary.has_frame_delta = summary.added_in_frame_count != 0
        || summary.changed_in_frame_count != 0
        || summary.removed_from_frame_count != 0
        || summary.readiness_changed_count != 0;
    summary.renderer_handoff_ready = binding_plan.renderer_handoff_ready
        && summary.blocked_texture_count == 0
        && summary.missing_upload_result_count == 0;

    std::map<render_image_texture_id, bool> unique_texture_ids;
    std::map<std::string, bool> unique_texture_cache_keys;
    std::map<std::string, bool> unique_sampler_keys;
    for (const render_image_texture_frame_upload_handoff_entry& entry : summary.entries) {
        if (!entry.removed && entry.texture_id != 0) {
            unique_texture_ids.emplace(entry.texture_id, true);
        }
        if (!entry.removed) {
            append_unique_render_image_texture_frame_upload_handoff_summary_fragment(
                unique_texture_cache_keys,
                summary.cache_key_summary,
                entry.stable_texture_cache_key);
            append_unique_render_image_texture_frame_upload_handoff_summary_fragment(
                unique_sampler_keys,
                summary.sampler_summary,
                entry.sampler_summary);
        }
    }

    summary.unique_texture_id_count = unique_texture_ids.size();
    summary.unique_texture_cache_key_count = unique_texture_cache_keys.size();
    if (summary.cache_key_summary.empty()) {
        summary.cache_key_summary = "no texture cache keys";
    }
    if (summary.sampler_summary.empty()) {
        summary.sampler_summary = "no sampler policies";
    }
    if (summary.retry_blocker_summary.empty()) {
        summary.retry_blocker_summary = "no retry blockers";
    }

    summary.mip_level_summary = "accepted_mips=" + std::to_string(summary.accepted_mip_level_count)
        + "; rejected_mips=" + std::to_string(summary.rejected_mip_level_count)
        + "; uploaded_bytes=" + std::to_string(summary.uploaded_byte_count);

    if (summary.has_frame_delta) {
        summary.frame_delta_summary = "added=" + std::to_string(summary.added_in_frame_count)
            + "; changed=" + std::to_string(summary.changed_in_frame_count)
            + "; removed=" + std::to_string(summary.removed_from_frame_count)
            + "; readiness_changed=" + std::to_string(summary.readiness_changed_count);
    } else {
        summary.frame_delta_summary = "image frame upload handoff has no frame delta";
    }

    if (summary.binding_packet_count == 0 && summary.missing_frame_binding_count != 0) {
        summary.diagnostic = "image frame upload handoff has upload results without frame bindings";
    } else if (summary.binding_packet_count == 0) {
        summary.diagnostic = "image frame upload handoff has no texture requests";
    } else if (summary.has_blockers) {
        summary.diagnostic = "image frame upload handoff has blocked texture uploads";
    } else if (summary.has_placeholders) {
        summary.diagnostic = "image frame upload handoff ready with placeholder textures";
    } else if (summary.has_frame_delta) {
        summary.diagnostic = "image frame upload handoff ready with frame delta";
    } else {
        summary.diagnostic = "image frame upload handoff ready";
    }
}

inline render_image_texture_frame_upload_handoff_summary make_render_image_texture_frame_upload_handoff_summary(
    const render_image_texture_frame_snapshot& frame,
    const render_image_texture_frame_binding_plan& binding_plan,
    const render_image_texture_upload_result_snapshot& upload_result)
{
    render_image_texture_frame_upload_handoff_summary summary{
        .frame_request_count = frame.request_count,
        .binding_packet_count = binding_plan.packet_count,
        .current_binding_packet_count = binding_plan.current_packet_count,
        .upload_packet_count = upload_result.packet_count,
    };

    std::map<std::size_t, bool> matched_upload_packet_indexes;
    for (const render_image_texture_frame_binding_packet& binding_packet : binding_plan.packets) {
        const render_image_texture_upload_result_packet_snapshot* upload_packet =
            render_image_texture_frame_upload_result_packet_for_binding_packet(upload_result, binding_packet);
        if (upload_packet != nullptr) {
            matched_upload_packet_indexes.emplace(upload_packet->packet_index, true);
        }
        render_image_texture_frame_upload_handoff_entry entry =
            make_render_image_texture_frame_upload_handoff_entry(binding_packet, upload_packet);
        count_render_image_texture_frame_upload_handoff_entry(summary, entry);
        summary.entries.push_back(std::move(entry));
    }

    for (const render_image_texture_upload_result_packet_snapshot& upload_packet : upload_result.packets) {
        if (matched_upload_packet_indexes.contains(upload_packet.packet_index)) {
            continue;
        }
        render_image_texture_frame_upload_handoff_entry entry =
            make_render_image_texture_frame_upload_handoff_missing_binding_entry(
                upload_packet,
                frame.request_count + upload_packet.packet_index);
        count_render_image_texture_frame_upload_handoff_entry(summary, entry);
        summary.entries.push_back(std::move(entry));
    }

    finalize_render_image_texture_frame_upload_handoff_summary(summary, binding_plan, upload_result);
    return summary;
}

inline render_image_texture_frame_upload_handoff_summary make_render_image_texture_frame_upload_handoff_summary(
    const render_image_texture_frame_snapshot& frame,
    const render_image_texture_upload_result_snapshot& upload_result)
{
    return make_render_image_texture_frame_upload_handoff_summary(
        frame,
        make_render_image_texture_frame_binding_plan(frame),
        upload_result);
}

inline render_image_texture_frame_upload_handoff_summary make_render_image_texture_frame_upload_handoff_summary(
    const render_image_texture_frame_snapshot& before,
    const render_image_texture_frame_snapshot& after,
    const render_image_texture_upload_result_snapshot& upload_result)
{
    return make_render_image_texture_frame_upload_handoff_summary(
        after,
        make_render_image_texture_frame_binding_plan(before, after),
        upload_result);
}

enum class render_image_texture_frame_upload_handoff_diff_entry_status {
    unchanged,
    added,
    removed,
    changed,
};

inline std::string render_image_texture_frame_upload_handoff_diff_entry_status_name(
    render_image_texture_frame_upload_handoff_diff_entry_status status)
{
    switch (status) {
    case render_image_texture_frame_upload_handoff_diff_entry_status::unchanged:
        return "unchanged";
    case render_image_texture_frame_upload_handoff_diff_entry_status::added:
        return "added";
    case render_image_texture_frame_upload_handoff_diff_entry_status::removed:
        return "removed";
    case render_image_texture_frame_upload_handoff_diff_entry_status::changed:
        return "changed";
    }

    return "unknown";
}

struct render_image_texture_frame_upload_handoff_entry_diff {
    std::size_t request_index = 0;
    render_image_texture_frame_upload_handoff_diff_entry_status status =
        render_image_texture_frame_upload_handoff_diff_entry_status::unchanged;
    std::string status_name = render_image_texture_frame_upload_handoff_diff_entry_status_name(
        render_image_texture_frame_upload_handoff_diff_entry_status::unchanged);
    bool before_present = false;
    bool after_present = false;
    render_image_texture_frame_upload_handoff_entry_status before_handoff_status =
        render_image_texture_frame_upload_handoff_entry_status::missing_upload_result;
    render_image_texture_frame_upload_handoff_entry_status after_handoff_status =
        render_image_texture_frame_upload_handoff_entry_status::missing_upload_result;
    std::string before_handoff_status_name;
    std::string after_handoff_status_name;
    std::string before_render_image_uri;
    std::string after_render_image_uri;
    render_image_cache_key before_cache_key;
    render_image_cache_key after_cache_key;
    std::string before_stable_texture_cache_key;
    std::string after_stable_texture_cache_key;
    std::string before_sampler_summary;
    std::string after_sampler_summary;
    render_image_texture_id before_texture_id = 0;
    render_image_texture_id after_texture_id = 0;
    std::uint64_t before_upload_request_id = 0;
    std::uint64_t after_upload_request_id = 0;
    std::size_t before_mip_level_count = 0;
    std::size_t after_mip_level_count = 0;
    std::int64_t mip_level_count_delta = 0;
    std::size_t before_uploaded_byte_count = 0;
    std::size_t after_uploaded_byte_count = 0;
    std::int64_t uploaded_byte_delta = 0;
    bool before_ready = false;
    bool after_ready = false;
    bool before_placeholder_texture = false;
    bool after_placeholder_texture = false;
    bool before_blocked = false;
    bool after_blocked = false;
    bool before_retryable_blocker = false;
    bool after_retryable_blocker = false;
    bool before_missing_frame_binding = false;
    bool after_missing_frame_binding = false;
    bool before_removed = false;
    bool after_removed = false;
    bool readiness_changed = false;
    bool readiness_regressed = false;
    bool readiness_recovered = false;
    bool placeholder_changed = false;
    bool blocker_changed = false;
    bool retry_blocker_changed = false;
    bool cache_key_changed = false;
    bool sampler_changed = false;
    bool texture_changed = false;
    bool upload_request_changed = false;
    bool uploaded_byte_changed = false;
    bool mip_level_changed = false;
    bool regression = false;
    bool recovery = false;
    std::string diagnostic;

    bool changed() const
    {
        return status != render_image_texture_frame_upload_handoff_diff_entry_status::unchanged;
    }

    bool ok() const
    {
        return !regression;
    }
};

struct render_image_texture_frame_upload_handoff_summary_diff {
    std::size_t before_requested_texture_count = 0;
    std::size_t after_requested_texture_count = 0;
    std::int64_t requested_texture_delta = 0;
    std::size_t before_ready_texture_count = 0;
    std::size_t after_ready_texture_count = 0;
    std::int64_t ready_texture_delta = 0;
    std::size_t before_placeholder_texture_count = 0;
    std::size_t after_placeholder_texture_count = 0;
    std::int64_t placeholder_texture_delta = 0;
    std::size_t before_blocked_texture_count = 0;
    std::size_t after_blocked_texture_count = 0;
    std::int64_t blocked_texture_delta = 0;
    std::size_t before_retry_blocker_count = 0;
    std::size_t after_retry_blocker_count = 0;
    std::int64_t retry_blocker_delta = 0;
    std::size_t before_uploaded_byte_count = 0;
    std::size_t after_uploaded_byte_count = 0;
    std::int64_t uploaded_byte_delta = 0;
    std::size_t before_mip_level_count = 0;
    std::size_t after_mip_level_count = 0;
    std::int64_t mip_level_delta = 0;
    std::size_t unchanged_entry_count = 0;
    std::size_t added_entry_count = 0;
    std::size_t removed_entry_count = 0;
    std::size_t changed_entry_count = 0;
    std::size_t readiness_changed_count = 0;
    std::size_t readiness_regressed_count = 0;
    std::size_t readiness_recovered_count = 0;
    std::size_t placeholder_changed_count = 0;
    std::size_t blocker_changed_count = 0;
    std::size_t retry_blocker_changed_count = 0;
    std::size_t cache_key_changed_count = 0;
    std::size_t sampler_changed_count = 0;
    std::size_t texture_changed_count = 0;
    bool before_renderer_handoff_ready = false;
    bool after_renderer_handoff_ready = false;
    bool renderer_handoff_regressed = false;
    bool renderer_handoff_recovered = false;
    bool has_changes = false;
    bool has_regression = false;
    bool has_recovery = false;
    std::string changed_summary;
    std::string regression_summary;
    std::vector<render_image_texture_frame_upload_handoff_entry_diff> entries;
    std::string diagnostic;

    bool ok() const
    {
        return !has_regression;
    }
};

inline const render_image_texture_frame_upload_handoff_entry*
render_image_texture_frame_upload_handoff_entry_for_request_index(
    const render_image_texture_frame_upload_handoff_summary& summary,
    std::size_t request_index)
{
    for (const render_image_texture_frame_upload_handoff_entry& entry : summary.entries) {
        if (entry.request_index == request_index) {
            return &entry;
        }
    }
    return nullptr;
}

inline void append_render_image_texture_frame_upload_handoff_request_indexes(
    std::map<std::size_t, bool>& request_indexes,
    const render_image_texture_frame_upload_handoff_summary& summary)
{
    for (const render_image_texture_frame_upload_handoff_entry& entry : summary.entries) {
        request_indexes.emplace(entry.request_index, true);
    }
}

inline bool render_image_texture_frame_upload_handoff_entry_equal(
    const render_image_texture_frame_upload_handoff_entry& before,
    const render_image_texture_frame_upload_handoff_entry& after)
{
    return before.status == after.status
        && before.render_image_uri == after.render_image_uri
        && before.normalized_uri == after.normalized_uri
        && before.cache_key == after.cache_key
        && before.sampler == after.sampler
        && before.sampler_summary == after.sampler_summary
        && before.stable_texture_cache_key == after.stable_texture_cache_key
        && before.texture_id == after.texture_id
        && before.texture_revision == after.texture_revision
        && before.upload_request_id == after.upload_request_id
        && before.mip_level_count == after.mip_level_count
        && before.uploaded_byte_count == after.uploaded_byte_count
        && before.ready == after.ready
        && before.placeholder_texture == after.placeholder_texture
        && before.blocked == after.blocked
        && before.retryable_blocker == after.retryable_blocker
        && before.removed == after.removed;
}

inline render_image_texture_frame_upload_handoff_entry_diff
make_render_image_texture_frame_upload_handoff_entry_diff(
    const render_image_texture_frame_upload_handoff_entry* before,
    const render_image_texture_frame_upload_handoff_entry* after,
    std::size_t request_index)
{
    render_image_texture_frame_upload_handoff_entry_diff diff{
        .request_index = request_index,
        .before_present = before != nullptr,
        .after_present = after != nullptr,
    };

    if (before != nullptr) {
        diff.before_handoff_status = before->status;
        diff.before_handoff_status_name = before->status_name;
        diff.before_render_image_uri = before->render_image_uri;
        diff.before_cache_key = before->cache_key;
        diff.before_stable_texture_cache_key = before->stable_texture_cache_key;
        diff.before_sampler_summary = before->sampler_summary;
        diff.before_texture_id = before->texture_id;
        diff.before_upload_request_id = before->upload_request_id;
        diff.before_mip_level_count = before->mip_level_count;
        diff.before_uploaded_byte_count = before->uploaded_byte_count;
        diff.before_ready = before->ready;
        diff.before_placeholder_texture = before->placeholder_texture;
        diff.before_blocked = before->blocked;
        diff.before_retryable_blocker = before->retryable_blocker;
        diff.before_missing_frame_binding = before->missing_frame_binding;
        diff.before_removed = before->removed;
    }

    if (after != nullptr) {
        diff.after_handoff_status = after->status;
        diff.after_handoff_status_name = after->status_name;
        diff.after_render_image_uri = after->render_image_uri;
        diff.after_cache_key = after->cache_key;
        diff.after_stable_texture_cache_key = after->stable_texture_cache_key;
        diff.after_sampler_summary = after->sampler_summary;
        diff.after_texture_id = after->texture_id;
        diff.after_upload_request_id = after->upload_request_id;
        diff.after_mip_level_count = after->mip_level_count;
        diff.after_uploaded_byte_count = after->uploaded_byte_count;
        diff.after_ready = after->ready;
        diff.after_placeholder_texture = after->placeholder_texture;
        diff.after_blocked = after->blocked;
        diff.after_retryable_blocker = after->retryable_blocker;
        diff.after_missing_frame_binding = after->missing_frame_binding;
        diff.after_removed = after->removed;
    }

    diff.mip_level_count_delta =
        render_image_texture_upload_result_size_delta(diff.before_mip_level_count, diff.after_mip_level_count);
    diff.uploaded_byte_delta =
        render_image_texture_upload_result_size_delta(diff.before_uploaded_byte_count, diff.after_uploaded_byte_count);

    if (before == nullptr && after != nullptr) {
        diff.status = render_image_texture_frame_upload_handoff_diff_entry_status::added;
        diff.readiness_changed = after->ready || after->placeholder_texture || after->blocked;
        diff.placeholder_changed = after->placeholder_texture;
        diff.blocker_changed = after->blocked;
        diff.retry_blocker_changed = after->retryable_blocker;
        diff.texture_changed = after->texture_id != 0;
        diff.upload_request_changed = after->upload_request_id != 0;
        diff.uploaded_byte_changed = after->uploaded_byte_count != 0;
        diff.mip_level_changed = after->mip_level_count != 0;
        diff.regression = after->blocked || after->placeholder_texture;
        diff.recovery = after->ready;
    } else if (before != nullptr && after == nullptr) {
        diff.status = render_image_texture_frame_upload_handoff_diff_entry_status::removed;
        diff.readiness_changed = before->ready || before->placeholder_texture || before->blocked;
        diff.readiness_regressed = before->ready || before->placeholder_texture;
        diff.placeholder_changed = before->placeholder_texture;
        diff.blocker_changed = before->blocked;
        diff.retry_blocker_changed = before->retryable_blocker;
        diff.texture_changed = before->texture_id != 0;
        diff.upload_request_changed = before->upload_request_id != 0;
        diff.uploaded_byte_changed = before->uploaded_byte_count != 0;
        diff.mip_level_changed = before->mip_level_count != 0;
        diff.regression = before->ready || before->placeholder_texture;
    } else if (before != nullptr && after != nullptr) {
        diff.readiness_changed = before->ready != after->ready
            || before->placeholder_texture != after->placeholder_texture
            || before->blocked != after->blocked;
        diff.readiness_regressed = (before->ready || before->placeholder_texture) && after->blocked;
        diff.readiness_recovered = before->blocked && (after->ready || after->placeholder_texture);
        diff.placeholder_changed = before->placeholder_texture != after->placeholder_texture;
        diff.blocker_changed = before->blocked != after->blocked;
        diff.retry_blocker_changed = before->retryable_blocker != after->retryable_blocker;
        diff.cache_key_changed = before->stable_texture_cache_key != after->stable_texture_cache_key
            || before->cache_key != after->cache_key;
        diff.sampler_changed = before->sampler_summary != after->sampler_summary
            || !(before->sampler == after->sampler);
        diff.texture_changed = before->texture_id != after->texture_id
            || before->texture_revision != after->texture_revision;
        diff.upload_request_changed = before->upload_request_id != after->upload_request_id;
        diff.uploaded_byte_changed = before->uploaded_byte_count != after->uploaded_byte_count;
        diff.mip_level_changed = before->mip_level_count != after->mip_level_count;
        diff.regression = diff.readiness_regressed
            || (!before->placeholder_texture && after->placeholder_texture)
            || (!before->blocked && after->blocked);
        diff.recovery = diff.readiness_recovered || (before->placeholder_texture && after->ready);
        if (!render_image_texture_frame_upload_handoff_entry_equal(*before, *after)
            || diff.cache_key_changed || diff.sampler_changed || diff.texture_changed
            || diff.upload_request_changed || diff.uploaded_byte_changed || diff.mip_level_changed) {
            diff.status = render_image_texture_frame_upload_handoff_diff_entry_status::changed;
        }
    }

    diff.status_name = render_image_texture_frame_upload_handoff_diff_entry_status_name(diff.status);
    diff.diagnostic = diff.status == render_image_texture_frame_upload_handoff_diff_entry_status::unchanged
        ? "image frame upload handoff entry unchanged"
        : (diff.regression
            ? "image frame upload handoff entry changed with regression"
            : "image frame upload handoff entry changed");
    return diff;
}

inline void count_render_image_texture_frame_upload_handoff_diff_entry(
    render_image_texture_frame_upload_handoff_summary_diff& diff,
    const render_image_texture_frame_upload_handoff_entry_diff& entry)
{
    switch (entry.status) {
    case render_image_texture_frame_upload_handoff_diff_entry_status::unchanged:
        ++diff.unchanged_entry_count;
        break;
    case render_image_texture_frame_upload_handoff_diff_entry_status::added:
        ++diff.added_entry_count;
        break;
    case render_image_texture_frame_upload_handoff_diff_entry_status::removed:
        ++diff.removed_entry_count;
        break;
    case render_image_texture_frame_upload_handoff_diff_entry_status::changed:
        ++diff.changed_entry_count;
        break;
    }
    if (entry.readiness_changed) {
        ++diff.readiness_changed_count;
    }
    if (entry.readiness_regressed) {
        ++diff.readiness_regressed_count;
    }
    if (entry.readiness_recovered) {
        ++diff.readiness_recovered_count;
    }
    if (entry.placeholder_changed) {
        ++diff.placeholder_changed_count;
    }
    if (entry.blocker_changed) {
        ++diff.blocker_changed_count;
    }
    if (entry.retry_blocker_changed) {
        ++diff.retry_blocker_changed_count;
    }
    if (entry.cache_key_changed) {
        ++diff.cache_key_changed_count;
    }
    if (entry.sampler_changed) {
        ++diff.sampler_changed_count;
    }
    if (entry.texture_changed) {
        ++diff.texture_changed_count;
    }
    diff.has_changes = diff.has_changes || entry.changed();
    diff.has_regression = diff.has_regression || entry.regression;
    diff.has_recovery = diff.has_recovery || entry.recovery;
}

inline render_image_texture_frame_upload_handoff_summary_diff
diff_render_image_texture_frame_upload_handoff_summaries(
    const render_image_texture_frame_upload_handoff_summary& before,
    const render_image_texture_frame_upload_handoff_summary& after)
{
    render_image_texture_frame_upload_handoff_summary_diff diff{
        .before_requested_texture_count = before.requested_texture_count,
        .after_requested_texture_count = after.requested_texture_count,
        .requested_texture_delta = render_image_texture_upload_result_size_delta(
            before.requested_texture_count,
            after.requested_texture_count),
        .before_ready_texture_count = before.ready_texture_count,
        .after_ready_texture_count = after.ready_texture_count,
        .ready_texture_delta = render_image_texture_upload_result_size_delta(
            before.ready_texture_count,
            after.ready_texture_count),
        .before_placeholder_texture_count = before.placeholder_texture_count,
        .after_placeholder_texture_count = after.placeholder_texture_count,
        .placeholder_texture_delta = render_image_texture_upload_result_size_delta(
            before.placeholder_texture_count,
            after.placeholder_texture_count),
        .before_blocked_texture_count = before.blocked_texture_count,
        .after_blocked_texture_count = after.blocked_texture_count,
        .blocked_texture_delta = render_image_texture_upload_result_size_delta(
            before.blocked_texture_count,
            after.blocked_texture_count),
        .before_retry_blocker_count = before.retry_blocker_count,
        .after_retry_blocker_count = after.retry_blocker_count,
        .retry_blocker_delta = render_image_texture_upload_result_size_delta(
            before.retry_blocker_count,
            after.retry_blocker_count),
        .before_uploaded_byte_count = before.uploaded_byte_count,
        .after_uploaded_byte_count = after.uploaded_byte_count,
        .uploaded_byte_delta = render_image_texture_upload_result_size_delta(
            before.uploaded_byte_count,
            after.uploaded_byte_count),
        .before_mip_level_count = before.total_mip_level_count,
        .after_mip_level_count = after.total_mip_level_count,
        .mip_level_delta = render_image_texture_upload_result_size_delta(
            before.total_mip_level_count,
            after.total_mip_level_count),
        .before_renderer_handoff_ready = before.renderer_handoff_ready,
        .after_renderer_handoff_ready = after.renderer_handoff_ready,
    };
    diff.renderer_handoff_regressed = before.renderer_handoff_ready && !after.renderer_handoff_ready;
    diff.renderer_handoff_recovered = !before.renderer_handoff_ready && after.renderer_handoff_ready;

    std::map<std::size_t, bool> request_indexes;
    append_render_image_texture_frame_upload_handoff_request_indexes(request_indexes, before);
    append_render_image_texture_frame_upload_handoff_request_indexes(request_indexes, after);

    for (const auto& [request_index, _] : request_indexes) {
        render_image_texture_frame_upload_handoff_entry_diff entry =
            make_render_image_texture_frame_upload_handoff_entry_diff(
                render_image_texture_frame_upload_handoff_entry_for_request_index(before, request_index),
                render_image_texture_frame_upload_handoff_entry_for_request_index(after, request_index),
                request_index);
        count_render_image_texture_frame_upload_handoff_diff_entry(diff, entry);
        diff.entries.push_back(std::move(entry));
    }

    diff.has_changes = diff.has_changes
        || diff.requested_texture_delta != 0
        || diff.ready_texture_delta != 0
        || diff.placeholder_texture_delta != 0
        || diff.blocked_texture_delta != 0
        || diff.retry_blocker_delta != 0
        || diff.uploaded_byte_delta != 0
        || diff.mip_level_delta != 0
        || diff.renderer_handoff_regressed
        || diff.renderer_handoff_recovered;
    diff.has_regression = diff.has_regression
        || diff.renderer_handoff_regressed
        || diff.ready_texture_delta < 0
        || diff.blocked_texture_delta > 0
        || diff.placeholder_texture_delta > 0
        || diff.retry_blocker_delta > 0;
    diff.has_recovery = diff.has_recovery
        || diff.renderer_handoff_recovered
        || diff.ready_texture_delta > 0
        || diff.blocked_texture_delta < 0
        || diff.retry_blocker_delta < 0;

    if (diff.added_entry_count != 0) {
        append_render_image_texture_frame_upload_handoff_summary_fragment(
            diff.changed_summary,
            "added=" + std::to_string(diff.added_entry_count));
    }
    if (diff.removed_entry_count != 0) {
        append_render_image_texture_frame_upload_handoff_summary_fragment(
            diff.changed_summary,
            "removed=" + std::to_string(diff.removed_entry_count));
    }
    if (diff.changed_entry_count != 0) {
        append_render_image_texture_frame_upload_handoff_summary_fragment(
            diff.changed_summary,
            "changed=" + std::to_string(diff.changed_entry_count));
    }
    if (diff.changed_summary.empty()) {
        diff.changed_summary = diff.has_changes
            ? "image frame upload handoff aggregate counts changed"
            : "image frame upload handoff diff has no changes";
    }

    if (diff.renderer_handoff_regressed) {
        append_render_image_texture_frame_regression_reason(diff.regression_summary, "upload handoff regressed");
    }
    if (diff.ready_texture_delta < 0) {
        append_render_image_texture_frame_regression_reason(diff.regression_summary, "ready uploads decreased");
    }
    if (diff.blocked_texture_delta > 0) {
        append_render_image_texture_frame_regression_reason(diff.regression_summary, "blocked uploads increased");
    }
    if (diff.placeholder_texture_delta > 0) {
        append_render_image_texture_frame_regression_reason(diff.regression_summary, "placeholder uploads increased");
    }
    if (diff.retry_blocker_delta > 0) {
        append_render_image_texture_frame_regression_reason(diff.regression_summary, "retry blockers increased");
    }
    if (diff.has_regression && diff.regression_summary.empty()) {
        diff.regression_summary = "image frame upload handoff regressed";
    }
    if (diff.regression_summary.empty()) {
        diff.regression_summary = diff.has_changes
            ? "image frame upload handoff diff has changes without regressions"
            : "image frame upload handoff diff has no changes";
    }

    diff.diagnostic = diff.has_regression
        ? "image frame upload handoff diff reports regressions"
        : (diff.has_changes
            ? "image frame upload handoff diff reports changes"
            : "image frame upload handoff diff is unchanged");
    return diff;
}

inline render_image_texture_frame_upload_handoff_summary_diff
diff_render_image_texture_frame_upload_handoff_summaries(
    const render_image_texture_frame_snapshot& before_frame,
    const render_image_texture_frame_snapshot& after_frame,
    const render_image_texture_upload_result_snapshot& before_upload_result,
    const render_image_texture_upload_result_snapshot& after_upload_result)
{
    return diff_render_image_texture_frame_upload_handoff_summaries(
        make_render_image_texture_frame_upload_handoff_summary(before_frame, before_upload_result),
        make_render_image_texture_frame_upload_handoff_summary(after_frame, after_upload_result));
}

} // namespace quiz_vulkan::render

#include "render/image/image_texture_frame_binding_summary.h"
