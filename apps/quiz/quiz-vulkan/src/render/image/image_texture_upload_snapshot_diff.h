#pragma once

#include "render/image/image_texture_upload.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

enum class fake_image_texture_upload_snapshot_diff_entry_status {
    unchanged,
    added,
    removed,
    changed,
};

inline std::string fake_image_texture_upload_snapshot_diff_entry_status_name(
    fake_image_texture_upload_snapshot_diff_entry_status status)
{
    switch (status) {
    case fake_image_texture_upload_snapshot_diff_entry_status::unchanged:
        return "unchanged";
    case fake_image_texture_upload_snapshot_diff_entry_status::added:
        return "added";
    case fake_image_texture_upload_snapshot_diff_entry_status::removed:
        return "removed";
    case fake_image_texture_upload_snapshot_diff_entry_status::changed:
        return "changed";
    }

    return "unknown";
}

struct fake_image_texture_upload_snapshot_entry_diff {
    fake_image_texture_upload_generation_id generation_id = 0;
    fake_image_texture_upload_snapshot_diff_entry_status status =
        fake_image_texture_upload_snapshot_diff_entry_status::unchanged;
    std::string status_name;
    bool before_present = false;
    bool after_present = false;
    bool before_request_present = false;
    bool after_request_present = false;
    bool before_result_present = false;
    bool after_result_present = false;
    bool before_queue_present = false;
    bool after_queue_present = false;
    render_image_texture_key before_key;
    render_image_texture_key after_key;
    render_image_texture_upload_status before_upload_status =
        render_image_texture_upload_status::invalid_image;
    render_image_texture_upload_status after_upload_status =
        render_image_texture_upload_status::invalid_image;
    fake_image_texture_upload_retry_eligibility before_retry_eligibility =
        fake_image_texture_upload_retry_eligibility::ineligible;
    fake_image_texture_upload_retry_eligibility after_retry_eligibility =
        fake_image_texture_upload_retry_eligibility::ineligible;
    render_image_texture_handle before_texture;
    render_image_texture_handle after_texture;
    std::size_t before_staging_byte_count = 0;
    std::size_t after_staging_byte_count = 0;
    std::int64_t staging_byte_delta = 0;
    std::size_t before_mip_level_count = 0;
    std::size_t after_mip_level_count = 0;
    std::int64_t mip_level_count_delta = 0;
    std::size_t before_mipmap_byte_count = 0;
    std::size_t after_mipmap_byte_count = 0;
    std::int64_t mipmap_byte_delta = 0;
    render_image_texture_mipmap_upload_plan_status before_mipmap_plan_status =
        render_image_texture_mipmap_upload_plan_status::invalid_image;
    render_image_texture_mipmap_upload_plan_status after_mipmap_plan_status =
        render_image_texture_mipmap_upload_plan_status::invalid_image;
    std::size_t before_queue_depth_after_enqueue = 0;
    std::size_t after_queue_depth_after_enqueue = 0;
    std::size_t before_queue_depth_after_completion = 0;
    std::size_t after_queue_depth_after_completion = 0;
    bool request_snapshot_changed = false;
    bool result_snapshot_changed = false;
    bool queue_snapshot_changed = false;
    bool retryability_changed = false;
    bool retryable_to_nonretryable = false;
    bool nonretryable_to_retryable = false;
    bool queue_depth_regressed = false;
    bool queue_depth_recovered = false;
    bool mipmap_plan_status_changed = false;
    bool invalid_plan_transition = false;
    bool invalid_plan_regression = false;
    bool invalid_plan_recovery = false;
    bool overflow_plan_transition = false;
    bool overflow_plan_regression = false;
    bool overflow_plan_recovery = false;
    bool unsupported_plan_transition = false;
    bool unsupported_plan_regression = false;
    bool unsupported_plan_recovery = false;
    bool texture_handle_added = false;
    bool texture_handle_removed = false;
    bool texture_handle_changed = false;
    bool regression = false;
    std::string diagnostic;

    bool changed() const
    {
        return status != fake_image_texture_upload_snapshot_diff_entry_status::unchanged;
    }

    bool ok() const
    {
        return !regression;
    }
};

struct fake_image_texture_upload_snapshot_diff {
    std::size_t before_upload_count = 0;
    std::size_t after_upload_count = 0;
    std::size_t before_request_snapshot_count = 0;
    std::size_t after_request_snapshot_count = 0;
    std::size_t before_result_snapshot_count = 0;
    std::size_t after_result_snapshot_count = 0;
    std::size_t before_queue_snapshot_count = 0;
    std::size_t after_queue_snapshot_count = 0;
    std::size_t unchanged_upload_count = 0;
    std::size_t added_upload_count = 0;
    std::size_t removed_upload_count = 0;
    std::size_t changed_upload_count = 0;
    std::size_t request_snapshot_added_count = 0;
    std::size_t request_snapshot_removed_count = 0;
    std::size_t request_snapshot_changed_count = 0;
    std::size_t result_snapshot_added_count = 0;
    std::size_t result_snapshot_removed_count = 0;
    std::size_t result_snapshot_changed_count = 0;
    std::size_t queue_snapshot_added_count = 0;
    std::size_t queue_snapshot_removed_count = 0;
    std::size_t queue_snapshot_changed_count = 0;
    std::size_t before_staged_byte_count = 0;
    std::size_t after_staged_byte_count = 0;
    std::int64_t staged_byte_delta = 0;
    std::size_t before_attempted_staging_byte_count = 0;
    std::size_t after_attempted_staging_byte_count = 0;
    std::int64_t attempted_staging_byte_delta = 0;
    std::size_t before_mip_level_count = 0;
    std::size_t after_mip_level_count = 0;
    std::int64_t mip_level_count_delta = 0;
    std::size_t before_mipmap_byte_count = 0;
    std::size_t after_mipmap_byte_count = 0;
    std::int64_t mipmap_byte_delta = 0;
    std::size_t retryability_changed_count = 0;
    std::size_t retryable_to_nonretryable_count = 0;
    std::size_t nonretryable_to_retryable_count = 0;
    std::size_t queue_depth_regression_count = 0;
    std::size_t queue_depth_recovery_count = 0;
    std::size_t mipmap_plan_status_changed_count = 0;
    std::size_t invalid_plan_transition_count = 0;
    std::size_t invalid_plan_regression_count = 0;
    std::size_t invalid_plan_recovery_count = 0;
    std::size_t overflow_plan_transition_count = 0;
    std::size_t overflow_plan_regression_count = 0;
    std::size_t overflow_plan_recovery_count = 0;
    std::size_t unsupported_plan_transition_count = 0;
    std::size_t unsupported_plan_regression_count = 0;
    std::size_t unsupported_plan_recovery_count = 0;
    std::size_t texture_handle_added_count = 0;
    std::size_t texture_handle_removed_count = 0;
    std::size_t texture_handle_changed_count = 0;
    bool has_changes = false;
    bool has_regression = false;
    bool has_recovery = false;
    std::string regression_summary;
    std::vector<fake_image_texture_upload_snapshot_entry_diff> entries;
    std::string diagnostic;

    bool ok() const
    {
        return !has_regression;
    }
};

inline std::int64_t fake_image_texture_upload_snapshot_size_delta(
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

inline bool fake_image_texture_upload_texture_handle_equal(
    const render_image_texture_handle& left,
    const render_image_texture_handle& right)
{
    return left.id == right.id && left.revision == right.revision
        && left.width == right.width && left.height == right.height;
}

inline bool fake_image_texture_upload_mipmap_level_plan_equal(
    const render_image_texture_mipmap_level_upload_plan& left,
    const render_image_texture_mipmap_level_upload_plan& right)
{
    return left.level == right.level && left.width == right.width && left.height == right.height
        && left.pixel_count == right.pixel_count && left.byte_count == right.byte_count
        && left.staging_byte_offset == right.staging_byte_offset;
}

inline bool fake_image_texture_upload_mipmap_plan_equal(
    const render_image_texture_mipmap_upload_plan& left,
    const render_image_texture_mipmap_upload_plan& right)
{
    if (left.status != right.status || left.status_name != right.status_name
        || left.mipmap_mode != right.mipmap_mode || left.mipmap_mode_name != right.mipmap_mode_name
        || left.pixel_format != right.pixel_format || left.bytes_per_pixel != right.bytes_per_pixel
        || left.base_width != right.base_width || left.base_height != right.base_height
        || left.mipmaps_requested != right.mipmaps_requested
        || left.upload_plannable != right.upload_plannable
        || left.requested_mip_level_count != right.requested_mip_level_count
        || left.generated_mip_level_count != right.generated_mip_level_count
        || left.total_pixel_count != right.total_pixel_count
        || left.total_staging_byte_count != right.total_staging_byte_count
        || left.total_upload_byte_count != right.total_upload_byte_count
        || left.diagnostic != right.diagnostic || left.levels.size() != right.levels.size()) {
        return false;
    }

    for (std::size_t index = 0; index < left.levels.size(); ++index) {
        if (!fake_image_texture_upload_mipmap_level_plan_equal(left.levels[index], right.levels[index])) {
            return false;
        }
    }
    return true;
}

inline bool fake_image_texture_upload_retry_snapshot_equal(
    const fake_image_texture_upload_retry_snapshot& left,
    const fake_image_texture_upload_retry_snapshot& right)
{
    return left.generation_id == right.generation_id && left.key == right.key
        && left.status == right.status && left.eligibility == right.eligibility
        && left.attempt_count_for_key == right.attempt_count_for_key
        && left.failed_attempt_count_for_key == right.failed_attempt_count_for_key
        && left.retry_after_queue_sequence_delta == right.retry_after_queue_sequence_delta
        && left.next_retry_sequence == right.next_retry_sequence
        && left.diagnostic == right.diagnostic;
}

inline bool fake_image_texture_upload_request_snapshot_equal(
    const fake_image_texture_upload_request_snapshot& left,
    const fake_image_texture_upload_request_snapshot& right)
{
    return left.generation_id == right.generation_id && left.key == right.key
        && left.sampler == right.sampler && left.width == right.width && left.height == right.height
        && left.pixel_format == right.pixel_format && left.pixel_count == right.pixel_count
        && left.pixel_byte_count == right.pixel_byte_count
        && left.decoded_byte_count == right.decoded_byte_count
        && left.staging_byte_count == right.staging_byte_count
        && fake_image_texture_upload_mipmap_plan_equal(left.mipmap_upload_plan, right.mipmap_upload_plan)
        && left.enqueue_sequence == right.enqueue_sequence
        && left.queue_depth_before_enqueue == right.queue_depth_before_enqueue
        && left.queue_depth_after_enqueue == right.queue_depth_after_enqueue
        && left.attempt_count_for_key == right.attempt_count_for_key;
}

inline bool fake_image_texture_upload_result_snapshot_equal(
    const fake_image_texture_upload_result_snapshot& left,
    const fake_image_texture_upload_result_snapshot& right)
{
    return left.generation_id == right.generation_id && left.status == right.status
        && left.key == right.key && left.sampler == right.sampler
        && fake_image_texture_upload_texture_handle_equal(left.texture, right.texture)
        && left.pixel_count == right.pixel_count && left.pixel_byte_count == right.pixel_byte_count
        && left.decoded_byte_count == right.decoded_byte_count
        && left.staging_byte_count == right.staging_byte_count
        && fake_image_texture_upload_mipmap_plan_equal(left.mipmap_upload_plan, right.mipmap_upload_plan)
        && left.diagnostic == right.diagnostic
        && left.completion_sequence == right.completion_sequence
        && left.queue_depth_after_completion == right.queue_depth_after_completion
        && fake_image_texture_upload_retry_snapshot_equal(left.retry, right.retry);
}

inline bool fake_image_texture_upload_queue_snapshot_equal(
    const fake_image_texture_upload_queue_entry_snapshot& left,
    const fake_image_texture_upload_queue_entry_snapshot& right)
{
    return left.enqueue_sequence == right.enqueue_sequence
        && left.completion_sequence == right.completion_sequence
        && left.generation_id == right.generation_id && left.key == right.key
        && left.status == right.status
        && fake_image_texture_upload_texture_handle_equal(left.texture, right.texture)
        && left.completed == right.completed && left.staging_byte_count == right.staging_byte_count
        && fake_image_texture_upload_mipmap_plan_equal(left.mipmap_upload_plan, right.mipmap_upload_plan)
        && left.queue_depth_before_enqueue == right.queue_depth_before_enqueue
        && left.queue_depth_after_enqueue == right.queue_depth_after_enqueue
        && left.queue_depth_after_completion == right.queue_depth_after_completion
        && left.diagnostic == right.diagnostic
        && fake_image_texture_upload_retry_snapshot_equal(left.retry, right.retry);
}

inline const fake_image_texture_upload_request_snapshot*
fake_image_texture_upload_request_snapshot_for_generation(
    const fake_image_texture_upload_snapshot& snapshot,
    fake_image_texture_upload_generation_id generation_id)
{
    for (const fake_image_texture_upload_request_snapshot& request : snapshot.request_snapshots) {
        if (request.generation_id == generation_id) {
            return &request;
        }
    }
    return nullptr;
}

inline const fake_image_texture_upload_result_snapshot*
fake_image_texture_upload_result_snapshot_for_generation(
    const fake_image_texture_upload_snapshot& snapshot,
    fake_image_texture_upload_generation_id generation_id)
{
    for (const fake_image_texture_upload_result_snapshot& result : snapshot.result_snapshots) {
        if (result.generation_id == generation_id) {
            return &result;
        }
    }
    return nullptr;
}

inline const fake_image_texture_upload_queue_entry_snapshot*
fake_image_texture_upload_queue_snapshot_for_generation(
    const fake_image_texture_upload_snapshot& snapshot,
    fake_image_texture_upload_generation_id generation_id)
{
    for (const fake_image_texture_upload_queue_entry_snapshot& queue_entry : snapshot.queue_entries) {
        if (queue_entry.generation_id == generation_id) {
            return &queue_entry;
        }
    }
    return nullptr;
}

inline const fake_image_texture_upload_snapshot_entry*
fake_image_texture_upload_entry_for_generation(
    const fake_image_texture_upload_snapshot& snapshot,
    fake_image_texture_upload_generation_id generation_id)
{
    for (const fake_image_texture_upload_snapshot_entry& entry : snapshot.entries) {
        if (entry.generation_id == generation_id) {
            return &entry;
        }
    }
    return nullptr;
}

inline void append_fake_image_texture_upload_generation_ids(
    std::map<fake_image_texture_upload_generation_id, bool>& generation_ids,
    const fake_image_texture_upload_snapshot& snapshot)
{
    for (const fake_image_texture_upload_request_snapshot& request : snapshot.request_snapshots) {
        generation_ids.emplace(request.generation_id, true);
    }
    for (const fake_image_texture_upload_result_snapshot& result : snapshot.result_snapshots) {
        generation_ids.emplace(result.generation_id, true);
    }
    for (const fake_image_texture_upload_queue_entry_snapshot& queue_entry : snapshot.queue_entries) {
        generation_ids.emplace(queue_entry.generation_id, true);
    }
    for (const fake_image_texture_upload_snapshot_entry& entry : snapshot.entries) {
        generation_ids.emplace(entry.generation_id, true);
    }
}

inline bool fake_image_texture_upload_retry_eligibility_is_retryable(
    fake_image_texture_upload_retry_eligibility eligibility)
{
    return eligibility == fake_image_texture_upload_retry_eligibility::eligible;
}

inline bool fake_image_texture_upload_retry_eligibility_is_nonretryable(
    fake_image_texture_upload_retry_eligibility eligibility)
{
    return eligibility == fake_image_texture_upload_retry_eligibility::ineligible;
}

inline bool fake_image_texture_mipmap_upload_plan_status_is_invalid(
    render_image_texture_mipmap_upload_plan_status status)
{
    return status == render_image_texture_mipmap_upload_plan_status::invalid_sampler
        || status == render_image_texture_mipmap_upload_plan_status::invalid_image;
}

inline bool fake_image_texture_mipmap_upload_plan_status_is_overflow(
    render_image_texture_mipmap_upload_plan_status status)
{
    return status == render_image_texture_mipmap_upload_plan_status::overflow;
}

inline bool fake_image_texture_mipmap_upload_plan_status_is_unsupported(
    render_image_texture_mipmap_upload_plan_status status)
{
    return status == render_image_texture_mipmap_upload_plan_status::unsupported_format;
}

inline std::size_t fake_image_texture_upload_queue_depth_pressure(
    const fake_image_texture_upload_queue_entry_snapshot* queue_entry)
{
    if (queue_entry == nullptr) {
        return 0;
    }
    return queue_entry->queue_depth_after_enqueue > queue_entry->queue_depth_after_completion
        ? queue_entry->queue_depth_after_enqueue
        : queue_entry->queue_depth_after_completion;
}

inline std::size_t fake_image_texture_upload_snapshot_mip_level_count(
    const fake_image_texture_upload_snapshot& snapshot)
{
    std::size_t level_count = 0;
    for (const fake_image_texture_upload_result_snapshot& result : snapshot.result_snapshots) {
        level_count += result.mipmap_upload_plan.generated_mip_level_count;
    }
    return level_count;
}

inline std::size_t fake_image_texture_upload_snapshot_mipmap_byte_count(
    const fake_image_texture_upload_snapshot& snapshot)
{
    std::size_t byte_count = 0;
    for (const fake_image_texture_upload_result_snapshot& result : snapshot.result_snapshots) {
        byte_count += result.mipmap_upload_plan.total_upload_byte_count;
    }
    return byte_count;
}

inline void append_fake_image_texture_upload_snapshot_regression_reason(
    std::string& summary,
    const std::string& reason)
{
    if (!summary.empty()) {
        summary += "; ";
    }
    summary += reason;
}

inline fake_image_texture_upload_snapshot_entry_diff make_fake_image_texture_upload_snapshot_entry_diff(
    const fake_image_texture_upload_request_snapshot* before_request,
    const fake_image_texture_upload_request_snapshot* after_request,
    const fake_image_texture_upload_result_snapshot* before_result,
    const fake_image_texture_upload_result_snapshot* after_result,
    const fake_image_texture_upload_queue_entry_snapshot* before_queue,
    const fake_image_texture_upload_queue_entry_snapshot* after_queue,
    fake_image_texture_upload_generation_id generation_id)
{
    fake_image_texture_upload_snapshot_entry_diff diff{
        .generation_id = generation_id,
        .before_request_present = before_request != nullptr,
        .after_request_present = after_request != nullptr,
        .before_result_present = before_result != nullptr,
        .after_result_present = after_result != nullptr,
        .before_queue_present = before_queue != nullptr,
        .after_queue_present = after_queue != nullptr,
    };
    diff.before_present = diff.before_request_present || diff.before_result_present || diff.before_queue_present;
    diff.after_present = diff.after_request_present || diff.after_result_present || diff.after_queue_present;

    if (before_request != nullptr) {
        diff.before_key = before_request->key;
    }
    if (after_request != nullptr) {
        diff.after_key = after_request->key;
    }
    if (before_result != nullptr) {
        diff.before_key = before_result->key;
        diff.before_upload_status = before_result->status;
        diff.before_retry_eligibility = before_result->retry.eligibility;
        diff.before_texture = before_result->texture;
        diff.before_staging_byte_count = before_result->staging_byte_count;
        diff.before_mip_level_count = before_result->mipmap_upload_plan.generated_mip_level_count;
        diff.before_mipmap_byte_count = before_result->mipmap_upload_plan.total_upload_byte_count;
        diff.before_mipmap_plan_status = before_result->mipmap_upload_plan.status;
    } else if (before_request != nullptr) {
        diff.before_staging_byte_count = before_request->staging_byte_count;
        diff.before_mip_level_count = before_request->mipmap_upload_plan.generated_mip_level_count;
        diff.before_mipmap_byte_count = before_request->mipmap_upload_plan.total_upload_byte_count;
        diff.before_mipmap_plan_status = before_request->mipmap_upload_plan.status;
    }
    if (after_result != nullptr) {
        diff.after_key = after_result->key;
        diff.after_upload_status = after_result->status;
        diff.after_retry_eligibility = after_result->retry.eligibility;
        diff.after_texture = after_result->texture;
        diff.after_staging_byte_count = after_result->staging_byte_count;
        diff.after_mip_level_count = after_result->mipmap_upload_plan.generated_mip_level_count;
        diff.after_mipmap_byte_count = after_result->mipmap_upload_plan.total_upload_byte_count;
        diff.after_mipmap_plan_status = after_result->mipmap_upload_plan.status;
    } else if (after_request != nullptr) {
        diff.after_staging_byte_count = after_request->staging_byte_count;
        diff.after_mip_level_count = after_request->mipmap_upload_plan.generated_mip_level_count;
        diff.after_mipmap_byte_count = after_request->mipmap_upload_plan.total_upload_byte_count;
        diff.after_mipmap_plan_status = after_request->mipmap_upload_plan.status;
    }
    if (before_queue != nullptr) {
        diff.before_key = before_queue->key;
        diff.before_upload_status = before_queue->status;
        diff.before_retry_eligibility = before_queue->retry.eligibility;
        diff.before_texture = before_queue->texture;
        diff.before_queue_depth_after_enqueue = before_queue->queue_depth_after_enqueue;
        diff.before_queue_depth_after_completion = before_queue->queue_depth_after_completion;
    }
    if (after_queue != nullptr) {
        diff.after_key = after_queue->key;
        diff.after_upload_status = after_queue->status;
        diff.after_retry_eligibility = after_queue->retry.eligibility;
        diff.after_texture = after_queue->texture;
        diff.after_queue_depth_after_enqueue = after_queue->queue_depth_after_enqueue;
        diff.after_queue_depth_after_completion = after_queue->queue_depth_after_completion;
    }

    diff.staging_byte_delta = fake_image_texture_upload_snapshot_size_delta(
        diff.before_staging_byte_count,
        diff.after_staging_byte_count);
    diff.mip_level_count_delta = fake_image_texture_upload_snapshot_size_delta(
        diff.before_mip_level_count,
        diff.after_mip_level_count);
    diff.mipmap_byte_delta = fake_image_texture_upload_snapshot_size_delta(
        diff.before_mipmap_byte_count,
        diff.after_mipmap_byte_count);

    if (!diff.before_present && diff.after_present) {
        diff.status = fake_image_texture_upload_snapshot_diff_entry_status::added;
        diff.texture_handle_added = diff.after_texture.valid();
    } else if (diff.before_present && !diff.after_present) {
        diff.status = fake_image_texture_upload_snapshot_diff_entry_status::removed;
        diff.texture_handle_removed = diff.before_texture.valid();
    } else if (diff.before_present && diff.after_present) {
        diff.request_snapshot_changed = before_request != nullptr && after_request != nullptr
            && !fake_image_texture_upload_request_snapshot_equal(*before_request, *after_request);
        diff.result_snapshot_changed = before_result != nullptr && after_result != nullptr
            && !fake_image_texture_upload_result_snapshot_equal(*before_result, *after_result);
        diff.queue_snapshot_changed = before_queue != nullptr && after_queue != nullptr
            && !fake_image_texture_upload_queue_snapshot_equal(*before_queue, *after_queue);
        diff.texture_handle_added = !diff.before_texture.valid() && diff.after_texture.valid();
        diff.texture_handle_removed = diff.before_texture.valid() && !diff.after_texture.valid();
        diff.texture_handle_changed = diff.before_texture.valid() && diff.after_texture.valid()
            && !fake_image_texture_upload_texture_handle_equal(diff.before_texture, diff.after_texture);
    }

    if (diff.before_present && diff.after_present) {
        diff.retryability_changed = diff.before_retry_eligibility != diff.after_retry_eligibility;
        diff.retryable_to_nonretryable =
            fake_image_texture_upload_retry_eligibility_is_retryable(diff.before_retry_eligibility)
            && fake_image_texture_upload_retry_eligibility_is_nonretryable(diff.after_retry_eligibility);
        diff.nonretryable_to_retryable =
            fake_image_texture_upload_retry_eligibility_is_nonretryable(diff.before_retry_eligibility)
            && fake_image_texture_upload_retry_eligibility_is_retryable(diff.after_retry_eligibility);
        const std::size_t before_queue_pressure = fake_image_texture_upload_queue_depth_pressure(before_queue);
        const std::size_t after_queue_pressure = fake_image_texture_upload_queue_depth_pressure(after_queue);
        diff.queue_depth_regressed = after_queue_pressure > before_queue_pressure;
        diff.queue_depth_recovered = after_queue_pressure < before_queue_pressure;
        diff.mipmap_plan_status_changed = diff.before_mipmap_plan_status != diff.after_mipmap_plan_status;

        const bool before_invalid = fake_image_texture_mipmap_upload_plan_status_is_invalid(
            diff.before_mipmap_plan_status);
        const bool after_invalid = fake_image_texture_mipmap_upload_plan_status_is_invalid(
            diff.after_mipmap_plan_status);
        diff.invalid_plan_transition = before_invalid != after_invalid;
        diff.invalid_plan_regression = !before_invalid && after_invalid;
        diff.invalid_plan_recovery = before_invalid && !after_invalid;

        const bool before_overflow = fake_image_texture_mipmap_upload_plan_status_is_overflow(
            diff.before_mipmap_plan_status);
        const bool after_overflow = fake_image_texture_mipmap_upload_plan_status_is_overflow(
            diff.after_mipmap_plan_status);
        diff.overflow_plan_transition = before_overflow != after_overflow;
        diff.overflow_plan_regression = !before_overflow && after_overflow;
        diff.overflow_plan_recovery = before_overflow && !after_overflow;

        const bool before_unsupported = fake_image_texture_mipmap_upload_plan_status_is_unsupported(
            diff.before_mipmap_plan_status);
        const bool after_unsupported = fake_image_texture_mipmap_upload_plan_status_is_unsupported(
            diff.after_mipmap_plan_status);
        diff.unsupported_plan_transition = before_unsupported != after_unsupported;
        diff.unsupported_plan_regression = !before_unsupported && after_unsupported;
        diff.unsupported_plan_recovery = before_unsupported && !after_unsupported;
    }

    if (diff.status == fake_image_texture_upload_snapshot_diff_entry_status::unchanged
        && (diff.request_snapshot_changed || diff.result_snapshot_changed || diff.queue_snapshot_changed
            || diff.retryability_changed || diff.queue_depth_regressed || diff.queue_depth_recovered
            || diff.mipmap_plan_status_changed || diff.texture_handle_added
            || diff.texture_handle_removed || diff.texture_handle_changed)) {
        diff.status = fake_image_texture_upload_snapshot_diff_entry_status::changed;
    }

    diff.regression = diff.retryable_to_nonretryable || diff.queue_depth_regressed
        || diff.invalid_plan_regression || diff.overflow_plan_regression
        || diff.unsupported_plan_regression || diff.texture_handle_removed;
    diff.status_name = fake_image_texture_upload_snapshot_diff_entry_status_name(diff.status);
    diff.diagnostic = diff.status == fake_image_texture_upload_snapshot_diff_entry_status::unchanged
        ? "fake image texture upload snapshot entry unchanged"
        : (diff.regression
            ? "fake image texture upload snapshot entry changed with regression"
            : "fake image texture upload snapshot entry changed");
    return diff;
}

inline fake_image_texture_upload_snapshot_diff diff_fake_image_texture_upload_snapshots(
    const fake_image_texture_upload_snapshot& before,
    const fake_image_texture_upload_snapshot& after)
{
    fake_image_texture_upload_snapshot_diff diff{
        .before_upload_count = before.upload_count,
        .after_upload_count = after.upload_count,
        .before_request_snapshot_count = before.request_snapshots.size(),
        .after_request_snapshot_count = after.request_snapshots.size(),
        .before_result_snapshot_count = before.result_snapshots.size(),
        .after_result_snapshot_count = after.result_snapshots.size(),
        .before_queue_snapshot_count = before.queue_entries.size(),
        .after_queue_snapshot_count = after.queue_entries.size(),
        .before_staged_byte_count = before.staged_byte_count,
        .after_staged_byte_count = after.staged_byte_count,
        .staged_byte_delta = fake_image_texture_upload_snapshot_size_delta(
            before.staged_byte_count,
            after.staged_byte_count),
        .before_attempted_staging_byte_count = before.attempted_staging_byte_count,
        .after_attempted_staging_byte_count = after.attempted_staging_byte_count,
        .attempted_staging_byte_delta = fake_image_texture_upload_snapshot_size_delta(
            before.attempted_staging_byte_count,
            after.attempted_staging_byte_count),
        .before_mip_level_count = fake_image_texture_upload_snapshot_mip_level_count(before),
        .after_mip_level_count = fake_image_texture_upload_snapshot_mip_level_count(after),
        .before_mipmap_byte_count = fake_image_texture_upload_snapshot_mipmap_byte_count(before),
        .after_mipmap_byte_count = fake_image_texture_upload_snapshot_mipmap_byte_count(after),
    };
    diff.mip_level_count_delta = fake_image_texture_upload_snapshot_size_delta(
        diff.before_mip_level_count,
        diff.after_mip_level_count);
    diff.mipmap_byte_delta = fake_image_texture_upload_snapshot_size_delta(
        diff.before_mipmap_byte_count,
        diff.after_mipmap_byte_count);

    std::map<fake_image_texture_upload_generation_id, bool> generation_ids;
    append_fake_image_texture_upload_generation_ids(generation_ids, before);
    append_fake_image_texture_upload_generation_ids(generation_ids, after);

    for (const auto& [generation_id, _] : generation_ids) {
        const fake_image_texture_upload_request_snapshot* before_request =
            fake_image_texture_upload_request_snapshot_for_generation(before, generation_id);
        const fake_image_texture_upload_request_snapshot* after_request =
            fake_image_texture_upload_request_snapshot_for_generation(after, generation_id);
        const fake_image_texture_upload_result_snapshot* before_result =
            fake_image_texture_upload_result_snapshot_for_generation(before, generation_id);
        const fake_image_texture_upload_result_snapshot* after_result =
            fake_image_texture_upload_result_snapshot_for_generation(after, generation_id);
        const fake_image_texture_upload_queue_entry_snapshot* before_queue =
            fake_image_texture_upload_queue_snapshot_for_generation(before, generation_id);
        const fake_image_texture_upload_queue_entry_snapshot* after_queue =
            fake_image_texture_upload_queue_snapshot_for_generation(after, generation_id);

        fake_image_texture_upload_snapshot_entry_diff entry_diff =
            make_fake_image_texture_upload_snapshot_entry_diff(
                before_request,
                after_request,
                before_result,
                after_result,
                before_queue,
                after_queue,
                generation_id);

        switch (entry_diff.status) {
        case fake_image_texture_upload_snapshot_diff_entry_status::unchanged:
            ++diff.unchanged_upload_count;
            break;
        case fake_image_texture_upload_snapshot_diff_entry_status::added:
            ++diff.added_upload_count;
            break;
        case fake_image_texture_upload_snapshot_diff_entry_status::removed:
            ++diff.removed_upload_count;
            break;
        case fake_image_texture_upload_snapshot_diff_entry_status::changed:
            ++diff.changed_upload_count;
            break;
        }

        if (!entry_diff.before_request_present && entry_diff.after_request_present) {
            ++diff.request_snapshot_added_count;
        } else if (entry_diff.before_request_present && !entry_diff.after_request_present) {
            ++diff.request_snapshot_removed_count;
        } else if (entry_diff.request_snapshot_changed) {
            ++diff.request_snapshot_changed_count;
        }
        if (!entry_diff.before_result_present && entry_diff.after_result_present) {
            ++diff.result_snapshot_added_count;
        } else if (entry_diff.before_result_present && !entry_diff.after_result_present) {
            ++diff.result_snapshot_removed_count;
        } else if (entry_diff.result_snapshot_changed) {
            ++diff.result_snapshot_changed_count;
        }
        if (!entry_diff.before_queue_present && entry_diff.after_queue_present) {
            ++diff.queue_snapshot_added_count;
        } else if (entry_diff.before_queue_present && !entry_diff.after_queue_present) {
            ++diff.queue_snapshot_removed_count;
        } else if (entry_diff.queue_snapshot_changed) {
            ++diff.queue_snapshot_changed_count;
        }
        if (entry_diff.retryability_changed) {
            ++diff.retryability_changed_count;
        }
        if (entry_diff.retryable_to_nonretryable) {
            ++diff.retryable_to_nonretryable_count;
        }
        if (entry_diff.nonretryable_to_retryable) {
            ++diff.nonretryable_to_retryable_count;
        }
        if (entry_diff.queue_depth_regressed) {
            ++diff.queue_depth_regression_count;
        }
        if (entry_diff.queue_depth_recovered) {
            ++diff.queue_depth_recovery_count;
        }
        if (entry_diff.mipmap_plan_status_changed) {
            ++diff.mipmap_plan_status_changed_count;
        }
        if (entry_diff.invalid_plan_transition) {
            ++diff.invalid_plan_transition_count;
        }
        if (entry_diff.invalid_plan_regression) {
            ++diff.invalid_plan_regression_count;
        }
        if (entry_diff.invalid_plan_recovery) {
            ++diff.invalid_plan_recovery_count;
        }
        if (entry_diff.overflow_plan_transition) {
            ++diff.overflow_plan_transition_count;
        }
        if (entry_diff.overflow_plan_regression) {
            ++diff.overflow_plan_regression_count;
        }
        if (entry_diff.overflow_plan_recovery) {
            ++diff.overflow_plan_recovery_count;
        }
        if (entry_diff.unsupported_plan_transition) {
            ++diff.unsupported_plan_transition_count;
        }
        if (entry_diff.unsupported_plan_regression) {
            ++diff.unsupported_plan_regression_count;
        }
        if (entry_diff.unsupported_plan_recovery) {
            ++diff.unsupported_plan_recovery_count;
        }
        if (entry_diff.texture_handle_added) {
            ++diff.texture_handle_added_count;
        }
        if (entry_diff.texture_handle_removed) {
            ++diff.texture_handle_removed_count;
        }
        if (entry_diff.texture_handle_changed) {
            ++diff.texture_handle_changed_count;
        }
        if (entry_diff.regression) {
            diff.has_regression = true;
        }
        if (entry_diff.queue_depth_recovered || entry_diff.nonretryable_to_retryable
            || entry_diff.invalid_plan_recovery || entry_diff.overflow_plan_recovery
            || entry_diff.unsupported_plan_recovery || entry_diff.texture_handle_added) {
            diff.has_recovery = true;
        }

        diff.entries.push_back(std::move(entry_diff));
    }

    diff.has_changes = diff.added_upload_count != 0 || diff.removed_upload_count != 0
        || diff.changed_upload_count != 0 || diff.staged_byte_delta != 0
        || diff.attempted_staging_byte_delta != 0 || diff.mip_level_count_delta != 0
        || diff.mipmap_byte_delta != 0 || diff.request_snapshot_added_count != 0
        || diff.request_snapshot_removed_count != 0 || diff.request_snapshot_changed_count != 0
        || diff.result_snapshot_added_count != 0 || diff.result_snapshot_removed_count != 0
        || diff.result_snapshot_changed_count != 0 || diff.queue_snapshot_added_count != 0
        || diff.queue_snapshot_removed_count != 0 || diff.queue_snapshot_changed_count != 0;

    if (diff.retryable_to_nonretryable_count != 0) {
        append_fake_image_texture_upload_snapshot_regression_reason(
            diff.regression_summary,
            "retryable uploads became nonretryable");
    }
    if (diff.queue_depth_regression_count != 0) {
        append_fake_image_texture_upload_snapshot_regression_reason(
            diff.regression_summary,
            "queue depth increased");
    }
    if (diff.invalid_plan_regression_count != 0) {
        append_fake_image_texture_upload_snapshot_regression_reason(
            diff.regression_summary,
            "mipmap plans became invalid");
    }
    if (diff.overflow_plan_regression_count != 0) {
        append_fake_image_texture_upload_snapshot_regression_reason(
            diff.regression_summary,
            "mipmap plans overflowed");
    }
    if (diff.unsupported_plan_regression_count != 0) {
        append_fake_image_texture_upload_snapshot_regression_reason(
            diff.regression_summary,
            "mipmap plans became unsupported");
    }
    if (diff.texture_handle_removed_count != 0) {
        append_fake_image_texture_upload_snapshot_regression_reason(
            diff.regression_summary,
            "texture handles were removed");
    }
    if (diff.regression_summary.empty()) {
        diff.regression_summary = diff.has_changes
            ? "fake image texture upload snapshot diff has changes without regressions"
            : "fake image texture upload snapshot diff has no changes";
    }

    diff.diagnostic = diff.has_regression
        ? "fake image texture upload snapshot diff reports regressions"
        : (diff.has_changes
            ? "fake image texture upload snapshot diff reports changes"
            : "fake image texture upload snapshot diff is unchanged");
    return diff;
}

} // namespace quiz_vulkan::render
