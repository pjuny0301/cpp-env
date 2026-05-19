#include "render/text/text_frame_upload_handoff.h"

#include <cassert>
#include <cstdio>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

void require(const bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "Requirement failed: %s\n", message);
    }
    assert((condition) && message);
}

quiz_vulkan::render::glyph_atlas_key key_for(const std::uint32_t glyph_id)
{
    return quiz_vulkan::render::glyph_atlas_key{
        .face_id = 7,
        .glyph_id = glyph_id,
        .pixel_size = 20,
    };
}

quiz_vulkan::render::render_text_frame_resource_packet_materialization_entry ready_resource_packet(
    std::string resource_packet_id,
    const std::uint32_t glyph_id,
    const std::size_t packet_index)
{
    using namespace quiz_vulkan::render;

    const glyph_atlas_key cache_key = key_for(glyph_id);
    return render_text_frame_resource_packet_materialization_entry{
        .resource_packet_id = std::move(resource_packet_id),
        .stable_packet_key = "stable-glyph-" + std::to_string(glyph_id),
        .frame_id = "frame-1",
        .source_label = "text-draw-list-frame:v1:frame=frame-1:node=question-title",
        .draw_packet_id = "draw-" + std::to_string(packet_index),
        .upload_handoff_id = "handoff-" + std::to_string(packet_index),
        .upload_operation_id = "operation-" + std::to_string(packet_index),
        .upload_request_id = "upload-" + std::to_string(packet_index),
        .stable_page_id = "page:2",
        .sampler_key = render_text_frame_resource_packet_sampler_key_for(2, 5),
        .sampler_summary = render_text_frame_resource_packet_sampler_summary_for(2, 5, 64, 64),
        .packet_index = packet_index,
        .materialization_index = packet_index,
        .run_index = 1,
        .cluster_byte_offset = 4 + packet_index,
        .cluster_byte_count = 1,
        .cache_key = cache_key,
        .resolved_glyph_id = glyph_id,
        .resolved_face_id = cache_key.face_id,
        .page_id = 2,
        .page_revision = 5,
        .page_width = 64,
        .page_height = 64,
        .layout_bounds = render_rect{10.0f + static_cast<float>(packet_index), 20.0f, 8.0f, 9.0f},
        .atlas_bounds = render_rect{1.0f + static_cast<float>(packet_index), 2.0f, 8.0f, 9.0f},
        .update_bounds = render_rect{1.0f + static_cast<float>(packet_index), 2.0f, 8.0f, 9.0f},
        .uv_bounds = render_text_frame_draw_uv_rect{
            .u0 = (1.0f + static_cast<float>(packet_index)) / 64.0f,
            .v0 = 2.0f / 64.0f,
            .u1 = (9.0f + static_cast<float>(packet_index)) / 64.0f,
            .v1 = 11.0f / 64.0f,
            .valid = true,
        },
        .draw_status = render_text_frame_draw_packet_status::draw_ready,
        .handoff_status = render_text_frame_upload_handoff_packet_status::ready_uploaded,
        .upload_result_status = render_text_glyph_atlas_upload_result_status::accepted_upload,
        .status = render_text_frame_resource_packet_materialization_status::materialized_uploaded,
        .has_upload_handoff = true,
        .ready = true,
        .blocked = false,
        .renderer_boundary_ready = true,
        .uploaded = true,
        .used_deterministic_fallback = glyph_id == U'A',
        .used_real_backend = glyph_id == U'R',
        .glyph_supported = true,
        .upload_consumed = true,
        .upload_rgba_bytes = 256,
        .atlas_consumption = render_text_atlas_packet_consumption_evidence{
            .cluster_index = 3 + packet_index,
            .line_index = 2,
            .run_index = 1,
            .cluster_byte_offset = 4 + packet_index,
            .cluster_byte_count = 1,
            .cache_key = cache_key,
            .resolved_glyph_id = glyph_id,
            .resolved_face_id = cache_key.face_id,
            .pen_x = 10.0f + static_cast<float>(packet_index),
            .pen_y = 20.0f,
            .baseline = 29.0f,
            .upload_generation = 5,
            .used_fallback_glyph_id = glyph_id == U'?',
        },
        .diagnostic = "resource packet ready",
    };
}

quiz_vulkan::render::render_text_frame_resource_packet_materialization resources_for(
    std::vector<quiz_vulkan::render::render_text_frame_resource_packet_materialization_entry> entries,
    const bool frame_ready = true)
{
    using namespace quiz_vulkan::render;

    return render_text_frame_resource_packet_materialization{
        .frame_id = "frame-1",
        .source_label = "quad-packet-test",
        .frame_status = frame_ready ? render_text_frame_snapshot_status::ready
                                    : render_text_frame_snapshot_status::pending_atlas_updates,
        .frame_ready_for_renderer = frame_ready,
        .entries = std::move(entries),
        .diagnostic = "test resource packets",
    };
}

quiz_vulkan::render::render_text_renderer_glyph_quad_packet_snapshot make_quads(
    quiz_vulkan::render::render_text_frame_resource_packet_materialization resources)
{
    return quiz_vulkan::render::make_render_text_renderer_glyph_quad_packets(
        quiz_vulkan::render::render_text_renderer_glyph_quad_packet_request{
            .resource_packets = std::move(resources),
        });
}

quiz_vulkan::render::render_text_frame_draw_packet_snapshot draw_packet_for_resource(
    const quiz_vulkan::render::render_text_frame_resource_packet_materialization_entry& entry)
{
    using namespace quiz_vulkan::render;

    return render_text_frame_draw_packet_snapshot{
        .packet_id = entry.draw_packet_id,
        .frame_id = entry.frame_id,
        .source_label = entry.source_label,
        .atlas_upload_request_id = entry.upload_request_id,
        .status = entry.draw_status,
        .item_index = 0,
        .materialization_index = entry.materialization_index,
        .run_index = entry.run_index,
        .cluster_byte_offset = entry.cluster_byte_offset,
        .cluster_byte_count = entry.cluster_byte_count,
        .cache_key = entry.cache_key,
        .resolved_glyph_id = entry.resolved_glyph_id,
        .resolved_face_id = entry.resolved_face_id,
        .page_id = entry.page_id,
        .page_revision = entry.page_revision,
        .page_width = entry.page_width,
        .page_height = entry.page_height,
        .layout_bounds = entry.layout_bounds,
        .has_layout_bounds = entry.layout_bounds.width > 0.0f && entry.layout_bounds.height > 0.0f,
        .atlas_bounds = entry.atlas_bounds,
        .has_atlas_bounds = entry.atlas_bounds.width > 0.0f && entry.atlas_bounds.height > 0.0f,
        .uv_bounds = entry.uv_bounds,
        .frame_ready_for_renderer = entry.renderer_boundary_ready,
        .fallback_incomplete = entry.status
            == render_text_frame_resource_packet_materialization_status::blocked_draw_packet,
        .used_deterministic_fallback = entry.used_deterministic_fallback,
        .used_real_backend = entry.used_real_backend,
        .glyph_supported = entry.glyph_supported,
        .stable_cache_key = entry.cache_key.face_id != 0U
            && entry.cache_key.glyph_id != 0U
            && entry.cache_key.pixel_size != 0U,
        .upload_consumed = entry.upload_consumed,
        .atlas_consumption = entry.atlas_consumption,
        .diagnostic = entry.diagnostic,
    };
}

quiz_vulkan::render::render_text_frame_draw_plan_snapshot draw_plan_for_resources(
    const std::vector<quiz_vulkan::render::render_text_frame_resource_packet_materialization_entry>& entries,
    const bool frame_ready = true)
{
    using namespace quiz_vulkan::render;

    render_text_frame_draw_plan_snapshot draw_plan{
        .frame_id = "frame-1",
        .source_label = "quad-packet-test",
        .frame_status = frame_ready ? render_text_frame_snapshot_status::ready
                                    : render_text_frame_snapshot_status::pending_atlas_updates,
        .frame_ready_for_renderer = frame_ready,
        .diagnostic = "test draw plan",
    };
    for (const render_text_frame_resource_packet_materialization_entry& entry : entries) {
        append_render_text_frame_draw_packet(
            draw_plan.packets,
            draw_plan.policy,
            draw_packet_for_resource(entry));
    }
    return draw_plan;
}

quiz_vulkan::render::render_text_frame_snapshot frame_for_resources(
    const std::vector<quiz_vulkan::render::render_text_frame_resource_packet_materialization_entry>& entries,
    const bool frame_ready = true)
{
    using namespace quiz_vulkan::render;

    render_text_frame_snapshot frame{
        .status = frame_ready ? render_text_frame_snapshot_status::ready
                              : render_text_frame_snapshot_status::pending_atlas_updates,
        .frame_id = "frame-1",
        .source_label = "quad-packet-test",
        .diagnostic = "test frame",
    };
    for (const render_text_frame_resource_packet_materialization_entry& entry : entries) {
        frame.atlas_uploads.push_back(render_text_frame_atlas_upload_snapshot{
            .request_id = entry.upload_request_id,
            .status = render_text_atlas_upload_request_status::upload_ready,
            .materialization_index = entry.materialization_index,
            .cache_key = entry.cache_key,
            .resolved_glyph_id = entry.resolved_glyph_id,
            .resolved_face_id = entry.resolved_face_id,
            .page = render_text_atlas_page{
                .id = entry.page_id,
                .revision = entry.page_revision,
                .width = entry.page_width,
                .height = entry.page_height,
            },
            .updated_bounds = entry.update_bounds,
            .upload_rgba_bytes = entry.upload_rgba_bytes,
            .has_upload_request = true,
            .queued = true,
            .consumed = entry.upload_consumed,
            .stable_request_id = true,
        });
    }
    frame.policy.upload_request_count = frame.atlas_uploads.size();
    frame.policy.queued_upload_request_id_count = frame.atlas_uploads.size();
    frame.policy.consumed_upload_request_id_count = frame_ready ? frame.atlas_uploads.size() : 0U;
    frame.policy.total_upload_rgba_bytes = 0U;
    for (const auto& upload : frame.atlas_uploads) {
        frame.policy.total_upload_rgba_bytes += upload.upload_rgba_bytes;
        frame.queued_atlas_upload_request_ids.push_back(upload.request_id);
        if (upload.consumed) {
            frame.consumed_atlas_upload_request_ids.push_back(upload.request_id);
        }
    }
    return frame;
}

quiz_vulkan::render::render_text_render_frame_handoff_summary_snapshot summary_for_resources(
    std::vector<quiz_vulkan::render::render_text_frame_resource_packet_materialization_entry> entries,
    const bool frame_ready = true)
{
    using namespace quiz_vulkan::render;

    const render_text_frame_snapshot frame = frame_for_resources(entries, frame_ready);
    const render_text_frame_draw_plan_snapshot draw_plan = draw_plan_for_resources(entries, frame_ready);
    const render_text_renderer_glyph_quad_packet_snapshot quads =
        make_quads(resources_for(std::move(entries), frame_ready));
    return make_render_text_render_frame_handoff_summary(
        render_text_render_frame_handoff_summary_request{
            .frame = frame,
            .draw_plan = draw_plan,
            .glyph_quads = quads,
        });
}

void test_ready_resource_packets_become_glyph_quads()
{
    using namespace quiz_vulkan::render;

    const render_text_frame_resource_packet_materialization_entry entry =
        ready_resource_packet("resource-a", U'A', 0);
    const render_text_renderer_glyph_quad_packet_snapshot quads =
        make_quads(resources_for({entry}));

    require(quads.ok(), "ready resource packet produces ready glyph quad packet");
    require(quads.policy.resource_packet_count == 1U, "resource packet count is preserved");
    require(quads.policy.ready_quad_count == 1U, "ready glyph quad is counted");
    require(quads.policy.uploaded_quad_count == 1U, "uploaded glyph quad is counted");
    require(quads.policy.total_upload_rgba_bytes == 256U, "upload byte evidence is preserved");
    require(quads.policy.used_deterministic_fallback, "fallback flag is preserved");
    require(quads.packets.size() == 1U, "one glyph quad packet is emitted");

    const render_text_renderer_glyph_quad_packet_record& packet = quads.packets.front();
    require(packet.drawable(), "packet reports drawable");
    require(packet.status == render_text_renderer_glyph_quad_packet_status::quad_ready, "packet status is ready");
    require(packet.resource_packet_id == entry.resource_packet_id, "resource packet identity is preserved");
    require(packet.draw_packet_id == entry.draw_packet_id, "draw packet identity is preserved");
    require(packet.source_node_id_hint == "question-title", "node identity hint is derived from text handoff source label");
    require(packet.layout_bounds.x == entry.layout_bounds.x, "layout bounds x is preserved");
    require(packet.layout_bounds.height == entry.layout_bounds.height, "layout bounds height is preserved");
    require(
        packet.atlas_consumption.cluster_index == entry.atlas_consumption.cluster_index,
        "cluster identity is preserved");
    require(
        packet.atlas_consumption.line_index == entry.atlas_consumption.line_index,
        "line identity is preserved");
    require(packet.run_index == entry.run_index, "run identity is preserved");
    require(
        packet.atlas_consumption.pen_x == entry.atlas_consumption.pen_x
            && packet.atlas_consumption.pen_y == entry.atlas_consumption.pen_y,
        "pen position is preserved");
    require(
        packet.atlas_consumption.baseline == entry.atlas_consumption.baseline,
        "baseline is preserved");
    require(packet.uv_bounds.valid, "UV bounds stay valid");
    require(packet.uv_bounds.u0 == entry.uv_bounds.u0, "UV u0 is preserved");
    require(packet.uv_bounds.v1 == entry.uv_bounds.v1, "UV v1 is preserved");
    require(packet.page_id == entry.page_id && packet.page_revision == entry.page_revision, "page identity is preserved");
    require(
        packet.atlas_consumption.upload_generation
            == entry.atlas_consumption.upload_generation,
        "upload generation is preserved");
    require(packet.sampler_key == entry.sampler_key, "sampler key is preserved");
    require(!packet.atlas_consumption.missing_glyph, "missing glyph evidence is preserved as false");
    require(
        !packet.atlas_consumption.used_fallback_glyph_id,
        "fallback glyph evidence is preserved as false");
}

void test_blocked_resource_packet_stays_blocked()
{
    using namespace quiz_vulkan::render;

    render_text_frame_resource_packet_materialization_entry blocked =
        ready_resource_packet("resource-blocked", U'B', 0);
    blocked.status = render_text_frame_resource_packet_materialization_status::blocked_missing_upload_handoff;
    blocked.ready = false;
    blocked.blocked = true;
    blocked.renderer_boundary_ready = false;
    blocked.uploaded = false;
    blocked.upload_rgba_bytes = 0;
    blocked.blocker_summary = "draw packet has no ready upload handoff evidence";

    const render_text_renderer_glyph_quad_packet_snapshot quads =
        make_quads(resources_for({blocked}));

    require(!quads.ok(), "blocked resource packet keeps glyph quad snapshot blocked");
    require(quads.has_blockers(), "glyph quad snapshot reports blockers");
    require(quads.policy.resource_blocked_count == 1U, "blocked resource packet is counted");
    require(quads.policy.blocked_quad_count == 1U, "blocked quad is counted");
    require(
        quads.packets.front().status == render_text_renderer_glyph_quad_packet_status::blocked_resource_packet,
        "blocked resource status is preserved");
    require(
        quads.packets.front().blocker_summary == "draw packet has no ready upload handoff evidence",
        "resource blocker summary is preserved");
}

void test_glyph_quad_packet_order_is_deterministic()
{
    using namespace quiz_vulkan::render;

    const render_text_renderer_glyph_quad_packet_snapshot quads = make_quads(
        resources_for({
            ready_resource_packet("resource-b", U'B', 1),
            ready_resource_packet("resource-a", U'A', 0),
            ready_resource_packet("resource-r", U'R', 2),
        }));

    require(quads.packets.size() == 3U, "all resource packets produce glyph quad records");
    require(quads.packets[0].resource_packet_id == "resource-b", "first packet follows input order");
    require(quads.packets[1].resource_packet_id == "resource-a", "second packet follows input order");
    require(quads.packets[2].resource_packet_id == "resource-r", "third packet follows input order");
    require(quads.policy.used_real_backend, "real backend evidence is preserved across ordered packets");
}

void test_duplicate_and_missing_identities_are_diagnosed()
{
    using namespace quiz_vulkan::render;

    render_text_frame_resource_packet_materialization_entry duplicate_a =
        ready_resource_packet("resource-duplicate", U'A', 0);
    render_text_frame_resource_packet_materialization_entry duplicate_b =
        ready_resource_packet("resource-duplicate", U'B', 1);
    render_text_frame_resource_packet_materialization_entry missing_resource =
        ready_resource_packet("", U'C', 2);
    render_text_frame_resource_packet_materialization_entry missing_stable =
        ready_resource_packet("resource-missing-stable", U'D', 3);
    missing_stable.stable_packet_key = {};

    const render_text_renderer_glyph_quad_packet_snapshot quads = make_quads(
        resources_for({duplicate_a, duplicate_b, missing_resource, missing_stable}));

    require(!quads.ok(), "identity problems block glyph quad snapshot");
    require(quads.policy.duplicate_packet_id_count == 1U, "duplicate resource identity is counted");
    require(quads.policy.missing_identity_count == 1U, "missing resource identity is counted");
    require(quads.policy.missing_stable_packet_key_count == 1U, "missing stable key is counted");
    require(quads.duplicate_quad_packet_ids.size() == 1U, "duplicate quad identity is exposed");
    require(quads.missing_identity_quad_packet_ids.size() == 1U, "missing identity quad is exposed");
    require(
        quads.packets[1].status == render_text_renderer_glyph_quad_packet_status::duplicate_packet_id,
        "second duplicate packet is explicitly blocked");
    require(
        quads.packets[2].status
            == render_text_renderer_glyph_quad_packet_status::blocked_missing_resource_packet_id,
        "missing resource id is explicitly blocked");
    require(
        quads.packets[3].status
            == render_text_renderer_glyph_quad_packet_status::blocked_missing_stable_packet_key,
        "missing stable packet key is explicitly blocked");
}

void test_render_frame_handoff_summary_preserves_ready_quad_facts()
{
    using namespace quiz_vulkan::render;

    const render_text_frame_resource_packet_materialization_entry entry =
        ready_resource_packet("resource-ready-summary", U'R', 0);
    const render_text_render_frame_handoff_summary_snapshot summary =
        summary_for_resources({entry});

    require(summary.ok(), "ready render frame handoff summary is renderer-ready");
    require(summary.frame_id == "frame-1", "summary preserves frame id");
    require(summary.source_label == "quad-packet-test", "summary preserves source label");
    require(summary.policy.draw_packet_count == 1U, "summary counts draw packets");
    require(summary.policy.draw_ready_count == 1U, "summary counts ready draw packets");
    require(summary.policy.draw_blocked_count == 0U, "summary has no blocked draw packets");
    require(summary.policy.glyph_quad_count == 1U, "summary counts glyph quad packets");
    require(summary.policy.glyph_quad_ready_count == 1U, "summary counts ready glyph quads");
    require(summary.policy.atlas_page_count == 1U, "summary preserves atlas page id");
    require(summary.policy.upload_request_count == 1U, "summary preserves upload request id");
    require(summary.policy.total_upload_rgba_bytes == 256U, "summary preserves upload byte count");
    require(summary.policy.used_real_backend, "summary preserves real backend evidence");
    require(!summary.policy.has_blockers, "ready summary has no blockers");
    require(summary.ready_draw_packet_ids.size() == 1U, "ready draw packet id is exposed");
    require(summary.ready_glyph_quad_packet_ids.size() == 1U, "ready glyph quad id is exposed");
    require(!summary.atlas_page_ids.empty(), "summary exposes atlas page ids");
    require(!summary.upload_request_ids.empty(), "summary exposes upload request ids");
}

void test_render_frame_handoff_summary_reports_missing_quad_and_fallback_blockers()
{
    using namespace quiz_vulkan::render;

    render_text_frame_resource_packet_materialization_entry entry =
        ready_resource_packet("resource-blocked-summary", U'A', 0);
    render_text_frame_draw_plan_snapshot draw_plan = draw_plan_for_resources({entry});
    render_text_frame_snapshot frame = frame_for_resources({entry});
    render_text_renderer_glyph_quad_packet_snapshot no_quads{
        .frame_id = frame.frame_id,
        .source_label = frame.source_label,
        .frame_ready_for_renderer = true,
    };
    render_text_render_frame_handoff_summary_snapshot missing_quad =
        make_render_text_render_frame_handoff_summary(
            render_text_render_frame_handoff_summary_request{
                .frame = frame,
                .draw_plan = draw_plan,
                .glyph_quads = no_quads,
            });

    require(!missing_quad.ok(), "missing glyph quad blocks render frame summary");
    require(
        missing_quad.policy.missing_glyph_quad_packet_count == 1U,
        "summary counts missing glyph quad packet");
    require(
        missing_quad.missing_glyph_quad_draw_packet_ids.size() == 1U,
        "summary exposes draw packet missing a glyph quad");

    draw_plan.packets.front().status = render_text_frame_draw_packet_status::skipped_materialization;
    draw_plan.packets.front().upload_consumed = false;
    draw_plan.packets.front().used_deterministic_fallback = true;
    draw_plan.packets.front().diagnostic = "deterministic fallback payload was not upload-ready";
    render_text_render_frame_handoff_summary_snapshot skipped =
        make_render_text_render_frame_handoff_summary(
            render_text_render_frame_handoff_summary_request{
                .frame = frame,
                .draw_plan = draw_plan,
                .glyph_quads = no_quads,
            });

    require(!skipped.ok(), "skipped fallback packet blocks render frame summary");
    require(
        skipped.policy.missing_atlas_upload_blocker_count == 1U,
        "summary counts missing atlas upload evidence");
    require(
        skipped.policy.missing_materialization_blocker_count == 1U,
        "summary counts missing materialization blocker");
    require(
        skipped.policy.non_upload_ready_atlas_payload_blocker_count == 1U,
        "summary counts non-upload-ready atlas payload blocker");
    require(
        skipped.policy.skipped_fallback_packet_count == 1U,
        "summary counts skipped deterministic fallback packet");
    require(skipped.has_blockers(), "blocked summary exposes blocker helper");
}

void test_render_frame_handoff_summary_carries_diff_packet_ids()
{
    using namespace quiz_vulkan::render;

    const render_text_frame_resource_packet_materialization_entry before_entry =
        ready_resource_packet("resource-before-summary", U'A', 0);
    const render_text_frame_resource_packet_materialization_entry after_entry =
        ready_resource_packet("resource-after-summary", U'R', 1);
    const render_text_frame_draw_plan_snapshot before_draw =
        draw_plan_for_resources({before_entry});
    const render_text_frame_draw_plan_snapshot after_draw =
        draw_plan_for_resources({after_entry});
    const render_text_renderer_glyph_quad_packet_snapshot before_quads =
        make_quads(resources_for({before_entry}));
    const render_text_renderer_glyph_quad_packet_snapshot after_quads =
        make_quads(resources_for({after_entry}));
    const render_text_render_frame_handoff_summary_snapshot summary =
        make_render_text_render_frame_handoff_summary(
            render_text_render_frame_handoff_summary_request{
                .frame = frame_for_resources({after_entry}),
                .draw_plan = after_draw,
                .glyph_quads = after_quads,
                .draw_packet_diff = diff_render_text_frame_draw_plans(before_draw, after_draw),
                .has_draw_packet_diff = true,
                .glyph_quad_diff = diff_render_text_renderer_glyph_quad_packet_snapshots(
                    before_quads,
                    after_quads),
                .has_glyph_quad_diff = true,
            });

    require(summary.policy.added_packet_count >= 1U, "summary carries added packet diff count");
    require(summary.policy.removed_packet_count >= 1U, "summary carries removed packet diff count");
    require(!summary.added_packet_ids.empty(), "summary exposes added packet ids");
    require(!summary.removed_packet_ids.empty(), "summary exposes removed packet ids");
    require(!summary.policy.stable_no_change, "summary records changed frame handoff");
}

void test_glyph_quad_diff_reports_stable_no_change()
{
    using namespace quiz_vulkan::render;

    const render_text_renderer_glyph_quad_packet_snapshot quads =
        make_quads(resources_for({ready_resource_packet("resource-stable", U'A', 0)}));
    const render_text_renderer_glyph_quad_packet_diff_snapshot diff =
        diff_render_text_renderer_glyph_quad_packet_snapshots(quads, quads);

    require(diff.stable_no_change(), "identical glyph quad snapshots report stable no-change");
    require(!diff.has_changes(), "identical glyph quad snapshots have no changes");
    require(diff.policy.unchanged_packet_count == 1U, "unchanged packet is counted");
    require(diff.policy.added_packet_count == 0U, "no-op diff has no added packets");
    require(diff.policy.removed_packet_count == 0U, "no-op diff has no removed packets");
    require(diff.policy.changed_packet_count == 0U, "no-op diff has no changed packets");
    require(diff.unchanged_quad_packet_ids.size() == 1U, "unchanged packet id is exposed");
    require(!diff.summary.empty(), "no-op diff carries summary text");
}

void test_glyph_quad_diff_classifies_ready_blocked_transitions()
{
    using namespace quiz_vulkan::render;

    render_text_frame_resource_packet_materialization_entry ready =
        ready_resource_packet("resource-transition", U'A', 0);
    render_text_frame_resource_packet_materialization_entry blocked = ready;
    blocked.status = render_text_frame_resource_packet_materialization_status::blocked_missing_upload_handoff;
    blocked.ready = false;
    blocked.blocked = true;
    blocked.renderer_boundary_ready = false;
    blocked.uploaded = false;
    blocked.upload_rgba_bytes = 0;
    blocked.blocker_summary = "draw packet has no ready upload handoff evidence";

    const render_text_renderer_glyph_quad_packet_snapshot ready_quads =
        make_quads(resources_for({ready}));
    const render_text_renderer_glyph_quad_packet_snapshot blocked_quads =
        make_quads(resources_for({blocked}));
    const render_text_renderer_glyph_quad_packet_diff_snapshot regression =
        diff_render_text_renderer_glyph_quad_packet_snapshots(ready_quads, blocked_quads);
    const render_text_renderer_glyph_quad_packet_diff_snapshot recovery =
        diff_render_text_renderer_glyph_quad_packet_snapshots(blocked_quads, ready_quads);

    require(regression.has_changes(), "ready-to-blocked diff reports a change");
    require(regression.policy.readiness_regression_count == 1U, "ready-to-blocked transition is a regression");
    require(regression.policy.readiness_changed_count == 1U, "readiness change is counted");
    require(regression.readiness_regressed_quad_packet_ids.size() == 1U, "regressed packet id is exposed");
    require(regression.packet_diffs.front().readiness_regressed, "packet diff marks readiness regression");
    require(recovery.policy.readiness_recovery_count == 1U, "blocked-to-ready transition is a recovery");
    require(recovery.readiness_recovered_quad_packet_ids.size() == 1U, "recovered packet id is exposed");
    require(recovery.packet_diffs.front().readiness_recovered, "packet diff marks readiness recovery");
}

void test_glyph_quad_diff_counts_layout_uv_page_sampler_and_upload_changes()
{
    using namespace quiz_vulkan::render;

    const render_text_renderer_glyph_quad_packet_snapshot before =
        make_quads(resources_for({ready_resource_packet("resource-change", U'A', 0)}));

    render_text_frame_resource_packet_materialization_entry changed =
        ready_resource_packet("resource-change", U'A', 0);
    changed.layout_bounds.x += 2.0f;
    changed.atlas_bounds.y += 3.0f;
    changed.uv_bounds.u1 += 0.125f;
    changed.resolved_glyph_id += 1;
    changed.cache_key.glyph_id += 1;
    changed.atlas_consumption.resolved_glyph_id += 1;
    changed.atlas_consumption.cache_key.glyph_id += 1;
    changed.atlas_consumption.cluster_index += 1;
    changed.atlas_consumption.line_index += 1;
    changed.run_index += 1;
    changed.atlas_consumption.run_index += 1;
    changed.atlas_consumption.pen_x += 4.0f;
    changed.atlas_consumption.pen_y += 5.0f;
    changed.atlas_consumption.baseline += 6.0f;
    changed.page_revision += 1;
    changed.atlas_consumption.upload_generation += 1;
    changed.sampler_key = render_text_frame_resource_packet_sampler_key_for(2, changed.page_revision);
    changed.upload_request_id = "upload-changed";
    changed.upload_operation_id = "operation-changed";
    changed.upload_rgba_bytes = 512;
    changed.clean_reuse = true;
    changed.atlas_consumption.clean_reuse = true;
    changed.atlas_consumption.used_fallback_glyph_id = true;
    changed.atlas_consumption.missing_glyph = true;
    const render_text_renderer_glyph_quad_packet_snapshot after =
        make_quads(resources_for({changed}));

    const render_text_renderer_glyph_quad_packet_diff_snapshot diff =
        diff_render_text_renderer_glyph_quad_packet_snapshots(before, after);

    require(diff.policy.changed_packet_count == 1U, "field changes produce one changed packet");
    require(diff.policy.layout_bounds_changed_count == 1U, "layout bounds changes are counted");
    require(diff.policy.atlas_bounds_changed_count == 1U, "atlas bounds changes are counted");
    require(diff.policy.uv_bounds_changed_count == 1U, "UV changes are counted");
    require(
        diff.policy.atlas_consumption_diff.glyph_identity_changed_count == 1U,
        "glyph identity changes are counted");
    require(
        diff.policy.atlas_consumption_diff.line_run_changed_count == 1U,
        "line/run changes are counted");
    require(
        diff.policy.atlas_consumption_diff.pen_position_changed_count == 1U,
        "pen position changes are counted");
    require(
        diff.policy.atlas_consumption_diff.baseline_changed_count == 1U,
        "baseline changes are counted");
    require(diff.policy.page_revision_changed_count == 1U, "page revision changes are counted");
    require(
        diff.policy.atlas_consumption_diff.upload_generation_changed_count == 1U,
        "upload generation changes are counted");
    require(diff.policy.sampler_key_changed_count == 1U, "sampler changes are counted");
    require(diff.policy.upload_request_id_changed_count == 1U, "upload request changes are counted");
    require(diff.policy.upload_operation_id_changed_count == 1U, "upload operation changes are counted");
    require(diff.policy.uploaded_byte_count_changed_count == 1U, "upload byte changes are counted");
    require(
        diff.policy.atlas_consumption_diff.reuse_status_changed_count == 1U,
        "clean-reuse status changes are counted");
    require(
        diff.policy.atlas_consumption_diff.fallback_glyph_changed_count == 1U,
        "fallback glyph changes are counted");
    require(
        diff.policy.atlas_consumption_diff.missing_glyph_changed_count == 1U,
        "missing glyph changes are counted");
    require(diff.packet_diffs.front().upload_rgba_bytes_delta == 256, "upload byte delta is preserved");
}

void test_glyph_quad_diff_reports_added_removed_duplicate_and_missing_identity()
{
    using namespace quiz_vulkan::render;

    const render_text_renderer_glyph_quad_packet_snapshot before =
        make_quads(resources_for({ready_resource_packet("resource-removed", U'A', 0)}));

    render_text_renderer_glyph_quad_packet_snapshot after =
        make_quads(resources_for({
            ready_resource_packet("resource-added", U'B', 1),
            ready_resource_packet("resource-added", U'C', 2),
            ready_resource_packet("resource-missing", U'D', 3),
        }));
    after.packets.back().quad_packet_id = {};

    const render_text_renderer_glyph_quad_packet_diff_snapshot diff =
        diff_render_text_renderer_glyph_quad_packet_snapshots(before, after);

    require(diff.has_changes(), "added/removed/identity changes are reported");
    require(diff.policy.added_packet_count == 3U, "added packet records are counted");
    require(diff.policy.removed_packet_count == 1U, "removed packet records are counted");
    require(diff.policy.duplicate_identity_count == 2U, "duplicate current identities are counted deterministically");
    require(diff.policy.missing_identity_count == 1U, "missing current identity is counted");
    require(diff.removed_quad_packet_ids.size() == 1U, "removed packet id is exposed");
    require(diff.added_quad_packet_ids.size() == 1U, "added packet ids are de-duplicated");
    require(diff.duplicate_identity_quad_packet_ids.size() == 1U, "duplicate identity id is exposed once");
}

} // namespace

int main()
{
    test_ready_resource_packets_become_glyph_quads();
    test_blocked_resource_packet_stays_blocked();
    test_glyph_quad_packet_order_is_deterministic();
    test_duplicate_and_missing_identities_are_diagnosed();
    test_render_frame_handoff_summary_preserves_ready_quad_facts();
    test_render_frame_handoff_summary_reports_missing_quad_and_fallback_blockers();
    test_render_frame_handoff_summary_carries_diff_packet_ids();
    test_glyph_quad_diff_reports_stable_no_change();
    test_glyph_quad_diff_classifies_ready_blocked_transitions();
    test_glyph_quad_diff_counts_layout_uv_page_sampler_and_upload_changes();
    test_glyph_quad_diff_reports_added_removed_duplicate_and_missing_identity();
    return 0;
}
