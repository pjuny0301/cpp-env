#pragma once

#include "render/text/font_rasterizer.h"
#include "render/text/font_shaping_backend.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

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
    std::vector<std::uint32_t> shaped_glyph_ids;
    font_face_id resolved_face_id = 0;
    glyph_atlas_key cache_key;
    bool has_cache_key = false;
    render_text_font_rasterizer_status rasterizer_status =
        render_text_font_rasterizer_status::missing_font_source;
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
    std::vector<std::uint32_t> shaped_glyph_ids;
    font_face_id resolved_face_id = 0;
    glyph_atlas_key cache_key;
    bool has_cache_key = false;
    render_text_font_rasterizer_status rasterizer_status =
        render_text_font_rasterizer_status::missing_font_source;
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
        .shaped_glyph_ids = std::move(request.shaped_glyph_ids),
        .resolved_face_id = request.resolved_face_id,
        .cache_key = request.cache_key,
        .has_cache_key = request.has_cache_key,
        .rasterizer_status = request.rasterizer_status,
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
