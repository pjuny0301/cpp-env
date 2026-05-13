#pragma once

#include "render/image/image_texture_frame_resource_packet_materialization.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

enum class render_image_texture_frame_resource_materialization_diff_entry_status {
    unchanged,
    added,
    removed,
    changed,
};

enum class render_image_texture_frame_resource_materialization_change_classification {
    neutral,
    regression,
    improvement,
    churn,
};

inline std::string render_image_texture_frame_resource_materialization_diff_entry_status_name(
    render_image_texture_frame_resource_materialization_diff_entry_status status)
{
    switch (status) {
    case render_image_texture_frame_resource_materialization_diff_entry_status::unchanged:
        return "unchanged";
    case render_image_texture_frame_resource_materialization_diff_entry_status::added:
        return "added";
    case render_image_texture_frame_resource_materialization_diff_entry_status::removed:
        return "removed";
    case render_image_texture_frame_resource_materialization_diff_entry_status::changed:
        return "changed";
    }

    return "unknown";
}

inline std::string render_image_texture_frame_resource_materialization_change_classification_name(
    render_image_texture_frame_resource_materialization_change_classification classification)
{
    switch (classification) {
    case render_image_texture_frame_resource_materialization_change_classification::neutral:
        return "neutral";
    case render_image_texture_frame_resource_materialization_change_classification::regression:
        return "regression";
    case render_image_texture_frame_resource_materialization_change_classification::improvement:
        return "improvement";
    case render_image_texture_frame_resource_materialization_change_classification::churn:
        return "churn";
    }

    return "unknown";
}

struct render_image_texture_frame_resource_cache_handoff_delta {
    std::size_t request_index = 0;
    bool before_present = false;
    bool after_present = false;
    std::string before_render_image_uri;
    std::string after_render_image_uri;
    std::string before_normalized_uri;
    std::string after_normalized_uri;
    render_image_cache_key before_cache_key;
    render_image_cache_key after_cache_key;
    render_image_texture_key before_texture_key;
    render_image_texture_key after_texture_key;
    std::string before_stable_texture_cache_key;
    std::string after_stable_texture_cache_key;
    render_image_texture_id before_texture_id = 0;
    render_image_texture_id after_texture_id = 0;
    render_image_revision before_texture_revision = 0;
    render_image_revision after_texture_revision = 0;
    std::size_t before_texture_width = 0;
    std::size_t after_texture_width = 0;
    std::size_t before_texture_height = 0;
    std::size_t after_texture_height = 0;
    bool before_placeholder_backed = false;
    bool after_placeholder_backed = false;
    bool before_cache_reused = false;
    bool after_cache_reused = false;
    bool before_expected_cache_reuse = false;
    bool after_expected_cache_reuse = false;
    bool before_renderer_boundary_ready = false;
    bool after_renderer_boundary_ready = false;
    bool cache_key_changed = false;
    bool stable_texture_cache_key_changed = false;
    bool texture_key_changed = false;
    bool texture_handle_changed = false;
    bool texture_size_changed = false;
    bool placeholder_changed = false;
    bool cache_reuse_changed = false;
    bool renderer_boundary_ready_changed = false;
    bool changed = false;
    std::string diagnostic;

    bool ok() const
    {
        return !after_present || after_renderer_boundary_ready;
    }
};

struct render_image_texture_frame_resource_upload_handoff_delta {
    std::size_t request_index = 0;
    bool before_present = false;
    bool after_present = false;
    std::uint64_t before_upload_request_id = 0;
    std::uint64_t after_upload_request_id = 0;
    std::uint64_t before_upload_generation_id = 0;
    std::uint64_t after_upload_generation_id = 0;
    std::size_t before_mip_level_count = 0;
    std::size_t after_mip_level_count = 0;
    std::int64_t mip_level_count_delta = 0;
    std::size_t before_uploaded_byte_count = 0;
    std::size_t after_uploaded_byte_count = 0;
    std::int64_t uploaded_byte_delta = 0;
    bool before_upload_result_present = false;
    bool after_upload_result_present = false;
    bool before_placeholder_backed = false;
    bool after_placeholder_backed = false;
    bool before_renderer_boundary_ready = false;
    bool after_renderer_boundary_ready = false;
    bool upload_request_changed = false;
    bool upload_generation_changed = false;
    bool upload_result_presence_changed = false;
    bool mip_level_changed = false;
    bool uploaded_byte_changed = false;
    bool placeholder_changed = false;
    bool renderer_boundary_ready_changed = false;
    bool changed = false;
    std::string diagnostic;

    bool ok() const
    {
        return !after_present || after_renderer_boundary_ready;
    }
};

struct render_image_texture_frame_resource_sampler_handoff_delta {
    std::size_t request_index = 0;
    bool before_present = false;
    bool after_present = false;
    render_image_sampler_policy before_sampler;
    render_image_sampler_policy after_sampler;
    std::string before_sampler_key;
    std::string after_sampler_key;
    std::string before_sampler_summary;
    std::string after_sampler_summary;
    bool before_placeholder_backed = false;
    bool after_placeholder_backed = false;
    bool before_renderer_boundary_ready = false;
    bool after_renderer_boundary_ready = false;
    bool sampler_changed = false;
    bool sampler_key_changed = false;
    bool sampler_summary_changed = false;
    bool placeholder_changed = false;
    bool renderer_boundary_ready_changed = false;
    bool changed = false;
    std::string diagnostic;

    bool ok() const
    {
        return !after_present || after_renderer_boundary_ready;
    }
};

struct render_image_texture_frame_resource_materialization_entry_diff {
    std::size_t request_index = 0;
    render_image_texture_frame_resource_materialization_diff_entry_status status =
        render_image_texture_frame_resource_materialization_diff_entry_status::unchanged;
    std::string status_name = render_image_texture_frame_resource_materialization_diff_entry_status_name(
        render_image_texture_frame_resource_materialization_diff_entry_status::unchanged);
    bool before_present = false;
    bool after_present = false;
    render_image_texture_frame_resource_packet_materialization_status before_materialization_status =
        render_image_texture_frame_resource_packet_materialization_status::blocked;
    render_image_texture_frame_resource_packet_materialization_status after_materialization_status =
        render_image_texture_frame_resource_packet_materialization_status::blocked;
    std::string before_materialization_status_name;
    std::string after_materialization_status_name;
    render_image_texture_frame_resource_packet_status before_packet_status =
        render_image_texture_frame_resource_packet_status::blocked;
    render_image_texture_frame_resource_packet_status after_packet_status =
        render_image_texture_frame_resource_packet_status::blocked;
    std::string before_packet_status_name;
    std::string after_packet_status_name;
    bool before_materialized = false;
    bool after_materialized = false;
    bool before_placeholder_backed = false;
    bool after_placeholder_backed = false;
    bool before_blocked = false;
    bool after_blocked = false;
    bool before_removed = false;
    bool after_removed = false;
    bool before_missing_upload_result = false;
    bool after_missing_upload_result = false;
    bool before_missing_frame_binding = false;
    bool after_missing_frame_binding = false;
    bool before_retry_backoff_blocked = false;
    bool after_retry_backoff_blocked = false;
    bool materialization_status_changed = false;
    bool packet_status_changed = false;
    bool materialized_changed = false;
    bool placeholder_changed = false;
    bool blocker_changed = false;
    bool removed_changed = false;
    bool missing_upload_result_changed = false;
    bool missing_frame_binding_changed = false;
    bool retry_backoff_changed = false;
    bool cache_handoff_changed = false;
    bool upload_handoff_changed = false;
    bool sampler_handoff_changed = false;
    render_image_texture_frame_resource_cache_handoff_delta cache_delta;
    render_image_texture_frame_resource_upload_handoff_delta upload_delta;
    render_image_texture_frame_resource_sampler_handoff_delta sampler_delta;
    bool regression = false;
    bool recovery = false;
    bool improvement = false;
    bool churn = false;
    bool placeholder_to_real = false;
    bool real_to_placeholder = false;
    bool cache_key_churn = false;
    bool upload_handoff_lost = false;
    bool upload_handoff_gained = false;
    bool sampler_policy_churn = false;
    bool materialization_failure_added = false;
    bool materialization_failure_removed = false;
    bool materialization_failure_changed = false;
    render_image_texture_frame_resource_materialization_change_classification classification =
        render_image_texture_frame_resource_materialization_change_classification::neutral;
    std::string classification_name = render_image_texture_frame_resource_materialization_change_classification_name(
        render_image_texture_frame_resource_materialization_change_classification::neutral);
    std::string classification_reason;
    std::string before_blocker_summary;
    std::string after_blocker_summary;
    std::string diagnostic;

    bool changed() const
    {
        return status != render_image_texture_frame_resource_materialization_diff_entry_status::unchanged;
    }

    bool ok() const
    {
        return !regression;
    }
};

struct render_image_texture_frame_resource_materialization_diff {
    std::size_t before_frame_request_count = 0;
    std::size_t after_frame_request_count = 0;
    std::int64_t frame_request_delta = 0;
    std::size_t before_planned_packet_count = 0;
    std::size_t after_planned_packet_count = 0;
    std::int64_t planned_packet_delta = 0;
    std::size_t before_materialized_packet_count = 0;
    std::size_t after_materialized_packet_count = 0;
    std::int64_t materialized_packet_delta = 0;
    std::size_t before_placeholder_packet_count = 0;
    std::size_t after_placeholder_packet_count = 0;
    std::int64_t placeholder_packet_delta = 0;
    std::size_t before_blocked_packet_count = 0;
    std::size_t after_blocked_packet_count = 0;
    std::int64_t blocked_packet_delta = 0;
    std::size_t before_removed_packet_count = 0;
    std::size_t after_removed_packet_count = 0;
    std::int64_t removed_packet_delta = 0;
    std::size_t before_missing_upload_result_count = 0;
    std::size_t after_missing_upload_result_count = 0;
    std::int64_t missing_upload_result_delta = 0;
    std::size_t before_missing_frame_binding_count = 0;
    std::size_t after_missing_frame_binding_count = 0;
    std::int64_t missing_frame_binding_delta = 0;
    std::size_t before_retry_backoff_blocked_count = 0;
    std::size_t after_retry_backoff_blocked_count = 0;
    std::int64_t retry_backoff_blocked_delta = 0;
    std::size_t before_cache_record_count = 0;
    std::size_t after_cache_record_count = 0;
    std::int64_t cache_record_delta = 0;
    std::size_t before_upload_record_count = 0;
    std::size_t after_upload_record_count = 0;
    std::int64_t upload_record_delta = 0;
    std::size_t before_sampler_record_count = 0;
    std::size_t after_sampler_record_count = 0;
    std::int64_t sampler_record_delta = 0;
    std::size_t before_uploaded_byte_count = 0;
    std::size_t after_uploaded_byte_count = 0;
    std::int64_t uploaded_byte_delta = 0;
    std::size_t unchanged_entry_count = 0;
    std::size_t added_entry_count = 0;
    std::size_t removed_entry_count = 0;
    std::size_t changed_entry_count = 0;
    std::size_t regression_entry_count = 0;
    std::size_t improvement_entry_count = 0;
    std::size_t churn_entry_count = 0;
    std::size_t materialization_status_changed_count = 0;
    std::size_t packet_status_changed_count = 0;
    std::size_t materialized_changed_count = 0;
    std::size_t placeholder_changed_count = 0;
    std::size_t blocker_changed_count = 0;
    std::size_t removed_changed_count = 0;
    std::size_t missing_upload_result_changed_count = 0;
    std::size_t missing_frame_binding_changed_count = 0;
    std::size_t retry_backoff_changed_count = 0;
    std::size_t cache_handoff_changed_count = 0;
    std::size_t upload_handoff_changed_count = 0;
    std::size_t sampler_handoff_changed_count = 0;
    std::size_t cache_key_changed_count = 0;
    std::size_t stable_texture_cache_key_changed_count = 0;
    std::size_t texture_key_changed_count = 0;
    std::size_t texture_handle_changed_count = 0;
    std::size_t texture_size_changed_count = 0;
    std::size_t upload_request_changed_count = 0;
    std::size_t upload_generation_changed_count = 0;
    std::size_t upload_result_presence_changed_count = 0;
    std::size_t uploaded_byte_changed_count = 0;
    std::size_t mip_level_changed_count = 0;
    std::size_t sampler_changed_count = 0;
    std::size_t sampler_key_changed_count = 0;
    std::size_t sampler_summary_changed_count = 0;
    std::size_t placeholder_to_real_count = 0;
    std::size_t real_to_placeholder_count = 0;
    std::size_t cache_key_churn_count = 0;
    std::size_t upload_handoff_lost_count = 0;
    std::size_t upload_handoff_gained_count = 0;
    std::size_t sampler_policy_churn_count = 0;
    std::size_t materialization_failure_added_count = 0;
    std::size_t materialization_failure_removed_count = 0;
    std::size_t materialization_failure_changed_count = 0;
    bool before_renderer_boundary_ready = false;
    bool after_renderer_boundary_ready = false;
    bool renderer_boundary_regressed = false;
    bool renderer_boundary_recovered = false;
    bool has_changes = false;
    bool has_regression = false;
    bool has_recovery = false;
    bool has_improvement = false;
    bool has_churn = false;
    std::string changed_summary;
    std::string cache_handoff_delta_summary;
    std::string upload_handoff_delta_summary;
    std::string sampler_handoff_delta_summary;
    std::string regression_summary;
    std::string improvement_summary;
    std::string churn_summary;
    std::string materialization_failure_summary;
    std::string classification_summary;
    std::vector<render_image_texture_frame_resource_materialization_entry_diff> entries;
    std::string diagnostic;

    bool ok() const
    {
        return !has_regression;
    }
};

inline const render_image_texture_frame_resource_packet_materialization_entry*
render_image_texture_frame_resource_materialization_entry_for_request_index(
    const render_image_texture_frame_resource_packet_materialization& materialization,
    std::size_t request_index)
{
    for (const render_image_texture_frame_resource_packet_materialization_entry& entry : materialization.entries) {
        if (entry.request_index == request_index) {
            return &entry;
        }
    }
    return nullptr;
}

inline void append_render_image_texture_frame_resource_materialization_request_indexes(
    std::map<std::size_t, bool>& request_indexes,
    const render_image_texture_frame_resource_packet_materialization& materialization)
{
    for (const render_image_texture_frame_resource_packet_materialization_entry& entry : materialization.entries) {
        request_indexes.emplace(entry.request_index, true);
    }
}

inline std::size_t render_image_texture_frame_resource_materialization_uploaded_byte_count(
    const render_image_texture_frame_resource_packet_materialization& materialization)
{
    std::size_t uploaded_byte_count = 0;
    for (const render_image_texture_frame_resource_upload_handoff_record& record :
         materialization.upload_handoff_records) {
        uploaded_byte_count += record.uploaded_byte_count;
    }
    return uploaded_byte_count;
}

inline void append_render_image_texture_frame_resource_materialization_classification_reason(
    std::string& summary,
    const std::string& reason)
{
    append_render_image_texture_frame_upload_handoff_summary_fragment(summary, reason);
}

inline bool render_image_texture_frame_resource_cache_handoff_record_equal(
    const render_image_texture_frame_resource_cache_handoff_record& before,
    const render_image_texture_frame_resource_cache_handoff_record& after)
{
    return before.request_index == after.request_index
        && before.render_image_uri == after.render_image_uri
        && before.normalized_uri == after.normalized_uri
        && before.cache_key == after.cache_key
        && before.texture_key == after.texture_key
        && before.stable_texture_cache_key == after.stable_texture_cache_key
        && before.texture_id == after.texture_id
        && before.texture_revision == after.texture_revision
        && before.texture_width == after.texture_width
        && before.texture_height == after.texture_height
        && before.placeholder_backed == after.placeholder_backed
        && before.cache_reused == after.cache_reused
        && before.expected_cache_reuse == after.expected_cache_reuse
        && before.renderer_boundary_ready == after.renderer_boundary_ready;
}

inline bool render_image_texture_frame_resource_upload_handoff_record_equal(
    const render_image_texture_frame_resource_upload_handoff_record& before,
    const render_image_texture_frame_resource_upload_handoff_record& after)
{
    return before.request_index == after.request_index
        && before.upload_request_id == after.upload_request_id
        && before.upload_generation_id == after.upload_generation_id
        && before.mip_level_count == after.mip_level_count
        && before.uploaded_byte_count == after.uploaded_byte_count
        && before.upload_result_present == after.upload_result_present
        && before.placeholder_backed == after.placeholder_backed
        && before.renderer_boundary_ready == after.renderer_boundary_ready;
}

inline bool render_image_texture_frame_resource_sampler_handoff_record_equal(
    const render_image_texture_frame_resource_sampler_handoff_record& before,
    const render_image_texture_frame_resource_sampler_handoff_record& after)
{
    return before.request_index == after.request_index
        && before.sampler == after.sampler
        && before.sampler_key == after.sampler_key
        && before.sampler_summary == after.sampler_summary
        && before.placeholder_backed == after.placeholder_backed
        && before.renderer_boundary_ready == after.renderer_boundary_ready;
}

inline bool render_image_texture_frame_resource_materialization_entry_equal(
    const render_image_texture_frame_resource_packet_materialization_entry& before,
    const render_image_texture_frame_resource_packet_materialization_entry& after)
{
    const bool cache_records_equal = before.cache_record_present == after.cache_record_present
        && (!before.cache_record_present
            || render_image_texture_frame_resource_cache_handoff_record_equal(
                before.cache_record,
                after.cache_record));
    const bool upload_records_equal = before.upload_record_present == after.upload_record_present
        && (!before.upload_record_present
            || render_image_texture_frame_resource_upload_handoff_record_equal(
                before.upload_record,
                after.upload_record));
    const bool sampler_records_equal = before.sampler_record_present == after.sampler_record_present
        && (!before.sampler_record_present
            || render_image_texture_frame_resource_sampler_handoff_record_equal(
                before.sampler_record,
                after.sampler_record));

    return before.request_index == after.request_index
        && before.status == after.status
        && before.packet_status == after.packet_status
        && before.materialized == after.materialized
        && before.placeholder_backed == after.placeholder_backed
        && before.blocked == after.blocked
        && before.removed == after.removed
        && before.missing_upload_result == after.missing_upload_result
        && before.missing_frame_binding == after.missing_frame_binding
        && before.retry_backoff_blocked == after.retry_backoff_blocked
        && before.blocker_summary == after.blocker_summary
        && cache_records_equal
        && upload_records_equal
        && sampler_records_equal;
}

inline render_image_texture_frame_resource_cache_handoff_delta
make_render_image_texture_frame_resource_cache_handoff_delta(
    const render_image_texture_frame_resource_cache_handoff_record* before,
    const render_image_texture_frame_resource_cache_handoff_record* after,
    std::size_t request_index)
{
    render_image_texture_frame_resource_cache_handoff_delta delta{
        .request_index = request_index,
        .before_present = before != nullptr,
        .after_present = after != nullptr,
    };

    if (before != nullptr) {
        delta.before_render_image_uri = before->render_image_uri;
        delta.before_normalized_uri = before->normalized_uri;
        delta.before_cache_key = before->cache_key;
        delta.before_texture_key = before->texture_key;
        delta.before_stable_texture_cache_key = before->stable_texture_cache_key;
        delta.before_texture_id = before->texture_id;
        delta.before_texture_revision = before->texture_revision;
        delta.before_texture_width = before->texture_width;
        delta.before_texture_height = before->texture_height;
        delta.before_placeholder_backed = before->placeholder_backed;
        delta.before_cache_reused = before->cache_reused;
        delta.before_expected_cache_reuse = before->expected_cache_reuse;
        delta.before_renderer_boundary_ready = before->renderer_boundary_ready;
    }
    if (after != nullptr) {
        delta.after_render_image_uri = after->render_image_uri;
        delta.after_normalized_uri = after->normalized_uri;
        delta.after_cache_key = after->cache_key;
        delta.after_texture_key = after->texture_key;
        delta.after_stable_texture_cache_key = after->stable_texture_cache_key;
        delta.after_texture_id = after->texture_id;
        delta.after_texture_revision = after->texture_revision;
        delta.after_texture_width = after->texture_width;
        delta.after_texture_height = after->texture_height;
        delta.after_placeholder_backed = after->placeholder_backed;
        delta.after_cache_reused = after->cache_reused;
        delta.after_expected_cache_reuse = after->expected_cache_reuse;
        delta.after_renderer_boundary_ready = after->renderer_boundary_ready;
    }

    delta.cache_key_changed = delta.before_cache_key != delta.after_cache_key;
    delta.stable_texture_cache_key_changed =
        delta.before_stable_texture_cache_key != delta.after_stable_texture_cache_key;
    delta.texture_key_changed = !(delta.before_texture_key == delta.after_texture_key);
    delta.texture_handle_changed = delta.before_texture_id != delta.after_texture_id
        || delta.before_texture_revision != delta.after_texture_revision;
    delta.texture_size_changed = delta.before_texture_width != delta.after_texture_width
        || delta.before_texture_height != delta.after_texture_height;
    delta.placeholder_changed = delta.before_placeholder_backed != delta.after_placeholder_backed;
    delta.cache_reuse_changed = delta.before_cache_reused != delta.after_cache_reused
        || delta.before_expected_cache_reuse != delta.after_expected_cache_reuse;
    delta.renderer_boundary_ready_changed =
        delta.before_renderer_boundary_ready != delta.after_renderer_boundary_ready;
    delta.changed = before == nullptr
        ? after != nullptr
        : (after == nullptr
            || !render_image_texture_frame_resource_cache_handoff_record_equal(*before, *after));

    if (!delta.before_present && delta.after_present) {
        delta.diagnostic = "image frame resource cache handoff was added";
    } else if (delta.before_present && !delta.after_present) {
        delta.diagnostic = "image frame resource cache handoff was removed";
    } else if (delta.changed) {
        delta.diagnostic = delta.texture_handle_changed
            ? "image frame resource cache handoff changed texture handle"
            : "image frame resource cache handoff changed";
    } else {
        delta.diagnostic = "image frame resource cache handoff unchanged";
    }
    return delta;
}

inline render_image_texture_frame_resource_upload_handoff_delta
make_render_image_texture_frame_resource_upload_handoff_delta(
    const render_image_texture_frame_resource_upload_handoff_record* before,
    const render_image_texture_frame_resource_upload_handoff_record* after,
    std::size_t request_index)
{
    render_image_texture_frame_resource_upload_handoff_delta delta{
        .request_index = request_index,
        .before_present = before != nullptr,
        .after_present = after != nullptr,
    };

    if (before != nullptr) {
        delta.before_upload_request_id = before->upload_request_id;
        delta.before_upload_generation_id = before->upload_generation_id;
        delta.before_mip_level_count = before->mip_level_count;
        delta.before_uploaded_byte_count = before->uploaded_byte_count;
        delta.before_upload_result_present = before->upload_result_present;
        delta.before_placeholder_backed = before->placeholder_backed;
        delta.before_renderer_boundary_ready = before->renderer_boundary_ready;
    }
    if (after != nullptr) {
        delta.after_upload_request_id = after->upload_request_id;
        delta.after_upload_generation_id = after->upload_generation_id;
        delta.after_mip_level_count = after->mip_level_count;
        delta.after_uploaded_byte_count = after->uploaded_byte_count;
        delta.after_upload_result_present = after->upload_result_present;
        delta.after_placeholder_backed = after->placeholder_backed;
        delta.after_renderer_boundary_ready = after->renderer_boundary_ready;
    }

    delta.mip_level_count_delta = render_image_texture_upload_result_size_delta(
        delta.before_mip_level_count,
        delta.after_mip_level_count);
    delta.uploaded_byte_delta = render_image_texture_upload_result_size_delta(
        delta.before_uploaded_byte_count,
        delta.after_uploaded_byte_count);
    delta.upload_request_changed = delta.before_upload_request_id != delta.after_upload_request_id;
    delta.upload_generation_changed = delta.before_upload_generation_id != delta.after_upload_generation_id;
    delta.upload_result_presence_changed =
        delta.before_upload_result_present != delta.after_upload_result_present;
    delta.mip_level_changed = delta.before_mip_level_count != delta.after_mip_level_count;
    delta.uploaded_byte_changed = delta.before_uploaded_byte_count != delta.after_uploaded_byte_count;
    delta.placeholder_changed = delta.before_placeholder_backed != delta.after_placeholder_backed;
    delta.renderer_boundary_ready_changed =
        delta.before_renderer_boundary_ready != delta.after_renderer_boundary_ready;
    delta.changed = before == nullptr
        ? after != nullptr
        : (after == nullptr
            || !render_image_texture_frame_resource_upload_handoff_record_equal(*before, *after));

    if (!delta.before_present && delta.after_present) {
        delta.diagnostic = "image frame resource upload handoff was added";
    } else if (delta.before_present && !delta.after_present) {
        delta.diagnostic = "image frame resource upload handoff was removed";
    } else if (delta.changed) {
        delta.diagnostic = delta.upload_request_changed
            ? "image frame resource upload handoff changed upload request"
            : "image frame resource upload handoff changed";
    } else {
        delta.diagnostic = "image frame resource upload handoff unchanged";
    }
    return delta;
}

inline render_image_texture_frame_resource_sampler_handoff_delta
make_render_image_texture_frame_resource_sampler_handoff_delta(
    const render_image_texture_frame_resource_sampler_handoff_record* before,
    const render_image_texture_frame_resource_sampler_handoff_record* after,
    std::size_t request_index)
{
    render_image_texture_frame_resource_sampler_handoff_delta delta{
        .request_index = request_index,
        .before_present = before != nullptr,
        .after_present = after != nullptr,
    };

    if (before != nullptr) {
        delta.before_sampler = before->sampler;
        delta.before_sampler_key = before->sampler_key;
        delta.before_sampler_summary = before->sampler_summary;
        delta.before_placeholder_backed = before->placeholder_backed;
        delta.before_renderer_boundary_ready = before->renderer_boundary_ready;
    }
    if (after != nullptr) {
        delta.after_sampler = after->sampler;
        delta.after_sampler_key = after->sampler_key;
        delta.after_sampler_summary = after->sampler_summary;
        delta.after_placeholder_backed = after->placeholder_backed;
        delta.after_renderer_boundary_ready = after->renderer_boundary_ready;
    }

    delta.sampler_changed = !(delta.before_sampler == delta.after_sampler);
    delta.sampler_key_changed = delta.before_sampler_key != delta.after_sampler_key;
    delta.sampler_summary_changed = delta.before_sampler_summary != delta.after_sampler_summary;
    delta.placeholder_changed = delta.before_placeholder_backed != delta.after_placeholder_backed;
    delta.renderer_boundary_ready_changed =
        delta.before_renderer_boundary_ready != delta.after_renderer_boundary_ready;
    delta.changed = before == nullptr
        ? after != nullptr
        : (after == nullptr
            || !render_image_texture_frame_resource_sampler_handoff_record_equal(*before, *after));

    if (!delta.before_present && delta.after_present) {
        delta.diagnostic = "image frame resource sampler handoff was added";
    } else if (delta.before_present && !delta.after_present) {
        delta.diagnostic = "image frame resource sampler handoff was removed";
    } else if (delta.changed) {
        delta.diagnostic = "image frame resource sampler handoff changed";
    } else {
        delta.diagnostic = "image frame resource sampler handoff unchanged";
    }
    return delta;
}

inline render_image_texture_frame_resource_materialization_entry_diff
make_render_image_texture_frame_resource_materialization_entry_diff(
    const render_image_texture_frame_resource_packet_materialization_entry* before,
    const render_image_texture_frame_resource_packet_materialization_entry* after,
    std::size_t request_index)
{
    render_image_texture_frame_resource_materialization_entry_diff diff{
        .request_index = request_index,
        .before_present = before != nullptr,
        .after_present = after != nullptr,
    };

    const render_image_texture_frame_resource_cache_handoff_record* before_cache =
        before != nullptr && before->cache_record_present ? &before->cache_record : nullptr;
    const render_image_texture_frame_resource_cache_handoff_record* after_cache =
        after != nullptr && after->cache_record_present ? &after->cache_record : nullptr;
    const render_image_texture_frame_resource_upload_handoff_record* before_upload =
        before != nullptr && before->upload_record_present ? &before->upload_record : nullptr;
    const render_image_texture_frame_resource_upload_handoff_record* after_upload =
        after != nullptr && after->upload_record_present ? &after->upload_record : nullptr;
    const render_image_texture_frame_resource_sampler_handoff_record* before_sampler =
        before != nullptr && before->sampler_record_present ? &before->sampler_record : nullptr;
    const render_image_texture_frame_resource_sampler_handoff_record* after_sampler =
        after != nullptr && after->sampler_record_present ? &after->sampler_record : nullptr;

    diff.cache_delta = make_render_image_texture_frame_resource_cache_handoff_delta(
        before_cache,
        after_cache,
        request_index);
    diff.upload_delta = make_render_image_texture_frame_resource_upload_handoff_delta(
        before_upload,
        after_upload,
        request_index);
    diff.sampler_delta = make_render_image_texture_frame_resource_sampler_handoff_delta(
        before_sampler,
        after_sampler,
        request_index);

    if (before != nullptr) {
        diff.before_materialization_status = before->status;
        diff.before_materialization_status_name = before->status_name;
        diff.before_packet_status = before->packet_status;
        diff.before_packet_status_name = before->packet_status_name;
        diff.before_materialized = before->materialized;
        diff.before_placeholder_backed = before->placeholder_backed;
        diff.before_blocked = before->blocked;
        diff.before_removed = before->removed;
        diff.before_missing_upload_result = before->missing_upload_result;
        diff.before_missing_frame_binding = before->missing_frame_binding;
        diff.before_retry_backoff_blocked = before->retry_backoff_blocked;
        diff.before_blocker_summary = before->blocker_summary;
    }
    if (after != nullptr) {
        diff.after_materialization_status = after->status;
        diff.after_materialization_status_name = after->status_name;
        diff.after_packet_status = after->packet_status;
        diff.after_packet_status_name = after->packet_status_name;
        diff.after_materialized = after->materialized;
        diff.after_placeholder_backed = after->placeholder_backed;
        diff.after_blocked = after->blocked;
        diff.after_removed = after->removed;
        diff.after_missing_upload_result = after->missing_upload_result;
        diff.after_missing_frame_binding = after->missing_frame_binding;
        diff.after_retry_backoff_blocked = after->retry_backoff_blocked;
        diff.after_blocker_summary = after->blocker_summary;
    }

    diff.materialization_status_changed =
        diff.before_materialization_status != diff.after_materialization_status;
    diff.packet_status_changed = diff.before_packet_status != diff.after_packet_status;
    diff.materialized_changed = diff.before_materialized != diff.after_materialized;
    diff.placeholder_changed = diff.before_placeholder_backed != diff.after_placeholder_backed;
    diff.blocker_changed = diff.before_blocked != diff.after_blocked
        || diff.before_blocker_summary != diff.after_blocker_summary;
    diff.removed_changed = diff.before_removed != diff.after_removed;
    diff.missing_upload_result_changed =
        diff.before_missing_upload_result != diff.after_missing_upload_result;
    diff.missing_frame_binding_changed =
        diff.before_missing_frame_binding != diff.after_missing_frame_binding;
    diff.retry_backoff_changed = diff.before_retry_backoff_blocked != diff.after_retry_backoff_blocked;
    diff.cache_handoff_changed = diff.cache_delta.changed;
    diff.upload_handoff_changed = diff.upload_delta.changed;
    diff.sampler_handoff_changed = diff.sampler_delta.changed;
    diff.placeholder_to_real = before != nullptr && after != nullptr
        && before->placeholder_backed && after->materialized && !after->placeholder_backed;
    diff.real_to_placeholder = before != nullptr && after != nullptr
        && before->materialized && !before->placeholder_backed && after->placeholder_backed;
    diff.cache_key_churn = before_cache != nullptr && after_cache != nullptr
        && (diff.cache_delta.cache_key_changed
            || diff.cache_delta.stable_texture_cache_key_changed
            || diff.cache_delta.texture_key_changed);
    diff.upload_handoff_lost = before_upload != nullptr && after_upload == nullptr;
    diff.upload_handoff_gained = before_upload == nullptr && after_upload != nullptr;
    diff.sampler_policy_churn = before_sampler != nullptr && after_sampler != nullptr
        && (diff.sampler_delta.sampler_changed
            || diff.sampler_delta.sampler_key_changed
            || diff.sampler_delta.sampler_summary_changed);
    diff.materialization_failure_added = (before == nullptr && after != nullptr && after->blocked)
        || (before != nullptr && after != nullptr && !before->blocked && after->blocked);
    diff.materialization_failure_removed = before != nullptr && before->blocked
        && ((after != nullptr && !after->blocked) || after == nullptr);
    diff.materialization_failure_changed = before != nullptr && after != nullptr
        && before->blocked && after->blocked
        && (diff.materialization_status_changed || diff.before_blocker_summary != diff.after_blocker_summary);

    if (before == nullptr && after != nullptr) {
        diff.status = render_image_texture_frame_resource_materialization_diff_entry_status::added;
        diff.regression = after->blocked;
        diff.recovery = after->materialized && !after->placeholder_backed;
    } else if (before != nullptr && after == nullptr) {
        diff.status = render_image_texture_frame_resource_materialization_diff_entry_status::removed;
        diff.regression = before->materialized;
    } else if (before != nullptr && after != nullptr) {
        diff.regression = (before->materialized && (after->blocked || after->removed))
            || (before->materialized && !before->placeholder_backed && after->placeholder_backed)
            || (!before->blocked && after->blocked);
        diff.recovery = (before->blocked && after->materialized)
            || (before->placeholder_backed && after->materialized && !after->placeholder_backed);
        if (!render_image_texture_frame_resource_materialization_entry_equal(*before, *after)
            || diff.cache_handoff_changed
            || diff.upload_handoff_changed
            || diff.sampler_handoff_changed) {
            diff.status = render_image_texture_frame_resource_materialization_diff_entry_status::changed;
        }
    }
    append_render_image_texture_frame_resource_materialization_classification_reason(
        diff.classification_reason,
        diff.placeholder_to_real ? "placeholder became real resource" : "");
    append_render_image_texture_frame_resource_materialization_classification_reason(
        diff.classification_reason,
        diff.real_to_placeholder ? "real resource fell back to placeholder" : "");
    append_render_image_texture_frame_resource_materialization_classification_reason(
        diff.classification_reason,
        diff.cache_key_churn ? "stable cache key changed" : "");
    append_render_image_texture_frame_resource_materialization_classification_reason(
        diff.classification_reason,
        diff.upload_handoff_lost ? "upload handoff was lost" : "");
    append_render_image_texture_frame_resource_materialization_classification_reason(
        diff.classification_reason,
        diff.upload_handoff_gained ? "upload handoff was gained" : "");
    append_render_image_texture_frame_resource_materialization_classification_reason(
        diff.classification_reason,
        diff.sampler_policy_churn ? "sampler policy changed" : "");
    append_render_image_texture_frame_resource_materialization_classification_reason(
        diff.classification_reason,
        diff.materialization_failure_added ? "materialization failure was added" : "");
    append_render_image_texture_frame_resource_materialization_classification_reason(
        diff.classification_reason,
        diff.materialization_failure_removed ? "materialization failure was removed" : "");
    append_render_image_texture_frame_resource_materialization_classification_reason(
        diff.classification_reason,
        diff.materialization_failure_changed ? "materialization failure changed" : "");
    if (diff.classification_reason.empty()) {
        diff.classification_reason = diff.changed()
            ? "materialization changed without classified severity"
            : "no materialization classification changes";
    }

    if (diff.regression || diff.real_to_placeholder || diff.upload_handoff_lost
        || diff.materialization_failure_added) {
        diff.classification = render_image_texture_frame_resource_materialization_change_classification::regression;
    } else if (diff.recovery || diff.placeholder_to_real || diff.upload_handoff_gained
        || diff.materialization_failure_removed) {
        diff.classification = render_image_texture_frame_resource_materialization_change_classification::improvement;
    } else if (diff.cache_key_churn || diff.sampler_policy_churn || diff.materialization_failure_changed
        || diff.cache_handoff_changed || diff.upload_handoff_changed || diff.sampler_handoff_changed) {
        diff.classification = render_image_texture_frame_resource_materialization_change_classification::churn;
    } else {
        diff.classification = render_image_texture_frame_resource_materialization_change_classification::neutral;
    }
    diff.classification_name = render_image_texture_frame_resource_materialization_change_classification_name(
        diff.classification);
    diff.improvement =
        diff.classification == render_image_texture_frame_resource_materialization_change_classification::improvement;
    diff.churn =
        diff.classification == render_image_texture_frame_resource_materialization_change_classification::churn;
    diff.status_name = render_image_texture_frame_resource_materialization_diff_entry_status_name(diff.status);

    if (diff.status == render_image_texture_frame_resource_materialization_diff_entry_status::unchanged) {
        diff.diagnostic = "image frame resource materialization diff entry unchanged";
    } else if (diff.regression) {
        diff.diagnostic = "image frame resource materialization diff entry changed with regression";
    } else if (diff.recovery) {
        diff.diagnostic = "image frame resource materialization diff entry changed with recovery";
    } else {
        diff.diagnostic = "image frame resource materialization diff entry changed";
    }
    return diff;
}

inline void count_render_image_texture_frame_resource_materialization_diff_entry(
    render_image_texture_frame_resource_materialization_diff& diff,
    const render_image_texture_frame_resource_materialization_entry_diff& entry)
{
    switch (entry.status) {
    case render_image_texture_frame_resource_materialization_diff_entry_status::unchanged:
        ++diff.unchanged_entry_count;
        break;
    case render_image_texture_frame_resource_materialization_diff_entry_status::added:
        ++diff.added_entry_count;
        break;
    case render_image_texture_frame_resource_materialization_diff_entry_status::removed:
        ++diff.removed_entry_count;
        break;
    case render_image_texture_frame_resource_materialization_diff_entry_status::changed:
        ++diff.changed_entry_count;
        break;
    }

    if (entry.materialization_status_changed) {
        ++diff.materialization_status_changed_count;
    }
    if (entry.packet_status_changed) {
        ++diff.packet_status_changed_count;
    }
    if (entry.materialized_changed) {
        ++diff.materialized_changed_count;
    }
    if (entry.placeholder_changed) {
        ++diff.placeholder_changed_count;
    }
    if (entry.blocker_changed) {
        ++diff.blocker_changed_count;
    }
    if (entry.removed_changed) {
        ++diff.removed_changed_count;
    }
    if (entry.missing_upload_result_changed) {
        ++diff.missing_upload_result_changed_count;
    }
    if (entry.missing_frame_binding_changed) {
        ++diff.missing_frame_binding_changed_count;
    }
    if (entry.retry_backoff_changed) {
        ++diff.retry_backoff_changed_count;
    }
    if (entry.regression) {
        ++diff.regression_entry_count;
    }
    if (entry.improvement) {
        ++diff.improvement_entry_count;
        append_render_image_texture_frame_upload_handoff_summary_fragment(
            diff.improvement_summary,
            "request=" + std::to_string(entry.request_index) + ":" + entry.classification_reason);
    }
    if (entry.churn) {
        ++diff.churn_entry_count;
        append_render_image_texture_frame_upload_handoff_summary_fragment(
            diff.churn_summary,
            "request=" + std::to_string(entry.request_index) + ":" + entry.classification_reason);
    }
    if (entry.cache_handoff_changed) {
        ++diff.cache_handoff_changed_count;
    }
    if (entry.upload_handoff_changed) {
        ++diff.upload_handoff_changed_count;
    }
    if (entry.sampler_handoff_changed) {
        ++diff.sampler_handoff_changed_count;
    }
    if (entry.cache_delta.cache_key_changed) {
        ++diff.cache_key_changed_count;
    }
    if (entry.cache_delta.stable_texture_cache_key_changed) {
        ++diff.stable_texture_cache_key_changed_count;
    }
    if (entry.cache_delta.texture_key_changed) {
        ++diff.texture_key_changed_count;
    }
    if (entry.cache_delta.texture_handle_changed) {
        ++diff.texture_handle_changed_count;
    }
    if (entry.cache_delta.texture_size_changed) {
        ++diff.texture_size_changed_count;
    }
    if (entry.upload_delta.upload_request_changed) {
        ++diff.upload_request_changed_count;
    }
    if (entry.upload_delta.upload_generation_changed) {
        ++diff.upload_generation_changed_count;
    }
    if (entry.upload_delta.upload_result_presence_changed) {
        ++diff.upload_result_presence_changed_count;
    }
    if (entry.upload_delta.uploaded_byte_changed) {
        ++diff.uploaded_byte_changed_count;
    }
    if (entry.upload_delta.mip_level_changed) {
        ++diff.mip_level_changed_count;
    }
    if (entry.sampler_delta.sampler_changed) {
        ++diff.sampler_changed_count;
    }
    if (entry.sampler_delta.sampler_key_changed) {
        ++diff.sampler_key_changed_count;
    }
    if (entry.sampler_delta.sampler_summary_changed) {
        ++diff.sampler_summary_changed_count;
    }
    if (entry.placeholder_to_real) {
        ++diff.placeholder_to_real_count;
    }
    if (entry.real_to_placeholder) {
        ++diff.real_to_placeholder_count;
    }
    if (entry.cache_key_churn) {
        ++diff.cache_key_churn_count;
    }
    if (entry.upload_handoff_lost) {
        ++diff.upload_handoff_lost_count;
    }
    if (entry.upload_handoff_gained) {
        ++diff.upload_handoff_gained_count;
    }
    if (entry.sampler_policy_churn) {
        ++diff.sampler_policy_churn_count;
    }
    if (entry.materialization_failure_added) {
        ++diff.materialization_failure_added_count;
        append_render_image_texture_frame_upload_handoff_summary_fragment(
            diff.materialization_failure_summary,
            "request=" + std::to_string(entry.request_index) + ":materialization failure was added");
    }
    if (entry.materialization_failure_removed) {
        ++diff.materialization_failure_removed_count;
        append_render_image_texture_frame_upload_handoff_summary_fragment(
            diff.materialization_failure_summary,
            "request=" + std::to_string(entry.request_index) + ":materialization failure was removed");
    }
    if (entry.materialization_failure_changed) {
        ++diff.materialization_failure_changed_count;
        append_render_image_texture_frame_upload_handoff_summary_fragment(
            diff.materialization_failure_summary,
            "request=" + std::to_string(entry.request_index) + ":materialization failure changed");
    }
    if (entry.changed()) {
        append_render_image_texture_frame_upload_handoff_summary_fragment(
            diff.classification_summary,
            "request=" + std::to_string(entry.request_index) + ":"
                + entry.classification_name + ":" + entry.classification_reason);
    }

    diff.has_changes = diff.has_changes || entry.changed();
    diff.has_regression = diff.has_regression || entry.regression;
    diff.has_recovery = diff.has_recovery || entry.recovery;
    diff.has_improvement = diff.has_improvement || entry.improvement;
    diff.has_churn = diff.has_churn || entry.churn;
}

inline render_image_texture_frame_resource_materialization_diff
diff_render_image_texture_frame_resource_materializations(
    const render_image_texture_frame_resource_packet_materialization& before,
    const render_image_texture_frame_resource_packet_materialization& after)
{
    const std::size_t before_uploaded_byte_count =
        render_image_texture_frame_resource_materialization_uploaded_byte_count(before);
    const std::size_t after_uploaded_byte_count =
        render_image_texture_frame_resource_materialization_uploaded_byte_count(after);

    render_image_texture_frame_resource_materialization_diff diff{
        .before_frame_request_count = before.frame_request_count,
        .after_frame_request_count = after.frame_request_count,
        .frame_request_delta = render_image_texture_upload_result_size_delta(
            before.frame_request_count,
            after.frame_request_count),
        .before_planned_packet_count = before.planned_packet_count,
        .after_planned_packet_count = after.planned_packet_count,
        .planned_packet_delta = render_image_texture_upload_result_size_delta(
            before.planned_packet_count,
            after.planned_packet_count),
        .before_materialized_packet_count = before.materialized_packet_count,
        .after_materialized_packet_count = after.materialized_packet_count,
        .materialized_packet_delta = render_image_texture_upload_result_size_delta(
            before.materialized_packet_count,
            after.materialized_packet_count),
        .before_placeholder_packet_count = before.placeholder_packet_count,
        .after_placeholder_packet_count = after.placeholder_packet_count,
        .placeholder_packet_delta = render_image_texture_upload_result_size_delta(
            before.placeholder_packet_count,
            after.placeholder_packet_count),
        .before_blocked_packet_count = before.blocked_packet_count,
        .after_blocked_packet_count = after.blocked_packet_count,
        .blocked_packet_delta = render_image_texture_upload_result_size_delta(
            before.blocked_packet_count,
            after.blocked_packet_count),
        .before_removed_packet_count = before.removed_packet_count,
        .after_removed_packet_count = after.removed_packet_count,
        .removed_packet_delta = render_image_texture_upload_result_size_delta(
            before.removed_packet_count,
            after.removed_packet_count),
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
        .before_cache_record_count = before.cache_record_count,
        .after_cache_record_count = after.cache_record_count,
        .cache_record_delta = render_image_texture_upload_result_size_delta(
            before.cache_record_count,
            after.cache_record_count),
        .before_upload_record_count = before.upload_record_count,
        .after_upload_record_count = after.upload_record_count,
        .upload_record_delta = render_image_texture_upload_result_size_delta(
            before.upload_record_count,
            after.upload_record_count),
        .before_sampler_record_count = before.sampler_record_count,
        .after_sampler_record_count = after.sampler_record_count,
        .sampler_record_delta = render_image_texture_upload_result_size_delta(
            before.sampler_record_count,
            after.sampler_record_count),
        .before_uploaded_byte_count = before_uploaded_byte_count,
        .after_uploaded_byte_count = after_uploaded_byte_count,
        .uploaded_byte_delta = render_image_texture_upload_result_size_delta(
            before_uploaded_byte_count,
            after_uploaded_byte_count),
        .before_renderer_boundary_ready = before.renderer_boundary_ready,
        .after_renderer_boundary_ready = after.renderer_boundary_ready,
    };
    diff.renderer_boundary_regressed = before.renderer_boundary_ready && !after.renderer_boundary_ready;
    diff.renderer_boundary_recovered = !before.renderer_boundary_ready && after.renderer_boundary_ready;

    std::map<std::size_t, bool> request_indexes;
    append_render_image_texture_frame_resource_materialization_request_indexes(request_indexes, before);
    append_render_image_texture_frame_resource_materialization_request_indexes(request_indexes, after);

    for (const auto& [request_index, _] : request_indexes) {
        render_image_texture_frame_resource_materialization_entry_diff entry =
            make_render_image_texture_frame_resource_materialization_entry_diff(
                render_image_texture_frame_resource_materialization_entry_for_request_index(before, request_index),
                render_image_texture_frame_resource_materialization_entry_for_request_index(after, request_index),
                request_index);
        count_render_image_texture_frame_resource_materialization_diff_entry(diff, entry);
        diff.entries.push_back(std::move(entry));
    }

    diff.has_changes = diff.has_changes
        || diff.frame_request_delta != 0
        || diff.planned_packet_delta != 0
        || diff.materialized_packet_delta != 0
        || diff.placeholder_packet_delta != 0
        || diff.blocked_packet_delta != 0
        || diff.removed_packet_delta != 0
        || diff.missing_upload_result_delta != 0
        || diff.missing_frame_binding_delta != 0
        || diff.retry_backoff_blocked_delta != 0
        || diff.cache_record_delta != 0
        || diff.upload_record_delta != 0
        || diff.sampler_record_delta != 0
        || diff.uploaded_byte_delta != 0
        || diff.renderer_boundary_regressed
        || diff.renderer_boundary_recovered;
    diff.has_regression = diff.has_regression
        || diff.renderer_boundary_regressed
        || diff.materialized_packet_delta < 0
        || diff.placeholder_packet_delta > 0
        || diff.blocked_packet_delta > 0
        || diff.missing_upload_result_delta > 0
        || diff.missing_frame_binding_delta > 0
        || diff.retry_backoff_blocked_delta > 0;
    diff.has_recovery = diff.has_recovery
        || diff.renderer_boundary_recovered
        || diff.materialized_packet_delta > 0
        || diff.blocked_packet_delta < 0
        || diff.missing_upload_result_delta < 0
        || diff.missing_frame_binding_delta < 0
        || diff.retry_backoff_blocked_delta < 0;
    diff.has_improvement = diff.has_improvement || diff.improvement_entry_count != 0;
    diff.has_churn = diff.has_churn || diff.churn_entry_count != 0;

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
            ? "image frame resource materialization aggregate counts changed"
            : "image frame resource materialization diff has no changes";
    }

    diff.cache_handoff_delta_summary = "cache_records="
        + std::to_string(diff.before_cache_record_count)
        + "->" + std::to_string(diff.after_cache_record_count)
        + "; changed=" + std::to_string(diff.cache_handoff_changed_count)
        + "; stable_cache_keys_changed=" + std::to_string(diff.stable_texture_cache_key_changed_count)
        + "; texture_handles_changed=" + std::to_string(diff.texture_handle_changed_count);
    diff.upload_handoff_delta_summary = "upload_records="
        + std::to_string(diff.before_upload_record_count)
        + "->" + std::to_string(diff.after_upload_record_count)
        + "; changed=" + std::to_string(diff.upload_handoff_changed_count)
        + "; upload_requests_changed=" + std::to_string(diff.upload_request_changed_count)
        + "; uploaded_bytes=" + std::to_string(diff.before_uploaded_byte_count)
        + "->" + std::to_string(diff.after_uploaded_byte_count);
    diff.sampler_handoff_delta_summary = "sampler_records="
        + std::to_string(diff.before_sampler_record_count)
        + "->" + std::to_string(diff.after_sampler_record_count)
        + "; changed=" + std::to_string(diff.sampler_handoff_changed_count)
        + "; sampler_keys_changed=" + std::to_string(diff.sampler_key_changed_count);

    if (diff.renderer_boundary_regressed) {
        append_render_image_texture_frame_regression_reason(
            diff.regression_summary,
            "resource materialization renderer boundary regressed");
    }
    if (diff.materialized_packet_delta < 0) {
        append_render_image_texture_frame_regression_reason(
            diff.regression_summary,
            "materialized packets decreased");
    }
    if (diff.placeholder_packet_delta > 0) {
        append_render_image_texture_frame_regression_reason(
            diff.regression_summary,
            "placeholder packets increased");
    }
    if (diff.blocked_packet_delta > 0) {
        append_render_image_texture_frame_regression_reason(
            diff.regression_summary,
            "blocked packets increased");
    }
    if (diff.missing_upload_result_delta > 0) {
        append_render_image_texture_frame_regression_reason(
            diff.regression_summary,
            "missing upload results increased");
    }
    if (diff.missing_frame_binding_delta > 0) {
        append_render_image_texture_frame_regression_reason(
            diff.regression_summary,
            "missing frame bindings increased");
    }
    if (diff.retry_backoff_blocked_delta > 0) {
        append_render_image_texture_frame_regression_reason(
            diff.regression_summary,
            "retry backoff blockers increased");
    }
    if (diff.has_regression && diff.regression_summary.empty()) {
        diff.regression_summary = "image frame resource materialization regressed";
    }
    if (diff.regression_summary.empty()) {
        diff.regression_summary = diff.has_changes
            ? "image frame resource materialization diff has changes without regressions"
            : "image frame resource materialization diff has no changes";
    }
    if (diff.improvement_summary.empty()) {
        diff.improvement_summary = diff.has_changes
            ? "image frame resource materialization diff has changes without improvements"
            : "image frame resource materialization diff has no changes";
    }
    if (diff.churn_summary.empty()) {
        diff.churn_summary = diff.has_changes
            ? "image frame resource materialization diff has no churn"
            : "image frame resource materialization diff has no changes";
    }
    if (diff.materialization_failure_summary.empty()) {
        diff.materialization_failure_summary = diff.has_changes
            ? "image frame resource materialization diff has no materialization failure changes"
            : "image frame resource materialization diff has no changes";
    }
    if (diff.classification_summary.empty()) {
        diff.classification_summary = diff.has_changes
            ? "image frame resource materialization diff has changes without classified reasons"
            : "image frame resource materialization diff has no changes";
    }

    diff.diagnostic = diff.has_regression
        ? "image frame resource materialization diff reports regressions"
        : (diff.has_changes
            ? "image frame resource materialization diff reports changes"
            : "image frame resource materialization diff is unchanged");
    return diff;
}

} // namespace quiz_vulkan::render
