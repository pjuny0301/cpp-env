#include "app/app_image_texture_payloads.h"

#include "render/image/image_texture_frame_resource_packet_plan.h"
#include "render/image/image_texture_pipeline.h"
#include "render/vulkan/vulkan_renderer.h"

#include <optional>

namespace quiz_vulkan {
namespace {

bool upload_packet_matches_frame(
    const render::render_image_texture_upload_result_packet_snapshot& packet,
    const render::render_image_texture_frame_snapshot& frame)
{
    for (const render::render_image_texture_frame_entry_snapshot& entry : frame.entries) {
        if (!packet.stable_cache_key.empty() && packet.stable_cache_key == entry.stable_texture_cache_key) {
            return true;
        }
        if (packet.texture_id != 0 && packet.texture_id == entry.texture_id) {
            return true;
        }
    }
    return false;
}

void append_upload_result_packet(
    render::render_image_texture_upload_result_snapshot& snapshot,
    const render::render_image_texture_upload_result_packet_snapshot& packet)
{
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
        render::append_render_image_texture_upload_result_summary(
            snapshot.rejected_summary,
            packet.blocker_summary);
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
    if (packet.status == render::render_image_texture_upload_result_packet_status::rejected_invalid_mipmap_plan) {
        ++snapshot.invalid_mipmap_plan_packet_count;
    }
    if (packet.status == render::render_image_texture_upload_result_packet_status::rejected_missing_snapshot) {
        ++snapshot.missing_snapshot_packet_count;
    }
    if (packet.status == render::render_image_texture_upload_result_packet_status::rejected_missing_texture_handle) {
        ++snapshot.missing_texture_handle_packet_count;
    }

    snapshot.total_mip_level_count += packet.mip_level_count;
    snapshot.accepted_mip_level_count += packet.accepted_mip_level_count;
    snapshot.rejected_mip_level_count += packet.rejected_mip_level_count;
    snapshot.total_uploaded_byte_count += packet.uploaded_byte_count;
    snapshot.total_planned_staging_byte_count += packet.planned_staging_byte_count;
    snapshot.total_planned_mipmap_byte_count += packet.planned_mipmap_byte_count;
    if (packet.accepted) {
        render::append_render_image_texture_upload_result_summary(snapshot.accepted_summary, packet.stable_cache_key);
    }
    if (packet.accepted && packet.has_texture_handle) {
        render::append_render_image_texture_upload_result_summary(
            snapshot.texture_summary,
            packet.stable_cache_key + "#texture=" + std::to_string(packet.texture_id));
    }

    snapshot.packets.push_back(packet);
    snapshot.packet_count = snapshot.packets.size();
}

void finalize_upload_result_snapshot(render::render_image_texture_upload_result_snapshot& snapshot)
{
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
}

render::render_image_texture_upload_result_snapshot filter_upload_result_snapshot_to_frame(
    const render::render_image_texture_upload_result_snapshot& source,
    const render::render_image_texture_frame_snapshot& frame)
{
    render::render_image_texture_upload_result_snapshot filtered{
        .source_upload_count = source.source_upload_count,
        .operation_packet_count = source.operation_packet_count,
    };

    for (const render::render_image_texture_upload_result_packet_snapshot& packet : source.packets) {
        if (upload_packet_matches_frame(packet, frame)) {
            append_upload_result_packet(filtered, packet);
        }
    }

    finalize_upload_result_snapshot(filtered);
    return filtered;
}

std::optional<render::render_image_texture_upload_result_snapshot> upload_result_snapshot_for_pipeline(
    const render::image_texture_pipeline_interface& image_texture_pipeline,
    const render::render_image_texture_frame_snapshot& frame)
{
    const auto* fake_pipeline = dynamic_cast<const render::fake_image_texture_pipeline*>(&image_texture_pipeline);
    if (fake_pipeline != nullptr) {
        const render::render_image_texture_upload_result_snapshot upload_result =
            render::make_render_image_texture_upload_result_snapshot_from_fake_upload_snapshot(
                fake_pipeline->diagnostic_snapshot().upload_snapshot);
        return filter_upload_result_snapshot_to_frame(upload_result, frame);
    }

    const auto* standard_pipeline =
        dynamic_cast<const render::standard_image_texture_pipeline*>(&image_texture_pipeline);
    if (standard_pipeline != nullptr) {
        const render::render_image_texture_upload_result_snapshot upload_result =
            render::make_render_image_texture_upload_result_snapshot_from_fake_upload_snapshot(
                standard_pipeline->diagnostic_snapshot().upload_snapshot);
        return filter_upload_result_snapshot_to_frame(upload_result, frame);
    }

    return std::nullopt;
}

render::vulkan_renderer_image_texture_payload make_vulkan_renderer_image_texture_payload(
    const render::render_image_renderer_texture_quad_draw_payload& payload)
{
    return render::vulkan_renderer_image_texture_payload{
        .payload_index = payload.payload_index,
        .draw_command_index = payload.draw_command_index,
        .node_id = payload.node_id,
        .bounds = payload.bounds,
        .content_bounds = payload.content_bounds,
        .stable_texture_cache_key = payload.stable_texture_cache_key,
        .texture_id = payload.texture_id,
        .texture_revision = payload.texture_revision,
        .texture_width = payload.texture_width,
        .texture_height = payload.texture_height,
        .draw_ready = payload.draw_ready,
        .placeholder_backed = payload.placeholder_backed,
        .blocked = payload.blocked,
    };
}

render::vulkan_renderer_image_texture_payload_frame make_vulkan_renderer_image_texture_payload_frame(
    const render::render_image_renderer_texture_quad_draw_payload_frame& source)
{
    render::vulkan_renderer_image_texture_payload_frame frame{
        .payload_count = source.payload_count,
        .draw_ready_payload_count = source.draw_ready_payload_count,
        .placeholder_payload_count = source.placeholder_payload_count,
        .blocked_payload_count = source.blocked_payload_count,
        .draw_payloads_ready = source.draw_payloads_ready,
    };
    frame.payloads.reserve(source.payloads.size());
    for (const render::render_image_renderer_texture_quad_draw_payload& payload : source.payloads) {
        frame.payloads.push_back(make_vulkan_renderer_image_texture_payload(payload));
    }
    return frame;
}

} // namespace

app_image_texture_payload_report prepare_app_image_texture_payloads(
    const render::render_image_draw_list_frame_handoff_snapshot& handoff,
    const render::render_image_texture_batch_plan& plan,
    const render::render_image_texture_frame_snapshot& texture_frame,
    const render::image_texture_pipeline_interface& image_texture_pipeline,
    render::vulkan_renderer_image_texture_payload_frame* renderer_payloads)
{
    app_image_texture_payload_report report;
    const std::optional<render::render_image_texture_upload_result_snapshot> upload_result =
        upload_result_snapshot_for_pipeline(image_texture_pipeline, texture_frame);
    report.upload_result_available = upload_result.has_value();
    if (!upload_result.has_value()) {
        report.diagnostic = "image texture upload result diagnostics unavailable";
        return report;
    }

    const render::render_image_texture_frame_resource_packet_plan resource_packets =
        render::make_render_image_texture_frame_resource_packet_plan(texture_frame, *upload_result);
    const render::render_image_draw_list_texture_frame_composition composition =
        render::make_render_image_draw_list_texture_frame_composition(
            handoff,
            plan,
            texture_frame,
            resource_packets);
    const render::render_image_renderer_texture_quad_packet_summary quad_packets =
        render::make_render_image_renderer_texture_quad_packet_summary(composition);
    const render::render_image_renderer_texture_quad_draw_payload_frame draw_payloads =
        render::make_render_image_renderer_texture_quad_draw_payload_frame(quad_packets);
    if (renderer_payloads != nullptr) {
        *renderer_payloads = make_vulkan_renderer_image_texture_payload_frame(draw_payloads);
    }

    report.resource_packet_count = resource_packets.entries.size();
    report.resource_ready_count = resource_packets.resource_packet_count;
    report.quad_packet_count = quad_packets.packet_count;
    report.quad_ready_count = quad_packets.ready_packet_count;
    report.payload_count = draw_payloads.payload_count;
    report.payload_ready_count = draw_payloads.draw_ready_payload_count;
    report.payload_placeholder_count = draw_payloads.placeholder_payload_count;
    report.payload_blocked_count = draw_payloads.blocked_payload_count;
    report.resource_packets_ready = resource_packets.ok();
    report.quad_packets_ready = quad_packets.ok();
    report.draw_payloads_ready = draw_payloads.draw_payloads_ready;
    report.diagnostic = draw_payloads.draw_payloads_ready
        ? draw_payloads.diagnostic
        : (quad_packets.ok() ? draw_payloads.diagnostic : quad_packets.diagnostic);
    return report;
}

} // namespace quiz_vulkan
