#pragma once

#include "render/image/image_texture_placeholder_policy.h"
#include "render/image/image_texture_upload_snapshot_diff.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace quiz_vulkan::render {

enum class render_image_texture_upload_operation_packet_status {
    ready,
    placeholder_ready,
    blocked_retryable,
    blocked_nonretryable,
    blocked_invalid_mipmap_plan,
    blocked_missing_snapshot,
    blocked_missing_texture_handle,
};

inline std::string render_image_texture_upload_operation_packet_status_name(
    render_image_texture_upload_operation_packet_status status)
{
    switch (status) {
    case render_image_texture_upload_operation_packet_status::ready:
        return "ready";
    case render_image_texture_upload_operation_packet_status::placeholder_ready:
        return "placeholder_ready";
    case render_image_texture_upload_operation_packet_status::blocked_retryable:
        return "blocked_retryable";
    case render_image_texture_upload_operation_packet_status::blocked_nonretryable:
        return "blocked_nonretryable";
    case render_image_texture_upload_operation_packet_status::blocked_invalid_mipmap_plan:
        return "blocked_invalid_mipmap_plan";
    case render_image_texture_upload_operation_packet_status::blocked_missing_snapshot:
        return "blocked_missing_snapshot";
    case render_image_texture_upload_operation_packet_status::blocked_missing_texture_handle:
        return "blocked_missing_texture_handle";
    }

    return "unknown";
}

inline std::string render_image_texture_upload_operation_upload_status_name(
    render_image_texture_upload_status status)
{
    switch (status) {
    case render_image_texture_upload_status::uploaded:
        return "uploaded";
    case render_image_texture_upload_status::invalid_key:
        return "invalid_key";
    case render_image_texture_upload_status::invalid_sampler:
        return "invalid_sampler";
    case render_image_texture_upload_status::unsupported_format:
        return "unsupported_format";
    case render_image_texture_upload_status::invalid_image:
        return "invalid_image";
    }

    return "unknown";
}

struct render_image_texture_upload_operation_packet {
    std::size_t packet_index = 0;
    std::uint64_t generation_id = 0;
    render_image_texture_upload_operation_packet_status status =
        render_image_texture_upload_operation_packet_status::blocked_missing_snapshot;
    std::string status_name = render_image_texture_upload_operation_packet_status_name(
        render_image_texture_upload_operation_packet_status::blocked_missing_snapshot);
    render_image_texture_upload_status upload_status =
        render_image_texture_upload_status::invalid_image;
    std::string upload_status_name = render_image_texture_upload_operation_upload_status_name(
        render_image_texture_upload_status::invalid_image);
    render_image_texture_key texture_key;
    render_image_texture_handle texture;
    render_image_sampler_policy sampler;
    render_image_texture_mipmap_upload_plan mipmap_upload_plan;
    render_image_decoded_payload_evidence decoded_payload;
    render_image_texture_upload_payload_layout_evidence payload_layout;
    std::size_t staging_byte_count = 0;
    std::size_t mip_level_count = 0;
    std::size_t mipmap_byte_count = 0;
    std::string retry_eligibility_name = fake_image_texture_upload_retry_eligibility_name(
        fake_image_texture_upload_retry_eligibility::not_needed);
    std::size_t attempt_count_for_key = 0;
    std::size_t failed_attempt_count_for_key = 0;
    std::size_t retry_after_queue_sequence_delta = 0;
    std::size_t next_retry_sequence = 0;
    bool request_snapshot_present = false;
    bool result_snapshot_present = false;
    bool queue_snapshot_present = false;
    bool has_texture_handle = false;
    bool placeholder_texture = false;
    bool fallback_texture = false;
    bool retryable = false;
    bool nonretryable_failure = false;
    bool ready_for_upload = false;
    bool blocked = true;
    std::string readiness_summary;
    std::string blocker_summary;
    std::string diagnostic;

    bool ok() const
    {
        return ready_for_upload && !blocked;
    }
};

struct render_image_texture_upload_operation_plan {
    std::size_t upload_count = 0;
    std::size_t request_snapshot_count = 0;
    std::size_t result_snapshot_count = 0;
    std::size_t queue_snapshot_count = 0;
    std::size_t packet_count = 0;
    std::size_t ready_packet_count = 0;
    std::size_t placeholder_packet_count = 0;
    std::size_t fallback_packet_count = 0;
    std::size_t blocked_packet_count = 0;
    std::size_t retryable_blocked_packet_count = 0;
    std::size_t nonretryable_blocked_packet_count = 0;
    std::size_t invalid_mipmap_plan_packet_count = 0;
    std::size_t overflow_mipmap_plan_packet_count = 0;
    std::size_t unsupported_mipmap_plan_packet_count = 0;
    std::size_t missing_snapshot_packet_count = 0;
    std::size_t missing_texture_handle_packet_count = 0;
    std::size_t total_staging_byte_count = 0;
    std::size_t total_mip_level_count = 0;
    std::size_t total_mipmap_byte_count = 0;
    bool all_packets_ready = true;
    bool has_blockers = false;
    std::string blocker_summary;
    std::vector<render_image_texture_upload_operation_packet> packets;
    std::string diagnostic;

    bool ok() const
    {
        return all_packets_ready && !has_blockers;
    }
};

inline void append_render_image_texture_upload_operation_blocker(
    std::string& summary,
    const std::string& blocker)
{
    if (blocker.empty()) {
        return;
    }
    if (!summary.empty()) {
        summary += "; ";
    }
    summary += blocker;
}

inline const render_image_texture_mipmap_upload_plan& render_image_texture_upload_operation_plan_for(
    const fake_image_texture_upload_request_snapshot* request,
    const fake_image_texture_upload_result_snapshot* result,
    const fake_image_texture_upload_queue_entry_snapshot* queue_entry)
{
    if (result != nullptr) {
        return result->mipmap_upload_plan;
    }
    if (queue_entry != nullptr) {
        return queue_entry->mipmap_upload_plan;
    }
    if (request != nullptr) {
        return request->mipmap_upload_plan;
    }

    static const render_image_texture_mipmap_upload_plan empty_plan{};
    return empty_plan;
}

inline render_image_texture_key render_image_texture_upload_operation_key_for(
    const fake_image_texture_upload_request_snapshot* request,
    const fake_image_texture_upload_result_snapshot* result,
    const fake_image_texture_upload_queue_entry_snapshot* queue_entry)
{
    if (result != nullptr) {
        return result->key;
    }
    if (queue_entry != nullptr) {
        return queue_entry->key;
    }
    if (request != nullptr) {
        return request->key;
    }
    return {};
}

inline render_image_sampler_policy render_image_texture_upload_operation_sampler_for(
    const fake_image_texture_upload_request_snapshot* request,
    const fake_image_texture_upload_result_snapshot* result)
{
    if (result != nullptr) {
        return result->sampler;
    }
    if (request != nullptr) {
        return request->sampler;
    }
    return {};
}

inline render_image_texture_upload_payload_layout_evidence
render_image_texture_upload_operation_payload_layout_for(
    const fake_image_texture_upload_request_snapshot* request,
    const fake_image_texture_upload_result_snapshot* result,
    const fake_image_texture_upload_queue_entry_snapshot* queue_entry)
{
    if (request != nullptr) {
        return request->payload_layout;
    }
    if (result != nullptr) {
        return result->payload_layout;
    }
    if (queue_entry != nullptr) {
        return queue_entry->payload_layout;
    }
    return {};
}

inline render_image_texture_upload_operation_packet make_render_image_texture_upload_operation_packet(
    const fake_image_texture_upload_request_snapshot* request,
    const fake_image_texture_upload_result_snapshot* result,
    const fake_image_texture_upload_queue_entry_snapshot* queue_entry,
    fake_image_texture_upload_generation_id generation_id,
    std::size_t packet_index)
{
    const render_image_texture_mipmap_upload_plan& mipmap_plan =
        render_image_texture_upload_operation_plan_for(request, result, queue_entry);
    render_image_texture_upload_operation_packet packet{
        .packet_index = packet_index,
        .generation_id = generation_id,
        .texture_key = render_image_texture_upload_operation_key_for(request, result, queue_entry),
        .sampler = render_image_texture_upload_operation_sampler_for(request, result),
        .mipmap_upload_plan = mipmap_plan,
        .decoded_payload = request != nullptr
            ? request->decoded_payload
            : render_image_decoded_payload_evidence{},
        .payload_layout = render_image_texture_upload_operation_payload_layout_for(
            request,
            result,
            queue_entry),
        .staging_byte_count = result != nullptr
            ? result->staging_byte_count
            : (queue_entry != nullptr
                    ? queue_entry->staging_byte_count
                    : (request != nullptr ? request->staging_byte_count : 0)),
        .mip_level_count = mipmap_plan.generated_mip_level_count,
        .mipmap_byte_count = mipmap_plan.total_upload_byte_count,
        .request_snapshot_present = request != nullptr,
        .result_snapshot_present = result != nullptr,
        .queue_snapshot_present = queue_entry != nullptr,
    };

    fake_image_texture_upload_retry_snapshot retry_snapshot{};
    if (result != nullptr) {
        packet.upload_status = result->status;
        packet.upload_status_name = render_image_texture_upload_operation_upload_status_name(result->status);
        packet.texture = result->texture;
        retry_snapshot = result->retry;
    } else if (queue_entry != nullptr) {
        packet.upload_status = queue_entry->status;
        packet.upload_status_name = render_image_texture_upload_operation_upload_status_name(queue_entry->status);
        packet.texture = queue_entry->texture;
        retry_snapshot = queue_entry->retry;
    }

    packet.retry_eligibility_name = fake_image_texture_upload_retry_eligibility_name(retry_snapshot.eligibility);
    packet.attempt_count_for_key = retry_snapshot.attempt_count_for_key;
    packet.failed_attempt_count_for_key = retry_snapshot.failed_attempt_count_for_key;
    packet.retry_after_queue_sequence_delta = retry_snapshot.retry_after_queue_sequence_delta;
    packet.next_retry_sequence = retry_snapshot.next_retry_sequence;
    packet.has_texture_handle = packet.texture.valid();
    packet.placeholder_texture = is_fake_image_texture_placeholder_key(packet.texture_key);
    packet.fallback_texture = packet.placeholder_texture;
    packet.retryable = retry_snapshot.eligibility == fake_image_texture_upload_retry_eligibility::eligible;
    packet.nonretryable_failure =
        retry_snapshot.eligibility == fake_image_texture_upload_retry_eligibility::ineligible
        && packet.upload_status != render_image_texture_upload_status::uploaded;

    if (result == nullptr && queue_entry == nullptr) {
        packet.status = render_image_texture_upload_operation_packet_status::blocked_missing_snapshot;
        packet.blocker_summary = "upload result and queue snapshots are missing";
    } else if (packet.upload_status != render_image_texture_upload_status::uploaded) {
        packet.status = packet.retryable
            ? render_image_texture_upload_operation_packet_status::blocked_retryable
            : render_image_texture_upload_operation_packet_status::blocked_nonretryable;
        packet.blocker_summary = packet.retryable
            ? "upload failed but can retry: " + packet.upload_status_name
            : "upload failed without retry: " + packet.upload_status_name;
    } else if (!mipmap_plan.upload_plannable) {
        packet.status = render_image_texture_upload_operation_packet_status::blocked_invalid_mipmap_plan;
        packet.blocker_summary = "mipmap upload plan is not plannable: " + mipmap_plan.status_name;
    } else if (!packet.has_texture_handle) {
        packet.status = render_image_texture_upload_operation_packet_status::blocked_missing_texture_handle;
        packet.blocker_summary = "uploaded texture handle is missing";
    } else {
        packet.status = packet.placeholder_texture
            ? render_image_texture_upload_operation_packet_status::placeholder_ready
            : render_image_texture_upload_operation_packet_status::ready;
        packet.ready_for_upload = true;
        packet.blocked = false;
        packet.readiness_summary = packet.placeholder_texture
            ? "placeholder texture upload packet is ready"
            : "texture upload packet is ready";
    }

    packet.status_name = render_image_texture_upload_operation_packet_status_name(packet.status);
    if (packet.blocked && packet.readiness_summary.empty()) {
        packet.readiness_summary = "texture upload packet is blocked";
    }
    packet.diagnostic = packet.blocked
        ? "texture upload operation packet blocked: " + packet.blocker_summary
        : packet.readiness_summary;
    return packet;
}

inline render_image_texture_upload_operation_plan plan_render_image_texture_upload_operations(
    const fake_image_texture_upload_snapshot& snapshot)
{
    render_image_texture_upload_operation_plan plan{
        .upload_count = snapshot.upload_count,
        .request_snapshot_count = snapshot.request_snapshots.size(),
        .result_snapshot_count = snapshot.result_snapshots.size(),
        .queue_snapshot_count = snapshot.queue_entries.size(),
    };

    std::map<fake_image_texture_upload_generation_id, bool> generation_ids;
    append_fake_image_texture_upload_generation_ids(generation_ids, snapshot);

    std::size_t packet_index = 0;
    for (const auto& [generation_id, _] : generation_ids) {
        render_image_texture_upload_operation_packet packet =
            make_render_image_texture_upload_operation_packet(
                fake_image_texture_upload_request_snapshot_for_generation(snapshot, generation_id),
                fake_image_texture_upload_result_snapshot_for_generation(snapshot, generation_id),
                fake_image_texture_upload_queue_snapshot_for_generation(snapshot, generation_id),
                generation_id,
                packet_index++);

        if (packet.ok()) {
            ++plan.ready_packet_count;
        } else {
            ++plan.blocked_packet_count;
            plan.has_blockers = true;
            plan.all_packets_ready = false;
            append_render_image_texture_upload_operation_blocker(
                plan.blocker_summary,
                packet.blocker_summary);
        }
        if (packet.placeholder_texture) {
            ++plan.placeholder_packet_count;
        }
        if (packet.fallback_texture) {
            ++plan.fallback_packet_count;
        }
        if (packet.retryable && packet.blocked) {
            ++plan.retryable_blocked_packet_count;
        }
        if (packet.nonretryable_failure && packet.blocked) {
            ++plan.nonretryable_blocked_packet_count;
        }
        if (!packet.mipmap_upload_plan.upload_plannable) {
            ++plan.invalid_mipmap_plan_packet_count;
        }
        if (fake_image_texture_mipmap_upload_plan_status_is_overflow(packet.mipmap_upload_plan.status)) {
            ++plan.overflow_mipmap_plan_packet_count;
        }
        if (fake_image_texture_mipmap_upload_plan_status_is_unsupported(packet.mipmap_upload_plan.status)) {
            ++plan.unsupported_mipmap_plan_packet_count;
        }
        if (packet.status == render_image_texture_upload_operation_packet_status::blocked_missing_snapshot) {
            ++plan.missing_snapshot_packet_count;
        }
        if (packet.status == render_image_texture_upload_operation_packet_status::blocked_missing_texture_handle) {
            ++plan.missing_texture_handle_packet_count;
        }
        plan.total_staging_byte_count += packet.staging_byte_count;
        plan.total_mip_level_count += packet.mip_level_count;
        plan.total_mipmap_byte_count += packet.mipmap_byte_count;
        plan.packets.push_back(packet);
    }

    plan.packet_count = plan.packets.size();
    plan.all_packets_ready = !plan.has_blockers;
    if (plan.blocker_summary.empty()) {
        plan.blocker_summary = plan.has_blockers
            ? "texture upload operation plan has blocked packets"
            : "texture upload operation plan has no blockers";
    }
    plan.diagnostic = plan.has_blockers
        ? "texture upload operation plan has blocked packets"
        : (plan.packet_count == 0
            ? "texture upload operation plan has no packets"
            : "texture upload operation plan is ready");
    return plan;
}

} // namespace quiz_vulkan::render
