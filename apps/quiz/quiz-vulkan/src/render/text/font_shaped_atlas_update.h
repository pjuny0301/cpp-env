#pragma once

#include "render/text/font_rasterizer.h"
#include "render/text/font_shaping_backend.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

enum class render_text_glyph_atlas_materialization_status {
    materialized_upload_ready,
    materialized_clean_reuse,
    skipped_missing_cache_key,
    skipped_raster_payload,
    skipped_unsupported_glyph,
    payload_byte_count_mismatch,
};

inline std::string render_text_glyph_atlas_materialization_status_name(
    const render_text_glyph_atlas_materialization_status status)
{
    switch (status) {
    case render_text_glyph_atlas_materialization_status::materialized_upload_ready:
        return "materialized_upload_ready";
    case render_text_glyph_atlas_materialization_status::materialized_clean_reuse:
        return "materialized_clean_reuse";
    case render_text_glyph_atlas_materialization_status::skipped_missing_cache_key:
        return "skipped_missing_cache_key";
    case render_text_glyph_atlas_materialization_status::skipped_raster_payload:
        return "skipped_raster_payload";
    case render_text_glyph_atlas_materialization_status::skipped_unsupported_glyph:
        return "skipped_unsupported_glyph";
    case render_text_glyph_atlas_materialization_status::payload_byte_count_mismatch:
        return "payload_byte_count_mismatch";
    }

    return "unknown";
}

struct render_text_glyph_atlas_materialization_request {
    std::size_t cluster_index = 0;
    std::size_t run_index = 0;
    std::size_t cluster_byte_offset = 0;
    std::size_t cluster_byte_count = 0;
    std::uint32_t codepoint = 0;
    std::vector<std::uint32_t> shaped_glyph_ids;
    std::uint32_t resolved_glyph_id = 0;
    font_face_id resolved_face_id = 0;
    glyph_atlas_key cache_key;
    bool has_cache_key = false;
    bool glyph_supported = true;
    bool glyph_id_from_selection = false;
    bool glyph_id_matches_codepoint = false;
    bool used_fallback_glyph_id = false;
    std::uint32_t glyph_id_offset = 0;
    render_rect layout_bounds;
    bool has_layout_bounds = false;
    render_text_font_backend_library shaping_font_backend_library =
        render_text_font_backend_library::deterministic_fake;
    std::string shaping_font_backend_label;
    render_text_font_backend_capability_status shaping_font_backend_capability_status =
        render_text_font_backend_capability_status::unavailable;
    bool shaping_font_backend_used_deterministic_fallback = false;
    bool shaping_font_backend_fallback_only = false;
    render_text_font_backend_library raster_font_backend_library =
        render_text_font_backend_library::deterministic_fake;
    std::string raster_font_backend_label;
    render_text_font_backend_capability_status raster_font_backend_capability_status =
        render_text_font_backend_capability_status::unavailable;
    bool raster_font_backend_used_deterministic_fallback = false;
    bool raster_font_backend_fallback_only = false;
    render_text_font_rasterizer_status rasterizer_status =
        render_text_font_rasterizer_status::missing_font_source;
    bool raster_payload_matches_cache_key = false;
    bool rasterized_payload_skipped = true;
    bool payload_upload_ready = false;
    std::size_t payload_alpha_bytes = 0;
    std::size_t payload_rgba_bytes = 0;
    bool has_atlas_placement = false;
    render_text_atlas_page page;
    render_rect atlas_bounds;
    bool has_atlas_update = false;
    render_rect atlas_update_bounds;
    std::size_t atlas_update_rgba_bytes = 0;
};

struct render_text_glyph_atlas_materialization_snapshot {
    render_text_glyph_atlas_materialization_status status =
        render_text_glyph_atlas_materialization_status::skipped_missing_cache_key;
    std::size_t cluster_index = 0;
    std::size_t run_index = 0;
    std::size_t cluster_byte_offset = 0;
    std::size_t cluster_byte_count = 0;
    std::uint32_t codepoint = 0;
    std::vector<std::uint32_t> shaped_glyph_ids;
    std::uint32_t resolved_glyph_id = 0;
    font_face_id resolved_face_id = 0;
    glyph_atlas_key cache_key;
    bool has_cache_key = false;
    bool glyph_supported = true;
    bool glyph_id_from_selection = false;
    bool glyph_id_matches_codepoint = false;
    bool used_fallback_glyph_id = false;
    std::uint32_t glyph_id_offset = 0;
    render_rect layout_bounds;
    bool has_layout_bounds = false;
    render_text_font_backend_library shaping_font_backend_library =
        render_text_font_backend_library::deterministic_fake;
    std::string shaping_font_backend_label;
    render_text_font_backend_capability_status shaping_font_backend_capability_status =
        render_text_font_backend_capability_status::unavailable;
    bool shaping_font_backend_used_deterministic_fallback = false;
    bool shaping_font_backend_fallback_only = false;
    render_text_font_backend_library raster_font_backend_library =
        render_text_font_backend_library::deterministic_fake;
    std::string raster_font_backend_label;
    render_text_font_backend_capability_status raster_font_backend_capability_status =
        render_text_font_backend_capability_status::unavailable;
    bool raster_font_backend_used_deterministic_fallback = false;
    bool raster_font_backend_fallback_only = false;
    render_text_font_rasterizer_status rasterizer_status =
        render_text_font_rasterizer_status::missing_font_source;
    bool raster_payload_matches_cache_key = false;
    bool rasterized_payload_skipped = true;
    bool payload_upload_ready = false;
    std::size_t payload_alpha_bytes = 0;
    std::size_t payload_rgba_bytes = 0;
    std::size_t expected_payload_rgba_bytes = 0;
    bool payload_byte_count_matches = true;
    bool has_atlas_placement = false;
    render_text_atlas_page page;
    render_rect atlas_bounds;
    bool has_atlas_update = false;
    render_rect atlas_update_bounds;
    std::size_t atlas_update_rgba_bytes = 0;
    bool materialized = false;
    bool queued = false;
    bool clean_reuse = false;
    std::string diagnostic;
};

struct render_text_glyph_atlas_materialization_policy_snapshot {
    std::size_t request_count = 0;
    std::size_t materialized_count = 0;
    std::size_t upload_ready_count = 0;
    std::size_t clean_reuse_count = 0;
    std::size_t skipped_count = 0;
    std::size_t missing_cache_key_count = 0;
    std::size_t skipped_raster_payload_count = 0;
    std::size_t unsupported_glyph_count = 0;
    std::size_t payload_byte_count_mismatch_count = 0;
    std::size_t deterministic_fallback_count = 0;
    std::size_t real_backend_count = 0;
    std::size_t shaped_glyph_count = 0;
    std::size_t total_alpha_bytes = 0;
    std::size_t total_rgba_bytes = 0;
    std::size_t queued_atlas_update_bytes = 0;
};

enum class render_text_glyph_atlas_materialization_diff_status {
    unchanged,
    added,
    removed,
    changed,
};

inline std::string render_text_glyph_atlas_materialization_diff_status_name(
    const render_text_glyph_atlas_materialization_diff_status status)
{
    switch (status) {
    case render_text_glyph_atlas_materialization_diff_status::unchanged:
        return "unchanged";
    case render_text_glyph_atlas_materialization_diff_status::added:
        return "added";
    case render_text_glyph_atlas_materialization_diff_status::removed:
        return "removed";
    case render_text_glyph_atlas_materialization_diff_status::changed:
        return "changed";
    }

    return "unknown";
}

struct render_text_glyph_atlas_materialization_diff_key {
    std::string stable_id;
    glyph_atlas_key cache_key;
    bool has_cache_key = false;
    std::size_t run_index = 0;
    std::size_t cluster_index = 0;
    std::size_t cluster_byte_offset = 0;
    std::size_t cluster_byte_count = 0;
    std::uint32_t resolved_glyph_id = 0;
    font_face_id resolved_face_id = 0;

    friend bool operator==(
        const render_text_glyph_atlas_materialization_diff_key& left,
        const render_text_glyph_atlas_materialization_diff_key& right)
    {
        return left.stable_id == right.stable_id;
    }
};

struct render_text_glyph_atlas_materialization_policy_diff_snapshot {
    render_text_glyph_atlas_materialization_policy_snapshot before;
    render_text_glyph_atlas_materialization_policy_snapshot after;
    std::ptrdiff_t request_count_delta = 0;
    std::ptrdiff_t materialized_count_delta = 0;
    std::ptrdiff_t upload_ready_count_delta = 0;
    std::ptrdiff_t clean_reuse_count_delta = 0;
    std::ptrdiff_t skipped_count_delta = 0;
    std::ptrdiff_t missing_cache_key_count_delta = 0;
    std::ptrdiff_t skipped_raster_payload_count_delta = 0;
    std::ptrdiff_t unsupported_glyph_count_delta = 0;
    std::ptrdiff_t payload_byte_count_mismatch_count_delta = 0;
    std::ptrdiff_t deterministic_fallback_count_delta = 0;
    std::ptrdiff_t real_backend_count_delta = 0;
    std::ptrdiff_t shaped_glyph_count_delta = 0;
    std::ptrdiff_t total_alpha_bytes_delta = 0;
    std::ptrdiff_t total_rgba_bytes_delta = 0;
    std::ptrdiff_t queued_atlas_update_bytes_delta = 0;
    bool has_changes = false;
    std::string summary;
};

struct render_text_glyph_atlas_materialization_diff_snapshot {
    render_text_glyph_atlas_materialization_diff_status diff_status =
        render_text_glyph_atlas_materialization_diff_status::unchanged;
    render_text_glyph_atlas_materialization_diff_key key;
    render_text_glyph_atlas_materialization_snapshot before;
    render_text_glyph_atlas_materialization_snapshot after;
    bool has_before = false;
    bool has_after = false;
    bool materialization_changed = false;
    bool status_changed = false;
    bool upload_ready_changed = false;
    bool clean_reuse_changed = false;
    bool skipped_changed = false;
    bool payload_byte_count_changed = false;
    bool atlas_update_byte_count_changed = false;
    bool deterministic_fallback_changed = false;
    bool real_backend_changed = false;
    bool backend_path_changed = false;
    bool became_upload_ready = false;
    bool stopped_upload_ready = false;
    bool became_clean_reuse = false;
    bool stopped_clean_reuse = false;
    bool became_skipped = false;
    bool recovered_from_skipped = false;
    bool unsupported_glyph_regression = false;
    bool unsupported_glyph_recovery = false;
    bool missing_cache_key_regression = false;
    bool missing_cache_key_recovery = false;
    bool payload_byte_count_mismatch_regression = false;
    bool payload_byte_count_mismatch_recovery = false;
    bool deterministic_fallback_to_real_backend = false;
    bool real_backend_to_deterministic_fallback = false;
    std::ptrdiff_t payload_alpha_bytes_delta = 0;
    std::ptrdiff_t payload_rgba_bytes_delta = 0;
    std::ptrdiff_t atlas_update_rgba_bytes_delta = 0;
    std::string summary;
};

struct render_text_glyph_atlas_materialization_batch_diff_snapshot {
    std::vector<render_text_glyph_atlas_materialization_diff_snapshot> entries;
    render_text_glyph_atlas_materialization_policy_diff_snapshot policy_diff;
    std::size_t added_count = 0;
    std::size_t removed_count = 0;
    std::size_t changed_count = 0;
    std::size_t unchanged_count = 0;
    std::size_t upload_ready_transition_count = 0;
    std::size_t clean_reuse_transition_count = 0;
    std::size_t skipped_regression_count = 0;
    std::size_t skipped_recovery_count = 0;
    std::size_t unsupported_glyph_regression_count = 0;
    std::size_t unsupported_glyph_recovery_count = 0;
    std::size_t missing_cache_key_regression_count = 0;
    std::size_t missing_cache_key_recovery_count = 0;
    std::size_t payload_byte_count_mismatch_regression_count = 0;
    std::size_t payload_byte_count_mismatch_recovery_count = 0;
    std::size_t deterministic_fallback_to_real_backend_count = 0;
    std::size_t real_backend_to_deterministic_fallback_count = 0;
    std::ptrdiff_t total_payload_alpha_bytes_delta = 0;
    std::ptrdiff_t total_payload_rgba_bytes_delta = 0;
    std::ptrdiff_t total_atlas_update_rgba_bytes_delta = 0;
    std::string summary;

    bool has_changes() const
    {
        return added_count > 0 || removed_count > 0 || changed_count > 0 || policy_diff.has_changes;
    }
};

inline render_text_glyph_atlas_materialization_snapshot make_render_text_glyph_atlas_materialization(
    render_text_glyph_atlas_materialization_request request)
{
    const std::size_t expected_rgba_bytes = request.payload_alpha_bytes * 4U;
    const bool byte_count_matches = request.payload_rgba_bytes == expected_rgba_bytes;

    render_text_glyph_atlas_materialization_status status =
        render_text_glyph_atlas_materialization_status::skipped_missing_cache_key;
    std::string diagnostic = "glyph atlas materialization skipped because no cache key is available";
    if (!request.glyph_supported) {
        status = render_text_glyph_atlas_materialization_status::skipped_unsupported_glyph;
        diagnostic = "glyph atlas materialization skipped because the glyph is unsupported";
    } else if (request.has_cache_key && !byte_count_matches) {
        status = render_text_glyph_atlas_materialization_status::payload_byte_count_mismatch;
        diagnostic = "glyph atlas materialization saw a raster payload byte-count mismatch";
    } else if (request.has_cache_key && (request.rasterized_payload_skipped || !request.payload_upload_ready)) {
        status = render_text_glyph_atlas_materialization_status::skipped_raster_payload;
        diagnostic = "glyph atlas materialization skipped because raster payload is not upload-ready";
    } else if (request.has_cache_key && request.has_atlas_update) {
        status = render_text_glyph_atlas_materialization_status::materialized_upload_ready;
        diagnostic = "glyph atlas materialization produced an upload-ready atlas update request";
    } else if (request.has_cache_key) {
        status = render_text_glyph_atlas_materialization_status::materialized_clean_reuse;
        diagnostic = "glyph atlas materialization reused a clean atlas placement";
    }

    return render_text_glyph_atlas_materialization_snapshot{
        .status = status,
        .cluster_index = request.cluster_index,
        .run_index = request.run_index,
        .cluster_byte_offset = request.cluster_byte_offset,
        .cluster_byte_count = request.cluster_byte_count,
        .codepoint = request.codepoint,
        .shaped_glyph_ids = std::move(request.shaped_glyph_ids),
        .resolved_glyph_id = request.resolved_glyph_id,
        .resolved_face_id = request.resolved_face_id,
        .cache_key = request.cache_key,
        .has_cache_key = request.has_cache_key,
        .glyph_supported = request.glyph_supported,
        .glyph_id_from_selection = request.glyph_id_from_selection,
        .glyph_id_matches_codepoint = request.glyph_id_matches_codepoint,
        .used_fallback_glyph_id = request.used_fallback_glyph_id,
        .glyph_id_offset = request.glyph_id_offset,
        .layout_bounds = request.layout_bounds,
        .has_layout_bounds = request.has_layout_bounds,
        .shaping_font_backend_library = request.shaping_font_backend_library,
        .shaping_font_backend_label = request.shaping_font_backend_label,
        .shaping_font_backend_capability_status = request.shaping_font_backend_capability_status,
        .shaping_font_backend_used_deterministic_fallback =
            request.shaping_font_backend_used_deterministic_fallback,
        .shaping_font_backend_fallback_only = request.shaping_font_backend_fallback_only,
        .raster_font_backend_library = request.raster_font_backend_library,
        .raster_font_backend_label = request.raster_font_backend_label,
        .raster_font_backend_capability_status = request.raster_font_backend_capability_status,
        .raster_font_backend_used_deterministic_fallback =
            request.raster_font_backend_used_deterministic_fallback,
        .raster_font_backend_fallback_only = request.raster_font_backend_fallback_only,
        .rasterizer_status = request.rasterizer_status,
        .raster_payload_matches_cache_key = request.raster_payload_matches_cache_key,
        .rasterized_payload_skipped = request.rasterized_payload_skipped,
        .payload_upload_ready = request.payload_upload_ready,
        .payload_alpha_bytes = request.payload_alpha_bytes,
        .payload_rgba_bytes = request.payload_rgba_bytes,
        .expected_payload_rgba_bytes = expected_rgba_bytes,
        .payload_byte_count_matches = byte_count_matches,
        .has_atlas_placement = request.has_atlas_placement,
        .page = request.page,
        .atlas_bounds = request.atlas_bounds,
        .has_atlas_update = request.has_atlas_update,
        .atlas_update_bounds = request.atlas_update_bounds,
        .atlas_update_rgba_bytes = request.atlas_update_rgba_bytes,
        .materialized = status == render_text_glyph_atlas_materialization_status::materialized_upload_ready
            || status == render_text_glyph_atlas_materialization_status::materialized_clean_reuse,
        .queued = status == render_text_glyph_atlas_materialization_status::materialized_upload_ready,
        .clean_reuse = status == render_text_glyph_atlas_materialization_status::materialized_clean_reuse,
        .diagnostic = std::move(diagnostic),
    };
}

inline void append_render_text_glyph_atlas_materialization(
    std::vector<render_text_glyph_atlas_materialization_snapshot>& snapshots,
    render_text_glyph_atlas_materialization_policy_snapshot& policy,
    render_text_glyph_atlas_materialization_snapshot snapshot)
{
    ++policy.request_count;
    policy.shaped_glyph_count += snapshot.shaped_glyph_ids.size();
    policy.total_alpha_bytes += snapshot.payload_alpha_bytes;
    policy.total_rgba_bytes += snapshot.payload_rgba_bytes;
    if (snapshot.queued) {
        policy.queued_atlas_update_bytes += snapshot.atlas_update_rgba_bytes;
    }
    if (snapshot.shaping_font_backend_used_deterministic_fallback
        || snapshot.raster_font_backend_used_deterministic_fallback) {
        ++policy.deterministic_fallback_count;
    } else if (
        snapshot.shaping_font_backend_library != render_text_font_backend_library::deterministic_fake
        || snapshot.raster_font_backend_library != render_text_font_backend_library::deterministic_fake) {
        ++policy.real_backend_count;
    }

    switch (snapshot.status) {
    case render_text_glyph_atlas_materialization_status::materialized_upload_ready:
        ++policy.materialized_count;
        ++policy.upload_ready_count;
        break;
    case render_text_glyph_atlas_materialization_status::materialized_clean_reuse:
        ++policy.materialized_count;
        ++policy.clean_reuse_count;
        break;
    case render_text_glyph_atlas_materialization_status::skipped_missing_cache_key:
        ++policy.skipped_count;
        ++policy.missing_cache_key_count;
        break;
    case render_text_glyph_atlas_materialization_status::skipped_raster_payload:
        ++policy.skipped_count;
        ++policy.skipped_raster_payload_count;
        break;
    case render_text_glyph_atlas_materialization_status::skipped_unsupported_glyph:
        ++policy.skipped_count;
        ++policy.unsupported_glyph_count;
        break;
    case render_text_glyph_atlas_materialization_status::payload_byte_count_mismatch:
        ++policy.skipped_count;
        ++policy.payload_byte_count_mismatch_count;
        break;
    }

    snapshots.push_back(std::move(snapshot));
}

inline std::ptrdiff_t render_text_glyph_atlas_materialization_delta(
    const std::size_t before,
    const std::size_t after)
{
    return static_cast<std::ptrdiff_t>(after) - static_cast<std::ptrdiff_t>(before);
}

inline bool render_text_glyph_atlas_materialization_rect_equal(
    const render_rect& before,
    const render_rect& after)
{
    return before.x == after.x
        && before.y == after.y
        && before.width == after.width
        && before.height == after.height;
}

inline bool render_text_glyph_atlas_materialization_is_upload_ready(
    const render_text_glyph_atlas_materialization_snapshot& snapshot)
{
    return snapshot.status == render_text_glyph_atlas_materialization_status::materialized_upload_ready;
}

inline bool render_text_glyph_atlas_materialization_is_clean_reuse(
    const render_text_glyph_atlas_materialization_snapshot& snapshot)
{
    return snapshot.status == render_text_glyph_atlas_materialization_status::materialized_clean_reuse;
}

inline bool render_text_glyph_atlas_materialization_is_skipped(
    const render_text_glyph_atlas_materialization_snapshot& snapshot)
{
    return snapshot.status == render_text_glyph_atlas_materialization_status::skipped_missing_cache_key
        || snapshot.status == render_text_glyph_atlas_materialization_status::skipped_raster_payload
        || snapshot.status == render_text_glyph_atlas_materialization_status::skipped_unsupported_glyph
        || snapshot.status == render_text_glyph_atlas_materialization_status::payload_byte_count_mismatch;
}

inline bool render_text_glyph_atlas_materialization_is_missing_cache_key(
    const render_text_glyph_atlas_materialization_snapshot& snapshot)
{
    return snapshot.status == render_text_glyph_atlas_materialization_status::skipped_missing_cache_key;
}

inline bool render_text_glyph_atlas_materialization_is_unsupported_glyph(
    const render_text_glyph_atlas_materialization_snapshot& snapshot)
{
    return snapshot.status == render_text_glyph_atlas_materialization_status::skipped_unsupported_glyph;
}

inline bool render_text_glyph_atlas_materialization_has_payload_byte_count_mismatch(
    const render_text_glyph_atlas_materialization_snapshot& snapshot)
{
    return snapshot.status == render_text_glyph_atlas_materialization_status::payload_byte_count_mismatch
        || !snapshot.payload_byte_count_matches;
}

inline bool render_text_glyph_atlas_materialization_uses_deterministic_fallback(
    const render_text_glyph_atlas_materialization_snapshot& materialization)
{
    return materialization.shaping_font_backend_used_deterministic_fallback
        || materialization.raster_font_backend_used_deterministic_fallback
        || materialization.shaping_font_backend_fallback_only
        || materialization.raster_font_backend_fallback_only;
}

inline bool render_text_glyph_atlas_materialization_uses_real_backend(
    const render_text_glyph_atlas_materialization_snapshot& materialization)
{
    return materialization.shaping_font_backend_library != render_text_font_backend_library::deterministic_fake
        || materialization.raster_font_backend_library != render_text_font_backend_library::deterministic_fake;
}

inline std::string render_text_glyph_atlas_materialization_stable_id_for(
    const render_text_glyph_atlas_materialization_snapshot& materialization)
{
    if (materialization.has_cache_key) {
        return "cache:"
            + std::to_string(materialization.cache_key.face_id)
            + ":"
            + std::to_string(materialization.cache_key.glyph_id)
            + ":"
            + std::to_string(materialization.cache_key.pixel_size);
    }

    return "run:"
        + std::to_string(materialization.run_index)
        + ":cluster:"
        + std::to_string(materialization.cluster_index)
        + ":bytes:"
        + std::to_string(materialization.cluster_byte_offset)
        + ":"
        + std::to_string(materialization.cluster_byte_count)
        + ":glyph:"
        + std::to_string(materialization.resolved_glyph_id)
        + ":face:"
        + std::to_string(materialization.resolved_face_id);
}

inline render_text_glyph_atlas_materialization_diff_key render_text_glyph_atlas_materialization_diff_key_for(
    const render_text_glyph_atlas_materialization_snapshot& materialization)
{
    return render_text_glyph_atlas_materialization_diff_key{
        .stable_id = render_text_glyph_atlas_materialization_stable_id_for(materialization),
        .cache_key = materialization.cache_key,
        .has_cache_key = materialization.has_cache_key,
        .run_index = materialization.run_index,
        .cluster_index = materialization.cluster_index,
        .cluster_byte_offset = materialization.cluster_byte_offset,
        .cluster_byte_count = materialization.cluster_byte_count,
        .resolved_glyph_id = materialization.resolved_glyph_id,
        .resolved_face_id = materialization.resolved_face_id,
    };
}

inline bool render_text_glyph_atlas_materialization_relevant_fields_equal(
    const render_text_glyph_atlas_materialization_snapshot& before,
    const render_text_glyph_atlas_materialization_snapshot& after)
{
    return before.status == after.status
        && before.cluster_index == after.cluster_index
        && before.run_index == after.run_index
        && before.cluster_byte_offset == after.cluster_byte_offset
        && before.cluster_byte_count == after.cluster_byte_count
        && before.codepoint == after.codepoint
        && before.shaped_glyph_ids == after.shaped_glyph_ids
        && before.resolved_glyph_id == after.resolved_glyph_id
        && before.resolved_face_id == after.resolved_face_id
        && before.cache_key == after.cache_key
        && before.has_cache_key == after.has_cache_key
        && before.glyph_supported == after.glyph_supported
        && before.glyph_id_from_selection == after.glyph_id_from_selection
        && before.glyph_id_matches_codepoint == after.glyph_id_matches_codepoint
        && before.used_fallback_glyph_id == after.used_fallback_glyph_id
        && before.glyph_id_offset == after.glyph_id_offset
        && render_text_glyph_atlas_materialization_rect_equal(before.layout_bounds, after.layout_bounds)
        && before.has_layout_bounds == after.has_layout_bounds
        && before.shaping_font_backend_library == after.shaping_font_backend_library
        && before.shaping_font_backend_label == after.shaping_font_backend_label
        && before.shaping_font_backend_capability_status == after.shaping_font_backend_capability_status
        && before.shaping_font_backend_used_deterministic_fallback
            == after.shaping_font_backend_used_deterministic_fallback
        && before.shaping_font_backend_fallback_only == after.shaping_font_backend_fallback_only
        && before.raster_font_backend_library == after.raster_font_backend_library
        && before.raster_font_backend_label == after.raster_font_backend_label
        && before.raster_font_backend_capability_status == after.raster_font_backend_capability_status
        && before.raster_font_backend_used_deterministic_fallback
            == after.raster_font_backend_used_deterministic_fallback
        && before.raster_font_backend_fallback_only == after.raster_font_backend_fallback_only
        && before.rasterizer_status == after.rasterizer_status
        && before.raster_payload_matches_cache_key == after.raster_payload_matches_cache_key
        && before.rasterized_payload_skipped == after.rasterized_payload_skipped
        && before.payload_upload_ready == after.payload_upload_ready
        && before.payload_alpha_bytes == after.payload_alpha_bytes
        && before.payload_rgba_bytes == after.payload_rgba_bytes
        && before.expected_payload_rgba_bytes == after.expected_payload_rgba_bytes
        && before.payload_byte_count_matches == after.payload_byte_count_matches
        && before.has_atlas_placement == after.has_atlas_placement
        && before.page.id == after.page.id
        && before.page.revision == after.page.revision
        && before.page.width == after.page.width
        && before.page.height == after.page.height
        && render_text_glyph_atlas_materialization_rect_equal(before.atlas_bounds, after.atlas_bounds)
        && before.has_atlas_update == after.has_atlas_update
        && render_text_glyph_atlas_materialization_rect_equal(
            before.atlas_update_bounds,
            after.atlas_update_bounds)
        && before.atlas_update_rgba_bytes == after.atlas_update_rgba_bytes
        && before.materialized == after.materialized
        && before.queued == after.queued
        && before.clean_reuse == after.clean_reuse;
}

inline std::string render_text_glyph_atlas_materialization_diff_summary_for(
    const render_text_glyph_atlas_materialization_diff_status status,
    const render_text_glyph_atlas_materialization_diff_key& key)
{
    return "glyph atlas materialization "
        + render_text_glyph_atlas_materialization_diff_status_name(status)
        + " for "
        + key.stable_id;
}

inline render_text_glyph_atlas_materialization_diff_snapshot diff_render_text_glyph_atlas_materializations(
    const render_text_glyph_atlas_materialization_snapshot* before,
    const render_text_glyph_atlas_materialization_snapshot* after)
{
    const bool has_before = before != nullptr;
    const bool has_after = after != nullptr;
    const render_text_glyph_atlas_materialization_snapshot before_value =
        has_before ? *before : render_text_glyph_atlas_materialization_snapshot{};
    const render_text_glyph_atlas_materialization_snapshot after_value =
        has_after ? *after : render_text_glyph_atlas_materialization_snapshot{};
    const render_text_glyph_atlas_materialization_diff_key key =
        has_after
            ? render_text_glyph_atlas_materialization_diff_key_for(after_value)
            : render_text_glyph_atlas_materialization_diff_key_for(before_value);

    const bool upload_ready_before = has_before
        && render_text_glyph_atlas_materialization_is_upload_ready(before_value);
    const bool upload_ready_after = has_after
        && render_text_glyph_atlas_materialization_is_upload_ready(after_value);
    const bool clean_reuse_before = has_before
        && render_text_glyph_atlas_materialization_is_clean_reuse(before_value);
    const bool clean_reuse_after = has_after
        && render_text_glyph_atlas_materialization_is_clean_reuse(after_value);
    const bool skipped_before = has_before
        && render_text_glyph_atlas_materialization_is_skipped(before_value);
    const bool skipped_after = has_after
        && render_text_glyph_atlas_materialization_is_skipped(after_value);
    const bool unsupported_before = has_before
        && render_text_glyph_atlas_materialization_is_unsupported_glyph(before_value);
    const bool unsupported_after = has_after
        && render_text_glyph_atlas_materialization_is_unsupported_glyph(after_value);
    const bool missing_cache_before = has_before
        && render_text_glyph_atlas_materialization_is_missing_cache_key(before_value);
    const bool missing_cache_after = has_after
        && render_text_glyph_atlas_materialization_is_missing_cache_key(after_value);
    const bool payload_mismatch_before = has_before
        && render_text_glyph_atlas_materialization_has_payload_byte_count_mismatch(before_value);
    const bool payload_mismatch_after = has_after
        && render_text_glyph_atlas_materialization_has_payload_byte_count_mismatch(after_value);
    const bool deterministic_before = has_before
        && render_text_glyph_atlas_materialization_uses_deterministic_fallback(before_value);
    const bool deterministic_after = has_after
        && render_text_glyph_atlas_materialization_uses_deterministic_fallback(after_value);
    const bool real_before = has_before
        && render_text_glyph_atlas_materialization_uses_real_backend(before_value);
    const bool real_after = has_after
        && render_text_glyph_atlas_materialization_uses_real_backend(after_value);

    render_text_glyph_atlas_materialization_diff_status status =
        render_text_glyph_atlas_materialization_diff_status::unchanged;
    if (!has_before && has_after) {
        status = render_text_glyph_atlas_materialization_diff_status::added;
    } else if (has_before && !has_after) {
        status = render_text_glyph_atlas_materialization_diff_status::removed;
    } else if (
        has_before
        && has_after
        && !render_text_glyph_atlas_materialization_relevant_fields_equal(before_value, after_value)) {
        status = render_text_glyph_atlas_materialization_diff_status::changed;
    }

    return render_text_glyph_atlas_materialization_diff_snapshot{
        .diff_status = status,
        .key = key,
        .before = before_value,
        .after = after_value,
        .has_before = has_before,
        .has_after = has_after,
        .materialization_changed = status != render_text_glyph_atlas_materialization_diff_status::unchanged,
        .status_changed = !has_before || !has_after || before_value.status != after_value.status,
        .upload_ready_changed = upload_ready_before != upload_ready_after,
        .clean_reuse_changed = clean_reuse_before != clean_reuse_after,
        .skipped_changed = skipped_before != skipped_after,
        .payload_byte_count_changed =
            !has_before || !has_after || before_value.payload_rgba_bytes != after_value.payload_rgba_bytes,
        .atlas_update_byte_count_changed =
            !has_before || !has_after || before_value.atlas_update_rgba_bytes != after_value.atlas_update_rgba_bytes,
        .deterministic_fallback_changed = deterministic_before != deterministic_after,
        .real_backend_changed = real_before != real_after,
        .backend_path_changed = deterministic_before != deterministic_after || real_before != real_after,
        .became_upload_ready = !upload_ready_before && upload_ready_after,
        .stopped_upload_ready = upload_ready_before && !upload_ready_after,
        .became_clean_reuse = !clean_reuse_before && clean_reuse_after,
        .stopped_clean_reuse = clean_reuse_before && !clean_reuse_after,
        .became_skipped = !skipped_before && skipped_after,
        .recovered_from_skipped = skipped_before && !skipped_after,
        .unsupported_glyph_regression = !unsupported_before && unsupported_after,
        .unsupported_glyph_recovery = unsupported_before && !unsupported_after,
        .missing_cache_key_regression = !missing_cache_before && missing_cache_after,
        .missing_cache_key_recovery = missing_cache_before && !missing_cache_after,
        .payload_byte_count_mismatch_regression = !payload_mismatch_before && payload_mismatch_after,
        .payload_byte_count_mismatch_recovery = payload_mismatch_before && !payload_mismatch_after,
        .deterministic_fallback_to_real_backend =
            deterministic_before && real_after && !deterministic_after,
        .real_backend_to_deterministic_fallback =
            real_before && deterministic_after && !real_after,
        .payload_alpha_bytes_delta = render_text_glyph_atlas_materialization_delta(
            before_value.payload_alpha_bytes,
            after_value.payload_alpha_bytes),
        .payload_rgba_bytes_delta = render_text_glyph_atlas_materialization_delta(
            before_value.payload_rgba_bytes,
            after_value.payload_rgba_bytes),
        .atlas_update_rgba_bytes_delta = render_text_glyph_atlas_materialization_delta(
            before_value.atlas_update_rgba_bytes,
            after_value.atlas_update_rgba_bytes),
        .summary = render_text_glyph_atlas_materialization_diff_summary_for(status, key),
    };
}

inline render_text_glyph_atlas_materialization_policy_snapshot
summarize_render_text_glyph_atlas_materialization_policy(
    const std::vector<render_text_glyph_atlas_materialization_snapshot>& materializations)
{
    std::vector<render_text_glyph_atlas_materialization_snapshot> ignored;
    render_text_glyph_atlas_materialization_policy_snapshot policy;
    ignored.reserve(materializations.size());
    for (render_text_glyph_atlas_materialization_snapshot materialization : materializations) {
        append_render_text_glyph_atlas_materialization(ignored, policy, std::move(materialization));
    }
    return policy;
}

inline render_text_glyph_atlas_materialization_policy_diff_snapshot
diff_render_text_glyph_atlas_materialization_policies(
    const render_text_glyph_atlas_materialization_policy_snapshot& before,
    const render_text_glyph_atlas_materialization_policy_snapshot& after)
{
    render_text_glyph_atlas_materialization_policy_diff_snapshot diff{
        .before = before,
        .after = after,
        .request_count_delta = render_text_glyph_atlas_materialization_delta(
            before.request_count,
            after.request_count),
        .materialized_count_delta = render_text_glyph_atlas_materialization_delta(
            before.materialized_count,
            after.materialized_count),
        .upload_ready_count_delta = render_text_glyph_atlas_materialization_delta(
            before.upload_ready_count,
            after.upload_ready_count),
        .clean_reuse_count_delta = render_text_glyph_atlas_materialization_delta(
            before.clean_reuse_count,
            after.clean_reuse_count),
        .skipped_count_delta = render_text_glyph_atlas_materialization_delta(
            before.skipped_count,
            after.skipped_count),
        .missing_cache_key_count_delta = render_text_glyph_atlas_materialization_delta(
            before.missing_cache_key_count,
            after.missing_cache_key_count),
        .skipped_raster_payload_count_delta = render_text_glyph_atlas_materialization_delta(
            before.skipped_raster_payload_count,
            after.skipped_raster_payload_count),
        .unsupported_glyph_count_delta = render_text_glyph_atlas_materialization_delta(
            before.unsupported_glyph_count,
            after.unsupported_glyph_count),
        .payload_byte_count_mismatch_count_delta = render_text_glyph_atlas_materialization_delta(
            before.payload_byte_count_mismatch_count,
            after.payload_byte_count_mismatch_count),
        .deterministic_fallback_count_delta = render_text_glyph_atlas_materialization_delta(
            before.deterministic_fallback_count,
            after.deterministic_fallback_count),
        .real_backend_count_delta = render_text_glyph_atlas_materialization_delta(
            before.real_backend_count,
            after.real_backend_count),
        .shaped_glyph_count_delta = render_text_glyph_atlas_materialization_delta(
            before.shaped_glyph_count,
            after.shaped_glyph_count),
        .total_alpha_bytes_delta = render_text_glyph_atlas_materialization_delta(
            before.total_alpha_bytes,
            after.total_alpha_bytes),
        .total_rgba_bytes_delta = render_text_glyph_atlas_materialization_delta(
            before.total_rgba_bytes,
            after.total_rgba_bytes),
        .queued_atlas_update_bytes_delta = render_text_glyph_atlas_materialization_delta(
            before.queued_atlas_update_bytes,
            after.queued_atlas_update_bytes),
    };
    diff.has_changes =
        diff.request_count_delta != 0
        || diff.materialized_count_delta != 0
        || diff.upload_ready_count_delta != 0
        || diff.clean_reuse_count_delta != 0
        || diff.skipped_count_delta != 0
        || diff.missing_cache_key_count_delta != 0
        || diff.skipped_raster_payload_count_delta != 0
        || diff.unsupported_glyph_count_delta != 0
        || diff.payload_byte_count_mismatch_count_delta != 0
        || diff.deterministic_fallback_count_delta != 0
        || diff.real_backend_count_delta != 0
        || diff.shaped_glyph_count_delta != 0
        || diff.total_alpha_bytes_delta != 0
        || diff.total_rgba_bytes_delta != 0
        || diff.queued_atlas_update_bytes_delta != 0;
    diff.summary = "glyph atlas materialization policy diff: "
        + std::to_string(before.request_count)
        + " -> "
        + std::to_string(after.request_count)
        + " requests";
    return diff;
}

inline const render_text_glyph_atlas_materialization_snapshot*
find_render_text_glyph_atlas_materialization_by_diff_key(
    const std::vector<render_text_glyph_atlas_materialization_snapshot>& materializations,
    const render_text_glyph_atlas_materialization_diff_key& key,
    const std::vector<bool>& used)
{
    for (std::size_t index = 0; index < materializations.size(); ++index) {
        if (used[index]) {
            continue;
        }
        if (render_text_glyph_atlas_materialization_diff_key_for(materializations[index]) == key) {
            return &materializations[index];
        }
    }
    return nullptr;
}

inline std::size_t render_text_glyph_atlas_materialization_index_for(
    const std::vector<render_text_glyph_atlas_materialization_snapshot>& materializations,
    const render_text_glyph_atlas_materialization_snapshot* materialization)
{
    return static_cast<std::size_t>(materialization - materializations.data());
}

inline render_text_glyph_atlas_materialization_batch_diff_snapshot
diff_render_text_glyph_atlas_materialization_batches(
    const std::vector<render_text_glyph_atlas_materialization_snapshot>& before,
    const render_text_glyph_atlas_materialization_policy_snapshot& before_policy,
    const std::vector<render_text_glyph_atlas_materialization_snapshot>& after,
    const render_text_glyph_atlas_materialization_policy_snapshot& after_policy)
{
    render_text_glyph_atlas_materialization_batch_diff_snapshot diff;
    diff.policy_diff = diff_render_text_glyph_atlas_materialization_policies(before_policy, after_policy);

    std::vector<bool> used_after(after.size(), false);
    diff.entries.reserve(before.size() + after.size());
    for (const render_text_glyph_atlas_materialization_snapshot& before_materialization : before) {
        const render_text_glyph_atlas_materialization_diff_key key =
            render_text_glyph_atlas_materialization_diff_key_for(before_materialization);
        const render_text_glyph_atlas_materialization_snapshot* after_materialization =
            find_render_text_glyph_atlas_materialization_by_diff_key(after, key, used_after);
        if (after_materialization != nullptr) {
            const std::size_t after_index =
                render_text_glyph_atlas_materialization_index_for(after, after_materialization);
            used_after[after_index] = true;
        }
        diff.entries.push_back(diff_render_text_glyph_atlas_materializations(
            &before_materialization,
            after_materialization));
    }
    for (std::size_t after_index = 0; after_index < after.size(); ++after_index) {
        if (used_after[after_index]) {
            continue;
        }
        diff.entries.push_back(diff_render_text_glyph_atlas_materializations(nullptr, &after[after_index]));
    }

    for (const render_text_glyph_atlas_materialization_diff_snapshot& entry : diff.entries) {
        switch (entry.diff_status) {
        case render_text_glyph_atlas_materialization_diff_status::unchanged:
            ++diff.unchanged_count;
            break;
        case render_text_glyph_atlas_materialization_diff_status::added:
            ++diff.added_count;
            break;
        case render_text_glyph_atlas_materialization_diff_status::removed:
            ++diff.removed_count;
            break;
        case render_text_glyph_atlas_materialization_diff_status::changed:
            ++diff.changed_count;
            break;
        }

        if (entry.upload_ready_changed) {
            ++diff.upload_ready_transition_count;
        }
        if (entry.clean_reuse_changed) {
            ++diff.clean_reuse_transition_count;
        }
        if (entry.became_skipped) {
            ++diff.skipped_regression_count;
        }
        if (entry.recovered_from_skipped) {
            ++diff.skipped_recovery_count;
        }
        if (entry.unsupported_glyph_regression) {
            ++diff.unsupported_glyph_regression_count;
        }
        if (entry.unsupported_glyph_recovery) {
            ++diff.unsupported_glyph_recovery_count;
        }
        if (entry.missing_cache_key_regression) {
            ++diff.missing_cache_key_regression_count;
        }
        if (entry.missing_cache_key_recovery) {
            ++diff.missing_cache_key_recovery_count;
        }
        if (entry.payload_byte_count_mismatch_regression) {
            ++diff.payload_byte_count_mismatch_regression_count;
        }
        if (entry.payload_byte_count_mismatch_recovery) {
            ++diff.payload_byte_count_mismatch_recovery_count;
        }
        if (entry.deterministic_fallback_to_real_backend) {
            ++diff.deterministic_fallback_to_real_backend_count;
        }
        if (entry.real_backend_to_deterministic_fallback) {
            ++diff.real_backend_to_deterministic_fallback_count;
        }
        diff.total_payload_alpha_bytes_delta += entry.payload_alpha_bytes_delta;
        diff.total_payload_rgba_bytes_delta += entry.payload_rgba_bytes_delta;
        diff.total_atlas_update_rgba_bytes_delta += entry.atlas_update_rgba_bytes_delta;
    }
    diff.summary = "glyph atlas materialization batch diff: "
        + std::to_string(diff.added_count)
        + " added, "
        + std::to_string(diff.removed_count)
        + " removed, "
        + std::to_string(diff.changed_count)
        + " changed";
    return diff;
}

inline render_text_glyph_atlas_materialization_batch_diff_snapshot
diff_render_text_glyph_atlas_materialization_batches(
    const std::vector<render_text_glyph_atlas_materialization_snapshot>& before,
    const std::vector<render_text_glyph_atlas_materialization_snapshot>& after)
{
    return diff_render_text_glyph_atlas_materialization_batches(
        before,
        summarize_render_text_glyph_atlas_materialization_policy(before),
        after,
        summarize_render_text_glyph_atlas_materialization_policy(after));
}

enum class render_text_glyph_atlas_page_plan_status {
    reused_existing_placement,
    selected_existing_page,
    allocated_new_page,
    skipped_materialization,
    overflow,
};

inline std::string render_text_glyph_atlas_page_plan_status_name(
    const render_text_glyph_atlas_page_plan_status status)
{
    switch (status) {
    case render_text_glyph_atlas_page_plan_status::reused_existing_placement:
        return "reused_existing_placement";
    case render_text_glyph_atlas_page_plan_status::selected_existing_page:
        return "selected_existing_page";
    case render_text_glyph_atlas_page_plan_status::allocated_new_page:
        return "allocated_new_page";
    case render_text_glyph_atlas_page_plan_status::skipped_materialization:
        return "skipped_materialization";
    case render_text_glyph_atlas_page_plan_status::overflow:
        return "overflow";
    }

    return "unknown";
}

struct render_text_glyph_atlas_page_plan_constraints {
    std::size_t width = 256;
    std::size_t height = 256;
    std::size_t padding = 1;
    std::size_t max_pages = 0;

    bool has_page_extent() const
    {
        return width > 0 && height > 0;
    }
};

struct render_text_glyph_atlas_page_plan_entry_snapshot {
    std::size_t materialization_index = 0;
    std::string materialization_id;
    std::size_t run_index = 0;
    std::size_t cluster_index = 0;
    std::size_t cluster_byte_offset = 0;
    std::size_t cluster_byte_count = 0;
    glyph_atlas_key cache_key;
    bool has_cache_key = false;
    render_text_glyph_atlas_materialization_status materialization_status =
        render_text_glyph_atlas_materialization_status::skipped_missing_cache_key;
    render_text_glyph_atlas_page_plan_status status =
        render_text_glyph_atlas_page_plan_status::skipped_materialization;
    render_text_atlas_page page;
    render_rect atlas_bounds;
    bool has_atlas_bounds = false;
    std::size_t page_index = 0;
    std::size_t glyph_width = 0;
    std::size_t glyph_height = 0;
    std::size_t padded_width = 0;
    std::size_t padded_height = 0;
    std::size_t placed_area = 0;
    std::size_t page_capacity = 0;
    std::size_t page_used_area_before = 0;
    std::size_t page_used_area_after = 0;
    float estimated_occupancy_before = 0.0f;
    float estimated_occupancy_after = 0.0f;
    float estimated_fragmentation_before = 0.0f;
    float estimated_fragmentation_after = 0.0f;
    std::size_t upload_rgba_bytes = 0;
    bool selected_existing_page = false;
    bool allocated_new_page = false;
    bool reused_existing_placement = false;
    bool skipped = true;
    bool overflow = false;
    bool upload_ready = false;
    bool clean_reuse = false;
    glyph_atlas_key eviction_candidate_key;
    bool has_eviction_candidate = false;
    std::string diagnostic;
};

struct render_text_glyph_atlas_page_plan_page_snapshot {
    render_text_atlas_page page;
    bool allocated_by_plan = false;
    bool referenced_by_pending_update = false;
    std::size_t materialization_count = 0;
    std::size_t upload_ready_count = 0;
    std::size_t clean_reuse_count = 0;
    std::size_t reused_placement_count = 0;
    std::size_t pending_update_count = 0;
    std::size_t page_capacity = 0;
    std::size_t used_area = 0;
    std::size_t available_area = 0;
    std::size_t upload_rgba_bytes = 0;
    std::size_t pending_update_rgba_bytes = 0;
    float estimated_occupancy = 0.0f;
    float estimated_fragmentation = 0.0f;
    bool overflow = false;
    glyph_atlas_key eviction_candidate_key;
    bool has_eviction_candidate = false;
};

struct render_text_glyph_atlas_page_plan_policy_snapshot {
    std::size_t materialization_count = 0;
    std::size_t planned_entry_count = 0;
    std::size_t skipped_count = 0;
    std::size_t overflow_count = 0;
    std::size_t allocated_new_page_count = 0;
    std::size_t selected_existing_page_count = 0;
    std::size_t reused_placement_count = 0;
    std::size_t pending_update_count = 0;
    std::size_t page_count = 0;
    std::size_t eviction_candidate_count = 0;
    std::size_t materialization_upload_rgba_bytes = 0;
    std::size_t pending_update_rgba_bytes = 0;
    std::size_t total_upload_rgba_bytes = 0;
    std::size_t total_page_capacity = 0;
    std::size_t total_used_area = 0;
    std::size_t total_available_area = 0;
    float estimated_occupancy = 0.0f;
    float estimated_fragmentation = 0.0f;
    bool has_overflow = false;
    bool has_eviction_candidates = false;
    std::string diagnostic;
};

struct render_text_glyph_atlas_page_plan_request {
    std::vector<render_text_glyph_atlas_materialization_snapshot> materializations;
    std::vector<render_text_atlas_update> pending_updates;
    render_text_glyph_atlas_page_plan_constraints constraints;
};

struct render_text_glyph_atlas_page_plan_snapshot {
    std::vector<render_text_glyph_atlas_page_plan_entry_snapshot> entries;
    std::vector<render_text_glyph_atlas_page_plan_page_snapshot> pages;
    render_text_glyph_atlas_page_plan_policy_snapshot policy;

    bool ok() const
    {
        return !policy.has_overflow;
    }

    bool has_overflow() const
    {
        return policy.has_overflow;
    }

    bool has_eviction_candidates() const
    {
        return policy.has_eviction_candidates;
    }
};

inline render_text_glyph_atlas_page_plan_constraints render_text_glyph_atlas_page_plan_constraints_for(
    const glyph_atlas_page_config& config,
    const std::size_t max_pages = 0)
{
    return render_text_glyph_atlas_page_plan_constraints{
        .width = config.width,
        .height = config.height,
        .padding = config.padding,
        .max_pages = max_pages,
    };
}

inline std::size_t render_text_glyph_atlas_page_plan_rect_dimension_for(
    const float value,
    const std::size_t fallback)
{
    if (value <= 0.0f) {
        return fallback;
    }

    const std::size_t truncated = static_cast<std::size_t>(value);
    return static_cast<float>(truncated) == value ? truncated : truncated + 1U;
}

inline std::size_t render_text_glyph_atlas_page_plan_rect_area(const render_rect& rect)
{
    const std::size_t width = render_text_glyph_atlas_page_plan_rect_dimension_for(rect.width, 0);
    const std::size_t height = render_text_glyph_atlas_page_plan_rect_dimension_for(rect.height, 0);
    return width * height;
}

inline float render_text_glyph_atlas_page_plan_ratio(
    const std::size_t numerator,
    const std::size_t denominator)
{
    if (denominator == 0U) {
        return 0.0f;
    }
    return static_cast<float>(numerator) / static_cast<float>(denominator);
}

inline std::size_t render_text_glyph_atlas_page_plan_pending_update_bytes(
    const render_text_atlas_update& update)
{
    return update.rgba.size();
}

inline bool render_text_glyph_atlas_page_plan_key_exists(
    const std::vector<glyph_atlas_key>& keys,
    const glyph_atlas_key& key)
{
    return std::find(keys.begin(), keys.end(), key) != keys.end();
}

inline std::size_t render_text_glyph_atlas_page_plan_width_for(
    const render_text_glyph_atlas_materialization_snapshot& materialization)
{
    if (materialization.has_atlas_placement && materialization.atlas_bounds.width > 0.0f) {
        return render_text_glyph_atlas_page_plan_rect_dimension_for(materialization.atlas_bounds.width, 1);
    }
    if (materialization.has_atlas_update && materialization.atlas_update_bounds.width > 0.0f) {
        return render_text_glyph_atlas_page_plan_rect_dimension_for(materialization.atlas_update_bounds.width, 1);
    }
    if (materialization.has_layout_bounds && materialization.layout_bounds.width > 0.0f) {
        return render_text_glyph_atlas_page_plan_rect_dimension_for(materialization.layout_bounds.width, 1);
    }
    return 1U;
}

inline std::size_t render_text_glyph_atlas_page_plan_height_for(
    const render_text_glyph_atlas_materialization_snapshot& materialization)
{
    if (materialization.has_atlas_placement && materialization.atlas_bounds.height > 0.0f) {
        return render_text_glyph_atlas_page_plan_rect_dimension_for(materialization.atlas_bounds.height, 1);
    }
    if (materialization.has_atlas_update && materialization.atlas_update_bounds.height > 0.0f) {
        return render_text_glyph_atlas_page_plan_rect_dimension_for(materialization.atlas_update_bounds.height, 1);
    }
    if (materialization.has_layout_bounds && materialization.layout_bounds.height > 0.0f) {
        return render_text_glyph_atlas_page_plan_rect_dimension_for(materialization.layout_bounds.height, 1);
    }
    return 1U;
}

inline render_text_glyph_atlas_page_plan_snapshot plan_render_text_glyph_atlas_pages(
    render_text_glyph_atlas_page_plan_request request)
{
    struct working_page {
        render_text_glyph_atlas_page_plan_page_snapshot snapshot;
        std::size_t cursor_x = 0;
        std::size_t cursor_y = 0;
        std::size_t row_height = 0;
        std::vector<glyph_atlas_key> keys;
    };

    auto page_capacity_for = [](const render_text_atlas_page& page) {
        return page.width * page.height;
    };
    auto finish_page_metrics = [&](render_text_glyph_atlas_page_plan_page_snapshot& page) {
        page.page_capacity = page_capacity_for(page.page);
        page.available_area = page.used_area >= page.page_capacity ? 0U : page.page_capacity - page.used_area;
        page.estimated_occupancy =
            render_text_glyph_atlas_page_plan_ratio(page.used_area, page.page_capacity);
        page.estimated_fragmentation =
            page.page_capacity == 0U ? 0.0f : 1.0f - page.estimated_occupancy;
    };

    render_text_glyph_atlas_page_plan_snapshot plan;
    plan.policy.materialization_count = request.materializations.size();
    plan.policy.pending_update_count = request.pending_updates.size();
    plan.entries.reserve(request.materializations.size());

    std::vector<working_page> pages;
    auto find_page = [&](const render_text_atlas_page_id page_id) -> working_page* {
        const auto match = std::find_if(
            pages.begin(),
            pages.end(),
            [&](const working_page& page) { return page.snapshot.page.id == page_id; });
        return match == pages.end() ? nullptr : &*match;
    };
    auto append_page = [&](render_text_atlas_page page, const bool allocated_by_plan) -> working_page& {
        if (page.id == 0U) {
            page.id = static_cast<render_text_atlas_page_id>(pages.size() + 1U);
        }
        if (page.width == 0U) {
            page.width = request.constraints.width;
        }
        if (page.height == 0U) {
            page.height = request.constraints.height;
        }
        pages.push_back(working_page{
            .snapshot = render_text_glyph_atlas_page_plan_page_snapshot{
                .page = page,
                .allocated_by_plan = allocated_by_plan,
                .page_capacity = page.width * page.height,
            },
        });
        return pages.back();
    };
    auto eviction_candidate_from_pages = [&]() {
        for (const working_page& page : pages) {
            if (!page.keys.empty()) {
                return page.keys.front();
            }
        }
        return glyph_atlas_key{};
    };
    auto has_eviction_candidate = [&]() {
        return std::any_of(
            pages.begin(),
            pages.end(),
            [](const working_page& page) { return !page.keys.empty(); });
    };
    auto mark_overflow_page = [&]() {
        if (!pages.empty()) {
            pages.front().snapshot.overflow = true;
        }
    };
    auto allocate_on_page = [&](working_page& page, const std::size_t width, const std::size_t height) {
        const std::size_t padded_width = width + (request.constraints.padding * 2U);
        const std::size_t padded_height = height + (request.constraints.padding * 2U);
        if (page.cursor_x + padded_width > page.snapshot.page.width) {
            page.cursor_x = 0;
            page.cursor_y += page.row_height;
            page.row_height = 0;
        }
        if (page.cursor_y + padded_height > page.snapshot.page.height) {
            return render_rect{};
        }

        const render_rect bounds{
            static_cast<float>(page.cursor_x + request.constraints.padding),
            static_cast<float>(page.cursor_y + request.constraints.padding),
            static_cast<float>(width),
            static_cast<float>(height),
        };
        page.cursor_x += padded_width;
        page.row_height = std::max(page.row_height, padded_height);
        return bounds;
    };

    for (const render_text_atlas_update& update : request.pending_updates) {
        plan.policy.pending_update_rgba_bytes += render_text_glyph_atlas_page_plan_pending_update_bytes(update);
        if (update.page.id == 0U) {
            continue;
        }

        working_page* page = find_page(update.page.id);
        if (page == nullptr) {
            page = &append_page(update.page, false);
        }
        page->snapshot.referenced_by_pending_update = true;
        ++page->snapshot.pending_update_count;
        page->snapshot.pending_update_rgba_bytes +=
            render_text_glyph_atlas_page_plan_pending_update_bytes(update);
    }

    for (std::size_t index = 0; index < request.materializations.size(); ++index) {
        const render_text_glyph_atlas_materialization_snapshot& materialization =
            request.materializations[index];
        render_text_glyph_atlas_page_plan_entry_snapshot entry{
            .materialization_index = index,
            .materialization_id = render_text_glyph_atlas_materialization_stable_id_for(materialization),
            .run_index = materialization.run_index,
            .cluster_index = materialization.cluster_index,
            .cluster_byte_offset = materialization.cluster_byte_offset,
            .cluster_byte_count = materialization.cluster_byte_count,
            .cache_key = materialization.cache_key,
            .has_cache_key = materialization.has_cache_key,
            .materialization_status = materialization.status,
            .glyph_width = render_text_glyph_atlas_page_plan_width_for(materialization),
            .glyph_height = render_text_glyph_atlas_page_plan_height_for(materialization),
            .upload_rgba_bytes = materialization.queued ? materialization.atlas_update_rgba_bytes : 0U,
            .upload_ready = materialization.queued,
            .clean_reuse = materialization.clean_reuse,
        };
        entry.padded_width = entry.glyph_width + (request.constraints.padding * 2U);
        entry.padded_height = entry.glyph_height + (request.constraints.padding * 2U);
        entry.placed_area = entry.glyph_width * entry.glyph_height;
        plan.policy.materialization_upload_rgba_bytes += entry.upload_rgba_bytes;

        if (render_text_glyph_atlas_materialization_is_skipped(materialization)
            || !materialization.materialized
            || !materialization.has_cache_key) {
            entry.status = render_text_glyph_atlas_page_plan_status::skipped_materialization;
            entry.skipped = true;
            entry.diagnostic = "atlas page planning skipped non-materialized glyph work";
            ++plan.policy.skipped_count;
            plan.entries.push_back(std::move(entry));
            continue;
        }

        const bool too_large_for_page =
            !request.constraints.has_page_extent()
            || entry.padded_width > request.constraints.width
            || entry.padded_height > request.constraints.height;
        if (too_large_for_page) {
            entry.status = render_text_glyph_atlas_page_plan_status::overflow;
            entry.skipped = false;
            entry.overflow = true;
            entry.has_eviction_candidate = has_eviction_candidate();
            entry.eviction_candidate_key = eviction_candidate_from_pages();
            entry.diagnostic = "atlas page planning overflowed because glyph dimensions exceed page constraints";
            mark_overflow_page();
            ++plan.policy.overflow_count;
            plan.policy.has_overflow = true;
            if (entry.has_eviction_candidate) {
                ++plan.policy.eviction_candidate_count;
                plan.policy.has_eviction_candidates = true;
            }
            plan.entries.push_back(std::move(entry));
            continue;
        }

        working_page* selected_page = nullptr;
        bool allocated_new_page = false;
        if (materialization.page.id != 0U) {
            selected_page = find_page(materialization.page.id);
            if (selected_page == nullptr) {
                if (request.constraints.max_pages == 0U || pages.size() < request.constraints.max_pages) {
                    selected_page = &append_page(materialization.page, false);
                    allocated_new_page = true;
                }
            }
        } else {
            for (working_page& page : pages) {
                const render_rect bounds = allocate_on_page(page, entry.glyph_width, entry.glyph_height);
                if (bounds.width > 0.0f && bounds.height > 0.0f) {
                    selected_page = &page;
                    entry.atlas_bounds = bounds;
                    entry.has_atlas_bounds = true;
                    break;
                }
            }
            if (selected_page == nullptr
                && (request.constraints.max_pages == 0U || pages.size() < request.constraints.max_pages)) {
                selected_page = &append_page(render_text_atlas_page{}, true);
                allocated_new_page = true;
                entry.atlas_bounds = allocate_on_page(
                    *selected_page,
                    entry.glyph_width,
                    entry.glyph_height);
                entry.has_atlas_bounds = entry.atlas_bounds.width > 0.0f && entry.atlas_bounds.height > 0.0f;
            }
        }

        if (selected_page == nullptr) {
            entry.status = render_text_glyph_atlas_page_plan_status::overflow;
            entry.skipped = false;
            entry.overflow = true;
            entry.has_eviction_candidate = has_eviction_candidate();
            entry.eviction_candidate_key = eviction_candidate_from_pages();
            entry.diagnostic = "atlas page planning overflowed because no page capacity was available";
            mark_overflow_page();
            ++plan.policy.overflow_count;
            plan.policy.has_overflow = true;
            if (entry.has_eviction_candidate) {
                ++plan.policy.eviction_candidate_count;
                plan.policy.has_eviction_candidates = true;
            }
            plan.entries.push_back(std::move(entry));
            continue;
        }

        if (!entry.has_atlas_bounds && materialization.has_atlas_placement) {
            entry.atlas_bounds = materialization.atlas_bounds;
            entry.has_atlas_bounds = true;
        }
        entry.page = selected_page->snapshot.page;
        entry.page_index = static_cast<std::size_t>(selected_page - pages.data());
        entry.page_capacity = page_capacity_for(selected_page->snapshot.page);
        entry.page_used_area_before = selected_page->snapshot.used_area;
        entry.estimated_occupancy_before = render_text_glyph_atlas_page_plan_ratio(
            entry.page_used_area_before,
            entry.page_capacity);
        entry.estimated_fragmentation_before =
            entry.page_capacity == 0U ? 0.0f : 1.0f - entry.estimated_occupancy_before;
        const bool key_already_known =
            render_text_glyph_atlas_page_plan_key_exists(selected_page->keys, materialization.cache_key);
        entry.reused_existing_placement = materialization.clean_reuse || key_already_known;
        entry.allocated_new_page = allocated_new_page;
        entry.selected_existing_page = !entry.allocated_new_page && !entry.reused_existing_placement;
        entry.skipped = false;
        if (entry.reused_existing_placement) {
            entry.status = render_text_glyph_atlas_page_plan_status::reused_existing_placement;
            ++plan.policy.reused_placement_count;
            ++selected_page->snapshot.reused_placement_count;
            entry.diagnostic = "atlas page planning reused an existing glyph placement";
        } else if (entry.allocated_new_page) {
            entry.status = render_text_glyph_atlas_page_plan_status::allocated_new_page;
            ++plan.policy.allocated_new_page_count;
            selected_page->snapshot.allocated_by_plan = true;
            entry.diagnostic = "atlas page planning selected a newly allocated page";
        } else {
            entry.status = render_text_glyph_atlas_page_plan_status::selected_existing_page;
            ++plan.policy.selected_existing_page_count;
            entry.diagnostic = "atlas page planning selected an existing page";
        }

        if (!key_already_known) {
            selected_page->snapshot.used_area += entry.placed_area;
            selected_page->keys.push_back(materialization.cache_key);
        }
        entry.page_used_area_after = selected_page->snapshot.used_area;
        entry.estimated_occupancy_after = render_text_glyph_atlas_page_plan_ratio(
            entry.page_used_area_after,
            entry.page_capacity);
        entry.estimated_fragmentation_after =
            entry.page_capacity == 0U ? 0.0f : 1.0f - entry.estimated_occupancy_after;
        ++selected_page->snapshot.materialization_count;
        if (entry.upload_ready) {
            ++selected_page->snapshot.upload_ready_count;
            selected_page->snapshot.upload_rgba_bytes += entry.upload_rgba_bytes;
        }
        if (entry.clean_reuse) {
            ++selected_page->snapshot.clean_reuse_count;
        }
        selected_page->snapshot.has_eviction_candidate = !selected_page->keys.empty();
        selected_page->snapshot.eviction_candidate_key =
            selected_page->keys.empty() ? glyph_atlas_key{} : selected_page->keys.front();
        plan.entries.push_back(std::move(entry));
    }

    plan.pages.reserve(pages.size());
    for (working_page& page : pages) {
        finish_page_metrics(page.snapshot);
        plan.policy.total_page_capacity += page.snapshot.page_capacity;
        plan.policy.total_used_area += page.snapshot.used_area;
        plan.policy.total_available_area += page.snapshot.available_area;
        plan.policy.has_eviction_candidates =
            plan.policy.has_eviction_candidates || page.snapshot.has_eviction_candidate;
        plan.pages.push_back(std::move(page.snapshot));
    }
    plan.policy.page_count = plan.pages.size();
    plan.policy.planned_entry_count = plan.entries.size();
    plan.policy.total_upload_rgba_bytes =
        plan.policy.materialization_upload_rgba_bytes + plan.policy.pending_update_rgba_bytes;
    plan.policy.estimated_occupancy = render_text_glyph_atlas_page_plan_ratio(
        plan.policy.total_used_area,
        plan.policy.total_page_capacity);
    plan.policy.estimated_fragmentation =
        plan.policy.total_page_capacity == 0U ? 0.0f : 1.0f - plan.policy.estimated_occupancy;
    plan.policy.diagnostic = "glyph atlas page plan: "
        + std::to_string(plan.policy.page_count)
        + " pages, "
        + std::to_string(plan.policy.overflow_count)
        + " overflows, "
        + std::to_string(plan.policy.total_upload_rgba_bytes)
        + " upload bytes";
    return plan;
}

inline render_text_glyph_atlas_page_plan_snapshot plan_render_text_glyph_atlas_pages(
    std::vector<render_text_glyph_atlas_materialization_snapshot> materializations,
    const glyph_atlas_page_config config = {},
    const std::size_t max_pages = 0)
{
    return plan_render_text_glyph_atlas_pages(render_text_glyph_atlas_page_plan_request{
        .materializations = std::move(materializations),
        .constraints = render_text_glyph_atlas_page_plan_constraints_for(config, max_pages),
    });
}

struct render_text_batch_ref {
    render_node_id node_id;
    std::vector<render_text_run> text_runs;
    render_rect bounds;
    render_text_options options;
    std::string source_label;
};

struct render_text_request_batch_item {
    std::size_t item_index = 0;
    render_node_id node_id;
    std::string source_label;
    std::vector<render_text_run> text_runs;
    render_rect bounds;
    render_text_style_catalog style_catalog;
    render_text_options options;
    std::vector<render_text_glyph_atlas_materialization_snapshot> materializations;
};

struct render_text_batch_normalized_style_key {
    render_style_id requested_style_token;
    render_style_id resolved_style_id;
    std::string normalized_font_family;
    float font_size = 0.0f;
    float line_height = 0.0f;
    float letter_spacing = 0.0f;
    int font_weight = 400;
    bool italic = false;
    bool used_fallback_style = false;
    std::string key;
};

struct render_text_batch_materialization_work_key {
    glyph_atlas_key cache_key;
    render_text_font_backend_library shaping_font_backend_library =
        render_text_font_backend_library::deterministic_fake;
    std::string shaping_font_backend_label;
    render_text_font_backend_library raster_font_backend_library =
        render_text_font_backend_library::deterministic_fake;
    std::string raster_font_backend_label;
    std::size_t payload_alpha_bytes = 0;
    std::size_t payload_rgba_bytes = 0;

    friend bool operator==(
        const render_text_batch_materialization_work_key&,
        const render_text_batch_materialization_work_key&) = default;
};

struct render_text_batch_layout_request_snapshot {
    std::size_t item_index = 0;
    render_node_id node_id;
    std::string source_label;
    render_rect bounds;
    render_text_options options;
    std::size_t run_count = 0;
    std::size_t style_key_offset = 0;
    std::size_t style_key_count = 0;
    std::size_t fallback_style_count = 0;
    bool planned = true;
};

struct render_text_batch_atlas_update_request_snapshot {
    std::size_t item_index = 0;
    std::size_t materialization_index = 0;
    std::size_t unique_work_index = 0;
    std::size_t duplicate_of = 0;
    render_text_glyph_atlas_materialization_status materialization_status =
        render_text_glyph_atlas_materialization_status::skipped_missing_cache_key;
    glyph_atlas_key cache_key;
    std::uint32_t resolved_glyph_id = 0;
    font_face_id resolved_face_id = 0;
    render_text_font_backend_library shaping_font_backend_library =
        render_text_font_backend_library::deterministic_fake;
    std::string shaping_font_backend_label;
    render_text_font_backend_library raster_font_backend_library =
        render_text_font_backend_library::deterministic_fake;
    std::string raster_font_backend_label;
    std::size_t payload_alpha_bytes = 0;
    std::size_t payload_rgba_bytes = 0;
    bool payload_upload_ready = false;
    bool payload_byte_count_matches = true;
    bool has_atlas_update = false;
    render_text_atlas_page page;
    render_rect atlas_update_bounds;
    std::size_t atlas_update_rgba_bytes = 0;
    bool materialized = false;
    bool duplicate = false;
    bool skipped = true;
    bool used_deterministic_fallback = false;
    bool used_real_backend = false;
    std::string diagnostic;
};

struct render_text_request_batch_plan_policy_snapshot {
    std::size_t item_count = 0;
    std::size_t layout_request_count = 0;
    std::size_t text_run_count = 0;
    std::size_t style_key_count = 0;
    std::size_t unique_style_key_count = 0;
    std::size_t fallback_style_count = 0;
    std::size_t materialization_count = 0;
    std::size_t atlas_update_request_count = 0;
    std::size_t unique_atlas_materialization_count = 0;
    std::size_t duplicate_atlas_materialization_count = 0;
    std::size_t skipped_materialization_count = 0;
    std::size_t fallback_materialization_count = 0;
    std::size_t real_backend_materialization_count = 0;
    std::size_t total_payload_rgba_bytes = 0;
    std::size_t planned_atlas_update_rgba_bytes = 0;
};

struct render_text_request_batch_plan_snapshot {
    std::vector<render_text_batch_layout_request_snapshot> layout_requests;
    std::vector<render_text_batch_atlas_update_request_snapshot> atlas_update_requests;
    std::vector<render_text_batch_normalized_style_key> style_keys;
    std::vector<std::string> unique_style_keys;
    std::vector<render_text_batch_materialization_work_key> unique_materialization_work;
    render_text_request_batch_plan_policy_snapshot policy;

    bool has_layout_requests() const
    {
        return !layout_requests.empty();
    }

    bool has_atlas_update_requests() const
    {
        return !atlas_update_requests.empty();
    }
};

inline std::string render_text_batch_normalize_font_family(const std::string_view family)
{
    std::string normalized;
    normalized.reserve(family.size());
    bool pending_space = false;
    for (const unsigned char value : family) {
        if (std::isspace(value) != 0) {
            pending_space = !normalized.empty();
            continue;
        }
        if (pending_space) {
            normalized.push_back(' ');
            pending_space = false;
        }
        normalized.push_back(static_cast<char>(std::tolower(value)));
    }
    return normalized;
}

inline std::string render_text_batch_float_key(const float value)
{
    std::string text = std::to_string(value);
    while (!text.empty() && text.back() == '0') {
        text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
        text.pop_back();
    }
    return text.empty() ? "0" : text;
}

inline std::string render_text_batch_make_style_key(
    const std::string& normalized_font_family,
    const render_text_style& style)
{
    return normalized_font_family
        + "|w=" + std::to_string(style.font_weight)
        + "|i=" + (style.italic ? "1" : "0")
        + "|fs=" + render_text_batch_float_key(style.font_size)
        + "|lh=" + render_text_batch_float_key(style.line_height)
        + "|ls=" + render_text_batch_float_key(style.letter_spacing);
}

inline render_text_batch_normalized_style_key render_text_batch_normalized_style_key_for(
    const render_text_style_catalog& catalog,
    const render_style_id& requested_style_token)
{
    const render_text_style* resolved = catalog.find(requested_style_token);
    const render_text_style& style = resolved == nullptr ? catalog.fallback_style : *resolved;
    const std::string normalized_font_family =
        render_text_batch_normalize_font_family(style.font_family);
    return render_text_batch_normalized_style_key{
        .requested_style_token = requested_style_token,
        .resolved_style_id = style.id,
        .normalized_font_family = normalized_font_family,
        .font_size = style.font_size,
        .line_height = style.line_height,
        .letter_spacing = style.letter_spacing,
        .font_weight = style.font_weight,
        .italic = style.italic,
        .used_fallback_style = resolved == nullptr,
        .key = render_text_batch_make_style_key(normalized_font_family, style),
    };
}

inline render_text_request_batch_item make_render_text_request_batch_item(
    render_text_request request,
    std::vector<render_text_glyph_atlas_materialization_snapshot> materializations = {},
    std::string source_label = {},
    render_node_id node_id = {})
{
    return render_text_request_batch_item{
        .node_id = std::move(node_id),
        .source_label = std::move(source_label),
        .text_runs = std::move(request.text_runs),
        .bounds = request.bounds,
        .style_catalog = std::move(request.style_catalog),
        .options = request.options,
        .materializations = std::move(materializations),
    };
}

inline render_text_request_batch_item make_render_text_request_batch_item(
    render_text_batch_ref text_ref,
    render_text_style_catalog style_catalog,
    std::vector<render_text_glyph_atlas_materialization_snapshot> materializations = {})
{
    return render_text_request_batch_item{
        .node_id = std::move(text_ref.node_id),
        .source_label = std::move(text_ref.source_label),
        .text_runs = std::move(text_ref.text_runs),
        .bounds = text_ref.bounds,
        .style_catalog = std::move(style_catalog),
        .options = text_ref.options,
        .materializations = std::move(materializations),
    };
}

inline render_text_request_batch_item make_render_text_request_batch_item(
    const render_draw_command& command,
    render_text_style_catalog style_catalog,
    std::vector<render_text_glyph_atlas_materialization_snapshot> materializations = {})
{
    return render_text_request_batch_item{
        .node_id = command.node_id,
        .source_label = command.node_id,
        .text_runs = command.text_runs,
        .bounds = command.bounds,
        .style_catalog = std::move(style_catalog),
        .options = command.text_options,
        .materializations = std::move(materializations),
    };
}

inline render_text_batch_materialization_work_key render_text_batch_materialization_work_key_for(
    const render_text_glyph_atlas_materialization_snapshot& materialization)
{
    return render_text_batch_materialization_work_key{
        .cache_key = materialization.cache_key,
        .shaping_font_backend_library = materialization.shaping_font_backend_library,
        .shaping_font_backend_label = materialization.shaping_font_backend_label,
        .raster_font_backend_library = materialization.raster_font_backend_library,
        .raster_font_backend_label = materialization.raster_font_backend_label,
        .payload_alpha_bytes = materialization.payload_alpha_bytes,
        .payload_rgba_bytes = materialization.payload_rgba_bytes,
    };
}

inline std::size_t render_text_batch_find_or_append_unique_style_key(
    std::vector<std::string>& unique_style_keys,
    const std::string& key)
{
    const auto match = std::find(unique_style_keys.begin(), unique_style_keys.end(), key);
    if (match != unique_style_keys.end()) {
        return static_cast<std::size_t>(match - unique_style_keys.begin());
    }
    unique_style_keys.push_back(key);
    return unique_style_keys.size() - 1U;
}

inline std::size_t render_text_batch_find_or_append_unique_materialization_work(
    std::vector<render_text_batch_materialization_work_key>& unique_work,
    const render_text_batch_materialization_work_key& key,
    bool& duplicate)
{
    const auto match = std::find(unique_work.begin(), unique_work.end(), key);
    if (match != unique_work.end()) {
        duplicate = true;
        return static_cast<std::size_t>(match - unique_work.begin());
    }
    duplicate = false;
    unique_work.push_back(key);
    return unique_work.size() - 1U;
}

inline bool render_text_batch_materialization_uses_deterministic_fallback(
    const render_text_glyph_atlas_materialization_snapshot& materialization)
{
    return render_text_glyph_atlas_materialization_uses_deterministic_fallback(materialization);
}

inline bool render_text_batch_materialization_uses_real_backend(
    const render_text_glyph_atlas_materialization_snapshot& materialization)
{
    return render_text_glyph_atlas_materialization_uses_real_backend(materialization);
}

inline render_text_request_batch_plan_snapshot plan_render_text_request_batch(
    std::vector<render_text_request_batch_item> items)
{
    render_text_request_batch_plan_snapshot plan;
    plan.policy.item_count = items.size();
    plan.layout_requests.reserve(items.size());

    for (std::size_t item_index = 0; item_index < items.size(); ++item_index) {
        render_text_request_batch_item& item = items[item_index];
        item.item_index = item_index;

        const std::size_t style_key_offset = plan.style_keys.size();
        std::size_t fallback_style_count = 0;
        for (const render_text_run& run : item.text_runs) {
            render_text_batch_normalized_style_key style_key =
                render_text_batch_normalized_style_key_for(item.style_catalog, run.style_token);
            if (style_key.used_fallback_style) {
                ++fallback_style_count;
            }
            render_text_batch_find_or_append_unique_style_key(plan.unique_style_keys, style_key.key);
            plan.style_keys.push_back(std::move(style_key));
        }

        plan.layout_requests.push_back(render_text_batch_layout_request_snapshot{
            .item_index = item_index,
            .node_id = item.node_id,
            .source_label = item.source_label,
            .bounds = item.bounds,
            .options = item.options,
            .run_count = item.text_runs.size(),
            .style_key_offset = style_key_offset,
            .style_key_count = item.text_runs.size(),
            .fallback_style_count = fallback_style_count,
            .planned = true,
        });

        ++plan.policy.layout_request_count;
        plan.policy.text_run_count += item.text_runs.size();
        plan.policy.style_key_count += item.text_runs.size();
        plan.policy.fallback_style_count += fallback_style_count;

        for (std::size_t materialization_index = 0;
             materialization_index < item.materializations.size();
             ++materialization_index) {
            const render_text_glyph_atlas_materialization_snapshot& materialization =
                item.materializations[materialization_index];
            ++plan.policy.materialization_count;
            plan.policy.total_payload_rgba_bytes += materialization.payload_rgba_bytes;
            if (!materialization.materialized) {
                ++plan.policy.skipped_materialization_count;
            }
            if (render_text_batch_materialization_uses_deterministic_fallback(materialization)) {
                ++plan.policy.fallback_materialization_count;
            }
            if (render_text_batch_materialization_uses_real_backend(materialization)) {
                ++plan.policy.real_backend_materialization_count;
            }

            bool duplicate = false;
            std::size_t unique_work_index = 0;
            std::size_t duplicate_of = 0;
            if (materialization.materialized && materialization.has_cache_key) {
                const render_text_batch_materialization_work_key work_key =
                    render_text_batch_materialization_work_key_for(materialization);
                unique_work_index = render_text_batch_find_or_append_unique_materialization_work(
                    plan.unique_materialization_work,
                    work_key,
                    duplicate);
                duplicate_of = unique_work_index;
                if (duplicate) {
                    ++plan.policy.duplicate_atlas_materialization_count;
                } else {
                    ++plan.policy.unique_atlas_materialization_count;
                    plan.policy.planned_atlas_update_rgba_bytes += materialization.atlas_update_rgba_bytes;
                }
            } else {
                duplicate = false;
                duplicate_of = plan.unique_materialization_work.size();
            }

            plan.atlas_update_requests.push_back(render_text_batch_atlas_update_request_snapshot{
                .item_index = item_index,
                .materialization_index = materialization_index,
                .unique_work_index = unique_work_index,
                .duplicate_of = duplicate_of,
                .materialization_status = materialization.status,
                .cache_key = materialization.cache_key,
                .resolved_glyph_id = materialization.resolved_glyph_id,
                .resolved_face_id = materialization.resolved_face_id,
                .shaping_font_backend_library = materialization.shaping_font_backend_library,
                .shaping_font_backend_label = materialization.shaping_font_backend_label,
                .raster_font_backend_library = materialization.raster_font_backend_library,
                .raster_font_backend_label = materialization.raster_font_backend_label,
                .payload_alpha_bytes = materialization.payload_alpha_bytes,
                .payload_rgba_bytes = materialization.payload_rgba_bytes,
                .payload_upload_ready = materialization.payload_upload_ready,
                .payload_byte_count_matches = materialization.payload_byte_count_matches,
                .has_atlas_update = materialization.has_atlas_update,
                .page = materialization.page,
                .atlas_update_bounds = materialization.atlas_update_bounds,
                .atlas_update_rgba_bytes = materialization.atlas_update_rgba_bytes,
                .materialized = materialization.materialized,
                .duplicate = duplicate,
                .skipped = !materialization.materialized,
                .used_deterministic_fallback =
                    render_text_batch_materialization_uses_deterministic_fallback(materialization),
                .used_real_backend = render_text_batch_materialization_uses_real_backend(materialization),
                .diagnostic = materialization.diagnostic,
            });
            ++plan.policy.atlas_update_request_count;
        }
    }

    plan.policy.unique_style_key_count = plan.unique_style_keys.size();
    return plan;
}

enum class render_text_atlas_upload_request_status {
    upload_ready,
    duplicate_suppressed,
    clean_reuse,
    skipped_materialization,
    payload_byte_count_mismatch,
};

inline std::string render_text_atlas_upload_request_status_name(
    const render_text_atlas_upload_request_status status)
{
    switch (status) {
    case render_text_atlas_upload_request_status::upload_ready:
        return "upload_ready";
    case render_text_atlas_upload_request_status::duplicate_suppressed:
        return "duplicate_suppressed";
    case render_text_atlas_upload_request_status::clean_reuse:
        return "clean_reuse";
    case render_text_atlas_upload_request_status::skipped_materialization:
        return "skipped_materialization";
    case render_text_atlas_upload_request_status::payload_byte_count_mismatch:
        return "payload_byte_count_mismatch";
    }

    return "unknown";
}

struct render_text_atlas_upload_request_snapshot {
    std::string request_id;
    render_text_atlas_upload_request_status status =
        render_text_atlas_upload_request_status::skipped_materialization;
    std::size_t batch_atlas_request_index = 0;
    std::size_t item_index = 0;
    std::size_t materialization_index = 0;
    std::size_t unique_work_index = 0;
    std::size_t duplicate_of = 0;
    glyph_atlas_key cache_key;
    std::uint32_t resolved_glyph_id = 0;
    font_face_id resolved_face_id = 0;
    render_text_glyph_atlas_materialization_status materialization_status =
        render_text_glyph_atlas_materialization_status::skipped_missing_cache_key;
    render_text_font_backend_library shaping_font_backend_library =
        render_text_font_backend_library::deterministic_fake;
    std::string shaping_font_backend_label;
    render_text_font_backend_library raster_font_backend_library =
        render_text_font_backend_library::deterministic_fake;
    std::string raster_font_backend_label;
    render_text_atlas_update upload_request;
    bool has_upload_request = false;
    bool duplicate = false;
    bool skipped = true;
    bool clean_reuse = false;
    bool payload_byte_count_matches = true;
    std::size_t payload_rgba_bytes = 0;
    std::size_t expected_upload_rgba_bytes = 0;
    std::size_t actual_upload_rgba_bytes = 0;
    bool stable_request_id = true;
    std::string diagnostic;

    bool ok() const
    {
        return status == render_text_atlas_upload_request_status::upload_ready
            || status == render_text_atlas_upload_request_status::duplicate_suppressed
            || status == render_text_atlas_upload_request_status::clean_reuse;
    }
};

struct render_text_atlas_upload_request_policy_snapshot {
    std::size_t batch_atlas_request_count = 0;
    std::size_t request_count = 0;
    std::size_t upload_request_count = 0;
    std::size_t unique_upload_request_count = 0;
    std::size_t duplicate_suppressed_count = 0;
    std::size_t clean_reuse_count = 0;
    std::size_t skipped_materialization_count = 0;
    std::size_t payload_byte_count_mismatch_count = 0;
    std::size_t stable_request_id_count = 0;
    std::size_t total_payload_rgba_bytes = 0;
    std::size_t total_upload_rgba_bytes = 0;
};

struct render_text_atlas_upload_request_bridge_snapshot {
    std::vector<render_text_atlas_upload_request_snapshot> requests;
    std::vector<render_text_atlas_update> upload_requests;
    std::vector<std::string> stable_request_ids;
    render_text_atlas_upload_request_policy_snapshot policy;
    std::string diagnostic;

    bool has_upload_requests() const
    {
        return !upload_requests.empty();
    }

    bool ok() const
    {
        return policy.skipped_materialization_count == 0U
            && policy.payload_byte_count_mismatch_count == 0U;
    }
};

inline std::string render_text_atlas_upload_request_rect_key(const render_rect& rect)
{
    return render_text_batch_float_key(rect.x)
        + "," + render_text_batch_float_key(rect.y)
        + "," + render_text_batch_float_key(rect.width)
        + "," + render_text_batch_float_key(rect.height);
}

inline std::string render_text_atlas_upload_request_stable_id_for(
    const render_text_batch_atlas_update_request_snapshot& request)
{
    return "text-atlas-upload:v1"
        + std::string{":face="} + std::to_string(request.cache_key.face_id)
        + ":glyph=" + std::to_string(request.cache_key.glyph_id)
        + ":px=" + std::to_string(request.cache_key.pixel_size)
        + ":page=" + std::to_string(request.page.id)
        + ":rev=" + std::to_string(request.page.revision)
        + ":bounds=" + render_text_atlas_upload_request_rect_key(request.atlas_update_bounds)
        + ":bytes=" + std::to_string(request.atlas_update_rgba_bytes);
}

inline bool render_text_atlas_upload_request_contains_id(
    const std::vector<std::string>& ids,
    const std::string& id)
{
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

inline void render_text_atlas_upload_request_append_unique_id(
    std::vector<std::string>& ids,
    const std::string& id)
{
    if (!render_text_atlas_upload_request_contains_id(ids, id)) {
        ids.push_back(id);
    }
}

inline std::vector<unsigned char> make_render_text_atlas_upload_request_rgba(
    const render_text_batch_atlas_update_request_snapshot& request)
{
    std::vector<unsigned char> rgba(request.atlas_update_rgba_bytes);
    const std::uint32_t seed =
        (request.cache_key.face_id * 31U)
        ^ (request.cache_key.glyph_id * 17U)
        ^ (request.cache_key.pixel_size * 13U)
        ^ (request.page.id * 7U)
        ^ static_cast<std::uint32_t>(request.page.revision & 0xffU);
    for (std::size_t index = 0; index < rgba.size(); ++index) {
        rgba[index] = static_cast<unsigned char>((seed + static_cast<std::uint32_t>(index)) & 0xffU);
    }
    return rgba;
}

inline render_text_atlas_upload_request_status render_text_atlas_upload_request_status_for(
    const render_text_batch_atlas_update_request_snapshot& request)
{
    if (request.duplicate) {
        return render_text_atlas_upload_request_status::duplicate_suppressed;
    }
    if (!request.payload_byte_count_matches
        || request.materialization_status
            == render_text_glyph_atlas_materialization_status::payload_byte_count_mismatch) {
        return render_text_atlas_upload_request_status::payload_byte_count_mismatch;
    }
    if (request.materialization_status
            == render_text_glyph_atlas_materialization_status::materialized_clean_reuse
        || (request.materialized && !request.has_atlas_update)) {
        return render_text_atlas_upload_request_status::clean_reuse;
    }
    if (!request.materialized
        || request.skipped
        || !request.payload_upload_ready
        || !request.has_atlas_update) {
        return render_text_atlas_upload_request_status::skipped_materialization;
    }
    return render_text_atlas_upload_request_status::upload_ready;
}

inline render_text_atlas_upload_request_snapshot make_render_text_atlas_upload_request(
    const render_text_batch_atlas_update_request_snapshot& request,
    const std::size_t batch_atlas_request_index)
{
    const render_text_atlas_upload_request_status status =
        render_text_atlas_upload_request_status_for(request);
    const bool has_upload = status == render_text_atlas_upload_request_status::upload_ready;
    render_text_atlas_update upload_request;
    if (has_upload) {
        upload_request = render_text_atlas_update{
            .page = request.page,
            .updated_bounds = request.atlas_update_bounds,
            .rgba = make_render_text_atlas_upload_request_rgba(request),
        };
    }

    std::string diagnostic = "atlas upload request skipped before renderer handoff";
    switch (status) {
    case render_text_atlas_upload_request_status::upload_ready:
        diagnostic = "atlas upload request is ready for text-engine handoff";
        break;
    case render_text_atlas_upload_request_status::duplicate_suppressed:
        diagnostic = "atlas upload request is a duplicate of stable materialization work";
        break;
    case render_text_atlas_upload_request_status::clean_reuse:
        diagnostic = "atlas upload request reused a clean atlas page";
        break;
    case render_text_atlas_upload_request_status::skipped_materialization:
        diagnostic = "atlas upload request skipped a non-materialized glyph";
        break;
    case render_text_atlas_upload_request_status::payload_byte_count_mismatch:
        diagnostic = "atlas upload request rejected a payload byte-count mismatch";
        break;
    }

    return render_text_atlas_upload_request_snapshot{
        .request_id = render_text_atlas_upload_request_stable_id_for(request),
        .status = status,
        .batch_atlas_request_index = batch_atlas_request_index,
        .item_index = request.item_index,
        .materialization_index = request.materialization_index,
        .unique_work_index = request.unique_work_index,
        .duplicate_of = request.duplicate_of,
        .cache_key = request.cache_key,
        .resolved_glyph_id = request.resolved_glyph_id,
        .resolved_face_id = request.resolved_face_id,
        .materialization_status = request.materialization_status,
        .shaping_font_backend_library = request.shaping_font_backend_library,
        .shaping_font_backend_label = request.shaping_font_backend_label,
        .raster_font_backend_library = request.raster_font_backend_library,
        .raster_font_backend_label = request.raster_font_backend_label,
        .upload_request = std::move(upload_request),
        .has_upload_request = has_upload,
        .duplicate = request.duplicate,
        .skipped = status == render_text_atlas_upload_request_status::skipped_materialization
            || status == render_text_atlas_upload_request_status::payload_byte_count_mismatch,
        .clean_reuse = status == render_text_atlas_upload_request_status::clean_reuse,
        .payload_byte_count_matches = request.payload_byte_count_matches,
        .payload_rgba_bytes = request.payload_rgba_bytes,
        .expected_upload_rgba_bytes = request.atlas_update_rgba_bytes,
        .actual_upload_rgba_bytes = has_upload ? request.atlas_update_rgba_bytes : 0U,
        .stable_request_id = true,
        .diagnostic = std::move(diagnostic),
    };
}

inline void append_render_text_atlas_upload_request(
    std::vector<render_text_atlas_upload_request_snapshot>& requests,
    std::vector<render_text_atlas_update>& upload_requests,
    std::vector<std::string>& stable_request_ids,
    render_text_atlas_upload_request_policy_snapshot& policy,
    render_text_atlas_upload_request_snapshot request)
{
    ++policy.request_count;
    policy.total_payload_rgba_bytes += request.payload_rgba_bytes;
    if (request.stable_request_id) {
        ++policy.stable_request_id_count;
    }
    render_text_atlas_upload_request_append_unique_id(stable_request_ids, request.request_id);

    switch (request.status) {
    case render_text_atlas_upload_request_status::upload_ready:
        ++policy.upload_request_count;
        ++policy.unique_upload_request_count;
        policy.total_upload_rgba_bytes += request.actual_upload_rgba_bytes;
        upload_requests.push_back(request.upload_request);
        break;
    case render_text_atlas_upload_request_status::duplicate_suppressed:
        ++policy.duplicate_suppressed_count;
        break;
    case render_text_atlas_upload_request_status::clean_reuse:
        ++policy.clean_reuse_count;
        break;
    case render_text_atlas_upload_request_status::skipped_materialization:
        ++policy.skipped_materialization_count;
        break;
    case render_text_atlas_upload_request_status::payload_byte_count_mismatch:
        ++policy.payload_byte_count_mismatch_count;
        break;
    }

    requests.push_back(std::move(request));
}

inline render_text_atlas_upload_request_bridge_snapshot bridge_render_text_atlas_upload_requests(
    const render_text_request_batch_plan_snapshot& plan)
{
    render_text_atlas_upload_request_bridge_snapshot bridge;
    bridge.policy.batch_atlas_request_count = plan.atlas_update_requests.size();
    bridge.requests.reserve(plan.atlas_update_requests.size());
    for (std::size_t index = 0; index < plan.atlas_update_requests.size(); ++index) {
        append_render_text_atlas_upload_request(
            bridge.requests,
            bridge.upload_requests,
            bridge.stable_request_ids,
            bridge.policy,
            make_render_text_atlas_upload_request(plan.atlas_update_requests[index], index));
    }

    bridge.diagnostic = bridge.ok()
        ? "atlas upload bridge produced renderer-agnostic text upload requests"
        : "atlas upload bridge skipped one or more materialized text glyph uploads";
    return bridge;
}

} // namespace quiz_vulkan::render

#include "render/text/text_frame_snapshot.h"

namespace quiz_vulkan::render {

enum class render_text_shaped_atlas_update_trace_status {
    upload_ready_payload_queued,
    clean_atlas_page_reused,
    rasterized_payload_skipped,
    shaped_glyph_without_cache_key,
    payload_byte_count_mismatch,
};

inline std::string render_text_shaped_atlas_update_trace_status_name(
    const render_text_shaped_atlas_update_trace_status status)
{
    switch (status) {
    case render_text_shaped_atlas_update_trace_status::upload_ready_payload_queued:
        return "upload_ready_payload_queued";
    case render_text_shaped_atlas_update_trace_status::clean_atlas_page_reused:
        return "clean_atlas_page_reused";
    case render_text_shaped_atlas_update_trace_status::rasterized_payload_skipped:
        return "rasterized_payload_skipped";
    case render_text_shaped_atlas_update_trace_status::shaped_glyph_without_cache_key:
        return "shaped_glyph_without_cache_key";
    case render_text_shaped_atlas_update_trace_status::payload_byte_count_mismatch:
        return "payload_byte_count_mismatch";
    }

    return "unknown";
}

struct render_text_shaped_atlas_update_trace_request {
    std::size_t cluster_index = 0;
    std::size_t run_index = 0;
    std::size_t cluster_byte_offset = 0;
    std::size_t cluster_byte_count = 0;
    std::uint32_t codepoint = 0;
    std::vector<std::uint32_t> shaped_glyph_ids;
    std::uint32_t resolved_glyph_id = 0;
    bool shaped_glyphs_match_cache_key = false;
    font_face_id resolved_face_id = 0;
    glyph_atlas_key cache_key;
    bool cache_key_matches_resolved_glyph_id = false;
    bool has_cache_key = false;
    render_text_font_backend_library shaping_font_backend_library =
        render_text_font_backend_library::deterministic_fake;
    std::string shaping_font_backend_label;
    render_text_font_backend_capability_status shaping_font_backend_capability_status =
        render_text_font_backend_capability_status::unavailable;
    bool shaping_font_backend_used_deterministic_fallback = false;
    bool shaping_font_backend_fallback_only = false;
    render_text_font_backend_library raster_font_backend_library =
        render_text_font_backend_library::deterministic_fake;
    std::string raster_font_backend_label;
    render_text_font_backend_capability_status raster_font_backend_capability_status =
        render_text_font_backend_capability_status::unavailable;
    bool raster_font_backend_used_deterministic_fallback = false;
    bool raster_font_backend_fallback_only = false;
    render_text_font_rasterizer_status rasterizer_status =
        render_text_font_rasterizer_status::missing_font_source;
    bool raster_payload_matches_cache_key = false;
    bool glyph_id_from_selection = false;
    bool glyph_id_matches_codepoint = false;
    bool used_fallback_glyph_id = false;
    std::uint32_t glyph_id_offset = 0;
    bool rasterized_payload_skipped = true;
    bool payload_upload_ready = false;
    std::size_t payload_alpha_bytes = 0;
    std::size_t payload_rgba_bytes = 0;
    bool has_atlas_placement = false;
    render_text_atlas_page page;
    render_rect atlas_bounds;
    bool has_atlas_update = false;
    render_rect atlas_update_bounds;
    std::size_t atlas_update_rgba_bytes = 0;
};

struct render_text_shaped_atlas_update_trace_snapshot {
    render_text_shaped_atlas_update_trace_status status =
        render_text_shaped_atlas_update_trace_status::shaped_glyph_without_cache_key;
    std::size_t cluster_index = 0;
    std::size_t run_index = 0;
    std::size_t cluster_byte_offset = 0;
    std::size_t cluster_byte_count = 0;
    std::uint32_t codepoint = 0;
    std::vector<std::uint32_t> shaped_glyph_ids;
    std::uint32_t resolved_glyph_id = 0;
    bool shaped_glyphs_match_cache_key = false;
    font_face_id resolved_face_id = 0;
    glyph_atlas_key cache_key;
    bool cache_key_matches_resolved_glyph_id = false;
    bool has_cache_key = false;
    render_text_font_backend_library shaping_font_backend_library =
        render_text_font_backend_library::deterministic_fake;
    std::string shaping_font_backend_label;
    render_text_font_backend_capability_status shaping_font_backend_capability_status =
        render_text_font_backend_capability_status::unavailable;
    bool shaping_font_backend_used_deterministic_fallback = false;
    bool shaping_font_backend_fallback_only = false;
    render_text_font_backend_library raster_font_backend_library =
        render_text_font_backend_library::deterministic_fake;
    std::string raster_font_backend_label;
    render_text_font_backend_capability_status raster_font_backend_capability_status =
        render_text_font_backend_capability_status::unavailable;
    bool raster_font_backend_used_deterministic_fallback = false;
    bool raster_font_backend_fallback_only = false;
    render_text_font_rasterizer_status rasterizer_status =
        render_text_font_rasterizer_status::missing_font_source;
    bool raster_payload_matches_cache_key = false;
    bool glyph_id_from_selection = false;
    bool glyph_id_matches_codepoint = false;
    bool used_fallback_glyph_id = false;
    std::uint32_t glyph_id_offset = 0;
    std::size_t payload_alpha_bytes = 0;
    std::size_t payload_rgba_bytes = 0;
    std::size_t expected_payload_rgba_bytes = 0;
    bool payload_upload_ready = false;
    bool payload_byte_count_matches = true;
    bool has_atlas_placement = false;
    render_text_atlas_page page;
    render_rect atlas_bounds;
    bool has_atlas_update = false;
    render_rect atlas_update_bounds;
    std::size_t atlas_update_rgba_bytes = 0;
    bool queued = false;
    bool clean_page_reused = false;
    std::string diagnostic;
};

struct render_text_shaped_atlas_update_trace_policy_snapshot {
    std::size_t trace_count = 0;
    std::size_t upload_ready_payload_queued_count = 0;
    std::size_t clean_atlas_page_reused_count = 0;
    std::size_t rasterized_payload_skipped_count = 0;
    std::size_t shaped_glyph_without_cache_key_count = 0;
    std::size_t payload_byte_count_mismatch_count = 0;
    std::size_t traced_shaped_glyph_count = 0;
    std::size_t upload_ready_payload_bytes = 0;
    std::size_t queued_atlas_update_bytes = 0;
};

inline std::size_t render_text_shaped_atlas_expected_payload_rgba_bytes(
    const std::size_t payload_alpha_bytes)
{
    return payload_alpha_bytes * 4U;
}

inline bool render_text_shaped_atlas_payload_byte_count_matches(
    const std::size_t payload_alpha_bytes,
    const std::size_t payload_rgba_bytes)
{
    return payload_rgba_bytes == render_text_shaped_atlas_expected_payload_rgba_bytes(payload_alpha_bytes);
}

inline render_text_shaped_atlas_update_trace_snapshot make_render_text_shaped_atlas_update_trace(
    render_text_shaped_atlas_update_trace_request request)
{
    const std::size_t expected_rgba_bytes =
        render_text_shaped_atlas_expected_payload_rgba_bytes(request.payload_alpha_bytes);
    const bool byte_count_matches =
        render_text_shaped_atlas_payload_byte_count_matches(
            request.payload_alpha_bytes,
            request.payload_rgba_bytes);

    render_text_shaped_atlas_update_trace_status status =
        render_text_shaped_atlas_update_trace_status::shaped_glyph_without_cache_key;
    std::string diagnostic = "shaped glyph cluster has no cacheable atlas key";
    if (request.has_cache_key && !byte_count_matches) {
        status = render_text_shaped_atlas_update_trace_status::payload_byte_count_mismatch;
        diagnostic = "rasterized payload RGBA byte count does not match alpha payload expansion";
    } else if (request.has_cache_key && (request.rasterized_payload_skipped || !request.payload_upload_ready)) {
        status = render_text_shaped_atlas_update_trace_status::rasterized_payload_skipped;
        diagnostic = "rasterized glyph payload was skipped before atlas upload";
    } else if (request.has_cache_key && request.has_atlas_update) {
        status = render_text_shaped_atlas_update_trace_status::upload_ready_payload_queued;
        diagnostic = "upload-ready rasterized glyph payload was represented in a queued atlas update";
    } else if (request.has_cache_key) {
        status = render_text_shaped_atlas_update_trace_status::clean_atlas_page_reused;
        diagnostic = "upload-ready glyph reused an existing clean atlas page";
    }

    return render_text_shaped_atlas_update_trace_snapshot{
        .status = status,
        .cluster_index = request.cluster_index,
        .run_index = request.run_index,
        .cluster_byte_offset = request.cluster_byte_offset,
        .cluster_byte_count = request.cluster_byte_count,
        .codepoint = request.codepoint,
        .shaped_glyph_ids = std::move(request.shaped_glyph_ids),
        .resolved_glyph_id = request.resolved_glyph_id,
        .shaped_glyphs_match_cache_key = request.shaped_glyphs_match_cache_key,
        .resolved_face_id = request.resolved_face_id,
        .cache_key = request.cache_key,
        .cache_key_matches_resolved_glyph_id = request.cache_key_matches_resolved_glyph_id,
        .has_cache_key = request.has_cache_key,
        .shaping_font_backend_library = request.shaping_font_backend_library,
        .shaping_font_backend_label = request.shaping_font_backend_label,
        .shaping_font_backend_capability_status = request.shaping_font_backend_capability_status,
        .shaping_font_backend_used_deterministic_fallback =
            request.shaping_font_backend_used_deterministic_fallback,
        .shaping_font_backend_fallback_only = request.shaping_font_backend_fallback_only,
        .raster_font_backend_library = request.raster_font_backend_library,
        .raster_font_backend_label = request.raster_font_backend_label,
        .raster_font_backend_capability_status = request.raster_font_backend_capability_status,
        .raster_font_backend_used_deterministic_fallback =
            request.raster_font_backend_used_deterministic_fallback,
        .raster_font_backend_fallback_only = request.raster_font_backend_fallback_only,
        .rasterizer_status = request.rasterizer_status,
        .raster_payload_matches_cache_key = request.raster_payload_matches_cache_key,
        .glyph_id_from_selection = request.glyph_id_from_selection,
        .glyph_id_matches_codepoint = request.glyph_id_matches_codepoint,
        .used_fallback_glyph_id = request.used_fallback_glyph_id,
        .glyph_id_offset = request.glyph_id_offset,
        .payload_alpha_bytes = request.payload_alpha_bytes,
        .payload_rgba_bytes = request.payload_rgba_bytes,
        .expected_payload_rgba_bytes = expected_rgba_bytes,
        .payload_upload_ready = request.payload_upload_ready,
        .payload_byte_count_matches = byte_count_matches,
        .has_atlas_placement = request.has_atlas_placement,
        .page = request.page,
        .atlas_bounds = request.atlas_bounds,
        .has_atlas_update = request.has_atlas_update,
        .atlas_update_bounds = request.atlas_update_bounds,
        .atlas_update_rgba_bytes = request.atlas_update_rgba_bytes,
        .queued = status == render_text_shaped_atlas_update_trace_status::upload_ready_payload_queued,
        .clean_page_reused = status == render_text_shaped_atlas_update_trace_status::clean_atlas_page_reused,
        .diagnostic = std::move(diagnostic),
    };
}

inline void append_render_text_shaped_atlas_update_trace(
    std::vector<render_text_shaped_atlas_update_trace_snapshot>& traces,
    render_text_shaped_atlas_update_trace_policy_snapshot& policy,
    render_text_shaped_atlas_update_trace_snapshot trace)
{
    ++policy.trace_count;
    policy.traced_shaped_glyph_count += trace.shaped_glyph_ids.size();
    if (trace.payload_upload_ready) {
        policy.upload_ready_payload_bytes += trace.payload_rgba_bytes;
    }
    if (trace.queued) {
        policy.queued_atlas_update_bytes += trace.atlas_update_rgba_bytes;
    }

    switch (trace.status) {
    case render_text_shaped_atlas_update_trace_status::upload_ready_payload_queued:
        ++policy.upload_ready_payload_queued_count;
        break;
    case render_text_shaped_atlas_update_trace_status::clean_atlas_page_reused:
        ++policy.clean_atlas_page_reused_count;
        break;
    case render_text_shaped_atlas_update_trace_status::rasterized_payload_skipped:
        ++policy.rasterized_payload_skipped_count;
        break;
    case render_text_shaped_atlas_update_trace_status::shaped_glyph_without_cache_key:
        ++policy.shaped_glyph_without_cache_key_count;
        break;
    case render_text_shaped_atlas_update_trace_status::payload_byte_count_mismatch:
        ++policy.payload_byte_count_mismatch_count;
        break;
    }

    traces.push_back(std::move(trace));
}

} // namespace quiz_vulkan::render
