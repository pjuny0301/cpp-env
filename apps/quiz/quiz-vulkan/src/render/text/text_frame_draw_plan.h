#pragma once

#include "render/text/text_frame_snapshot.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

enum class render_text_frame_draw_packet_status {
    draw_ready,
    frame_not_ready,
    fallback_incomplete,
    skipped_materialization,
    missing_cache_key,
    missing_layout_bounds,
    missing_atlas_bounds,
    missing_page_extent,
};

inline std::string render_text_frame_draw_packet_status_name(
    const render_text_frame_draw_packet_status status)
{
    switch (status) {
    case render_text_frame_draw_packet_status::draw_ready:
        return "draw_ready";
    case render_text_frame_draw_packet_status::frame_not_ready:
        return "frame_not_ready";
    case render_text_frame_draw_packet_status::fallback_incomplete:
        return "fallback_incomplete";
    case render_text_frame_draw_packet_status::skipped_materialization:
        return "skipped_materialization";
    case render_text_frame_draw_packet_status::missing_cache_key:
        return "missing_cache_key";
    case render_text_frame_draw_packet_status::missing_layout_bounds:
        return "missing_layout_bounds";
    case render_text_frame_draw_packet_status::missing_atlas_bounds:
        return "missing_atlas_bounds";
    case render_text_frame_draw_packet_status::missing_page_extent:
        return "missing_page_extent";
    }

    return "unknown";
}

struct render_text_frame_draw_uv_rect {
    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = 0.0f;
    float v1 = 0.0f;
    bool valid = false;
};

struct render_text_frame_draw_packet_snapshot {
    std::string packet_id;
    std::string frame_id;
    std::string source_label;
    std::string atlas_upload_request_id;
    render_text_frame_draw_packet_status status =
        render_text_frame_draw_packet_status::frame_not_ready;
    std::size_t item_index = 0;
    std::size_t materialization_index = 0;
    std::size_t run_index = 0;
    render_style_id requested_style_token;
    render_style_id resolved_style_id;
    std::size_t cluster_byte_offset = 0;
    std::size_t cluster_byte_count = 0;
    glyph_atlas_key cache_key;
    std::uint32_t resolved_glyph_id = 0;
    font_face_id resolved_face_id = 0;
    render_text_atlas_page_id page_id = 0;
    render_text_revision page_revision = 0;
    std::size_t page_width = 0;
    std::size_t page_height = 0;
    render_rect layout_bounds;
    bool has_layout_bounds = false;
    render_rect atlas_bounds;
    bool has_atlas_bounds = false;
    render_text_frame_draw_uv_rect uv_bounds;
    bool frame_ready_for_renderer = false;
    bool fallback_incomplete = false;
    bool used_deterministic_fallback = false;
    bool used_real_backend = false;
    bool glyph_supported = false;
    bool stable_cache_key = false;
    bool upload_consumed = false;
    std::string diagnostic;

    bool drawable() const
    {
        return status == render_text_frame_draw_packet_status::draw_ready;
    }
};

struct render_text_frame_draw_plan_policy {
    std::size_t materialization_count = 0;
    std::size_t packet_count = 0;
    std::size_t draw_ready_count = 0;
    std::size_t skipped_count = 0;
    std::size_t frame_not_ready_count = 0;
    std::size_t fallback_incomplete_count = 0;
    std::size_t missing_cache_key_count = 0;
    std::size_t missing_layout_bounds_count = 0;
    std::size_t missing_atlas_bounds_count = 0;
    std::size_t missing_page_extent_count = 0;
    std::size_t deterministic_fallback_count = 0;
    std::size_t real_backend_count = 0;
    std::size_t upload_consumed_count = 0;
};

struct render_text_frame_draw_plan_request {
    render_text_frame_snapshot frame;
    std::vector<render_text_glyph_atlas_materialization_snapshot> materializations;
    std::size_t item_index = 0;
};

struct render_text_frame_draw_plan_snapshot {
    std::string frame_id;
    std::string source_label;
    render_text_frame_snapshot_status frame_status =
        render_text_frame_snapshot_status::pending_atlas_updates;
    bool frame_ready_for_renderer = false;
    render_text_frame_draw_plan_policy policy;
    std::vector<render_text_frame_draw_packet_snapshot> packets;
    std::string diagnostic;

    bool ok() const
    {
        return policy.packet_count == policy.draw_ready_count;
    }

    bool has_draw_packets() const
    {
        return !packets.empty();
    }
};

inline render_text_frame_draw_uv_rect render_text_frame_draw_uv_rect_for(
    const render_rect& atlas_bounds,
    const render_text_atlas_page& page)
{
    if (page.width == 0U || page.height == 0U) {
        return {};
    }

    const float page_width = static_cast<float>(page.width);
    const float page_height = static_cast<float>(page.height);
    return render_text_frame_draw_uv_rect{
        .u0 = atlas_bounds.x / page_width,
        .v0 = atlas_bounds.y / page_height,
        .u1 = atlas_bounds.right() / page_width,
        .v1 = atlas_bounds.bottom() / page_height,
        .valid = true,
    };
}

inline const render_text_batch_layout_request_snapshot* render_text_frame_draw_layout_request_for(
    const render_text_frame_snapshot& frame,
    const std::size_t item_index)
{
    const auto match = std::find_if(
        frame.layout_requests.begin(),
        frame.layout_requests.end(),
        [&](const render_text_batch_layout_request_snapshot& request) {
            return request.item_index == item_index;
        });
    return match == frame.layout_requests.end() ? nullptr : &*match;
}

inline const render_text_batch_normalized_style_key* render_text_frame_draw_style_key_for(
    const render_text_frame_snapshot& frame,
    const std::size_t item_index,
    const std::size_t run_index)
{
    const render_text_batch_layout_request_snapshot* layout_request =
        render_text_frame_draw_layout_request_for(frame, item_index);
    if (layout_request == nullptr || run_index >= layout_request->style_key_count) {
        return nullptr;
    }

    const std::size_t style_key_index = layout_request->style_key_offset + run_index;
    if (style_key_index >= frame.style_keys.size()) {
        return nullptr;
    }
    return &frame.style_keys[style_key_index];
}

inline const render_text_frame_atlas_upload_snapshot* render_text_frame_draw_atlas_upload_for(
    const render_text_frame_snapshot& frame,
    const std::size_t item_index,
    const std::size_t materialization_index,
    const glyph_atlas_key& cache_key)
{
    const auto match = std::find_if(
        frame.atlas_uploads.begin(),
        frame.atlas_uploads.end(),
        [&](const render_text_frame_atlas_upload_snapshot& upload) {
            return upload.item_index == item_index
                && upload.materialization_index == materialization_index
                && upload.cache_key == cache_key;
        });
    return match == frame.atlas_uploads.end() ? nullptr : &*match;
}

inline std::string render_text_frame_draw_packet_stable_id_for(
    const render_text_frame_snapshot& frame,
    const std::size_t item_index,
    const std::size_t materialization_index,
    const glyph_atlas_key& cache_key,
    const render_text_atlas_page& page)
{
    return "text-frame-draw:v1"
        + std::string{":frame="} + frame.frame_id
        + ":item=" + std::to_string(item_index)
        + ":mat=" + std::to_string(materialization_index)
        + ":face=" + std::to_string(cache_key.face_id)
        + ":glyph=" + std::to_string(cache_key.glyph_id)
        + ":px=" + std::to_string(cache_key.pixel_size)
        + ":page=" + std::to_string(page.id)
        + ":rev=" + std::to_string(page.revision);
}

inline bool render_text_frame_draw_materialization_uses_deterministic_fallback(
    const render_text_glyph_atlas_materialization_snapshot& materialization)
{
    return materialization.shaping_font_backend_used_deterministic_fallback
        || materialization.raster_font_backend_used_deterministic_fallback
        || materialization.shaping_font_backend_fallback_only
        || materialization.raster_font_backend_fallback_only;
}

inline render_text_frame_draw_packet_status render_text_frame_draw_packet_status_for(
    const render_text_frame_snapshot& frame,
    const render_text_glyph_atlas_materialization_snapshot& materialization,
    const bool has_atlas_bounds,
    const render_text_frame_draw_uv_rect& uv_bounds)
{
    if (frame.status == render_text_frame_snapshot_status::font_fallback_incomplete) {
        return render_text_frame_draw_packet_status::fallback_incomplete;
    }
    if (!frame.ready_for_renderer()) {
        return render_text_frame_draw_packet_status::frame_not_ready;
    }
    if (!materialization.materialized) {
        return render_text_frame_draw_packet_status::skipped_materialization;
    }
    if (!materialization.has_cache_key) {
        return render_text_frame_draw_packet_status::missing_cache_key;
    }
    if (!materialization.has_layout_bounds) {
        return render_text_frame_draw_packet_status::missing_layout_bounds;
    }
    if (!has_atlas_bounds) {
        return render_text_frame_draw_packet_status::missing_atlas_bounds;
    }
    if (!uv_bounds.valid) {
        return render_text_frame_draw_packet_status::missing_page_extent;
    }
    return render_text_frame_draw_packet_status::draw_ready;
}

inline render_text_frame_draw_packet_snapshot make_render_text_frame_draw_packet(
    const render_text_frame_snapshot& frame,
    const render_text_glyph_atlas_materialization_snapshot& materialization,
    const std::size_t item_index,
    const std::size_t materialization_index)
{
    const render_text_frame_atlas_upload_snapshot* upload = render_text_frame_draw_atlas_upload_for(
        frame,
        item_index,
        materialization_index,
        materialization.cache_key);
    const render_text_atlas_page page = upload == nullptr || upload->page.id == 0U
        ? materialization.page
        : upload->page;
    const bool has_atlas_bounds = materialization.has_atlas_placement
        || (upload != nullptr && upload->has_upload_request);
    const render_rect atlas_bounds = materialization.has_atlas_placement
        ? materialization.atlas_bounds
        : (upload == nullptr ? render_rect{} : upload->updated_bounds);
    const render_text_frame_draw_uv_rect uv_bounds =
        render_text_frame_draw_uv_rect_for(atlas_bounds, page);
    const render_text_batch_normalized_style_key* style_key =
        render_text_frame_draw_style_key_for(frame, item_index, materialization.run_index);
    const render_text_frame_draw_packet_status status =
        render_text_frame_draw_packet_status_for(frame, materialization, has_atlas_bounds, uv_bounds);

    std::string diagnostic = "text frame draw packet is ready for renderer consumption";
    switch (status) {
    case render_text_frame_draw_packet_status::draw_ready:
        break;
    case render_text_frame_draw_packet_status::frame_not_ready:
        diagnostic = "text frame draw packet is blocked until frame atlas updates are consumed";
        break;
    case render_text_frame_draw_packet_status::fallback_incomplete:
        diagnostic = "text frame draw packet is blocked by incomplete font fallback coverage";
        break;
    case render_text_frame_draw_packet_status::skipped_materialization:
        diagnostic = "text frame draw packet skipped a non-upload-ready glyph materialization";
        break;
    case render_text_frame_draw_packet_status::missing_cache_key:
        diagnostic = "text frame draw packet is missing a glyph atlas cache key";
        break;
    case render_text_frame_draw_packet_status::missing_layout_bounds:
        diagnostic = "text frame draw packet is missing glyph layout bounds";
        break;
    case render_text_frame_draw_packet_status::missing_atlas_bounds:
        diagnostic = "text frame draw packet is missing glyph atlas bounds";
        break;
    case render_text_frame_draw_packet_status::missing_page_extent:
        diagnostic = "text frame draw packet cannot derive UV coordinates without atlas page extent";
        break;
    }

    return render_text_frame_draw_packet_snapshot{
        .packet_id = render_text_frame_draw_packet_stable_id_for(
            frame,
            item_index,
            materialization_index,
            materialization.cache_key,
            page),
        .frame_id = frame.frame_id,
        .source_label = frame.source_label,
        .atlas_upload_request_id = upload == nullptr ? std::string{} : upload->request_id,
        .status = status,
        .item_index = item_index,
        .materialization_index = materialization_index,
        .run_index = materialization.run_index,
        .requested_style_token = style_key == nullptr ? render_style_id{} : style_key->requested_style_token,
        .resolved_style_id = style_key == nullptr ? render_style_id{} : style_key->resolved_style_id,
        .cluster_byte_offset = materialization.cluster_byte_offset,
        .cluster_byte_count = materialization.cluster_byte_count,
        .cache_key = materialization.cache_key,
        .resolved_glyph_id = materialization.resolved_glyph_id,
        .resolved_face_id = materialization.resolved_face_id,
        .page_id = page.id,
        .page_revision = page.revision,
        .page_width = page.width,
        .page_height = page.height,
        .layout_bounds = materialization.layout_bounds,
        .has_layout_bounds = materialization.has_layout_bounds,
        .atlas_bounds = atlas_bounds,
        .has_atlas_bounds = has_atlas_bounds,
        .uv_bounds = uv_bounds,
        .frame_ready_for_renderer = frame.ready_for_renderer(),
        .fallback_incomplete = frame.status == render_text_frame_snapshot_status::font_fallback_incomplete,
        .used_deterministic_fallback =
            render_text_frame_draw_materialization_uses_deterministic_fallback(materialization),
        .used_real_backend = render_text_batch_materialization_uses_real_backend(materialization),
        .glyph_supported = materialization.glyph_supported,
        .stable_cache_key = materialization.has_cache_key
            && materialization.cache_key.face_id != 0U
            && materialization.cache_key.glyph_id != 0U
            && materialization.cache_key.pixel_size != 0U,
        .upload_consumed = upload != nullptr && upload->consumed,
        .diagnostic = std::move(diagnostic),
    };
}

inline void append_render_text_frame_draw_packet(
    std::vector<render_text_frame_draw_packet_snapshot>& packets,
    render_text_frame_draw_plan_policy& policy,
    render_text_frame_draw_packet_snapshot packet)
{
    ++policy.packet_count;
    switch (packet.status) {
    case render_text_frame_draw_packet_status::draw_ready:
        ++policy.draw_ready_count;
        break;
    case render_text_frame_draw_packet_status::frame_not_ready:
        ++policy.frame_not_ready_count;
        ++policy.skipped_count;
        break;
    case render_text_frame_draw_packet_status::fallback_incomplete:
        ++policy.fallback_incomplete_count;
        ++policy.skipped_count;
        break;
    case render_text_frame_draw_packet_status::skipped_materialization:
        ++policy.skipped_count;
        break;
    case render_text_frame_draw_packet_status::missing_cache_key:
        ++policy.missing_cache_key_count;
        ++policy.skipped_count;
        break;
    case render_text_frame_draw_packet_status::missing_layout_bounds:
        ++policy.missing_layout_bounds_count;
        ++policy.skipped_count;
        break;
    case render_text_frame_draw_packet_status::missing_atlas_bounds:
        ++policy.missing_atlas_bounds_count;
        ++policy.skipped_count;
        break;
    case render_text_frame_draw_packet_status::missing_page_extent:
        ++policy.missing_page_extent_count;
        ++policy.skipped_count;
        break;
    }
    if (packet.used_deterministic_fallback) {
        ++policy.deterministic_fallback_count;
    }
    if (packet.used_real_backend) {
        ++policy.real_backend_count;
    }
    if (packet.upload_consumed) {
        ++policy.upload_consumed_count;
    }
    packets.push_back(std::move(packet));
}

inline render_text_frame_draw_plan_snapshot plan_render_text_frame_draw_packets(
    render_text_frame_draw_plan_request request)
{
    render_text_frame_draw_plan_snapshot plan{
        .frame_id = request.frame.frame_id,
        .source_label = request.frame.source_label,
        .frame_status = request.frame.status,
        .frame_ready_for_renderer = request.frame.ready_for_renderer(),
    };
    plan.policy.materialization_count = request.materializations.size();
    plan.packets.reserve(request.materializations.size());

    for (std::size_t materialization_index = 0;
         materialization_index < request.materializations.size();
         ++materialization_index) {
        append_render_text_frame_draw_packet(
            plan.packets,
            plan.policy,
            make_render_text_frame_draw_packet(
                request.frame,
                request.materializations[materialization_index],
                request.item_index,
                materialization_index));
    }

    plan.diagnostic = plan.ok()
        ? "text frame draw plan produced renderer-facing glyph draw packet diagnostics"
        : "text frame draw plan found glyph draw packets that are not renderer-ready";
    return plan;
}

} // namespace quiz_vulkan::render
