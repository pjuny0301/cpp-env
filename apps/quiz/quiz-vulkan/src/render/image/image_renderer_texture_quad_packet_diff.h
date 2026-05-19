#pragma once

#include "render/image/image_renderer_texture_quad_packet_plan.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

enum class render_image_renderer_texture_quad_packet_diff_entry_status {
    unchanged,
    added,
    removed,
    changed,
};

inline std::string render_image_renderer_texture_quad_packet_diff_entry_status_name(
    render_image_renderer_texture_quad_packet_diff_entry_status status)
{
    switch (status) {
    case render_image_renderer_texture_quad_packet_diff_entry_status::unchanged:
        return "unchanged";
    case render_image_renderer_texture_quad_packet_diff_entry_status::added:
        return "added";
    case render_image_renderer_texture_quad_packet_diff_entry_status::removed:
        return "removed";
    case render_image_renderer_texture_quad_packet_diff_entry_status::changed:
        return "changed";
    }

    return "unknown";
}

enum class render_image_renderer_texture_quad_packet_diff_classification {
    neutral,
    regression,
    recovery,
    churn,
};

inline std::string render_image_renderer_texture_quad_packet_diff_classification_name(
    render_image_renderer_texture_quad_packet_diff_classification classification)
{
    switch (classification) {
    case render_image_renderer_texture_quad_packet_diff_classification::neutral:
        return "neutral";
    case render_image_renderer_texture_quad_packet_diff_classification::regression:
        return "regression";
    case render_image_renderer_texture_quad_packet_diff_classification::recovery:
        return "recovery";
    case render_image_renderer_texture_quad_packet_diff_classification::churn:
        return "churn";
    }

    return "unknown";
}

enum class render_image_renderer_texture_quad_packet_summary_diff_status {
    unchanged,
    changed,
};

inline std::string render_image_renderer_texture_quad_packet_summary_diff_status_name(
    render_image_renderer_texture_quad_packet_summary_diff_status status)
{
    switch (status) {
    case render_image_renderer_texture_quad_packet_summary_diff_status::unchanged:
        return "unchanged";
    case render_image_renderer_texture_quad_packet_summary_diff_status::changed:
        return "changed";
    }

    return "unknown";
}


struct render_image_renderer_texture_quad_packet_diff_entry {
    std::string stable_diff_identity;
    render_image_renderer_texture_quad_packet_diff_entry_status status =
        render_image_renderer_texture_quad_packet_diff_entry_status::unchanged;
    std::string status_name = render_image_renderer_texture_quad_packet_diff_entry_status_name(
        render_image_renderer_texture_quad_packet_diff_entry_status::unchanged);
    render_image_renderer_texture_quad_packet_diff_classification classification =
        render_image_renderer_texture_quad_packet_diff_classification::neutral;
    std::string classification_name = render_image_renderer_texture_quad_packet_diff_classification_name(
        render_image_renderer_texture_quad_packet_diff_classification::neutral);
    bool before_present = false;
    bool after_present = false;
    std::size_t before_packet_index = 0;
    std::size_t after_packet_index = 0;
    std::size_t before_draw_command_index = 0;
    std::size_t after_draw_command_index = 0;
    std::size_t before_image_command_index = 0;
    std::size_t after_image_command_index = 0;
    std::size_t before_texture_request_index = 0;
    std::size_t after_texture_request_index = 0;
    std::string before_stable_draw_command_identity;
    std::string after_stable_draw_command_identity;
    std::string before_stable_quad_packet_identity;
    std::string after_stable_quad_packet_identity;
    std::string before_stable_texture_cache_key;
    std::string after_stable_texture_cache_key;
    std::string before_sampler_key;
    std::string after_sampler_key;
    render_rect before_bounds;
    render_rect after_bounds;
    render_rect before_content_bounds;
    render_rect after_content_bounds;
    render_image_texture_id before_texture_id = 0;
    render_image_texture_id after_texture_id = 0;
    render_image_revision before_texture_revision = 0;
    render_image_revision after_texture_revision = 0;
    std::size_t before_texture_width = 0;
    std::size_t after_texture_width = 0;
    std::size_t before_texture_height = 0;
    std::size_t after_texture_height = 0;
    std::uint64_t before_upload_request_id = 0;
    std::uint64_t after_upload_request_id = 0;
    std::uint64_t before_upload_generation_id = 0;
    std::uint64_t after_upload_generation_id = 0;
    std::size_t before_uploaded_byte_count = 0;
    std::size_t after_uploaded_byte_count = 0;
    std::int64_t uploaded_byte_delta = 0;
    std::uint64_t before_decoded_payload_hash = 0;
    std::uint64_t after_decoded_payload_hash = 0;
    std::size_t before_decoded_byte_count = 0;
    std::size_t after_decoded_byte_count = 0;
    std::int64_t decoded_byte_delta = 0;
    std::size_t before_upload_layout_byte_count = 0;
    std::size_t after_upload_layout_byte_count = 0;
    std::int64_t upload_layout_byte_delta = 0;
    std::size_t before_staging_payload_byte_count = 0;
    std::size_t after_staging_payload_byte_count = 0;
    std::int64_t staging_payload_byte_delta = 0;
    render_image_renderer_texture_quad_packet_status before_quad_status =
        render_image_renderer_texture_quad_packet_status::blocked_handoff;
    render_image_renderer_texture_quad_packet_status after_quad_status =
        render_image_renderer_texture_quad_packet_status::blocked_handoff;
    std::string before_quad_status_name;
    std::string after_quad_status_name;
    bool before_ready = false;
    bool after_ready = false;
    bool before_blocked = false;
    bool after_blocked = false;
    bool before_missing_stable_identity = false;
    bool after_missing_stable_identity = false;
    bool before_duplicate_stable_identity = false;
    bool after_duplicate_stable_identity = false;
    bool before_decoded_resource_ready = false;
    bool after_decoded_resource_ready = false;
    bool before_decoded_resource_blocked = false;
    bool after_decoded_resource_blocked = false;
    bool stable_quad_packet_identity_changed = false;
    bool bounds_changed = false;
    bool content_bounds_changed = false;
    bool texture_id_changed = false;
    bool texture_revision_changed = false;
    bool texture_size_changed = false;
    bool sampler_changed = false;
    bool cache_key_changed = false;
    bool upload_request_changed = false;
    bool upload_generation_changed = false;
    bool uploaded_byte_count_changed = false;
    bool decoded_payload_hash_changed = false;
    bool decoded_byte_count_changed = false;
    bool upload_layout_byte_count_changed = false;
    bool staging_payload_byte_count_changed = false;
    bool decoded_resource_readiness_changed = false;
    bool decoded_resource_blocker_changed = false;
    bool readiness_changed = false;
    bool blocker_changed = false;
    bool missing_stable_identity_changed = false;
    bool duplicate_stable_identity_changed = false;
    bool regression = false;
    bool recovery = false;
    bool churn = false;
    std::string before_blocker_summary;
    std::string after_blocker_summary;
    std::string before_decoded_resource_blocker_summary;
    std::string after_decoded_resource_blocker_summary;
    std::string diagnostic;

    bool changed() const
    {
        return status != render_image_renderer_texture_quad_packet_diff_entry_status::unchanged;
    }

    bool ok() const
    {
        return !regression;
    }
};

struct render_image_renderer_texture_quad_packet_summary_diff {
    render_image_renderer_texture_quad_packet_summary_diff_status status =
        render_image_renderer_texture_quad_packet_summary_diff_status::unchanged;
    std::string status_name = render_image_renderer_texture_quad_packet_summary_diff_status_name(
        render_image_renderer_texture_quad_packet_summary_diff_status::unchanged);
    std::size_t before_packet_count = 0;
    std::size_t after_packet_count = 0;
    std::int64_t packet_count_delta = 0;
    std::size_t unchanged_packet_count = 0;
    std::size_t added_packet_count = 0;
    std::size_t removed_packet_count = 0;
    std::size_t changed_packet_count = 0;
    std::size_t stable_quad_packet_identity_changed_count = 0;
    std::size_t bounds_changed_count = 0;
    std::size_t content_bounds_changed_count = 0;
    std::size_t texture_id_changed_count = 0;
    std::size_t texture_revision_changed_count = 0;
    std::size_t texture_size_changed_count = 0;
    std::size_t sampler_changed_count = 0;
    std::size_t cache_key_changed_count = 0;
    std::size_t upload_request_changed_count = 0;
    std::size_t upload_generation_changed_count = 0;
    std::size_t uploaded_byte_count_changed_count = 0;
    std::size_t decoded_payload_hash_changed_count = 0;
    std::size_t decoded_byte_count_changed_count = 0;
    std::size_t upload_layout_byte_count_changed_count = 0;
    std::size_t staging_payload_byte_count_changed_count = 0;
    std::size_t decoded_resource_readiness_changed_count = 0;
    std::size_t decoded_resource_blocker_changed_count = 0;
    std::size_t readiness_changed_count = 0;
    std::size_t blocker_changed_count = 0;
    std::size_t missing_stable_identity_changed_count = 0;
    std::size_t duplicate_stable_identity_changed_count = 0;
    std::size_t regression_count = 0;
    std::size_t recovery_count = 0;
    std::size_t churn_count = 0;
    bool has_changes = false;
    bool has_regressions = false;
    bool has_recoveries = false;
    bool has_identity_changes = false;
    bool has_layout_changes = false;
    bool has_texture_changes = false;
    bool has_sampler_or_cache_changes = false;
    bool has_upload_changes = false;
    bool has_decoded_resource_changes = false;
    std::vector<render_image_renderer_texture_quad_packet_diff_entry> entries;
    std::string changed_identity_summary;
    std::string blocker_transition_summary;
    std::string decoded_resource_change_summary;
    std::string diagnostic;

    bool ok() const
    {
        return !has_regressions;
    }
};

inline bool render_image_renderer_texture_quad_packet_rect_equal(
    const render_rect& lhs,
    const render_rect& rhs)
{
    return lhs.x == rhs.x
        && lhs.y == rhs.y
        && lhs.width == rhs.width
        && lhs.height == rhs.height;
}

inline std::string render_image_renderer_texture_quad_packet_diff_base_identity_for(
    const render_image_renderer_texture_quad_packet& packet)
{
    if (!packet.stable_draw_command_identity.empty()) {
        return packet.stable_draw_command_identity;
    }
    if (!packet.stable_quad_packet_identity.empty()) {
        return packet.stable_quad_packet_identity;
    }

    return "missing_identity|frame=" + packet.frame_label
        + "|draw=" + std::to_string(packet.draw_command_index)
        + "|image=" + std::to_string(packet.image_command_index)
        + "|uri=" + packet.uri;
}

inline std::map<std::string, const render_image_renderer_texture_quad_packet*>
render_image_renderer_texture_quad_packet_diff_packet_map(
    const render_image_renderer_texture_quad_packet_summary& summary)
{
    std::map<std::string, std::size_t> occurrence_counts;
    std::map<std::string, const render_image_renderer_texture_quad_packet*> packets;
    for (const render_image_renderer_texture_quad_packet& packet : summary.packets) {
        const std::string base_identity =
            render_image_renderer_texture_quad_packet_diff_base_identity_for(packet);
        const std::size_t occurrence = occurrence_counts[base_identity]++;
        packets.emplace(
            base_identity + "|occurrence=" + std::to_string(occurrence),
            &packet);
    }
    return packets;
}

inline render_image_renderer_texture_quad_packet_diff_entry_status
render_image_renderer_texture_quad_packet_diff_entry_status_for(
    const render_image_renderer_texture_quad_packet_diff_entry& entry)
{
    if (!entry.before_present && entry.after_present) {
        return render_image_renderer_texture_quad_packet_diff_entry_status::added;
    }
    if (entry.before_present && !entry.after_present) {
        return render_image_renderer_texture_quad_packet_diff_entry_status::removed;
    }
    if (entry.stable_quad_packet_identity_changed
        || entry.bounds_changed
        || entry.content_bounds_changed
        || entry.texture_id_changed
        || entry.texture_revision_changed
        || entry.texture_size_changed
        || entry.sampler_changed
        || entry.cache_key_changed
        || entry.upload_request_changed
        || entry.upload_generation_changed
        || entry.uploaded_byte_count_changed
        || entry.decoded_payload_hash_changed
        || entry.decoded_byte_count_changed
        || entry.upload_layout_byte_count_changed
        || entry.staging_payload_byte_count_changed
        || entry.decoded_resource_readiness_changed
        || entry.decoded_resource_blocker_changed
        || entry.readiness_changed
        || entry.blocker_changed
        || entry.missing_stable_identity_changed
        || entry.duplicate_stable_identity_changed) {
        return render_image_renderer_texture_quad_packet_diff_entry_status::changed;
    }
    return render_image_renderer_texture_quad_packet_diff_entry_status::unchanged;
}

inline render_image_renderer_texture_quad_packet_diff_classification
render_image_renderer_texture_quad_packet_diff_classification_for(
    const render_image_renderer_texture_quad_packet_diff_entry& entry)
{
    if (entry.before_present && entry.after_present && entry.before_ready && entry.after_blocked) {
        return render_image_renderer_texture_quad_packet_diff_classification::regression;
    }
    if (entry.before_present && entry.after_present && entry.before_blocked && entry.after_ready) {
        return render_image_renderer_texture_quad_packet_diff_classification::recovery;
    }
    if (entry.changed()) {
        return render_image_renderer_texture_quad_packet_diff_classification::churn;
    }
    return render_image_renderer_texture_quad_packet_diff_classification::neutral;
}

inline void finalize_render_image_renderer_texture_quad_packet_diff_entry(
    render_image_renderer_texture_quad_packet_diff_entry& entry)
{
    entry.status = render_image_renderer_texture_quad_packet_diff_entry_status_for(entry);
    entry.status_name = render_image_renderer_texture_quad_packet_diff_entry_status_name(entry.status);
    entry.classification = render_image_renderer_texture_quad_packet_diff_classification_for(entry);
    entry.classification_name =
        render_image_renderer_texture_quad_packet_diff_classification_name(entry.classification);
    entry.regression =
        entry.classification == render_image_renderer_texture_quad_packet_diff_classification::regression;
    entry.recovery =
        entry.classification == render_image_renderer_texture_quad_packet_diff_classification::recovery;
    entry.churn =
        entry.classification == render_image_renderer_texture_quad_packet_diff_classification::churn;

    switch (entry.status) {
    case render_image_renderer_texture_quad_packet_diff_entry_status::unchanged:
        entry.diagnostic = "image renderer texture quad packet is unchanged";
        break;
    case render_image_renderer_texture_quad_packet_diff_entry_status::added:
        entry.diagnostic = "image renderer texture quad packet was added";
        break;
    case render_image_renderer_texture_quad_packet_diff_entry_status::removed:
        entry.diagnostic = "image renderer texture quad packet was removed";
        break;
    case render_image_renderer_texture_quad_packet_diff_entry_status::changed:
        if (entry.regression) {
            entry.diagnostic = "image renderer texture quad packet regressed from ready to blocked";
        } else if (entry.recovery) {
            entry.diagnostic = "image renderer texture quad packet recovered from blocked to ready";
        } else {
            entry.diagnostic = "image renderer texture quad packet changed";
        }
        break;
    }
}

inline render_image_renderer_texture_quad_packet_diff_entry
make_render_image_renderer_texture_quad_packet_diff_entry(
    std::string stable_diff_identity,
    const render_image_renderer_texture_quad_packet* before,
    const render_image_renderer_texture_quad_packet* after)
{
    render_image_renderer_texture_quad_packet_diff_entry entry{
        .stable_diff_identity = std::move(stable_diff_identity),
        .before_present = before != nullptr,
        .after_present = after != nullptr,
    };

    if (before != nullptr) {
        entry.before_packet_index = before->packet_index;
        entry.before_draw_command_index = before->draw_command_index;
        entry.before_image_command_index = before->image_command_index;
        entry.before_texture_request_index = before->texture_request_index;
        entry.before_stable_draw_command_identity = before->stable_draw_command_identity;
        entry.before_stable_quad_packet_identity = before->stable_quad_packet_identity;
        entry.before_stable_texture_cache_key = before->stable_texture_cache_key;
        entry.before_sampler_key = before->sampler_key;
        entry.before_bounds = before->bounds;
        entry.before_content_bounds = before->content_bounds;
        entry.before_texture_id = before->texture_id;
        entry.before_texture_revision = before->texture_revision;
        entry.before_texture_width = before->texture_width;
        entry.before_texture_height = before->texture_height;
        entry.before_upload_request_id = before->upload_request_id;
        entry.before_upload_generation_id = before->upload_generation_id;
        entry.before_uploaded_byte_count = before->uploaded_byte_count;
        entry.before_decoded_payload_hash = before->decoded_payload_hash;
        entry.before_decoded_byte_count = before->decoded_byte_count;
        entry.before_upload_layout_byte_count = before->upload_layout_byte_count;
        entry.before_staging_payload_byte_count = before->staging_payload_byte_count;
        entry.before_quad_status = before->status;
        entry.before_quad_status_name = before->status_name;
        entry.before_ready = before->ready;
        entry.before_blocked = before->blocked;
        entry.before_missing_stable_identity = before->missing_stable_identity;
        entry.before_duplicate_stable_identity = before->duplicate_stable_identity;
        entry.before_decoded_resource_ready = before->decoded_resource_ready;
        entry.before_decoded_resource_blocked = before->decoded_resource_blocked;
        entry.before_blocker_summary = before->blocker_summary;
        entry.before_decoded_resource_blocker_summary = before->decoded_resource_blocker_summary;
    }

    if (after != nullptr) {
        entry.after_packet_index = after->packet_index;
        entry.after_draw_command_index = after->draw_command_index;
        entry.after_image_command_index = after->image_command_index;
        entry.after_texture_request_index = after->texture_request_index;
        entry.after_stable_draw_command_identity = after->stable_draw_command_identity;
        entry.after_stable_quad_packet_identity = after->stable_quad_packet_identity;
        entry.after_stable_texture_cache_key = after->stable_texture_cache_key;
        entry.after_sampler_key = after->sampler_key;
        entry.after_bounds = after->bounds;
        entry.after_content_bounds = after->content_bounds;
        entry.after_texture_id = after->texture_id;
        entry.after_texture_revision = after->texture_revision;
        entry.after_texture_width = after->texture_width;
        entry.after_texture_height = after->texture_height;
        entry.after_upload_request_id = after->upload_request_id;
        entry.after_upload_generation_id = after->upload_generation_id;
        entry.after_uploaded_byte_count = after->uploaded_byte_count;
        entry.after_decoded_payload_hash = after->decoded_payload_hash;
        entry.after_decoded_byte_count = after->decoded_byte_count;
        entry.after_upload_layout_byte_count = after->upload_layout_byte_count;
        entry.after_staging_payload_byte_count = after->staging_payload_byte_count;
        entry.after_quad_status = after->status;
        entry.after_quad_status_name = after->status_name;
        entry.after_ready = after->ready;
        entry.after_blocked = after->blocked;
        entry.after_missing_stable_identity = after->missing_stable_identity;
        entry.after_duplicate_stable_identity = after->duplicate_stable_identity;
        entry.after_decoded_resource_ready = after->decoded_resource_ready;
        entry.after_decoded_resource_blocked = after->decoded_resource_blocked;
        entry.after_blocker_summary = after->blocker_summary;
        entry.after_decoded_resource_blocker_summary = after->decoded_resource_blocker_summary;
    }

    entry.uploaded_byte_delta = static_cast<std::int64_t>(entry.after_uploaded_byte_count)
        - static_cast<std::int64_t>(entry.before_uploaded_byte_count);
    entry.decoded_byte_delta = static_cast<std::int64_t>(entry.after_decoded_byte_count)
        - static_cast<std::int64_t>(entry.before_decoded_byte_count);
    entry.upload_layout_byte_delta = static_cast<std::int64_t>(entry.after_upload_layout_byte_count)
        - static_cast<std::int64_t>(entry.before_upload_layout_byte_count);
    entry.staging_payload_byte_delta = static_cast<std::int64_t>(entry.after_staging_payload_byte_count)
        - static_cast<std::int64_t>(entry.before_staging_payload_byte_count);

    if (entry.before_present && entry.after_present) {
        entry.stable_quad_packet_identity_changed =
            entry.before_stable_quad_packet_identity != entry.after_stable_quad_packet_identity;
        entry.bounds_changed =
            !render_image_renderer_texture_quad_packet_rect_equal(entry.before_bounds, entry.after_bounds);
        entry.content_bounds_changed =
            !render_image_renderer_texture_quad_packet_rect_equal(
                entry.before_content_bounds,
                entry.after_content_bounds);
        entry.texture_id_changed = entry.before_texture_id != entry.after_texture_id;
        entry.texture_revision_changed = entry.before_texture_revision != entry.after_texture_revision;
        entry.texture_size_changed = entry.before_texture_width != entry.after_texture_width
            || entry.before_texture_height != entry.after_texture_height;
        entry.sampler_changed = entry.before_sampler_key != entry.after_sampler_key;
        entry.cache_key_changed = entry.before_stable_texture_cache_key != entry.after_stable_texture_cache_key;
        entry.upload_request_changed = entry.before_upload_request_id != entry.after_upload_request_id;
        entry.upload_generation_changed = entry.before_upload_generation_id != entry.after_upload_generation_id;
        entry.uploaded_byte_count_changed =
            entry.before_uploaded_byte_count != entry.after_uploaded_byte_count;
        entry.decoded_payload_hash_changed =
            entry.before_decoded_payload_hash != entry.after_decoded_payload_hash;
        entry.decoded_byte_count_changed =
            entry.before_decoded_byte_count != entry.after_decoded_byte_count;
        entry.upload_layout_byte_count_changed =
            entry.before_upload_layout_byte_count != entry.after_upload_layout_byte_count;
        entry.staging_payload_byte_count_changed =
            entry.before_staging_payload_byte_count != entry.after_staging_payload_byte_count;
        entry.decoded_resource_readiness_changed =
            entry.before_decoded_resource_ready != entry.after_decoded_resource_ready
            || entry.before_decoded_resource_blocked != entry.after_decoded_resource_blocked;
        entry.decoded_resource_blocker_changed =
            entry.before_decoded_resource_blocker_summary
            != entry.after_decoded_resource_blocker_summary;
        entry.readiness_changed = entry.before_ready != entry.after_ready
            || entry.before_blocked != entry.after_blocked
            || entry.before_quad_status != entry.after_quad_status;
        entry.blocker_changed = entry.before_blocker_summary != entry.after_blocker_summary;
        entry.missing_stable_identity_changed =
            entry.before_missing_stable_identity != entry.after_missing_stable_identity;
        entry.duplicate_stable_identity_changed =
            entry.before_duplicate_stable_identity != entry.after_duplicate_stable_identity;
    }

    finalize_render_image_renderer_texture_quad_packet_diff_entry(entry);
    return entry;
}

inline void count_render_image_renderer_texture_quad_packet_diff_entry(
    render_image_renderer_texture_quad_packet_summary_diff& diff,
    const render_image_renderer_texture_quad_packet_diff_entry& entry)
{
    switch (entry.status) {
    case render_image_renderer_texture_quad_packet_diff_entry_status::unchanged:
        ++diff.unchanged_packet_count;
        break;
    case render_image_renderer_texture_quad_packet_diff_entry_status::added:
        ++diff.added_packet_count;
        diff.has_changes = true;
        break;
    case render_image_renderer_texture_quad_packet_diff_entry_status::removed:
        ++diff.removed_packet_count;
        diff.has_changes = true;
        break;
    case render_image_renderer_texture_quad_packet_diff_entry_status::changed:
        ++diff.changed_packet_count;
        diff.has_changes = true;
        break;
    }

    if (entry.stable_quad_packet_identity_changed) {
        ++diff.stable_quad_packet_identity_changed_count;
        diff.has_identity_changes = true;
        append_render_image_texture_frame_upload_handoff_summary_fragment(
            diff.changed_identity_summary,
            entry.stable_diff_identity);
    }
    if (entry.bounds_changed) {
        ++diff.bounds_changed_count;
        diff.has_layout_changes = true;
    }
    if (entry.content_bounds_changed) {
        ++diff.content_bounds_changed_count;
        diff.has_layout_changes = true;
    }
    if (entry.texture_id_changed) {
        ++diff.texture_id_changed_count;
        diff.has_texture_changes = true;
    }
    if (entry.texture_revision_changed) {
        ++diff.texture_revision_changed_count;
        diff.has_texture_changes = true;
    }
    if (entry.texture_size_changed) {
        ++diff.texture_size_changed_count;
        diff.has_texture_changes = true;
    }
    if (entry.sampler_changed) {
        ++diff.sampler_changed_count;
        diff.has_sampler_or_cache_changes = true;
    }
    if (entry.cache_key_changed) {
        ++diff.cache_key_changed_count;
        diff.has_sampler_or_cache_changes = true;
    }
    if (entry.upload_request_changed) {
        ++diff.upload_request_changed_count;
        diff.has_upload_changes = true;
    }
    if (entry.upload_generation_changed) {
        ++diff.upload_generation_changed_count;
        diff.has_upload_changes = true;
    }
    if (entry.uploaded_byte_count_changed) {
        ++diff.uploaded_byte_count_changed_count;
        diff.has_upload_changes = true;
    }
    if (entry.decoded_payload_hash_changed) {
        ++diff.decoded_payload_hash_changed_count;
        diff.has_decoded_resource_changes = true;
    }
    if (entry.decoded_byte_count_changed) {
        ++diff.decoded_byte_count_changed_count;
        diff.has_decoded_resource_changes = true;
    }
    if (entry.upload_layout_byte_count_changed) {
        ++diff.upload_layout_byte_count_changed_count;
        diff.has_decoded_resource_changes = true;
    }
    if (entry.staging_payload_byte_count_changed) {
        ++diff.staging_payload_byte_count_changed_count;
        diff.has_decoded_resource_changes = true;
    }
    if (entry.decoded_resource_readiness_changed) {
        ++diff.decoded_resource_readiness_changed_count;
        diff.has_decoded_resource_changes = true;
    }
    if (entry.decoded_resource_blocker_changed) {
        ++diff.decoded_resource_blocker_changed_count;
        diff.has_decoded_resource_changes = true;
        append_render_image_texture_frame_upload_handoff_summary_fragment(
            diff.decoded_resource_change_summary,
            entry.stable_diff_identity);
    }
    if (entry.readiness_changed) {
        ++diff.readiness_changed_count;
    }
    if (entry.blocker_changed) {
        ++diff.blocker_changed_count;
        append_render_image_texture_frame_upload_handoff_summary_fragment(
            diff.blocker_transition_summary,
            entry.stable_diff_identity);
    }
    if (entry.missing_stable_identity_changed) {
        ++diff.missing_stable_identity_changed_count;
        diff.has_identity_changes = true;
    }
    if (entry.duplicate_stable_identity_changed) {
        ++diff.duplicate_stable_identity_changed_count;
        diff.has_identity_changes = true;
    }
    if (entry.regression) {
        ++diff.regression_count;
        diff.has_regressions = true;
    }
    if (entry.recovery) {
        ++diff.recovery_count;
        diff.has_recoveries = true;
    }
    if (entry.churn) {
        ++diff.churn_count;
    }
}

inline render_image_renderer_texture_quad_packet_summary_diff
diff_render_image_renderer_texture_quad_packet_summaries(
    const render_image_renderer_texture_quad_packet_summary& before,
    const render_image_renderer_texture_quad_packet_summary& after)
{
    render_image_renderer_texture_quad_packet_summary_diff diff{
        .before_packet_count = before.packets.size(),
        .after_packet_count = after.packets.size(),
        .packet_count_delta = static_cast<std::int64_t>(after.packets.size())
            - static_cast<std::int64_t>(before.packets.size()),
    };

    const std::map<std::string, const render_image_renderer_texture_quad_packet*> before_packets =
        render_image_renderer_texture_quad_packet_diff_packet_map(before);
    const std::map<std::string, const render_image_renderer_texture_quad_packet*> after_packets =
        render_image_renderer_texture_quad_packet_diff_packet_map(after);

    std::map<std::string, bool> diff_identities;
    for (const auto& [identity, before_packet] : before_packets) {
        (void)before_packet;
        diff_identities.emplace(identity, true);
    }
    for (const auto& [identity, after_packet] : after_packets) {
        (void)after_packet;
        diff_identities.emplace(identity, true);
    }

    for (const auto& identity_entry : diff_identities) {
        const std::string& identity = identity_entry.first;
        const auto before_match = before_packets.find(identity);
        const auto after_match = after_packets.find(identity);
        const render_image_renderer_texture_quad_packet* before_packet =
            before_match == before_packets.end() ? nullptr : before_match->second;
        const render_image_renderer_texture_quad_packet* after_packet =
            after_match == after_packets.end() ? nullptr : after_match->second;
        render_image_renderer_texture_quad_packet_diff_entry entry =
            make_render_image_renderer_texture_quad_packet_diff_entry(
                identity,
                before_packet,
                after_packet);
        count_render_image_renderer_texture_quad_packet_diff_entry(diff, entry);
        diff.entries.push_back(std::move(entry));
    }

    if (diff.changed_identity_summary.empty()) {
        diff.changed_identity_summary = "no renderer texture quad packet identity changes";
    }
    if (diff.blocker_transition_summary.empty()) {
        diff.blocker_transition_summary = "no renderer texture quad packet blocker transitions";
    }
    if (diff.decoded_resource_change_summary.empty()) {
        diff.decoded_resource_change_summary = "no renderer texture quad decoded resource changes";
    }

    diff.status = diff.has_changes
        ? render_image_renderer_texture_quad_packet_summary_diff_status::changed
        : render_image_renderer_texture_quad_packet_summary_diff_status::unchanged;
    diff.status_name = render_image_renderer_texture_quad_packet_summary_diff_status_name(diff.status);
    diff.diagnostic = diff.has_changes
        ? "image renderer texture quad packet summary diff has changes"
        : "image renderer texture quad packet summary diff is unchanged";
    return diff;
}

} // namespace quiz_vulkan::render
