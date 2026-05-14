#pragma once

#include "render/image/image_texture_frame_upload_handoff.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

enum class render_image_texture_frame_binding_summary_status {
    empty,
    fully_upload_backed,
    placeholder_backed,
    missing_upload_result,
    missing_frame_binding,
    retry_backoff_blocked,
    blocked,
};

inline std::string render_image_texture_frame_binding_summary_status_name(
    render_image_texture_frame_binding_summary_status status)
{
    switch (status) {
    case render_image_texture_frame_binding_summary_status::empty:
        return "empty";
    case render_image_texture_frame_binding_summary_status::fully_upload_backed:
        return "fully_upload_backed";
    case render_image_texture_frame_binding_summary_status::placeholder_backed:
        return "placeholder_backed";
    case render_image_texture_frame_binding_summary_status::missing_upload_result:
        return "missing_upload_result";
    case render_image_texture_frame_binding_summary_status::missing_frame_binding:
        return "missing_frame_binding";
    case render_image_texture_frame_binding_summary_status::retry_backoff_blocked:
        return "retry_backoff_blocked";
    case render_image_texture_frame_binding_summary_status::blocked:
        return "blocked";
    }

    return "unknown";
}

struct render_image_texture_frame_binding_summary {
    render_image_texture_frame_binding_summary_status status =
        render_image_texture_frame_binding_summary_status::empty;
    std::string status_name = render_image_texture_frame_binding_summary_status_name(
        render_image_texture_frame_binding_summary_status::empty);
    std::size_t frame_request_count = 0;
    std::size_t handoff_entry_count = 0;
    std::size_t requested_texture_count = 0;
    std::size_t upload_backed_count = 0;
    std::size_t placeholder_backed_count = 0;
    std::size_t missing_upload_result_count = 0;
    std::size_t missing_frame_binding_count = 0;
    std::size_t retry_backoff_blocked_count = 0;
    std::size_t blocked_count = 0;
    std::size_t cache_reused_count = 0;
    std::size_t unique_texture_cache_key_count = 0;
    std::size_t uploaded_byte_count = 0;
    std::size_t mip_level_count = 0;
    std::size_t max_retry_after_queue_sequence_delta = 0;
    std::size_t next_retry_sequence = 0;
    bool fully_upload_backed = false;
    bool placeholder_backed = false;
    bool missing_upload_result = false;
    bool missing_frame_binding = false;
    bool retry_backoff_blocked = false;
    bool blocked = false;
    bool renderer_handoff_ready = false;
    bool cache_reused = false;
    std::string cache_key_summary;
    std::string sampler_summary;
    std::string retry_backoff_summary;
    std::string diagnostic;

    bool ok() const
    {
        return status == render_image_texture_frame_binding_summary_status::fully_upload_backed
            || status == render_image_texture_frame_binding_summary_status::placeholder_backed;
    }
};

inline render_image_texture_frame_binding_summary_status render_image_texture_frame_binding_summary_status_for(
    const render_image_texture_frame_upload_handoff_summary& handoff)
{
    if (handoff.entries.empty()) {
        return render_image_texture_frame_binding_summary_status::empty;
    }
    if (handoff.missing_upload_result_count != 0) {
        return render_image_texture_frame_binding_summary_status::missing_upload_result;
    }
    if (handoff.missing_frame_binding_count != 0) {
        return render_image_texture_frame_binding_summary_status::missing_frame_binding;
    }
    if (handoff.retry_blocker_count != 0) {
        return render_image_texture_frame_binding_summary_status::retry_backoff_blocked;
    }
    if (handoff.blocked_texture_count != 0) {
        return render_image_texture_frame_binding_summary_status::blocked;
    }
    if (handoff.placeholder_texture_count != 0) {
        return render_image_texture_frame_binding_summary_status::placeholder_backed;
    }
    return render_image_texture_frame_binding_summary_status::fully_upload_backed;
}

inline render_image_texture_frame_binding_summary make_render_image_texture_frame_binding_summary(
    const render_image_texture_frame_upload_handoff_summary& handoff)
{
    const render_image_texture_frame_binding_summary_status status =
        render_image_texture_frame_binding_summary_status_for(handoff);
    render_image_texture_frame_binding_summary summary{
        .status = status,
        .status_name = render_image_texture_frame_binding_summary_status_name(status),
        .frame_request_count = handoff.frame_request_count,
        .handoff_entry_count = handoff.entries.size(),
        .requested_texture_count = handoff.requested_texture_count,
        .upload_backed_count = handoff.ready_texture_count,
        .placeholder_backed_count = handoff.placeholder_texture_count,
        .missing_upload_result_count = handoff.missing_upload_result_count,
        .missing_frame_binding_count = handoff.missing_frame_binding_count,
        .retry_backoff_blocked_count = handoff.retry_blocker_count,
        .blocked_count = handoff.blocked_texture_count,
        .cache_reused_count = handoff.cache_reused_count,
        .unique_texture_cache_key_count = handoff.unique_texture_cache_key_count,
        .uploaded_byte_count = handoff.uploaded_byte_count,
        .mip_level_count = handoff.total_mip_level_count,
        .fully_upload_backed = status == render_image_texture_frame_binding_summary_status::fully_upload_backed,
        .placeholder_backed = handoff.placeholder_texture_count != 0,
        .missing_upload_result = handoff.missing_upload_result_count != 0,
        .missing_frame_binding = handoff.missing_frame_binding_count != 0,
        .retry_backoff_blocked = handoff.retry_blocker_count != 0,
        .blocked = handoff.blocked_texture_count != 0,
        .renderer_handoff_ready = handoff.renderer_handoff_ready,
        .cache_reused = handoff.cache_reused_count != 0,
        .cache_key_summary = handoff.cache_key_summary,
        .sampler_summary = handoff.sampler_summary,
        .retry_backoff_summary = handoff.retry_blocker_summary,
    };

    for (const render_image_texture_frame_upload_handoff_entry& entry : handoff.entries) {
        if (!entry.retryable_blocker) {
            continue;
        }
        if (entry.retry_after_queue_sequence_delta > summary.max_retry_after_queue_sequence_delta) {
            summary.max_retry_after_queue_sequence_delta = entry.retry_after_queue_sequence_delta;
        }
        if (entry.next_retry_sequence != 0
            && (summary.next_retry_sequence == 0 || entry.next_retry_sequence < summary.next_retry_sequence)) {
            summary.next_retry_sequence = entry.next_retry_sequence;
        }
    }

    if (summary.retry_backoff_summary.empty()) {
        summary.retry_backoff_summary = "no retry blockers";
    }

    switch (summary.status) {
    case render_image_texture_frame_binding_summary_status::empty:
        summary.diagnostic = "image frame binding summary has no handoff entries";
        break;
    case render_image_texture_frame_binding_summary_status::fully_upload_backed:
        summary.diagnostic = "image frame binding summary is fully upload-backed";
        break;
    case render_image_texture_frame_binding_summary_status::placeholder_backed:
        summary.diagnostic = "image frame binding summary is placeholder-backed";
        break;
    case render_image_texture_frame_binding_summary_status::missing_upload_result:
        summary.diagnostic = "image frame binding summary has missing upload results";
        break;
    case render_image_texture_frame_binding_summary_status::missing_frame_binding:
        summary.diagnostic = "image frame binding summary has upload results without frame bindings";
        break;
    case render_image_texture_frame_binding_summary_status::retry_backoff_blocked:
        summary.diagnostic = "image frame binding summary has retry backoff blockers";
        break;
    case render_image_texture_frame_binding_summary_status::blocked:
        summary.diagnostic = "image frame binding summary has blocked texture bindings";
        break;
    }

    return summary;
}

inline render_image_texture_frame_binding_summary make_render_image_texture_frame_binding_summary(
    const render_image_texture_frame_snapshot& frame,
    const render_image_texture_upload_result_snapshot& upload_result)
{
    return make_render_image_texture_frame_binding_summary(
        make_render_image_texture_frame_upload_handoff_summary(frame, upload_result));
}

enum class render_image_texture_frame_binding_payload_evidence_status {
    payload_proven,
    placeholder_payload,
    missing_upload_result,
    binding_failed,
    upload_rejected,
};

inline std::string render_image_texture_frame_binding_payload_evidence_status_name(
    render_image_texture_frame_binding_payload_evidence_status status)
{
    switch (status) {
    case render_image_texture_frame_binding_payload_evidence_status::payload_proven:
        return "payload_proven";
    case render_image_texture_frame_binding_payload_evidence_status::placeholder_payload:
        return "placeholder_payload";
    case render_image_texture_frame_binding_payload_evidence_status::missing_upload_result:
        return "missing_upload_result";
    case render_image_texture_frame_binding_payload_evidence_status::binding_failed:
        return "binding_failed";
    case render_image_texture_frame_binding_payload_evidence_status::upload_rejected:
        return "upload_rejected";
    }

    return "unknown";
}

struct render_image_texture_frame_binding_payload_evidence_entry {
    std::size_t sequence = 0;
    std::size_t request_index = 0;
    render_image_texture_frame_binding_payload_evidence_status status =
        render_image_texture_frame_binding_payload_evidence_status::missing_upload_result;
    std::string status_name = render_image_texture_frame_binding_payload_evidence_status_name(
        render_image_texture_frame_binding_payload_evidence_status::missing_upload_result);
    render_image_texture_frame_binding_packet_status binding_status =
        render_image_texture_frame_binding_packet_status::failed;
    std::string binding_status_name;
    render_image_texture_frame_upload_handoff_entry_status handoff_status =
        render_image_texture_frame_upload_handoff_entry_status::missing_upload_result;
    std::string handoff_status_name;
    render_image_texture_upload_result_packet_status upload_result_status =
        render_image_texture_upload_result_packet_status::rejected_missing_snapshot;
    std::string upload_result_status_name = render_image_texture_upload_result_packet_status_name(
        render_image_texture_upload_result_packet_status::rejected_missing_snapshot);
    render_image_texture_upload_status upload_status = render_image_texture_upload_status::invalid_image;
    std::string upload_status_name = render_image_texture_upload_operation_upload_status_name(
        render_image_texture_upload_status::invalid_image);
    std::string render_image_uri;
    std::string normalized_uri;
    render_image_cache_key cache_key;
    render_image_sampler_policy sampler;
    std::string sampler_key;
    render_image_texture_key texture_key;
    std::string stable_texture_cache_key;
    render_image_texture_id texture_id = 0;
    render_image_revision texture_revision = 0;
    std::uint64_t upload_request_id = 0;
    std::uint64_t upload_generation_id = 0;
    std::size_t uploaded_byte_count = 0;
    std::size_t decoded_byte_count = 0;
    std::uint64_t decoded_payload_hash = 0;
    bool binding_packet_present = true;
    bool upload_result_present = false;
    bool upload_result_accepted = false;
    bool decoded_payload_valid = false;
    bool decoded_payload_hash_present = false;
    bool renderer_handoff_ready = false;
    bool placeholder_texture = false;
    bool failed = true;
    bool blocked = true;
    bool cache_reused = false;
    bool expected_cache_reuse = false;
    std::string evidence_key;
    std::string diagnostic;

    bool ok() const
    {
        return status == render_image_texture_frame_binding_payload_evidence_status::payload_proven
            || status == render_image_texture_frame_binding_payload_evidence_status::placeholder_payload;
    }
};

struct render_image_texture_frame_binding_payload_evidence_summary {
    std::size_t binding_packet_count = 0;
    std::size_t evidence_entry_count = 0;
    std::size_t proven_payload_count = 0;
    std::size_t placeholder_payload_count = 0;
    std::size_t missing_upload_result_count = 0;
    std::size_t failed_binding_count = 0;
    std::size_t rejected_upload_count = 0;
    std::size_t decoded_payload_hash_count = 0;
    std::size_t accepted_upload_result_count = 0;
    std::size_t cache_reused_count = 0;
    bool binding_evidence_ready = false;
    bool renderer_handoff_ready = false;
    bool has_placeholders = false;
    bool has_failures = false;
    bool has_rejections = false;
    bool has_missing_upload_results = false;
    bool cache_reused = false;
    std::vector<render_image_texture_frame_binding_payload_evidence_entry> entries;
    std::string evidence_summary;
    std::string diagnostic;

    bool ok() const
    {
        return binding_evidence_ready && !has_failures && !has_rejections && !has_missing_upload_results;
    }
};

inline render_image_texture_frame_binding_payload_evidence_status
render_image_texture_frame_binding_payload_evidence_status_for(
    const render_image_texture_frame_binding_packet& binding_packet,
    const render_image_texture_frame_upload_handoff_entry* handoff_entry)
{
    if (handoff_entry == nullptr || !handoff_entry->upload_result_present) {
        return render_image_texture_frame_binding_payload_evidence_status::missing_upload_result;
    }
    if (binding_packet.failed) {
        return render_image_texture_frame_binding_payload_evidence_status::binding_failed;
    }
    if (handoff_entry->blocked
        || !render_image_texture_upload_result_packet_status_is_accepted(handoff_entry->upload_result_status)) {
        return render_image_texture_frame_binding_payload_evidence_status::upload_rejected;
    }
    if (binding_packet.placeholder_texture || handoff_entry->placeholder_texture) {
        return render_image_texture_frame_binding_payload_evidence_status::placeholder_payload;
    }
    return render_image_texture_frame_binding_payload_evidence_status::payload_proven;
}

inline std::string render_image_texture_frame_binding_payload_evidence_key_for(
    const render_image_texture_frame_binding_payload_evidence_entry& entry)
{
    return entry.stable_texture_cache_key
        + "|sampler=" + entry.sampler_key
        + "|texture=" + std::to_string(entry.texture_id)
        + "|upload=" + std::to_string(entry.upload_request_id)
        + "|upload_status=" + entry.upload_result_status_name
        + "|payload_hash=" + std::to_string(entry.decoded_payload_hash);
}

inline render_image_texture_frame_binding_payload_evidence_entry
make_render_image_texture_frame_binding_payload_evidence_entry(
    const render_image_texture_frame_binding_packet& binding_packet,
    const render_image_texture_frame_upload_handoff_entry* handoff_entry)
{
    const render_image_texture_frame_binding_payload_evidence_status status =
        render_image_texture_frame_binding_payload_evidence_status_for(binding_packet, handoff_entry);

    render_image_texture_frame_binding_payload_evidence_entry entry{
        .sequence = binding_packet.sequence,
        .request_index = binding_packet.request_index,
        .status = status,
        .status_name = render_image_texture_frame_binding_payload_evidence_status_name(status),
        .binding_status = binding_packet.status,
        .binding_status_name = binding_packet.status_name,
        .render_image_uri = binding_packet.render_image_uri,
        .normalized_uri = binding_packet.normalized_uri,
        .cache_key = binding_packet.cache_key,
        .sampler = binding_packet.sampler,
        .sampler_key = binding_packet.sampler_key,
        .texture_key = binding_packet.texture_key,
        .stable_texture_cache_key = binding_packet.stable_texture_cache_key,
        .texture_id = binding_packet.texture_id,
        .texture_revision = binding_packet.texture_revision,
        .renderer_handoff_ready = binding_packet.renderer_handoff_ready,
        .placeholder_texture = binding_packet.placeholder_texture,
        .failed = binding_packet.failed,
        .cache_reused = binding_packet.cache_reused,
        .expected_cache_reuse = binding_packet.expected_cache_reuse,
    };

    if (handoff_entry != nullptr) {
        entry.handoff_status = handoff_entry->status;
        entry.handoff_status_name = handoff_entry->status_name;
        entry.upload_result_status = handoff_entry->upload_result_status;
        entry.upload_result_status_name = handoff_entry->upload_result_status_name;
        entry.upload_status = handoff_entry->upload_status;
        entry.upload_status_name = handoff_entry->upload_status_name;
        entry.upload_request_id = handoff_entry->upload_request_id;
        entry.upload_generation_id = handoff_entry->upload_generation_id;
        entry.uploaded_byte_count = handoff_entry->uploaded_byte_count;
        entry.decoded_byte_count = handoff_entry->decoded_payload.decoded_byte_count;
        entry.decoded_payload_hash = handoff_entry->decoded_payload.stable_byte_hash;
        entry.upload_result_present = handoff_entry->upload_result_present;
        entry.upload_result_accepted =
            render_image_texture_upload_result_packet_status_is_accepted(handoff_entry->upload_result_status);
        entry.decoded_payload_valid = handoff_entry->decoded_payload.payload_valid;
        entry.decoded_payload_hash_present = handoff_entry->decoded_payload.stable_byte_hash != 0;
        entry.placeholder_texture = entry.placeholder_texture || handoff_entry->placeholder_texture;
        entry.blocked = handoff_entry->blocked;
    }

    entry.evidence_key = render_image_texture_frame_binding_payload_evidence_key_for(entry);
    switch (entry.status) {
    case render_image_texture_frame_binding_payload_evidence_status::payload_proven:
        entry.diagnostic = "image texture binding payload evidence is proven";
        break;
    case render_image_texture_frame_binding_payload_evidence_status::placeholder_payload:
        entry.diagnostic = "image texture binding payload evidence uses placeholder texture";
        break;
    case render_image_texture_frame_binding_payload_evidence_status::missing_upload_result:
        entry.diagnostic = "image texture binding payload evidence is missing upload result";
        break;
    case render_image_texture_frame_binding_payload_evidence_status::binding_failed:
        entry.diagnostic = "image texture binding payload evidence has failed binding";
        break;
    case render_image_texture_frame_binding_payload_evidence_status::upload_rejected:
        entry.diagnostic = "image texture binding payload evidence has rejected upload";
        break;
    }

    return entry;
}

inline void count_render_image_texture_frame_binding_payload_evidence_entry(
    render_image_texture_frame_binding_payload_evidence_summary& summary,
    const render_image_texture_frame_binding_payload_evidence_entry& entry)
{
    ++summary.evidence_entry_count;
    switch (entry.status) {
    case render_image_texture_frame_binding_payload_evidence_status::payload_proven:
        ++summary.proven_payload_count;
        break;
    case render_image_texture_frame_binding_payload_evidence_status::placeholder_payload:
        ++summary.placeholder_payload_count;
        summary.has_placeholders = true;
        break;
    case render_image_texture_frame_binding_payload_evidence_status::missing_upload_result:
        ++summary.missing_upload_result_count;
        summary.has_missing_upload_results = true;
        break;
    case render_image_texture_frame_binding_payload_evidence_status::binding_failed:
        ++summary.failed_binding_count;
        summary.has_failures = true;
        break;
    case render_image_texture_frame_binding_payload_evidence_status::upload_rejected:
        ++summary.rejected_upload_count;
        summary.has_rejections = true;
        break;
    }
    if (entry.decoded_payload_hash_present) {
        ++summary.decoded_payload_hash_count;
    }
    if (entry.upload_result_accepted) {
        ++summary.accepted_upload_result_count;
    }
    if (entry.cache_reused) {
        ++summary.cache_reused_count;
        summary.cache_reused = true;
    }
}

inline render_image_texture_frame_binding_payload_evidence_summary
make_render_image_texture_frame_binding_payload_evidence_summary(
    const render_image_texture_frame_binding_plan& binding_plan,
    const render_image_texture_frame_upload_handoff_summary& handoff)
{
    render_image_texture_frame_binding_payload_evidence_summary summary{
        .binding_packet_count = binding_plan.packet_count,
        .renderer_handoff_ready = binding_plan.renderer_handoff_ready,
    };

    for (const render_image_texture_frame_binding_packet& binding_packet : binding_plan.packets) {
        render_image_texture_frame_binding_payload_evidence_entry entry =
            make_render_image_texture_frame_binding_payload_evidence_entry(
                binding_packet,
                render_image_texture_frame_upload_handoff_entry_for_request_index(
                    handoff,
                    binding_packet.request_index));
        count_render_image_texture_frame_binding_payload_evidence_entry(summary, entry);
        summary.entries.push_back(std::move(entry));
    }

    summary.binding_evidence_ready =
        summary.binding_packet_count == summary.evidence_entry_count
        && !summary.has_missing_upload_results
        && !summary.has_failures
        && !summary.has_rejections;
    summary.evidence_summary = "binding_payloads="
        + std::to_string(summary.evidence_entry_count)
        + "; payload_hashes=" + std::to_string(summary.decoded_payload_hash_count)
        + "; accepted_uploads=" + std::to_string(summary.accepted_upload_result_count);
    if (summary.has_missing_upload_results) {
        summary.diagnostic = "image texture binding payload evidence has missing upload results";
    } else if (summary.has_failures) {
        summary.diagnostic = "image texture binding payload evidence has failed bindings";
    } else if (summary.has_rejections) {
        summary.diagnostic = "image texture binding payload evidence has rejected uploads";
    } else if (summary.has_placeholders) {
        summary.diagnostic = "image texture binding payload evidence is placeholder-backed";
    } else if (summary.binding_packet_count == 0) {
        summary.diagnostic = "image texture binding payload evidence has no binding packets";
    } else {
        summary.diagnostic = "image texture binding payload evidence is ready";
    }

    return summary;
}

inline render_image_texture_frame_binding_payload_evidence_summary
make_render_image_texture_frame_binding_payload_evidence_summary(
    const render_image_texture_frame_snapshot& frame,
    const render_image_texture_upload_result_snapshot& upload_result)
{
    const render_image_texture_frame_binding_plan binding_plan =
        make_render_image_texture_frame_binding_plan(frame);
    return make_render_image_texture_frame_binding_payload_evidence_summary(
        binding_plan,
        make_render_image_texture_frame_upload_handoff_summary(frame, binding_plan, upload_result));
}

struct render_image_texture_frame_binding_summary_diff {
    render_image_texture_frame_binding_summary_status before_status =
        render_image_texture_frame_binding_summary_status::empty;
    render_image_texture_frame_binding_summary_status after_status =
        render_image_texture_frame_binding_summary_status::empty;
    std::string before_status_name;
    std::string after_status_name;
    std::size_t before_requested_texture_count = 0;
    std::size_t after_requested_texture_count = 0;
    std::int64_t requested_texture_delta = 0;
    std::size_t before_upload_backed_count = 0;
    std::size_t after_upload_backed_count = 0;
    std::int64_t upload_backed_delta = 0;
    std::size_t before_placeholder_backed_count = 0;
    std::size_t after_placeholder_backed_count = 0;
    std::int64_t placeholder_backed_delta = 0;
    std::size_t before_missing_upload_result_count = 0;
    std::size_t after_missing_upload_result_count = 0;
    std::int64_t missing_upload_result_delta = 0;
    std::size_t before_missing_frame_binding_count = 0;
    std::size_t after_missing_frame_binding_count = 0;
    std::int64_t missing_frame_binding_delta = 0;
    std::size_t before_retry_backoff_blocked_count = 0;
    std::size_t after_retry_backoff_blocked_count = 0;
    std::int64_t retry_backoff_blocked_delta = 0;
    std::size_t before_blocked_count = 0;
    std::size_t after_blocked_count = 0;
    std::int64_t blocked_delta = 0;
    std::size_t before_uploaded_byte_count = 0;
    std::size_t after_uploaded_byte_count = 0;
    std::int64_t uploaded_byte_delta = 0;
    bool status_changed = false;
    bool upload_backing_changed = false;
    bool placeholder_backing_changed = false;
    bool missing_upload_result_changed = false;
    bool missing_frame_binding_changed = false;
    bool retry_backoff_changed = false;
    bool blocker_changed = false;
    bool cache_key_summary_changed = false;
    bool sampler_summary_changed = false;
    bool cache_key_or_sampler_changed = false;
    bool renderer_handoff_regressed = false;
    bool renderer_handoff_recovered = false;
    bool has_changes = false;
    bool has_regression = false;
    bool has_recovery = false;
    std::string changed_summary;
    std::string regression_summary;
    std::string diagnostic;

    bool ok() const
    {
        return !has_regression;
    }
};

inline render_image_texture_frame_binding_summary_diff diff_render_image_texture_frame_binding_summaries(
    const render_image_texture_frame_binding_summary& before,
    const render_image_texture_frame_binding_summary& after)
{
    render_image_texture_frame_binding_summary_diff diff{
        .before_status = before.status,
        .after_status = after.status,
        .before_status_name = before.status_name,
        .after_status_name = after.status_name,
        .before_requested_texture_count = before.requested_texture_count,
        .after_requested_texture_count = after.requested_texture_count,
        .requested_texture_delta = render_image_texture_upload_result_size_delta(
            before.requested_texture_count,
            after.requested_texture_count),
        .before_upload_backed_count = before.upload_backed_count,
        .after_upload_backed_count = after.upload_backed_count,
        .upload_backed_delta = render_image_texture_upload_result_size_delta(
            before.upload_backed_count,
            after.upload_backed_count),
        .before_placeholder_backed_count = before.placeholder_backed_count,
        .after_placeholder_backed_count = after.placeholder_backed_count,
        .placeholder_backed_delta = render_image_texture_upload_result_size_delta(
            before.placeholder_backed_count,
            after.placeholder_backed_count),
        .before_missing_upload_result_count = before.missing_upload_result_count,
        .after_missing_upload_result_count = after.missing_upload_result_count,
        .missing_upload_result_delta = render_image_texture_upload_result_size_delta(
            before.missing_upload_result_count,
            after.missing_upload_result_count),
        .before_missing_frame_binding_count = before.missing_frame_binding_count,
        .after_missing_frame_binding_count = after.missing_frame_binding_count,
        .missing_frame_binding_delta = render_image_texture_upload_result_size_delta(
            before.missing_frame_binding_count,
            after.missing_frame_binding_count),
        .before_retry_backoff_blocked_count = before.retry_backoff_blocked_count,
        .after_retry_backoff_blocked_count = after.retry_backoff_blocked_count,
        .retry_backoff_blocked_delta = render_image_texture_upload_result_size_delta(
            before.retry_backoff_blocked_count,
            after.retry_backoff_blocked_count),
        .before_blocked_count = before.blocked_count,
        .after_blocked_count = after.blocked_count,
        .blocked_delta = render_image_texture_upload_result_size_delta(before.blocked_count, after.blocked_count),
        .before_uploaded_byte_count = before.uploaded_byte_count,
        .after_uploaded_byte_count = after.uploaded_byte_count,
        .uploaded_byte_delta = render_image_texture_upload_result_size_delta(
            before.uploaded_byte_count,
            after.uploaded_byte_count),
    };

    diff.status_changed = before.status != after.status;
    diff.upload_backing_changed = before.upload_backed_count != after.upload_backed_count
        || before.fully_upload_backed != after.fully_upload_backed;
    diff.placeholder_backing_changed = before.placeholder_backed_count != after.placeholder_backed_count
        || before.placeholder_backed != after.placeholder_backed;
    diff.missing_upload_result_changed = before.missing_upload_result_count != after.missing_upload_result_count;
    diff.missing_frame_binding_changed = before.missing_frame_binding_count != after.missing_frame_binding_count;
    diff.retry_backoff_changed = before.retry_backoff_blocked_count != after.retry_backoff_blocked_count
        || before.max_retry_after_queue_sequence_delta != after.max_retry_after_queue_sequence_delta
        || before.next_retry_sequence != after.next_retry_sequence;
    diff.blocker_changed = before.blocked_count != after.blocked_count;
    diff.cache_key_summary_changed = before.cache_key_summary != after.cache_key_summary;
    diff.sampler_summary_changed = before.sampler_summary != after.sampler_summary;
    diff.cache_key_or_sampler_changed = diff.cache_key_summary_changed || diff.sampler_summary_changed;
    diff.renderer_handoff_regressed = before.renderer_handoff_ready && !after.renderer_handoff_ready;
    diff.renderer_handoff_recovered = !before.renderer_handoff_ready && after.renderer_handoff_ready;
    diff.has_changes = diff.status_changed
        || diff.requested_texture_delta != 0
        || diff.upload_backed_delta != 0
        || diff.placeholder_backed_delta != 0
        || diff.missing_upload_result_delta != 0
        || diff.missing_frame_binding_delta != 0
        || diff.retry_backoff_blocked_delta != 0
        || diff.blocked_delta != 0
        || diff.uploaded_byte_delta != 0
        || diff.retry_backoff_changed
        || diff.blocker_changed
        || diff.cache_key_or_sampler_changed;
    diff.has_regression = diff.renderer_handoff_regressed
        || diff.upload_backed_delta < 0
        || diff.placeholder_backed_delta > 0
        || diff.missing_upload_result_delta > 0
        || diff.missing_frame_binding_delta > 0
        || diff.retry_backoff_blocked_delta > 0
        || diff.blocked_delta > 0;
    diff.has_recovery = diff.renderer_handoff_recovered
        || diff.upload_backed_delta > 0
        || diff.missing_upload_result_delta < 0
        || diff.missing_frame_binding_delta < 0
        || diff.retry_backoff_blocked_delta < 0
        || diff.blocked_delta < 0;

    if (diff.cache_key_summary_changed) {
        append_render_image_texture_frame_upload_handoff_summary_fragment(
            diff.changed_summary,
            "cache keys changed");
    }
    if (diff.sampler_summary_changed) {
        append_render_image_texture_frame_upload_handoff_summary_fragment(
            diff.changed_summary,
            "samplers changed");
    }
    if (diff.status_changed) {
        append_render_image_texture_frame_upload_handoff_summary_fragment(
            diff.changed_summary,
            "status changed");
    }
    if (diff.retry_backoff_changed) {
        append_render_image_texture_frame_upload_handoff_summary_fragment(
            diff.changed_summary,
            "retry backoff changed");
    }
    if (diff.blocker_changed) {
        append_render_image_texture_frame_upload_handoff_summary_fragment(
            diff.changed_summary,
            "blockers changed");
    }
    if (diff.changed_summary.empty()) {
        diff.changed_summary = diff.has_changes
            ? "image frame binding summary aggregate counts changed"
            : "image frame binding summary diff has no changes";
    }

    if (diff.renderer_handoff_regressed) {
        append_render_image_texture_frame_regression_reason(diff.regression_summary, "binding summary handoff regressed");
    }
    if (diff.upload_backed_delta < 0) {
        append_render_image_texture_frame_regression_reason(diff.regression_summary, "upload-backed refs decreased");
    }
    if (diff.placeholder_backed_delta > 0) {
        append_render_image_texture_frame_regression_reason(diff.regression_summary, "placeholder-backed refs increased");
    }
    if (diff.missing_upload_result_delta > 0) {
        append_render_image_texture_frame_regression_reason(diff.regression_summary, "missing upload results increased");
    }
    if (diff.missing_frame_binding_delta > 0) {
        append_render_image_texture_frame_regression_reason(diff.regression_summary, "missing frame bindings increased");
    }
    if (diff.retry_backoff_blocked_delta > 0) {
        append_render_image_texture_frame_regression_reason(diff.regression_summary, "retry blockers increased");
    }
    if (diff.blocked_delta > 0) {
        append_render_image_texture_frame_regression_reason(diff.regression_summary, "blocked bindings increased");
    }
    if (diff.has_regression && diff.regression_summary.empty()) {
        diff.regression_summary = "image frame binding summary regressed";
    }
    if (diff.regression_summary.empty()) {
        diff.regression_summary = diff.has_changes
            ? "image frame binding summary diff has changes without regressions"
            : "image frame binding summary diff has no changes";
    }

    diff.diagnostic = diff.has_regression
        ? "image frame binding summary diff reports regressions"
        : (diff.has_changes
            ? "image frame binding summary diff reports changes"
            : "image frame binding summary diff is unchanged");
    return diff;
}

inline render_image_texture_frame_binding_summary_diff diff_render_image_texture_frame_binding_summaries(
    const render_image_texture_frame_upload_handoff_summary& before,
    const render_image_texture_frame_upload_handoff_summary& after)
{
    return diff_render_image_texture_frame_binding_summaries(
        make_render_image_texture_frame_binding_summary(before),
        make_render_image_texture_frame_binding_summary(after));
}

} // namespace quiz_vulkan::render
