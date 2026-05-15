#pragma once

#include "render/image/image_texture_upload_operation_plan.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <vector>

namespace quiz_vulkan::render {

enum class render_image_texture_upload_result_packet_status {
    accepted,
    accepted_placeholder,
    rejected_retryable,
    rejected_nonretryable,
    rejected_invalid_mipmap_plan,
    rejected_missing_snapshot,
    rejected_missing_texture_handle,
};

inline std::string render_image_texture_upload_result_packet_status_name(
    render_image_texture_upload_result_packet_status status)
{
    switch (status) {
    case render_image_texture_upload_result_packet_status::accepted:
        return "accepted";
    case render_image_texture_upload_result_packet_status::accepted_placeholder:
        return "accepted_placeholder";
    case render_image_texture_upload_result_packet_status::rejected_retryable:
        return "rejected_retryable";
    case render_image_texture_upload_result_packet_status::rejected_nonretryable:
        return "rejected_nonretryable";
    case render_image_texture_upload_result_packet_status::rejected_invalid_mipmap_plan:
        return "rejected_invalid_mipmap_plan";
    case render_image_texture_upload_result_packet_status::rejected_missing_snapshot:
        return "rejected_missing_snapshot";
    case render_image_texture_upload_result_packet_status::rejected_missing_texture_handle:
        return "rejected_missing_texture_handle";
    }

    return "unknown";
}

inline render_image_texture_upload_result_packet_status
render_image_texture_upload_result_packet_status_for(
    const render_image_texture_upload_operation_packet& packet)
{
    switch (packet.status) {
    case render_image_texture_upload_operation_packet_status::ready:
        return render_image_texture_upload_result_packet_status::accepted;
    case render_image_texture_upload_operation_packet_status::placeholder_ready:
        return render_image_texture_upload_result_packet_status::accepted_placeholder;
    case render_image_texture_upload_operation_packet_status::blocked_retryable:
        return render_image_texture_upload_result_packet_status::rejected_retryable;
    case render_image_texture_upload_operation_packet_status::blocked_nonretryable:
        return render_image_texture_upload_result_packet_status::rejected_nonretryable;
    case render_image_texture_upload_operation_packet_status::blocked_invalid_mipmap_plan:
        return render_image_texture_upload_result_packet_status::rejected_invalid_mipmap_plan;
    case render_image_texture_upload_operation_packet_status::blocked_missing_snapshot:
        return render_image_texture_upload_result_packet_status::rejected_missing_snapshot;
    case render_image_texture_upload_operation_packet_status::blocked_missing_texture_handle:
        return render_image_texture_upload_result_packet_status::rejected_missing_texture_handle;
    }

    return render_image_texture_upload_result_packet_status::rejected_nonretryable;
}

inline bool render_image_texture_upload_result_packet_status_is_accepted(
    render_image_texture_upload_result_packet_status status)
{
    return status == render_image_texture_upload_result_packet_status::accepted
        || status == render_image_texture_upload_result_packet_status::accepted_placeholder;
}

struct render_image_texture_upload_result_packet_snapshot {
    std::size_t packet_index = 0;
    std::uint64_t request_id = 0;
    std::uint64_t generation_id = 0;
    render_image_texture_upload_result_packet_status status =
        render_image_texture_upload_result_packet_status::rejected_missing_snapshot;
    std::string status_name = render_image_texture_upload_result_packet_status_name(
        render_image_texture_upload_result_packet_status::rejected_missing_snapshot);
    render_image_texture_upload_operation_packet_status operation_status =
        render_image_texture_upload_operation_packet_status::blocked_missing_snapshot;
    std::string operation_status_name;
    render_image_texture_upload_status upload_status =
        render_image_texture_upload_status::invalid_image;
    std::string upload_status_name;
    render_image_texture_key texture_key;
    std::string stable_cache_key;
    std::string source_key_summary;
    render_image_sampler_policy sampler;
    std::string sampler_summary;
    render_image_texture_handle texture;
    render_image_texture_id texture_id = 0;
    render_image_revision texture_revision = 0;
    std::size_t texture_width = 0;
    std::size_t texture_height = 0;
    std::size_t mip_level_count = 0;
    std::size_t accepted_mip_level_count = 0;
    std::size_t rejected_mip_level_count = 0;
    std::size_t uploaded_byte_count = 0;
    std::size_t planned_staging_byte_count = 0;
    std::size_t planned_mipmap_byte_count = 0;
    render_image_decoded_payload_evidence decoded_payload;
    render_image_texture_upload_payload_layout_evidence payload_layout;
    render_image_texture_staging_payload_plan staging_payload_plan;
    bool accepted = false;
    bool rejected = true;
    bool placeholder_texture = false;
    bool fallback_texture = false;
    bool retryable = false;
    bool nonretryable_failure = false;
    bool blocked = true;
    bool has_texture_handle = false;
    std::string retry_eligibility_name;
    std::size_t attempt_count_for_key = 0;
    std::size_t failed_attempt_count_for_key = 0;
    std::size_t retry_after_queue_sequence_delta = 0;
    std::size_t next_retry_sequence = 0;
    std::string readiness_summary;
    std::string blocker_summary;
    std::string diagnostic;

    bool ok() const
    {
        return accepted && !blocked;
    }
};

struct render_image_texture_upload_result_snapshot {
    std::size_t source_upload_count = 0;
    std::size_t operation_packet_count = 0;
    std::size_t packet_count = 0;
    std::size_t accepted_packet_count = 0;
    std::size_t rejected_packet_count = 0;
    std::size_t placeholder_packet_count = 0;
    std::size_t fallback_packet_count = 0;
    std::size_t retryable_rejected_packet_count = 0;
    std::size_t nonretryable_rejected_packet_count = 0;
    std::size_t blocker_count = 0;
    std::size_t invalid_mipmap_plan_packet_count = 0;
    std::size_t missing_snapshot_packet_count = 0;
    std::size_t missing_texture_handle_packet_count = 0;
    std::size_t texture_count = 0;
    std::size_t request_id_count = 0;
    std::size_t total_mip_level_count = 0;
    std::size_t accepted_mip_level_count = 0;
    std::size_t rejected_mip_level_count = 0;
    std::size_t total_uploaded_byte_count = 0;
    std::size_t total_planned_staging_byte_count = 0;
    std::size_t total_planned_mipmap_byte_count = 0;
    bool has_rejections = false;
    bool has_placeholders = false;
    bool has_retryable_rejections = false;
    std::vector<std::uint64_t> request_ids;
    std::vector<render_image_texture_id> texture_ids;
    std::vector<render_image_texture_upload_result_packet_snapshot> packets;
    std::string accepted_summary;
    std::string rejected_summary;
    std::string texture_summary;
    std::string diagnostic;

    bool ok() const
    {
        return !has_rejections;
    }
};

inline render_image_texture_upload_result_packet_snapshot
make_render_image_texture_upload_result_packet_snapshot(
    const render_image_texture_upload_operation_packet& packet)
{
    const render_image_texture_key_diagnostic key_diagnostic =
        make_render_image_texture_key_diagnostic(packet.texture_key);
    const render_image_texture_upload_result_packet_status status =
        render_image_texture_upload_result_packet_status_for(packet);
    const bool accepted = render_image_texture_upload_result_packet_status_is_accepted(status);

    return render_image_texture_upload_result_packet_snapshot{
        .packet_index = packet.packet_index,
        .request_id = packet.generation_id,
        .generation_id = packet.generation_id,
        .status = status,
        .status_name = render_image_texture_upload_result_packet_status_name(status),
        .operation_status = packet.status,
        .operation_status_name = packet.status_name,
        .upload_status = packet.upload_status,
        .upload_status_name = packet.upload_status_name,
        .texture_key = packet.texture_key,
        .stable_cache_key = key_diagnostic.stable_cache_key,
        .source_key_summary = packet.texture_key.source_key,
        .sampler = packet.sampler,
        .sampler_summary = render_image_sampler_policy_stable_fragment(packet.sampler),
        .texture = packet.texture,
        .texture_id = packet.texture.id,
        .texture_revision = packet.texture.revision,
        .texture_width = packet.texture.width,
        .texture_height = packet.texture.height,
        .mip_level_count = packet.mip_level_count,
        .accepted_mip_level_count = accepted ? packet.mip_level_count : 0,
        .rejected_mip_level_count = accepted ? 0 : packet.mip_level_count,
        .uploaded_byte_count = accepted ? packet.staging_byte_count : 0,
        .planned_staging_byte_count = packet.staging_byte_count,
        .planned_mipmap_byte_count = packet.mipmap_byte_count,
        .decoded_payload = packet.decoded_payload,
        .payload_layout = packet.payload_layout,
        .staging_payload_plan = packet.staging_payload_plan,
        .accepted = accepted,
        .rejected = !accepted,
        .placeholder_texture = packet.placeholder_texture,
        .fallback_texture = packet.fallback_texture,
        .retryable = packet.retryable,
        .nonretryable_failure = packet.nonretryable_failure,
        .blocked = packet.blocked,
        .has_texture_handle = packet.has_texture_handle,
        .retry_eligibility_name = packet.retry_eligibility_name,
        .attempt_count_for_key = packet.attempt_count_for_key,
        .failed_attempt_count_for_key = packet.failed_attempt_count_for_key,
        .retry_after_queue_sequence_delta = packet.retry_after_queue_sequence_delta,
        .next_retry_sequence = packet.next_retry_sequence,
        .readiness_summary = packet.readiness_summary,
        .blocker_summary = packet.blocker_summary,
        .diagnostic = accepted
            ? "texture upload result packet accepted: " + packet.readiness_summary
            : "texture upload result packet rejected: " + packet.blocker_summary,
    };
}

inline void append_render_image_texture_upload_result_summary(
    std::string& summary,
    const std::string& fragment)
{
    if (fragment.empty()) {
        return;
    }
    if (!summary.empty()) {
        summary += "; ";
    }
    summary += fragment;
}

inline render_image_texture_upload_result_snapshot make_render_image_texture_upload_result_snapshot(
    const render_image_texture_upload_operation_plan& operation_plan)
{
    render_image_texture_upload_result_snapshot snapshot{
        .source_upload_count = operation_plan.upload_count,
        .operation_packet_count = operation_plan.packet_count,
    };

    for (const render_image_texture_upload_operation_packet& operation_packet : operation_plan.packets) {
        render_image_texture_upload_result_packet_snapshot packet =
            make_render_image_texture_upload_result_packet_snapshot(operation_packet);

        snapshot.request_ids.push_back(packet.request_id);
        ++snapshot.request_id_count;
        if (packet.accepted && packet.has_texture_handle) {
            snapshot.texture_ids.push_back(packet.texture_id);
            ++snapshot.texture_count;
        }

        if (packet.accepted) {
            ++snapshot.accepted_packet_count;
        } else {
            ++snapshot.rejected_packet_count;
            ++snapshot.blocker_count;
            snapshot.has_rejections = true;
            append_render_image_texture_upload_result_summary(snapshot.rejected_summary, packet.blocker_summary);
        }
        if (packet.placeholder_texture) {
            ++snapshot.placeholder_packet_count;
            snapshot.has_placeholders = true;
        }
        if (packet.fallback_texture) {
            ++snapshot.fallback_packet_count;
        }
        if (packet.retryable && packet.rejected) {
            ++snapshot.retryable_rejected_packet_count;
            snapshot.has_retryable_rejections = true;
        }
        if (packet.nonretryable_failure && packet.rejected) {
            ++snapshot.nonretryable_rejected_packet_count;
        }
        if (packet.status == render_image_texture_upload_result_packet_status::rejected_invalid_mipmap_plan) {
            ++snapshot.invalid_mipmap_plan_packet_count;
        }
        if (packet.status == render_image_texture_upload_result_packet_status::rejected_missing_snapshot) {
            ++snapshot.missing_snapshot_packet_count;
        }
        if (packet.status == render_image_texture_upload_result_packet_status::rejected_missing_texture_handle) {
            ++snapshot.missing_texture_handle_packet_count;
        }

        snapshot.total_mip_level_count += packet.mip_level_count;
        snapshot.accepted_mip_level_count += packet.accepted_mip_level_count;
        snapshot.rejected_mip_level_count += packet.rejected_mip_level_count;
        snapshot.total_uploaded_byte_count += packet.uploaded_byte_count;
        snapshot.total_planned_staging_byte_count += packet.planned_staging_byte_count;
        snapshot.total_planned_mipmap_byte_count += packet.planned_mipmap_byte_count;
        if (packet.accepted) {
            append_render_image_texture_upload_result_summary(snapshot.accepted_summary, packet.stable_cache_key);
        }
        if (packet.accepted && packet.has_texture_handle) {
            append_render_image_texture_upload_result_summary(
                snapshot.texture_summary,
                packet.stable_cache_key + "#texture=" + std::to_string(packet.texture_id));
        }
        snapshot.packets.push_back(packet);
    }

    snapshot.packet_count = snapshot.packets.size();
    if (snapshot.accepted_summary.empty()) {
        snapshot.accepted_summary = "no accepted upload packets";
    }
    if (snapshot.rejected_summary.empty()) {
        snapshot.rejected_summary = "no rejected upload packets";
    }
    if (snapshot.texture_summary.empty()) {
        snapshot.texture_summary = "no uploaded texture handles";
    }
    snapshot.diagnostic = snapshot.has_rejections
        ? "image texture upload result snapshot has rejected packets"
        : (snapshot.packet_count == 0
            ? "image texture upload result snapshot has no packets"
            : "image texture upload result snapshot accepted all packets");
    return snapshot;
}

inline render_image_texture_upload_result_snapshot
make_render_image_texture_upload_result_snapshot_from_fake_upload_snapshot(
    const fake_image_texture_upload_snapshot& snapshot)
{
    return make_render_image_texture_upload_result_snapshot(
        plan_render_image_texture_upload_operations(snapshot));
}

enum class render_image_texture_upload_result_diff_entry_status {
    unchanged,
    added,
    removed,
    changed,
};

inline std::string render_image_texture_upload_result_diff_entry_status_name(
    render_image_texture_upload_result_diff_entry_status status)
{
    switch (status) {
    case render_image_texture_upload_result_diff_entry_status::unchanged:
        return "unchanged";
    case render_image_texture_upload_result_diff_entry_status::added:
        return "added";
    case render_image_texture_upload_result_diff_entry_status::removed:
        return "removed";
    case render_image_texture_upload_result_diff_entry_status::changed:
        return "changed";
    }

    return "unknown";
}

struct render_image_texture_upload_result_packet_diff {
    std::uint64_t request_id = 0;
    render_image_texture_upload_result_diff_entry_status status =
        render_image_texture_upload_result_diff_entry_status::unchanged;
    std::string status_name = render_image_texture_upload_result_diff_entry_status_name(
        render_image_texture_upload_result_diff_entry_status::unchanged);
    bool before_present = false;
    bool after_present = false;
    bool before_accepted = false;
    bool after_accepted = false;
    bool accepted_changed = false;
    bool accepted_to_rejected = false;
    bool rejected_to_accepted = false;
    bool texture_changed = false;
    bool cache_key_changed = false;
    bool sampler_changed = false;
    bool placeholder_changed = false;
    bool retryability_changed = false;
    bool blocker_changed = false;
    render_image_texture_staging_payload_plan_diff staging_payload_plan_diff;
    render_image_texture_upload_result_packet_status before_packet_status =
        render_image_texture_upload_result_packet_status::rejected_missing_snapshot;
    render_image_texture_upload_result_packet_status after_packet_status =
        render_image_texture_upload_result_packet_status::rejected_missing_snapshot;
    std::string before_packet_status_name;
    std::string after_packet_status_name;
    render_image_texture_id before_texture_id = 0;
    render_image_texture_id after_texture_id = 0;
    render_image_texture_key before_texture_key;
    render_image_texture_key after_texture_key;
    std::string before_stable_cache_key;
    std::string after_stable_cache_key;
    std::string before_sampler_summary;
    std::string after_sampler_summary;
    std::size_t before_mip_level_count = 0;
    std::size_t after_mip_level_count = 0;
    std::int64_t mip_level_count_delta = 0;
    std::size_t before_uploaded_byte_count = 0;
    std::size_t after_uploaded_byte_count = 0;
    std::int64_t uploaded_byte_delta = 0;
    std::size_t before_planned_mipmap_byte_count = 0;
    std::size_t after_planned_mipmap_byte_count = 0;
    std::int64_t planned_mipmap_byte_delta = 0;
    std::size_t before_staging_payload_byte_count = 0;
    std::size_t after_staging_payload_byte_count = 0;
    std::int64_t staging_payload_byte_delta = 0;
    bool before_placeholder_texture = false;
    bool after_placeholder_texture = false;
    bool before_retryable = false;
    bool after_retryable = false;
    std::string before_blocker_summary;
    std::string after_blocker_summary;
    bool regression = false;
    bool recovery = false;
    std::string diagnostic;

    bool changed() const
    {
        return status != render_image_texture_upload_result_diff_entry_status::unchanged;
    }

    bool ok() const
    {
        return !regression;
    }
};

struct render_image_texture_upload_result_snapshot_diff {
    std::size_t before_packet_count = 0;
    std::size_t after_packet_count = 0;
    std::size_t unchanged_packet_count = 0;
    std::size_t added_packet_count = 0;
    std::size_t removed_packet_count = 0;
    std::size_t changed_packet_count = 0;
    std::size_t before_accepted_packet_count = 0;
    std::size_t after_accepted_packet_count = 0;
    std::int64_t accepted_packet_delta = 0;
    std::size_t before_rejected_packet_count = 0;
    std::size_t after_rejected_packet_count = 0;
    std::int64_t rejected_packet_delta = 0;
    std::size_t accepted_to_rejected_count = 0;
    std::size_t rejected_to_accepted_count = 0;
    std::size_t texture_changed_count = 0;
    std::size_t cache_key_changed_count = 0;
    std::size_t sampler_changed_count = 0;
    std::size_t placeholder_changed_count = 0;
    std::size_t retryability_changed_count = 0;
    std::size_t blocker_changed_count = 0;
    std::size_t staging_payload_plan_changed_count = 0;
    std::size_t staging_row_copy_count_changed_count = 0;
    std::size_t staging_alignment_changed_count = 0;
    std::size_t staging_padding_changed_count = 0;
    std::size_t staging_byte_count_changed_count = 0;
    std::size_t staging_cache_key_changed_count = 0;
    std::size_t staging_sampler_changed_count = 0;
    std::size_t staging_mip_level_readiness_changed_count = 0;
    std::size_t staging_blocker_changed_count = 0;
    std::size_t staging_regression_count = 0;
    std::size_t staging_recovery_count = 0;
    std::size_t before_texture_count = 0;
    std::size_t after_texture_count = 0;
    std::int64_t texture_count_delta = 0;
    std::size_t before_placeholder_packet_count = 0;
    std::size_t after_placeholder_packet_count = 0;
    std::int64_t placeholder_packet_delta = 0;
    std::size_t before_retryable_rejected_packet_count = 0;
    std::size_t after_retryable_rejected_packet_count = 0;
    std::int64_t retryable_rejected_packet_delta = 0;
    std::size_t before_blocker_count = 0;
    std::size_t after_blocker_count = 0;
    std::int64_t blocker_count_delta = 0;
    std::size_t before_mip_level_count = 0;
    std::size_t after_mip_level_count = 0;
    std::int64_t mip_level_count_delta = 0;
    std::size_t before_uploaded_byte_count = 0;
    std::size_t after_uploaded_byte_count = 0;
    std::int64_t uploaded_byte_delta = 0;
    std::size_t before_planned_mipmap_byte_count = 0;
    std::size_t after_planned_mipmap_byte_count = 0;
    std::int64_t planned_mipmap_byte_delta = 0;
    std::size_t before_staging_payload_byte_count = 0;
    std::size_t after_staging_payload_byte_count = 0;
    std::int64_t staging_payload_byte_delta = 0;
    bool has_changes = false;
    bool has_regression = false;
    bool has_recovery = false;
    std::string changed_packet_summary;
    std::string changed_texture_summary;
    std::string staging_payload_summary;
    std::string regression_summary;
    std::vector<render_image_texture_upload_result_packet_diff> entries;
    std::string diagnostic;

    bool ok() const
    {
        return !has_regression;
    }
};

inline std::int64_t render_image_texture_upload_result_size_delta(
    std::size_t before_value,
    std::size_t after_value)
{
    constexpr auto max_delta = static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max());
    if (after_value >= before_value) {
        const std::size_t magnitude = after_value - before_value;
        return magnitude > max_delta ? std::numeric_limits<std::int64_t>::max()
                                     : static_cast<std::int64_t>(magnitude);
    }

    const std::size_t magnitude = before_value - after_value;
    return magnitude > max_delta ? std::numeric_limits<std::int64_t>::min()
                                 : -static_cast<std::int64_t>(magnitude);
}

inline const render_image_texture_upload_result_packet_snapshot*
render_image_texture_upload_result_packet_for_request_id(
    const render_image_texture_upload_result_snapshot& snapshot,
    std::uint64_t request_id)
{
    for (const render_image_texture_upload_result_packet_snapshot& packet : snapshot.packets) {
        if (packet.request_id == request_id) {
            return &packet;
        }
    }
    return nullptr;
}

inline void append_render_image_texture_upload_result_request_ids(
    std::map<std::uint64_t, bool>& request_ids,
    const render_image_texture_upload_result_snapshot& snapshot)
{
    for (const render_image_texture_upload_result_packet_snapshot& packet : snapshot.packets) {
        request_ids.emplace(packet.request_id, true);
    }
}

inline std::size_t render_image_texture_upload_result_staging_payload_byte_count(
    const render_image_texture_upload_result_snapshot& snapshot)
{
    std::size_t byte_count = 0;
    for (const render_image_texture_upload_result_packet_snapshot& packet : snapshot.packets) {
        byte_count += packet.staging_payload_plan.total_staging_byte_count;
    }
    return byte_count;
}

inline bool render_image_texture_upload_result_packet_equal(
    const render_image_texture_upload_result_packet_snapshot& before,
    const render_image_texture_upload_result_packet_snapshot& after)
{
    return before.status == after.status
        && before.upload_status == after.upload_status
        && before.texture_key == after.texture_key
        && before.stable_cache_key == after.stable_cache_key
        && before.sampler == after.sampler
        && before.sampler_summary == after.sampler_summary
        && fake_image_texture_upload_texture_handle_equal(before.texture, after.texture)
        && before.mip_level_count == after.mip_level_count
        && before.uploaded_byte_count == after.uploaded_byte_count
        && before.planned_staging_byte_count == after.planned_staging_byte_count
        && before.planned_mipmap_byte_count == after.planned_mipmap_byte_count
        && render_image_texture_upload_payload_layout_evidence_equal(
            before.payload_layout,
            after.payload_layout)
        && render_image_texture_staging_payload_plan_equal(
            before.staging_payload_plan,
            after.staging_payload_plan)
        && before.accepted == after.accepted
        && before.placeholder_texture == after.placeholder_texture
        && before.fallback_texture == after.fallback_texture
        && before.retryable == after.retryable
        && before.blocker_summary == after.blocker_summary;
}

inline render_image_texture_upload_result_packet_diff
make_render_image_texture_upload_result_packet_diff(
    const render_image_texture_upload_result_packet_snapshot* before,
    const render_image_texture_upload_result_packet_snapshot* after,
    std::uint64_t request_id)
{
    render_image_texture_upload_result_packet_diff diff{
        .request_id = request_id,
        .before_present = before != nullptr,
        .after_present = after != nullptr,
    };

    if (before != nullptr) {
        diff.before_accepted = before->accepted;
        diff.before_packet_status = before->status;
        diff.before_packet_status_name = before->status_name;
        diff.before_texture_id = before->texture_id;
        diff.before_texture_key = before->texture_key;
        diff.before_stable_cache_key = before->stable_cache_key;
        diff.before_sampler_summary = before->sampler_summary;
        diff.before_mip_level_count = before->mip_level_count;
        diff.before_uploaded_byte_count = before->uploaded_byte_count;
        diff.before_planned_mipmap_byte_count = before->planned_mipmap_byte_count;
        diff.before_staging_payload_byte_count = before->staging_payload_plan.total_staging_byte_count;
        diff.before_placeholder_texture = before->placeholder_texture;
        diff.before_retryable = before->retryable;
        diff.before_blocker_summary = before->blocker_summary;
    }
    if (after != nullptr) {
        diff.after_accepted = after->accepted;
        diff.after_packet_status = after->status;
        diff.after_packet_status_name = after->status_name;
        diff.after_texture_id = after->texture_id;
        diff.after_texture_key = after->texture_key;
        diff.after_stable_cache_key = after->stable_cache_key;
        diff.after_sampler_summary = after->sampler_summary;
        diff.after_mip_level_count = after->mip_level_count;
        diff.after_uploaded_byte_count = after->uploaded_byte_count;
        diff.after_planned_mipmap_byte_count = after->planned_mipmap_byte_count;
        diff.after_staging_payload_byte_count = after->staging_payload_plan.total_staging_byte_count;
        diff.after_placeholder_texture = after->placeholder_texture;
        diff.after_retryable = after->retryable;
        diff.after_blocker_summary = after->blocker_summary;
    }

    diff.staging_payload_plan_diff = make_render_image_texture_staging_payload_plan_diff(
        before == nullptr ? nullptr : &before->staging_payload_plan,
        after == nullptr ? nullptr : &after->staging_payload_plan);

    diff.mip_level_count_delta = render_image_texture_upload_result_size_delta(
        diff.before_mip_level_count,
        diff.after_mip_level_count);
    diff.uploaded_byte_delta = render_image_texture_upload_result_size_delta(
        diff.before_uploaded_byte_count,
        diff.after_uploaded_byte_count);
    diff.planned_mipmap_byte_delta = render_image_texture_upload_result_size_delta(
        diff.before_planned_mipmap_byte_count,
        diff.after_planned_mipmap_byte_count);
    diff.staging_payload_byte_delta = render_image_texture_upload_result_size_delta(
        diff.before_staging_payload_byte_count,
        diff.after_staging_payload_byte_count);
    diff.accepted_changed = diff.before_accepted != diff.after_accepted;
    diff.accepted_to_rejected = diff.before_present && diff.after_present
        && diff.before_accepted && !diff.after_accepted;
    diff.rejected_to_accepted = diff.before_present && diff.after_present
        && !diff.before_accepted && diff.after_accepted;
    diff.texture_changed = diff.before_texture_id != diff.after_texture_id;
    diff.cache_key_changed = diff.before_stable_cache_key != diff.after_stable_cache_key;
    diff.sampler_changed = diff.before_sampler_summary != diff.after_sampler_summary;
    diff.placeholder_changed = diff.before_placeholder_texture != diff.after_placeholder_texture;
    diff.retryability_changed = diff.before_retryable != diff.after_retryable;
    diff.blocker_changed = diff.before_blocker_summary != diff.after_blocker_summary;
    diff.regression = diff.accepted_to_rejected;
    diff.recovery = diff.rejected_to_accepted;
    if (diff.staging_payload_plan_diff.regression) {
        diff.regression = true;
    }
    if (diff.staging_payload_plan_diff.recovery) {
        diff.recovery = true;
    }

    if (before == nullptr && after != nullptr) {
        diff.status = render_image_texture_upload_result_diff_entry_status::added;
    } else if (before != nullptr && after == nullptr) {
        diff.status = render_image_texture_upload_result_diff_entry_status::removed;
        diff.regression = before->accepted;
    } else if (before != nullptr && after != nullptr
        && !render_image_texture_upload_result_packet_equal(*before, *after)) {
        diff.status = render_image_texture_upload_result_diff_entry_status::changed;
    } else {
        diff.status = render_image_texture_upload_result_diff_entry_status::unchanged;
    }
    diff.status_name = render_image_texture_upload_result_diff_entry_status_name(diff.status);

    if (diff.status == render_image_texture_upload_result_diff_entry_status::added) {
        diff.diagnostic = "texture upload result packet was added";
    } else if (diff.status == render_image_texture_upload_result_diff_entry_status::removed) {
        diff.diagnostic = "texture upload result packet was removed";
    } else if (diff.status == render_image_texture_upload_result_diff_entry_status::changed) {
        diff.diagnostic = diff.texture_changed
            ? "texture upload result packet changed texture handle"
            : "texture upload result packet changed";
    } else {
        diff.diagnostic = "texture upload result packet is unchanged";
    }
    return diff;
}

inline render_image_texture_upload_result_snapshot_diff
diff_render_image_texture_upload_result_snapshots(
    const render_image_texture_upload_result_snapshot& before,
    const render_image_texture_upload_result_snapshot& after)
{
    render_image_texture_upload_result_snapshot_diff diff{
        .before_packet_count = before.packet_count,
        .after_packet_count = after.packet_count,
        .before_accepted_packet_count = before.accepted_packet_count,
        .after_accepted_packet_count = after.accepted_packet_count,
        .accepted_packet_delta = render_image_texture_upload_result_size_delta(
            before.accepted_packet_count,
            after.accepted_packet_count),
        .before_rejected_packet_count = before.rejected_packet_count,
        .after_rejected_packet_count = after.rejected_packet_count,
        .rejected_packet_delta = render_image_texture_upload_result_size_delta(
            before.rejected_packet_count,
            after.rejected_packet_count),
        .before_texture_count = before.texture_count,
        .after_texture_count = after.texture_count,
        .texture_count_delta = render_image_texture_upload_result_size_delta(
            before.texture_count,
            after.texture_count),
        .before_placeholder_packet_count = before.placeholder_packet_count,
        .after_placeholder_packet_count = after.placeholder_packet_count,
        .placeholder_packet_delta = render_image_texture_upload_result_size_delta(
            before.placeholder_packet_count,
            after.placeholder_packet_count),
        .before_retryable_rejected_packet_count = before.retryable_rejected_packet_count,
        .after_retryable_rejected_packet_count = after.retryable_rejected_packet_count,
        .retryable_rejected_packet_delta = render_image_texture_upload_result_size_delta(
            before.retryable_rejected_packet_count,
            after.retryable_rejected_packet_count),
        .before_blocker_count = before.blocker_count,
        .after_blocker_count = after.blocker_count,
        .blocker_count_delta = render_image_texture_upload_result_size_delta(
            before.blocker_count,
            after.blocker_count),
        .before_mip_level_count = before.total_mip_level_count,
        .after_mip_level_count = after.total_mip_level_count,
        .mip_level_count_delta = render_image_texture_upload_result_size_delta(
            before.total_mip_level_count,
            after.total_mip_level_count),
        .before_uploaded_byte_count = before.total_uploaded_byte_count,
        .after_uploaded_byte_count = after.total_uploaded_byte_count,
        .uploaded_byte_delta = render_image_texture_upload_result_size_delta(
            before.total_uploaded_byte_count,
            after.total_uploaded_byte_count),
        .before_planned_mipmap_byte_count = before.total_planned_mipmap_byte_count,
        .after_planned_mipmap_byte_count = after.total_planned_mipmap_byte_count,
        .planned_mipmap_byte_delta = render_image_texture_upload_result_size_delta(
            before.total_planned_mipmap_byte_count,
            after.total_planned_mipmap_byte_count),
        .before_staging_payload_byte_count =
            render_image_texture_upload_result_staging_payload_byte_count(before),
        .after_staging_payload_byte_count =
            render_image_texture_upload_result_staging_payload_byte_count(after),
    };
    diff.staging_payload_byte_delta = render_image_texture_upload_result_size_delta(
        diff.before_staging_payload_byte_count,
        diff.after_staging_payload_byte_count);

    std::map<std::uint64_t, bool> request_ids;
    append_render_image_texture_upload_result_request_ids(request_ids, before);
    append_render_image_texture_upload_result_request_ids(request_ids, after);

    for (const auto& [request_id, _] : request_ids) {
        render_image_texture_upload_result_packet_diff entry =
            make_render_image_texture_upload_result_packet_diff(
                render_image_texture_upload_result_packet_for_request_id(before, request_id),
                render_image_texture_upload_result_packet_for_request_id(after, request_id),
                request_id);

        switch (entry.status) {
        case render_image_texture_upload_result_diff_entry_status::unchanged:
            ++diff.unchanged_packet_count;
            break;
        case render_image_texture_upload_result_diff_entry_status::added:
            ++diff.added_packet_count;
            break;
        case render_image_texture_upload_result_diff_entry_status::removed:
            ++diff.removed_packet_count;
            break;
        case render_image_texture_upload_result_diff_entry_status::changed:
            ++diff.changed_packet_count;
            break;
        }

        if (entry.accepted_to_rejected) {
            ++diff.accepted_to_rejected_count;
        }
        if (entry.rejected_to_accepted) {
            ++diff.rejected_to_accepted_count;
        }
        if (entry.texture_changed) {
            ++diff.texture_changed_count;
            append_render_image_texture_upload_result_summary(
                diff.changed_texture_summary,
                "request=" + std::to_string(entry.request_id)
                    + ":texture=" + std::to_string(entry.before_texture_id)
                    + "->" + std::to_string(entry.after_texture_id));
        }
        if (entry.cache_key_changed) {
            ++diff.cache_key_changed_count;
        }
        if (entry.sampler_changed) {
            ++diff.sampler_changed_count;
        }
        if (entry.placeholder_changed) {
            ++diff.placeholder_changed_count;
        }
        if (entry.retryability_changed) {
            ++diff.retryability_changed_count;
        }
        if (entry.blocker_changed) {
            ++diff.blocker_changed_count;
        }
        if (entry.staging_payload_plan_diff.changed()) {
            ++diff.staging_payload_plan_changed_count;
            append_render_image_texture_upload_result_summary(
                diff.staging_payload_summary,
                "request=" + std::to_string(entry.request_id)
                    + ":staging_bytes="
                    + std::to_string(entry.before_staging_payload_byte_count)
                    + "->"
                    + std::to_string(entry.after_staging_payload_byte_count));
        }
        if (entry.staging_payload_plan_diff.row_copy_count_changed) {
            ++diff.staging_row_copy_count_changed_count;
        }
        if (entry.staging_payload_plan_diff.alignment_changed) {
            ++diff.staging_alignment_changed_count;
        }
        if (entry.staging_payload_plan_diff.padding_changed) {
            ++diff.staging_padding_changed_count;
        }
        if (entry.staging_payload_plan_diff.total_staging_byte_count_changed) {
            ++diff.staging_byte_count_changed_count;
        }
        if (entry.staging_payload_plan_diff.cache_key_changed) {
            ++diff.staging_cache_key_changed_count;
        }
        if (entry.staging_payload_plan_diff.sampler_changed) {
            ++diff.staging_sampler_changed_count;
        }
        if (entry.staging_payload_plan_diff.mip_level_readiness_changed) {
            ++diff.staging_mip_level_readiness_changed_count;
        }
        if (entry.staging_payload_plan_diff.blocker_changed) {
            ++diff.staging_blocker_changed_count;
        }
        if (entry.staging_payload_plan_diff.regression) {
            ++diff.staging_regression_count;
        }
        if (entry.staging_payload_plan_diff.recovery) {
            ++diff.staging_recovery_count;
        }
        if (entry.regression) {
            diff.has_regression = true;
            append_render_image_texture_upload_result_summary(diff.regression_summary, entry.diagnostic);
        }
        if (entry.recovery) {
            diff.has_recovery = true;
        }
        if (entry.changed()) {
            append_render_image_texture_upload_result_summary(
                diff.changed_packet_summary,
                "request=" + std::to_string(entry.request_id) + ":" + entry.status_name);
        }
        diff.entries.push_back(entry);
    }

    diff.has_changes = diff.added_packet_count != 0 || diff.removed_packet_count != 0
        || diff.changed_packet_count != 0;
    if (diff.changed_packet_summary.empty()) {
        diff.changed_packet_summary = "no upload result packet changes";
    }
    if (diff.changed_texture_summary.empty()) {
        diff.changed_texture_summary = "no texture handle changes";
    }
    if (diff.staging_payload_summary.empty()) {
        diff.staging_payload_summary = "no staging payload plan changes";
    }
    if (diff.regression_summary.empty()) {
        diff.regression_summary = "no upload result regressions";
    }
    diff.diagnostic = diff.has_changes
        ? "image texture upload result snapshots changed"
        : "image texture upload result snapshots are unchanged";
    return diff;
}

} // namespace quiz_vulkan::render
