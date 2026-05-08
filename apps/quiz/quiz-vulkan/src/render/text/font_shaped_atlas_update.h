#pragma once

#include "render/text/font_coverage_run_segmentation.h"
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
    return materialization.shaping_font_backend_used_deterministic_fallback
        || materialization.raster_font_backend_used_deterministic_fallback
        || materialization.shaping_font_backend_fallback_only
        || materialization.raster_font_backend_fallback_only;
}

inline bool render_text_batch_materialization_uses_real_backend(
    const render_text_glyph_atlas_materialization_snapshot& materialization)
{
    return materialization.shaping_font_backend_library != render_text_font_backend_library::deterministic_fake
        || materialization.raster_font_backend_library != render_text_font_backend_library::deterministic_fake;
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

enum class render_text_frame_snapshot_status {
    ready,
    pending_atlas_updates,
    font_fallback_incomplete,
    atlas_upload_incomplete,
    consumed_update_mismatch,
};

inline std::string render_text_frame_snapshot_status_name(
    const render_text_frame_snapshot_status status)
{
    switch (status) {
    case render_text_frame_snapshot_status::ready:
        return "ready";
    case render_text_frame_snapshot_status::pending_atlas_updates:
        return "pending_atlas_updates";
    case render_text_frame_snapshot_status::font_fallback_incomplete:
        return "font_fallback_incomplete";
    case render_text_frame_snapshot_status::atlas_upload_incomplete:
        return "atlas_upload_incomplete";
    case render_text_frame_snapshot_status::consumed_update_mismatch:
        return "consumed_update_mismatch";
    }

    return "unknown";
}

struct render_text_frame_atlas_upload_snapshot {
    std::string request_id;
    render_text_atlas_upload_request_status status =
        render_text_atlas_upload_request_status::skipped_materialization;
    std::size_t batch_atlas_request_index = 0;
    std::size_t item_index = 0;
    std::size_t materialization_index = 0;
    glyph_atlas_key cache_key;
    std::uint32_t resolved_glyph_id = 0;
    font_face_id resolved_face_id = 0;
    render_text_atlas_page page;
    render_rect updated_bounds;
    std::size_t upload_rgba_bytes = 0;
    bool has_upload_request = false;
    bool queued = false;
    bool consumed = false;
    bool stable_request_id = false;
    std::string diagnostic;
};

struct render_text_frame_snapshot_policy {
    std::size_t layout_request_count = 0;
    std::size_t fallback_chain_run_count = 0;
    std::size_t fallback_chain_missing_glyph_count = 0;
    std::size_t fallback_chain_invalid_utf8_count = 0;
    std::size_t selected_face_count = 0;
    std::size_t materialization_count = 0;
    std::size_t upload_request_count = 0;
    std::size_t queued_upload_request_id_count = 0;
    std::size_t consumed_upload_request_id_count = 0;
    std::size_t consumed_atlas_update_count = 0;
    std::size_t total_upload_rgba_bytes = 0;
    bool all_queued_uploads_consumed = true;
    bool deterministic_fallback_used = false;
};

struct render_text_frame_snapshot_request {
    std::string frame_id;
    std::string source_label;
    render_text_request_batch_plan_snapshot batch_plan;
    render_text_font_fallback_chain_plan_snapshot fallback_chain_plan;
    std::vector<render_text_glyph_atlas_materialization_snapshot> materializations;
    render_text_glyph_atlas_materialization_policy_snapshot materialization_policy;
    render_text_atlas_upload_request_bridge_snapshot atlas_upload_bridge;
    std::vector<std::string> queued_atlas_upload_request_ids;
    std::vector<std::string> consumed_atlas_upload_request_ids;
    std::size_t consumed_atlas_update_count = 0;
};

struct render_text_frame_snapshot {
    render_text_frame_snapshot_status status =
        render_text_frame_snapshot_status::pending_atlas_updates;
    std::string frame_id;
    std::string source_label;
    render_text_request_batch_plan_policy_snapshot batch_policy;
    render_text_font_fallback_chain_plan_policy_snapshot fallback_chain_policy;
    render_text_glyph_atlas_materialization_policy_snapshot materialization_policy;
    render_text_atlas_upload_request_policy_snapshot atlas_upload_policy;
    render_text_frame_snapshot_policy policy;
    std::vector<render_text_batch_layout_request_snapshot> layout_requests;
    std::vector<font_face_id> selected_face_order;
    std::vector<render_text_font_fallback_chain_missing_glyph_snapshot> missing_glyphs;
    std::vector<render_text_frame_atlas_upload_snapshot> atlas_uploads;
    std::vector<std::string> queued_atlas_upload_request_ids;
    std::vector<std::string> consumed_atlas_upload_request_ids;
    std::string diagnostic;

    bool ok() const
    {
        return status == render_text_frame_snapshot_status::ready
            || status == render_text_frame_snapshot_status::pending_atlas_updates;
    }

    bool ready_for_renderer() const
    {
        return status == render_text_frame_snapshot_status::ready;
    }

    bool has_atlas_uploads() const
    {
        return !atlas_uploads.empty();
    }
};

inline std::vector<std::string> render_text_frame_upload_ready_request_ids(
    const render_text_atlas_upload_request_bridge_snapshot& bridge)
{
    std::vector<std::string> ids;
    ids.reserve(bridge.upload_requests.size());
    for (const render_text_atlas_upload_request_snapshot& request : bridge.requests) {
        if (request.has_upload_request) {
            ids.push_back(request.request_id);
        }
    }
    return ids;
}

inline bool render_text_frame_snapshot_ids_match(
    const std::vector<std::string>& expected,
    const std::vector<std::string>& actual)
{
    if (expected.size() != actual.size()) {
        return false;
    }

    return std::all_of(
        expected.begin(),
        expected.end(),
        [&](const std::string& id) {
            return render_text_atlas_upload_request_contains_id(actual, id);
        });
}

inline render_text_frame_snapshot_status render_text_frame_snapshot_status_for(
    const render_text_font_fallback_chain_plan_policy_snapshot& fallback_policy,
    const render_text_atlas_upload_request_bridge_snapshot& upload_bridge,
    const std::vector<std::string>& queued_ids,
    const std::vector<std::string>& consumed_ids,
    const std::size_t consumed_atlas_update_count)
{
    if (fallback_policy.missing_glyph_count > 0U || fallback_policy.invalid_utf8_count > 0U) {
        return render_text_frame_snapshot_status::font_fallback_incomplete;
    }
    if (!upload_bridge.ok()) {
        return render_text_frame_snapshot_status::atlas_upload_incomplete;
    }
    if (queued_ids.empty()) {
        if (!consumed_ids.empty() || consumed_atlas_update_count > 0U) {
            return render_text_frame_snapshot_status::consumed_update_mismatch;
        }
        return render_text_frame_snapshot_status::ready;
    }
    if (consumed_ids.empty() && consumed_atlas_update_count == 0U) {
        return render_text_frame_snapshot_status::pending_atlas_updates;
    }
    if (consumed_atlas_update_count != consumed_ids.size()) {
        return render_text_frame_snapshot_status::consumed_update_mismatch;
    }
    if (!render_text_frame_snapshot_ids_match(queued_ids, consumed_ids)) {
        return render_text_frame_snapshot_status::consumed_update_mismatch;
    }
    return render_text_frame_snapshot_status::ready;
}

inline render_text_frame_atlas_upload_snapshot make_render_text_frame_atlas_upload_snapshot(
    const render_text_atlas_upload_request_snapshot& request,
    const std::vector<std::string>& queued_ids,
    const std::vector<std::string>& consumed_ids)
{
    return render_text_frame_atlas_upload_snapshot{
        .request_id = request.request_id,
        .status = request.status,
        .batch_atlas_request_index = request.batch_atlas_request_index,
        .item_index = request.item_index,
        .materialization_index = request.materialization_index,
        .cache_key = request.cache_key,
        .resolved_glyph_id = request.resolved_glyph_id,
        .resolved_face_id = request.resolved_face_id,
        .page = request.upload_request.page,
        .updated_bounds = request.upload_request.updated_bounds,
        .upload_rgba_bytes = request.actual_upload_rgba_bytes,
        .has_upload_request = request.has_upload_request,
        .queued = render_text_atlas_upload_request_contains_id(queued_ids, request.request_id),
        .consumed = render_text_atlas_upload_request_contains_id(consumed_ids, request.request_id),
        .stable_request_id = request.stable_request_id,
        .diagnostic = request.diagnostic,
    };
}

inline render_text_frame_snapshot make_render_text_frame_snapshot(
    render_text_frame_snapshot_request request)
{
    if (request.queued_atlas_upload_request_ids.empty()) {
        request.queued_atlas_upload_request_ids =
            render_text_frame_upload_ready_request_ids(request.atlas_upload_bridge);
    }

    render_text_frame_snapshot snapshot{
        .status = render_text_frame_snapshot_status_for(
            request.fallback_chain_plan.policy,
            request.atlas_upload_bridge,
            request.queued_atlas_upload_request_ids,
            request.consumed_atlas_upload_request_ids,
            request.consumed_atlas_update_count),
        .frame_id = std::move(request.frame_id),
        .source_label = std::move(request.source_label),
        .batch_policy = request.batch_plan.policy,
        .fallback_chain_policy = request.fallback_chain_plan.policy,
        .materialization_policy = request.materialization_policy,
        .atlas_upload_policy = request.atlas_upload_bridge.policy,
        .layout_requests = std::move(request.batch_plan.layout_requests),
        .selected_face_order = std::move(request.fallback_chain_plan.deterministic_selected_face_order),
        .missing_glyphs = std::move(request.fallback_chain_plan.missing_glyphs),
        .queued_atlas_upload_request_ids = std::move(request.queued_atlas_upload_request_ids),
        .consumed_atlas_upload_request_ids = std::move(request.consumed_atlas_upload_request_ids),
    };

    snapshot.policy = render_text_frame_snapshot_policy{
        .layout_request_count = snapshot.batch_policy.layout_request_count,
        .fallback_chain_run_count = snapshot.fallback_chain_policy.run_count,
        .fallback_chain_missing_glyph_count = snapshot.fallback_chain_policy.missing_glyph_count,
        .fallback_chain_invalid_utf8_count = snapshot.fallback_chain_policy.invalid_utf8_count,
        .selected_face_count = snapshot.selected_face_order.size(),
        .materialization_count = request.materialization_policy.request_count,
        .upload_request_count = request.atlas_upload_bridge.policy.upload_request_count,
        .queued_upload_request_id_count = snapshot.queued_atlas_upload_request_ids.size(),
        .consumed_upload_request_id_count = snapshot.consumed_atlas_upload_request_ids.size(),
        .consumed_atlas_update_count = request.consumed_atlas_update_count,
        .total_upload_rgba_bytes = request.atlas_upload_bridge.policy.total_upload_rgba_bytes,
        .all_queued_uploads_consumed = render_text_frame_snapshot_ids_match(
            snapshot.queued_atlas_upload_request_ids,
            snapshot.consumed_atlas_upload_request_ids),
        .deterministic_fallback_used =
            snapshot.batch_policy.fallback_materialization_count > 0U
            || snapshot.fallback_chain_policy.deterministic_backend_selected,
    };

    snapshot.atlas_uploads.reserve(request.atlas_upload_bridge.requests.size());
    for (const render_text_atlas_upload_request_snapshot& upload_request :
         request.atlas_upload_bridge.requests) {
        snapshot.atlas_uploads.push_back(make_render_text_frame_atlas_upload_snapshot(
            upload_request,
            snapshot.queued_atlas_upload_request_ids,
            snapshot.consumed_atlas_upload_request_ids));
    }

    snapshot.diagnostic = snapshot.ok()
        ? "text frame snapshot captured renderer-agnostic layout and atlas handoff diagnostics"
        : "text frame snapshot captured incomplete layout or atlas handoff diagnostics";
    return snapshot;
}

inline render_text_frame_snapshot render_text_frame_snapshot_with_consumed_atlas_updates(
    render_text_frame_snapshot snapshot,
    std::vector<std::string> consumed_atlas_upload_request_ids,
    const std::size_t consumed_atlas_update_count)
{
    snapshot.consumed_atlas_upload_request_ids = std::move(consumed_atlas_upload_request_ids);
    snapshot.policy.consumed_upload_request_id_count =
        snapshot.consumed_atlas_upload_request_ids.size();
    snapshot.policy.consumed_atlas_update_count = consumed_atlas_update_count;
    snapshot.policy.all_queued_uploads_consumed = render_text_frame_snapshot_ids_match(
        snapshot.queued_atlas_upload_request_ids,
        snapshot.consumed_atlas_upload_request_ids);
    for (render_text_frame_atlas_upload_snapshot& upload : snapshot.atlas_uploads) {
        upload.consumed = render_text_atlas_upload_request_contains_id(
            snapshot.consumed_atlas_upload_request_ids,
            upload.request_id);
    }
    snapshot.status = render_text_frame_snapshot_status_for(
        snapshot.fallback_chain_policy,
        render_text_atlas_upload_request_bridge_snapshot{
            .policy = snapshot.atlas_upload_policy,
        },
        snapshot.queued_atlas_upload_request_ids,
        snapshot.consumed_atlas_upload_request_ids,
        consumed_atlas_update_count);
    snapshot.diagnostic = snapshot.ok()
        ? "text frame snapshot captured renderer-agnostic layout and atlas handoff diagnostics"
        : "text frame snapshot captured incomplete layout or atlas handoff diagnostics";
    return snapshot;
}

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
