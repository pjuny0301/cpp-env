#pragma once

#include "render/text/text_atlas_packet_consumption_evidence.h"
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
    render_text_atlas_packet_consumption_evidence atlas_consumption;
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
    const render_text_batch_layout_request_snapshot* layout_request =
        render_text_frame_draw_layout_request_for(frame, item_index);
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
        .source_label = layout_request == nullptr || layout_request->source_label.empty()
            ? frame.source_label
            : layout_request->source_label,
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
        .atlas_consumption = render_text_atlas_packet_consumption_evidence{
            .cluster_index = materialization.cluster_index,
            .line_index = materialization.line_index,
            .run_index = materialization.run_index,
            .cluster_byte_offset = materialization.cluster_byte_offset,
            .cluster_byte_count = materialization.cluster_byte_count,
            .cache_key = materialization.cache_key,
            .resolved_glyph_id = materialization.resolved_glyph_id,
            .resolved_face_id = materialization.resolved_face_id,
            .pen_x = materialization.pen_x,
            .pen_y = materialization.pen_y,
            .baseline = materialization.baseline,
            .upload_generation = page.revision,
            .missing_glyph = !materialization.glyph_supported,
            .used_fallback_glyph_id = materialization.used_fallback_glyph_id,
            .clean_reuse = materialization.clean_reuse,
        },
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

struct render_text_frame_draw_plan_diff_policy {
    bool frame_status_changed = false;
    bool frame_readiness_changed = false;
    bool frame_readiness_regressed = false;
    std::ptrdiff_t materialization_count_delta = 0;
    std::ptrdiff_t packet_count_delta = 0;
    std::ptrdiff_t draw_ready_count_delta = 0;
    std::ptrdiff_t glyph_quad_count_delta = 0;
    std::ptrdiff_t skipped_count_delta = 0;
    std::ptrdiff_t fallback_incomplete_count_delta = 0;
    std::ptrdiff_t deterministic_fallback_count_delta = 0;
    std::ptrdiff_t real_backend_count_delta = 0;
    std::ptrdiff_t upload_consumed_count_delta = 0;
    std::ptrdiff_t stable_glyph_key_count_delta = 0;
    std::ptrdiff_t stable_style_key_count_delta = 0;
    std::ptrdiff_t stable_run_key_count_delta = 0;
    std::size_t stable_glyph_changed_packet_count = 0;
    std::size_t stable_style_changed_packet_count = 0;
    std::size_t stable_run_changed_packet_count = 0;
    std::size_t added_packet_count = 0;
    std::size_t removed_packet_count = 0;
    std::size_t changed_packet_count = 0;
    std::size_t unchanged_packet_count = 0;
    std::size_t readiness_changed_packet_count = 0;
    std::size_t readiness_regression_count = 0;
    std::size_t readiness_recovery_count = 0;
    std::size_t fallback_changed_packet_count = 0;
    std::size_t page_revision_changed_packet_count = 0;
    std::size_t line_run_changed_packet_count = 0;
    std::size_t cluster_changed_packet_count = 0;
    std::size_t cache_key_changed_packet_count = 0;
    std::size_t atlas_page_changed_packet_count = 0;
    std::size_t upload_request_changed_packet_count = 0;
    std::size_t upload_generation_changed_packet_count = 0;
    std::size_t glyph_quad_count_changed_packet_count = 0;
    std::size_t status_changed_packet_count = 0;
    std::size_t blocker_changed_packet_count = 0;
    std::size_t missing_atlas_upload_blocker_count = 0;
    std::size_t missing_materialization_blocker_count = 0;
    std::size_t non_upload_ready_payload_blocker_count = 0;
    std::size_t missing_glyph_quad_fact_blocker_count = 0;
    std::size_t fallback_or_skipped_packet_count = 0;
    bool stable_no_change = false;
};

struct render_text_frame_draw_packet_consumption_diff {
    std::string packet_key;
    std::string previous_packet_id;
    std::string current_packet_id;
    std::string previous_upload_request_id;
    std::string current_upload_request_id;
    std::size_t previous_line_index = 0;
    std::size_t current_line_index = 0;
    std::size_t previous_run_index = 0;
    std::size_t current_run_index = 0;
    std::size_t previous_cluster_byte_offset = 0;
    std::size_t current_cluster_byte_offset = 0;
    std::size_t previous_cluster_byte_count = 0;
    std::size_t current_cluster_byte_count = 0;
    glyph_atlas_key previous_cache_key;
    glyph_atlas_key current_cache_key;
    render_text_atlas_page_id previous_page_id = 0;
    render_text_atlas_page_id current_page_id = 0;
    render_text_revision previous_upload_generation = 0;
    render_text_revision current_upload_generation = 0;
    std::size_t previous_glyph_quad_count = 0;
    std::size_t current_glyph_quad_count = 0;
    render_text_frame_draw_packet_status previous_status =
        render_text_frame_draw_packet_status::frame_not_ready;
    render_text_frame_draw_packet_status current_status =
        render_text_frame_draw_packet_status::frame_not_ready;
    bool added = false;
    bool removed = false;
    bool changed = false;
    bool unchanged = false;
    bool line_run_changed = false;
    bool cluster_changed = false;
    bool cache_key_changed = false;
    bool atlas_page_changed = false;
    bool upload_request_changed = false;
    bool upload_generation_changed = false;
    bool glyph_quad_count_changed = false;
    bool readiness_changed = false;
    bool readiness_regressed = false;
    bool readiness_recovered = false;
    bool status_changed = false;
    bool blocker_changed = false;
    bool missing_atlas_upload = false;
    bool missing_materialization = false;
    bool non_upload_ready_payload = false;
    bool missing_glyph_quad_facts = false;
    bool fallback_or_skipped = false;
    std::string previous_blocker_summary;
    std::string current_blocker_summary;
};

struct render_text_frame_draw_plan_diff {
    std::string previous_frame_id;
    std::string current_frame_id;
    render_text_frame_snapshot_status previous_frame_status =
        render_text_frame_snapshot_status::pending_atlas_updates;
    render_text_frame_snapshot_status current_frame_status =
        render_text_frame_snapshot_status::pending_atlas_updates;
    bool previous_ready_for_renderer = false;
    bool current_ready_for_renderer = false;
    render_text_frame_draw_plan_diff_policy policy;
    std::vector<render_text_frame_draw_packet_consumption_diff> packet_diffs;
    std::vector<std::string> added_packet_ids;
    std::vector<std::string> removed_packet_ids;
    std::vector<std::string> changed_packet_ids;
    std::vector<std::string> unchanged_packet_ids;
    std::vector<std::string> readiness_changed_packet_ids;
    std::vector<std::string> readiness_regressed_packet_ids;
    std::vector<std::string> readiness_recovered_packet_ids;
    std::vector<std::string> fallback_changed_packet_ids;
    std::vector<std::string> page_revision_changed_packet_ids;
    std::vector<std::string> blocker_changed_packet_ids;
    std::vector<std::string> stable_glyph_changed_packet_ids;
    std::vector<std::string> stable_style_changed_packet_ids;
    std::vector<std::string> stable_run_changed_packet_ids;
    std::string diagnostic;

    bool has_packet_changes() const
    {
        return !added_packet_ids.empty()
            || !removed_packet_ids.empty()
            || !changed_packet_ids.empty();
    }

    bool has_readiness_or_fallback_changes() const
    {
        return policy.frame_readiness_changed
            || !readiness_changed_packet_ids.empty()
            || !fallback_changed_packet_ids.empty()
            || policy.fallback_incomplete_count_delta != 0
            || policy.deterministic_fallback_count_delta != 0;
    }

    bool has_page_revision_changes() const
    {
        return !page_revision_changed_packet_ids.empty();
    }

    bool has_stable_key_deltas() const
    {
        return policy.stable_glyph_key_count_delta != 0
            || policy.stable_style_key_count_delta != 0
            || policy.stable_run_key_count_delta != 0
            || !stable_glyph_changed_packet_ids.empty()
            || !stable_style_changed_packet_ids.empty()
            || !stable_run_changed_packet_ids.empty();
    }

    bool stable_no_change() const
    {
        return policy.stable_no_change;
    }

    bool ok() const
    {
        return !policy.frame_readiness_regressed;
    }
};

inline std::string render_text_frame_draw_packet_diff_key_for(
    const render_text_frame_draw_packet_snapshot& packet)
{
    return "text-frame-draw-packet:v1"
        + std::string{":frame="} + packet.frame_id
        + ":item=" + std::to_string(packet.item_index)
        + ":mat=" + std::to_string(packet.materialization_index)
        + ":face=" + std::to_string(packet.cache_key.face_id)
        + ":glyph=" + std::to_string(packet.cache_key.glyph_id)
        + ":px=" + std::to_string(packet.cache_key.pixel_size);
}

inline std::string render_text_frame_draw_packet_slot_key_for(
    const render_text_frame_draw_packet_snapshot& packet)
{
    return "text-frame-draw-slot:v1"
        + std::string{":frame="} + packet.frame_id
        + ":item=" + std::to_string(packet.item_index)
        + ":mat=" + std::to_string(packet.materialization_index);
}

inline std::string render_text_frame_draw_packet_glyph_key_for(
    const render_text_frame_draw_packet_snapshot& packet)
{
    if (!packet.stable_cache_key) {
        return {};
    }
    return "glyph:v1"
        + std::string{":face="} + std::to_string(packet.cache_key.face_id)
        + ":glyph=" + std::to_string(packet.cache_key.glyph_id)
        + ":px=" + std::to_string(packet.cache_key.pixel_size);
}

inline std::string render_text_frame_draw_packet_style_key_for(
    const render_text_frame_draw_packet_snapshot& packet)
{
    return "style:v1"
        + std::string{":requested="} + packet.requested_style_token
        + ":resolved=" + packet.resolved_style_id;
}

inline std::string render_text_frame_draw_packet_run_key_for(
    const render_text_frame_draw_packet_snapshot& packet)
{
    return "run:v1"
        + std::string{":item="} + std::to_string(packet.item_index)
        + ":line=" + std::to_string(packet.atlas_consumption.line_index)
        + ":run=" + std::to_string(packet.run_index)
        + ":style=" + packet.resolved_style_id;
}

inline const render_text_frame_draw_packet_snapshot* render_text_frame_draw_plan_find_packet(
    const std::vector<render_text_frame_draw_packet_snapshot>& packets,
    const std::string& diff_key)
{
    const auto match = std::find_if(
        packets.begin(),
        packets.end(),
        [&](const render_text_frame_draw_packet_snapshot& packet) {
            return render_text_frame_draw_packet_diff_key_for(packet) == diff_key;
        });
    return match == packets.end() ? nullptr : &*match;
}

inline const render_text_frame_draw_packet_snapshot* render_text_frame_draw_plan_find_packet_by_slot(
    const std::vector<render_text_frame_draw_packet_snapshot>& packets,
    const std::string& slot_key)
{
    const auto match = std::find_if(
        packets.begin(),
        packets.end(),
        [&](const render_text_frame_draw_packet_snapshot& packet) {
            return render_text_frame_draw_packet_slot_key_for(packet) == slot_key;
        });
    return match == packets.end() ? nullptr : &*match;
}

inline bool render_text_frame_draw_uv_rects_equal(
    const render_text_frame_draw_uv_rect& lhs,
    const render_text_frame_draw_uv_rect& rhs)
{
    return lhs.u0 == rhs.u0
        && lhs.v0 == rhs.v0
        && lhs.u1 == rhs.u1
        && lhs.v1 == rhs.v1
        && lhs.valid == rhs.valid;
}

inline bool render_text_frame_draw_packets_equal(
    const render_text_frame_draw_packet_snapshot& lhs,
    const render_text_frame_draw_packet_snapshot& rhs)
{
    return lhs.packet_id == rhs.packet_id
        && lhs.atlas_upload_request_id == rhs.atlas_upload_request_id
        && lhs.status == rhs.status
        && lhs.item_index == rhs.item_index
        && lhs.materialization_index == rhs.materialization_index
        && lhs.run_index == rhs.run_index
        && lhs.requested_style_token == rhs.requested_style_token
        && lhs.resolved_style_id == rhs.resolved_style_id
        && lhs.cluster_byte_offset == rhs.cluster_byte_offset
        && lhs.cluster_byte_count == rhs.cluster_byte_count
        && lhs.cache_key == rhs.cache_key
        && lhs.resolved_glyph_id == rhs.resolved_glyph_id
        && lhs.resolved_face_id == rhs.resolved_face_id
        && lhs.page_id == rhs.page_id
        && lhs.page_revision == rhs.page_revision
        && lhs.page_width == rhs.page_width
        && lhs.page_height == rhs.page_height
        && render_text_frame_snapshot_rects_equal(lhs.layout_bounds, rhs.layout_bounds)
        && lhs.has_layout_bounds == rhs.has_layout_bounds
        && render_text_frame_snapshot_rects_equal(lhs.atlas_bounds, rhs.atlas_bounds)
        && lhs.has_atlas_bounds == rhs.has_atlas_bounds
        && render_text_frame_draw_uv_rects_equal(lhs.uv_bounds, rhs.uv_bounds)
        && lhs.frame_ready_for_renderer == rhs.frame_ready_for_renderer
        && lhs.fallback_incomplete == rhs.fallback_incomplete
        && lhs.used_deterministic_fallback == rhs.used_deterministic_fallback
        && lhs.used_real_backend == rhs.used_real_backend
        && lhs.glyph_supported == rhs.glyph_supported
        && lhs.stable_cache_key == rhs.stable_cache_key
        && lhs.upload_consumed == rhs.upload_consumed
        && !diff_render_text_atlas_packet_consumption_evidence(
                lhs.atlas_consumption,
                rhs.atlas_consumption)
                .has_changes();
}

inline std::size_t render_text_frame_draw_packet_glyph_quad_count_for(
    const render_text_frame_draw_packet_snapshot& packet)
{
    return packet.drawable()
            && packet.has_layout_bounds
            && packet.has_atlas_bounds
            && packet.uv_bounds.valid
        ? 1U
        : 0U;
}

inline std::size_t render_text_frame_draw_plan_glyph_quad_count(
    const render_text_frame_draw_plan_snapshot& plan)
{
    std::size_t count = 0;
    for (const render_text_frame_draw_packet_snapshot& packet : plan.packets) {
        count += render_text_frame_draw_packet_glyph_quad_count_for(packet);
    }
    return count;
}

inline bool render_text_frame_draw_packet_missing_atlas_upload(
    const render_text_frame_draw_packet_snapshot& packet)
{
    return packet.atlas_upload_request_id.empty() || !packet.upload_consumed;
}

inline bool render_text_frame_draw_packet_missing_materialization(
    const render_text_frame_draw_packet_snapshot& packet)
{
    return packet.status == render_text_frame_draw_packet_status::skipped_materialization
        || packet.status == render_text_frame_draw_packet_status::missing_cache_key;
}

inline bool render_text_frame_draw_packet_non_upload_ready_payload(
    const render_text_frame_draw_packet_snapshot& packet)
{
    return packet.status == render_text_frame_draw_packet_status::skipped_materialization;
}

inline bool render_text_frame_draw_packet_missing_glyph_quad_facts(
    const render_text_frame_draw_packet_snapshot& packet)
{
    return packet.status == render_text_frame_draw_packet_status::missing_layout_bounds
        || packet.status == render_text_frame_draw_packet_status::missing_atlas_bounds
        || packet.status == render_text_frame_draw_packet_status::missing_page_extent
        || !packet.has_layout_bounds
        || !packet.has_atlas_bounds
        || !packet.uv_bounds.valid;
}

inline bool render_text_frame_draw_packet_fallback_or_skipped(
    const render_text_frame_draw_packet_snapshot& packet)
{
    return packet.fallback_incomplete
        || packet.used_deterministic_fallback
        || packet.status == render_text_frame_draw_packet_status::fallback_incomplete
        || packet.status == render_text_frame_draw_packet_status::skipped_materialization;
}

inline std::string render_text_frame_draw_packet_blocker_summary_for(
    const render_text_frame_draw_packet_snapshot& packet)
{
    if (packet.drawable()) {
        return {};
    }
    if (!packet.diagnostic.empty()) {
        return packet.diagnostic;
    }
    switch (packet.status) {
    case render_text_frame_draw_packet_status::draw_ready:
        return {};
    case render_text_frame_draw_packet_status::frame_not_ready:
        return "draw packet is waiting for consumed atlas upload evidence";
    case render_text_frame_draw_packet_status::fallback_incomplete:
        return "draw packet is blocked by incomplete font fallback coverage";
    case render_text_frame_draw_packet_status::skipped_materialization:
        return "draw packet has no upload-ready glyph materialization";
    case render_text_frame_draw_packet_status::missing_cache_key:
        return "draw packet is missing a stable glyph atlas cache key";
    case render_text_frame_draw_packet_status::missing_layout_bounds:
        return "draw packet is missing glyph layout bounds";
    case render_text_frame_draw_packet_status::missing_atlas_bounds:
        return "draw packet is missing glyph atlas bounds";
    case render_text_frame_draw_packet_status::missing_page_extent:
        return "draw packet cannot derive glyph quad UVs without atlas page extent";
    }
    return "unknown draw packet blocker";
}

inline render_text_frame_draw_packet_consumption_diff
make_render_text_frame_draw_packet_consumption_diff(
    const render_text_frame_draw_packet_snapshot* previous,
    const render_text_frame_draw_packet_snapshot* current)
{
    render_text_frame_draw_packet_consumption_diff diff{
        .packet_key = current == nullptr
            ? (previous == nullptr ? std::string{} : render_text_frame_draw_packet_diff_key_for(*previous))
            : render_text_frame_draw_packet_diff_key_for(*current),
        .previous_packet_id = previous == nullptr ? std::string{} : previous->packet_id,
        .current_packet_id = current == nullptr ? std::string{} : current->packet_id,
        .previous_upload_request_id =
            previous == nullptr ? std::string{} : previous->atlas_upload_request_id,
        .current_upload_request_id =
            current == nullptr ? std::string{} : current->atlas_upload_request_id,
        .previous_line_index = previous == nullptr ? 0U : previous->atlas_consumption.line_index,
        .current_line_index = current == nullptr ? 0U : current->atlas_consumption.line_index,
        .previous_run_index = previous == nullptr ? 0U : previous->run_index,
        .current_run_index = current == nullptr ? 0U : current->run_index,
        .previous_cluster_byte_offset = previous == nullptr ? 0U : previous->cluster_byte_offset,
        .current_cluster_byte_offset = current == nullptr ? 0U : current->cluster_byte_offset,
        .previous_cluster_byte_count = previous == nullptr ? 0U : previous->cluster_byte_count,
        .current_cluster_byte_count = current == nullptr ? 0U : current->cluster_byte_count,
        .previous_cache_key = previous == nullptr ? glyph_atlas_key{} : previous->cache_key,
        .current_cache_key = current == nullptr ? glyph_atlas_key{} : current->cache_key,
        .previous_page_id = previous == nullptr ? 0U : previous->page_id,
        .current_page_id = current == nullptr ? 0U : current->page_id,
        .previous_upload_generation =
            previous == nullptr ? 0U : previous->atlas_consumption.upload_generation,
        .current_upload_generation =
            current == nullptr ? 0U : current->atlas_consumption.upload_generation,
        .previous_glyph_quad_count = previous == nullptr
            ? 0U
            : render_text_frame_draw_packet_glyph_quad_count_for(*previous),
        .current_glyph_quad_count = current == nullptr
            ? 0U
            : render_text_frame_draw_packet_glyph_quad_count_for(*current),
        .previous_status = previous == nullptr
            ? render_text_frame_draw_packet_status::frame_not_ready
            : previous->status,
        .current_status = current == nullptr
            ? render_text_frame_draw_packet_status::frame_not_ready
            : current->status,
        .added = previous == nullptr && current != nullptr,
        .removed = previous != nullptr && current == nullptr,
        .previous_blocker_summary = previous == nullptr
            ? std::string{}
            : render_text_frame_draw_packet_blocker_summary_for(*previous),
        .current_blocker_summary = current == nullptr
            ? std::string{}
            : render_text_frame_draw_packet_blocker_summary_for(*current),
    };
    diff.line_run_changed =
        !diff.added
        && !diff.removed
        && (diff.previous_line_index != diff.current_line_index
            || diff.previous_run_index != diff.current_run_index);
    diff.cluster_changed =
        !diff.added
        && !diff.removed
        && (diff.previous_cluster_byte_offset != diff.current_cluster_byte_offset
            || diff.previous_cluster_byte_count != diff.current_cluster_byte_count);
    diff.cache_key_changed =
        !diff.added
        && !diff.removed
        && diff.previous_cache_key != diff.current_cache_key;
    diff.atlas_page_changed =
        !diff.added
        && !diff.removed
        && diff.previous_page_id != diff.current_page_id;
    diff.upload_request_changed =
        !diff.added
        && !diff.removed
        && diff.previous_upload_request_id != diff.current_upload_request_id;
    diff.upload_generation_changed =
        !diff.added
        && !diff.removed
        && diff.previous_upload_generation != diff.current_upload_generation;
    diff.glyph_quad_count_changed =
        !diff.added
        && !diff.removed
        && diff.previous_glyph_quad_count != diff.current_glyph_quad_count;
    const bool previous_ready = previous != nullptr && previous->drawable();
    const bool current_ready = current != nullptr && current->drawable();
    diff.readiness_changed = previous_ready != current_ready;
    diff.readiness_regressed = previous_ready && !current_ready;
    diff.readiness_recovered = !previous_ready && current_ready && previous != nullptr;
    diff.status_changed =
        !diff.added && !diff.removed && diff.previous_status != diff.current_status;
    diff.blocker_changed = diff.previous_blocker_summary != diff.current_blocker_summary;
    if (current != nullptr) {
        diff.missing_atlas_upload = render_text_frame_draw_packet_missing_atlas_upload(*current);
        diff.missing_materialization =
            render_text_frame_draw_packet_missing_materialization(*current);
        diff.non_upload_ready_payload =
            render_text_frame_draw_packet_non_upload_ready_payload(*current);
        diff.missing_glyph_quad_facts =
            render_text_frame_draw_packet_missing_glyph_quad_facts(*current);
        diff.fallback_or_skipped =
            render_text_frame_draw_packet_fallback_or_skipped(*current);
    }
    diff.changed =
        !diff.added
        && !diff.removed
        && (diff.line_run_changed
            || diff.cluster_changed
            || diff.cache_key_changed
            || diff.atlas_page_changed
            || diff.upload_request_changed
            || diff.upload_generation_changed
            || diff.glyph_quad_count_changed
            || diff.readiness_changed
            || diff.status_changed
            || diff.blocker_changed);
    diff.unchanged = !diff.added && !diff.removed && !diff.changed;
    return diff;
}

inline void render_text_frame_draw_append_unique_nonempty(
    std::vector<std::string>& ids,
    const std::string& id)
{
    if (!id.empty()) {
        render_text_atlas_upload_request_append_unique_id(ids, id);
    }
}

inline std::vector<std::string> render_text_frame_draw_plan_stable_glyph_keys(
    const render_text_frame_draw_plan_snapshot& plan)
{
    std::vector<std::string> keys;
    keys.reserve(plan.packets.size());
    for (const render_text_frame_draw_packet_snapshot& packet : plan.packets) {
        render_text_frame_draw_append_unique_nonempty(
            keys,
            render_text_frame_draw_packet_glyph_key_for(packet));
    }
    return keys;
}

inline std::vector<std::string> render_text_frame_draw_plan_stable_style_keys(
    const render_text_frame_draw_plan_snapshot& plan)
{
    std::vector<std::string> keys;
    keys.reserve(plan.packets.size());
    for (const render_text_frame_draw_packet_snapshot& packet : plan.packets) {
        render_text_frame_draw_append_unique_nonempty(
            keys,
            render_text_frame_draw_packet_style_key_for(packet));
    }
    return keys;
}

inline std::vector<std::string> render_text_frame_draw_plan_stable_run_keys(
    const render_text_frame_draw_plan_snapshot& plan)
{
    std::vector<std::string> keys;
    keys.reserve(plan.packets.size());
    for (const render_text_frame_draw_packet_snapshot& packet : plan.packets) {
        render_text_frame_draw_append_unique_nonempty(
            keys,
            render_text_frame_draw_packet_run_key_for(packet));
    }
    return keys;
}

inline render_text_frame_draw_plan_diff diff_render_text_frame_draw_plans(
    const render_text_frame_draw_plan_snapshot& previous,
    const render_text_frame_draw_plan_snapshot& current)
{
    render_text_frame_draw_plan_diff diff{
        .previous_frame_id = previous.frame_id,
        .current_frame_id = current.frame_id,
        .previous_frame_status = previous.frame_status,
        .current_frame_status = current.frame_status,
        .previous_ready_for_renderer = previous.frame_ready_for_renderer,
        .current_ready_for_renderer = current.frame_ready_for_renderer,
    };

    diff.added_packet_ids.reserve(current.packets.size());
    for (const render_text_frame_draw_packet_snapshot& packet : current.packets) {
        if (render_text_frame_draw_plan_find_packet(
                previous.packets,
                render_text_frame_draw_packet_diff_key_for(packet)) == nullptr) {
            render_text_atlas_upload_request_append_unique_id(diff.added_packet_ids, packet.packet_id);
        }
    }

    diff.removed_packet_ids.reserve(previous.packets.size());
    for (const render_text_frame_draw_packet_snapshot& packet : previous.packets) {
        if (render_text_frame_draw_plan_find_packet(
                current.packets,
                render_text_frame_draw_packet_diff_key_for(packet)) == nullptr) {
            render_text_atlas_upload_request_append_unique_id(diff.removed_packet_ids, packet.packet_id);
        }
    }

    diff.changed_packet_ids.reserve(current.packets.size());
    diff.readiness_changed_packet_ids.reserve(current.packets.size());
    diff.fallback_changed_packet_ids.reserve(current.packets.size());
    diff.page_revision_changed_packet_ids.reserve(current.packets.size());
    diff.blocker_changed_packet_ids.reserve(current.packets.size());
    diff.stable_glyph_changed_packet_ids.reserve(current.packets.size());
    diff.stable_style_changed_packet_ids.reserve(current.packets.size());
    diff.stable_run_changed_packet_ids.reserve(current.packets.size());
    diff.packet_diffs.reserve(previous.packets.size() + current.packets.size());
    for (const render_text_frame_draw_packet_snapshot& current_packet : current.packets) {
        const render_text_frame_draw_packet_snapshot* previous_packet =
            render_text_frame_draw_plan_find_packet(
                previous.packets,
                render_text_frame_draw_packet_diff_key_for(current_packet));
        if (previous_packet != nullptr) {
            if (!render_text_frame_draw_packets_equal(*previous_packet, current_packet)) {
                render_text_atlas_upload_request_append_unique_id(
                    diff.changed_packet_ids,
                    current_packet.packet_id);
            }
            if (previous_packet->status != current_packet.status
                || previous_packet->frame_ready_for_renderer != current_packet.frame_ready_for_renderer) {
                render_text_atlas_upload_request_append_unique_id(
                    diff.readiness_changed_packet_ids,
                    current_packet.packet_id);
            }
            if (previous_packet->fallback_incomplete != current_packet.fallback_incomplete
                || previous_packet->used_deterministic_fallback != current_packet.used_deterministic_fallback
                || previous_packet->used_real_backend != current_packet.used_real_backend) {
                render_text_atlas_upload_request_append_unique_id(
                    diff.fallback_changed_packet_ids,
                    current_packet.packet_id);
            }
            if (previous_packet->page_id == current_packet.page_id
                && previous_packet->page_revision != current_packet.page_revision) {
                render_text_atlas_upload_request_append_unique_id(
                    diff.page_revision_changed_packet_ids,
                    current_packet.packet_id);
            }
            render_text_frame_draw_packet_consumption_diff packet_diff =
                make_render_text_frame_draw_packet_consumption_diff(previous_packet, &current_packet);
            if (packet_diff.unchanged) {
                render_text_atlas_upload_request_append_unique_id(
                    diff.unchanged_packet_ids,
                    current_packet.packet_id);
            }
            if (packet_diff.readiness_regressed) {
                render_text_atlas_upload_request_append_unique_id(
                    diff.readiness_regressed_packet_ids,
                    current_packet.packet_id);
            }
            if (packet_diff.readiness_recovered) {
                render_text_atlas_upload_request_append_unique_id(
                    diff.readiness_recovered_packet_ids,
                    current_packet.packet_id);
            }
            if (packet_diff.blocker_changed) {
                render_text_atlas_upload_request_append_unique_id(
                    diff.blocker_changed_packet_ids,
                    current_packet.packet_id);
            }
            diff.packet_diffs.push_back(std::move(packet_diff));
        } else {
            diff.packet_diffs.push_back(
                make_render_text_frame_draw_packet_consumption_diff(nullptr, &current_packet));
        }

        const render_text_frame_draw_packet_snapshot* previous_slot_packet =
            render_text_frame_draw_plan_find_packet_by_slot(
                previous.packets,
                render_text_frame_draw_packet_slot_key_for(current_packet));
        if (previous_slot_packet == nullptr) {
            continue;
        }
        if (render_text_frame_draw_packet_glyph_key_for(*previous_slot_packet)
            != render_text_frame_draw_packet_glyph_key_for(current_packet)) {
            render_text_atlas_upload_request_append_unique_id(
                diff.stable_glyph_changed_packet_ids,
                current_packet.packet_id);
        }
        if (render_text_frame_draw_packet_style_key_for(*previous_slot_packet)
            != render_text_frame_draw_packet_style_key_for(current_packet)) {
            render_text_atlas_upload_request_append_unique_id(
                diff.stable_style_changed_packet_ids,
                current_packet.packet_id);
        }
        if (render_text_frame_draw_packet_run_key_for(*previous_slot_packet)
            != render_text_frame_draw_packet_run_key_for(current_packet)) {
            render_text_atlas_upload_request_append_unique_id(
                diff.stable_run_changed_packet_ids,
                current_packet.packet_id);
        }
    }
    for (const render_text_frame_draw_packet_snapshot& previous_packet : previous.packets) {
        if (render_text_frame_draw_plan_find_packet(
                current.packets,
                render_text_frame_draw_packet_diff_key_for(previous_packet)) == nullptr) {
            diff.packet_diffs.push_back(
                make_render_text_frame_draw_packet_consumption_diff(&previous_packet, nullptr));
        }
    }

    const std::vector<std::string> previous_glyph_keys =
        render_text_frame_draw_plan_stable_glyph_keys(previous);
    const std::vector<std::string> current_glyph_keys =
        render_text_frame_draw_plan_stable_glyph_keys(current);
    const std::vector<std::string> previous_style_keys =
        render_text_frame_draw_plan_stable_style_keys(previous);
    const std::vector<std::string> current_style_keys =
        render_text_frame_draw_plan_stable_style_keys(current);
    const std::vector<std::string> previous_run_keys =
        render_text_frame_draw_plan_stable_run_keys(previous);
    const std::vector<std::string> current_run_keys =
        render_text_frame_draw_plan_stable_run_keys(current);

    diff.policy = render_text_frame_draw_plan_diff_policy{
        .frame_status_changed = previous.frame_status != current.frame_status,
        .frame_readiness_changed = previous.frame_ready_for_renderer != current.frame_ready_for_renderer,
        .frame_readiness_regressed = previous.frame_ready_for_renderer && !current.frame_ready_for_renderer,
        .materialization_count_delta = render_text_frame_snapshot_count_delta(
            previous.policy.materialization_count,
            current.policy.materialization_count),
        .packet_count_delta = render_text_frame_snapshot_count_delta(
            previous.policy.packet_count,
            current.policy.packet_count),
        .draw_ready_count_delta = render_text_frame_snapshot_count_delta(
            previous.policy.draw_ready_count,
            current.policy.draw_ready_count),
        .glyph_quad_count_delta = render_text_frame_snapshot_count_delta(
            render_text_frame_draw_plan_glyph_quad_count(previous),
            render_text_frame_draw_plan_glyph_quad_count(current)),
        .skipped_count_delta = render_text_frame_snapshot_count_delta(
            previous.policy.skipped_count,
            current.policy.skipped_count),
        .fallback_incomplete_count_delta = render_text_frame_snapshot_count_delta(
            previous.policy.fallback_incomplete_count,
            current.policy.fallback_incomplete_count),
        .deterministic_fallback_count_delta = render_text_frame_snapshot_count_delta(
            previous.policy.deterministic_fallback_count,
            current.policy.deterministic_fallback_count),
        .real_backend_count_delta = render_text_frame_snapshot_count_delta(
            previous.policy.real_backend_count,
            current.policy.real_backend_count),
        .upload_consumed_count_delta = render_text_frame_snapshot_count_delta(
            previous.policy.upload_consumed_count,
            current.policy.upload_consumed_count),
        .stable_glyph_key_count_delta = render_text_frame_snapshot_count_delta(
            previous_glyph_keys.size(),
            current_glyph_keys.size()),
        .stable_style_key_count_delta = render_text_frame_snapshot_count_delta(
            previous_style_keys.size(),
            current_style_keys.size()),
        .stable_run_key_count_delta = render_text_frame_snapshot_count_delta(
            previous_run_keys.size(),
            current_run_keys.size()),
        .stable_glyph_changed_packet_count = diff.stable_glyph_changed_packet_ids.size(),
        .stable_style_changed_packet_count = diff.stable_style_changed_packet_ids.size(),
        .stable_run_changed_packet_count = diff.stable_run_changed_packet_ids.size(),
        .added_packet_count = diff.added_packet_ids.size(),
        .removed_packet_count = diff.removed_packet_ids.size(),
        .changed_packet_count = diff.changed_packet_ids.size(),
        .unchanged_packet_count = diff.unchanged_packet_ids.size(),
        .readiness_changed_packet_count = diff.readiness_changed_packet_ids.size(),
        .readiness_regression_count = diff.readiness_regressed_packet_ids.size(),
        .readiness_recovery_count = diff.readiness_recovered_packet_ids.size(),
        .fallback_changed_packet_count = diff.fallback_changed_packet_ids.size(),
        .page_revision_changed_packet_count = diff.page_revision_changed_packet_ids.size(),
    };
    for (const render_text_frame_draw_packet_consumption_diff& packet_diff : diff.packet_diffs) {
        if (packet_diff.line_run_changed) {
            ++diff.policy.line_run_changed_packet_count;
        }
        if (packet_diff.cluster_changed) {
            ++diff.policy.cluster_changed_packet_count;
        }
        if (packet_diff.cache_key_changed) {
            ++diff.policy.cache_key_changed_packet_count;
        }
        if (packet_diff.atlas_page_changed) {
            ++diff.policy.atlas_page_changed_packet_count;
        }
        if (packet_diff.upload_request_changed) {
            ++diff.policy.upload_request_changed_packet_count;
        }
        if (packet_diff.upload_generation_changed) {
            ++diff.policy.upload_generation_changed_packet_count;
        }
        if (packet_diff.glyph_quad_count_changed) {
            ++diff.policy.glyph_quad_count_changed_packet_count;
        }
        if (packet_diff.status_changed) {
            ++diff.policy.status_changed_packet_count;
        }
        if (packet_diff.blocker_changed) {
            ++diff.policy.blocker_changed_packet_count;
        }
        if (packet_diff.missing_atlas_upload) {
            ++diff.policy.missing_atlas_upload_blocker_count;
        }
        if (packet_diff.missing_materialization) {
            ++diff.policy.missing_materialization_blocker_count;
        }
        if (packet_diff.non_upload_ready_payload) {
            ++diff.policy.non_upload_ready_payload_blocker_count;
        }
        if (packet_diff.missing_glyph_quad_facts) {
            ++diff.policy.missing_glyph_quad_fact_blocker_count;
        }
        if (packet_diff.fallback_or_skipped) {
            ++diff.policy.fallback_or_skipped_packet_count;
        }
    }
    diff.policy.stable_no_change =
        !diff.policy.frame_status_changed
        && !diff.policy.frame_readiness_changed
        && diff.policy.materialization_count_delta == 0
        && diff.policy.packet_count_delta == 0
        && diff.policy.draw_ready_count_delta == 0
        && diff.policy.glyph_quad_count_delta == 0
        && diff.policy.skipped_count_delta == 0
        && diff.policy.fallback_incomplete_count_delta == 0
        && diff.policy.deterministic_fallback_count_delta == 0
        && diff.policy.real_backend_count_delta == 0
        && diff.policy.upload_consumed_count_delta == 0
        && diff.policy.stable_glyph_key_count_delta == 0
        && diff.policy.stable_style_key_count_delta == 0
        && diff.policy.stable_run_key_count_delta == 0
        && diff.policy.added_packet_count == 0
        && diff.policy.removed_packet_count == 0
        && diff.policy.changed_packet_count == 0
        && diff.policy.readiness_changed_packet_count == 0
        && diff.policy.fallback_changed_packet_count == 0
        && diff.policy.page_revision_changed_packet_count == 0
        && diff.policy.line_run_changed_packet_count == 0
        && diff.policy.cluster_changed_packet_count == 0
        && diff.policy.cache_key_changed_packet_count == 0
        && diff.policy.atlas_page_changed_packet_count == 0
        && diff.policy.upload_request_changed_packet_count == 0
        && diff.policy.upload_generation_changed_packet_count == 0
        && diff.policy.glyph_quad_count_changed_packet_count == 0
        && diff.policy.status_changed_packet_count == 0
        && diff.policy.blocker_changed_packet_count == 0;

    diff.diagnostic = diff.has_packet_changes()
            || diff.has_readiness_or_fallback_changes()
            || diff.has_page_revision_changes()
            || diff.has_stable_key_deltas()
            || !diff.policy.stable_no_change
        ? "text frame draw plan diff found renderer-facing glyph packet diagnostic changes"
        : "text frame draw plan diff is renderer-agnostic and unchanged";
    return diff;
}

} // namespace quiz_vulkan::render
