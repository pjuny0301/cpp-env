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

quiz_vulkan::render::render_text_frame_draw_packet_snapshot ready_draw_packet(
    std::string packet_id,
    const std::uint32_t glyph_id,
    const std::size_t materialization_index)
{
    using namespace quiz_vulkan::render;

    const glyph_atlas_key cache_key = key_for(glyph_id);
    return render_text_frame_draw_packet_snapshot{
        .packet_id = std::move(packet_id),
        .frame_id = "frame-1",
        .source_label = "text-resource-test",
        .atlas_upload_request_id = "upload-" + std::to_string(materialization_index),
        .status = render_text_frame_draw_packet_status::draw_ready,
        .item_index = 0,
        .materialization_index = materialization_index,
        .run_index = 1,
        .requested_style_token = "body",
        .resolved_style_id = "body",
        .cluster_byte_offset = 4 + materialization_index,
        .cluster_byte_count = 1,
        .cache_key = cache_key,
        .resolved_glyph_id = glyph_id,
        .resolved_face_id = cache_key.face_id,
        .page_id = 2,
        .page_revision = 5,
        .page_width = 64,
        .page_height = 64,
        .layout_bounds = render_rect{10.0f + static_cast<float>(materialization_index), 20.0f, 8.0f, 8.0f},
        .has_layout_bounds = true,
        .atlas_bounds = render_rect{1.0f + static_cast<float>(materialization_index), 2.0f, 8.0f, 8.0f},
        .has_atlas_bounds = true,
        .uv_bounds = render_text_frame_draw_uv_rect{.u0 = 0.015625f, .v0 = 0.03125f, .u1 = 0.140625f, .v1 = 0.15625f, .valid = true},
        .frame_ready_for_renderer = true,
        .used_deterministic_fallback = glyph_id == U'A',
        .used_real_backend = glyph_id == U'R',
        .glyph_supported = true,
        .stable_cache_key = true,
        .upload_consumed = true,
        .diagnostic = "draw packet ready",
    };
}

quiz_vulkan::render::render_text_frame_upload_handoff_packet_snapshot
handoff_packet_for_draw(
    const quiz_vulkan::render::render_text_frame_draw_packet_snapshot& draw_packet,
    const quiz_vulkan::render::render_text_frame_upload_handoff_packet_status status,
    std::string blocker_reason = {})
{
    using namespace quiz_vulkan::render;

    const bool ready = status == render_text_frame_upload_handoff_packet_status::ready_uploaded
        || status == render_text_frame_upload_handoff_packet_status::ready_clean_reuse;
    const bool uploaded = status == render_text_frame_upload_handoff_packet_status::ready_uploaded;
    const bool clean_reuse = status == render_text_frame_upload_handoff_packet_status::ready_clean_reuse;
    const bool rejected = status == render_text_frame_upload_handoff_packet_status::blocked_upload_rejected;
    return render_text_frame_upload_handoff_packet_snapshot{
        .handoff_id = "handoff-" + draw_packet.packet_id,
        .stable_packet_key = "stable-" + draw_packet.packet_id,
        .frame_id = draw_packet.frame_id,
        .source_label = draw_packet.source_label,
        .draw_packet_id = draw_packet.packet_id,
        .upload_operation_id = "operation-" + draw_packet.packet_id,
        .upload_request_id = draw_packet.atlas_upload_request_id,
        .stable_page_id = "page:" + std::to_string(draw_packet.page_id),
        .materialization_index = draw_packet.materialization_index,
        .run_index = draw_packet.run_index,
        .cluster_byte_offset = draw_packet.cluster_byte_offset,
        .cluster_byte_count = draw_packet.cluster_byte_count,
        .cache_key = draw_packet.cache_key,
        .resolved_glyph_id = draw_packet.resolved_glyph_id,
        .resolved_face_id = draw_packet.resolved_face_id,
        .page_id = draw_packet.page_id,
        .page_revision = draw_packet.page_revision,
        .layout_bounds = draw_packet.layout_bounds,
        .atlas_bounds = draw_packet.atlas_bounds,
        .update_bounds = draw_packet.atlas_bounds,
        .draw_status = draw_packet.status,
        .upload_result_status = clean_reuse
            ? render_text_glyph_atlas_upload_result_status::accepted_clean_reuse
            : (rejected ? render_text_glyph_atlas_upload_result_status::rejected_missing_upload_request_id
                        : render_text_glyph_atlas_upload_result_status::accepted_upload),
        .handoff_status = status,
        .has_upload_result = true,
        .requested = true,
        .ready = ready,
        .blocked = !ready,
        .drawable = draw_packet.drawable(),
        .uploaded = uploaded,
        .clean_reuse = clean_reuse,
        .used_deterministic_fallback = draw_packet.used_deterministic_fallback,
        .used_real_backend = draw_packet.used_real_backend,
        .glyph_supported = draw_packet.glyph_supported,
        .upload_consumed = draw_packet.upload_consumed,
        .upload_rgba_bytes = uploaded ? 256U : 0U,
        .blocker_reason = std::move(blocker_reason),
        .diagnostic = ready ? "handoff ready" : "handoff blocked",
    };
}

quiz_vulkan::render::render_text_frame_draw_plan_snapshot draw_plan_for(
    std::vector<quiz_vulkan::render::render_text_frame_draw_packet_snapshot> packets,
    const bool frame_ready = true)
{
    using namespace quiz_vulkan::render;

    render_text_frame_draw_plan_snapshot plan{
        .frame_id = "frame-1",
        .source_label = "text-resource-test",
        .frame_status = frame_ready ? render_text_frame_snapshot_status::ready
                                    : render_text_frame_snapshot_status::pending_atlas_updates,
        .frame_ready_for_renderer = frame_ready,
        .packets = std::move(packets),
        .diagnostic = "test draw plan",
    };
    plan.policy.packet_count = plan.packets.size();
    for (const auto& packet : plan.packets) {
        if (packet.status == render_text_frame_draw_packet_status::draw_ready) {
            ++plan.policy.draw_ready_count;
        }
    }
    return plan;
}

quiz_vulkan::render::render_text_frame_upload_handoff_snapshot handoff_for(
    std::vector<quiz_vulkan::render::render_text_frame_upload_handoff_packet_snapshot> packets,
    const bool frame_ready = true)
{
    using namespace quiz_vulkan::render;

    render_text_frame_upload_handoff_snapshot handoff{
        .frame_id = "frame-1",
        .source_label = "text-resource-test",
        .frame_status = frame_ready ? render_text_frame_snapshot_status::ready
                                    : render_text_frame_snapshot_status::pending_atlas_updates,
        .packets = std::move(packets),
        .diagnostic = "test upload handoff",
    };
    handoff.policy.frame_ready_for_renderer = frame_ready;
    for (const auto& packet : handoff.packets) {
        if (packet.ready) {
            ++handoff.policy.ready_glyph_packet_count;
        }
        if (packet.blocked) {
            ++handoff.policy.blocked_glyph_packet_count;
            handoff.policy.has_blockers = true;
        }
    }
    return handoff;
}

quiz_vulkan::render::render_text_frame_resource_packet_materialization materialize(
    const quiz_vulkan::render::render_text_frame_draw_plan_snapshot& draw_plan,
    const quiz_vulkan::render::render_text_frame_upload_handoff_snapshot& handoff)
{
    return quiz_vulkan::render::materialize_render_text_frame_resource_packets(
        quiz_vulkan::render::render_text_frame_resource_packet_materialization_request{
            .draw_plan = draw_plan,
            .upload_handoff = handoff,
        });
}

void test_resource_packets_materialize_upload_and_clean_reuse()
{
    using namespace quiz_vulkan::render;

    const render_text_frame_draw_packet_snapshot uploaded_draw =
        ready_draw_packet("draw-uploaded", U'A', 0);
    const render_text_frame_draw_packet_snapshot clean_draw =
        ready_draw_packet("draw-clean", U'C', 1);
    const render_text_frame_draw_packet_snapshot real_draw =
        ready_draw_packet("draw-real", U'R', 2);
    const render_text_frame_draw_plan_snapshot draw_plan =
        draw_plan_for({uploaded_draw, clean_draw, real_draw});
    const render_text_frame_upload_handoff_snapshot handoff = handoff_for({
        handoff_packet_for_draw(
            uploaded_draw,
            render_text_frame_upload_handoff_packet_status::ready_uploaded),
        handoff_packet_for_draw(
            clean_draw,
            render_text_frame_upload_handoff_packet_status::ready_clean_reuse),
        handoff_packet_for_draw(
            real_draw,
            render_text_frame_upload_handoff_packet_status::ready_uploaded),
    });

    const render_text_frame_resource_packet_materialization packets =
        materialize(draw_plan, handoff);
    const render_text_frame_resource_packet_materialization_entry& uploaded_entry =
        packets.entries.front();
    const render_text_frame_resource_packet_materialization_entry& clean_entry =
        packets.entries[1];
    const render_text_frame_resource_packet_materialization_entry& real_entry =
        packets.entries[2];

    require(packets.ok(), "uploaded and clean-reuse resource packets are renderer-boundary ready");
    require(packets.entries.size() == 3U, "materialization preserves each draw packet");
    require(packets.policy.ready_packet_count == 3U, "all packets are counted ready");
    require(packets.policy.uploaded_packet_count == 2U, "uploaded payload packets are counted");
    require(packets.policy.clean_reuse_packet_count == 1U, "clean reuse packet is counted");
    require(packets.policy.total_upload_rgba_bytes == 512U, "uploaded bytes total only upload payloads");
    require(packets.policy.used_deterministic_fallback, "deterministic fallback flag is preserved");
    require(packets.policy.used_real_backend, "real backend flag is preserved");
    require(
        uploaded_entry.frame_id == uploaded_draw.frame_id,
        "resource packet preserves frame identity");
    require(
        uploaded_entry.resource_packet_id
            == render_text_frame_resource_packet_id_for(uploaded_draw.frame_id, uploaded_draw.packet_id),
        "resource packet id is stable from frame and draw packet identity");
    require(uploaded_entry.draw_packet_id == uploaded_draw.packet_id, "draw packet id is preserved");
    require(
        uploaded_entry.upload_request_id == uploaded_draw.atlas_upload_request_id,
        "upload request id is preserved");
    require(uploaded_entry.cache_key.face_id == uploaded_draw.cache_key.face_id, "cache key face is preserved");
    require(uploaded_entry.cache_key.glyph_id == uploaded_draw.cache_key.glyph_id, "cache key glyph is preserved");
    require(
        uploaded_entry.cache_key.pixel_size == uploaded_draw.cache_key.pixel_size,
        "cache key pixel size is preserved");
    require(uploaded_entry.page_id == uploaded_draw.page_id, "atlas page id is preserved");
    require(uploaded_entry.page_revision == uploaded_draw.page_revision, "atlas page revision is preserved");
    require(uploaded_entry.page_width == uploaded_draw.page_width, "atlas page width is preserved");
    require(uploaded_entry.page_height == uploaded_draw.page_height, "atlas page height is preserved");
    require(
        uploaded_entry.layout_bounds.x == uploaded_draw.layout_bounds.x
            && uploaded_entry.layout_bounds.y == uploaded_draw.layout_bounds.y
            && uploaded_entry.layout_bounds.width == uploaded_draw.layout_bounds.width
            && uploaded_entry.layout_bounds.height == uploaded_draw.layout_bounds.height,
        "layout bounds are preserved");
    require(!uploaded_entry.sampler_key.empty(), "resource packet exposes a text atlas sampler key");
    require(!uploaded_entry.sampler_summary.empty(), "resource packet exposes a text atlas sampler summary");
    require(uploaded_entry.uv_bounds.valid, "resource packet preserves UV bounds");
    require(uploaded_entry.renderer_boundary_ready, "ready upload packet is renderer-boundary ready");
    require(uploaded_entry.uploaded, "uploaded packet records upload readiness");
    require(uploaded_entry.upload_rgba_bytes == 256U, "uploaded packet preserves byte count");
    require(uploaded_entry.used_deterministic_fallback, "uploaded packet preserves deterministic fallback flag");
    require(
        uploaded_entry.status
            == render_text_frame_resource_packet_materialization_status::materialized_uploaded,
        "upload packet reports uploaded materialization status");
    require(
        clean_entry.status
            == render_text_frame_resource_packet_materialization_status::materialized_clean_reuse,
        "clean packet reports clean reuse materialization status");
    require(clean_entry.clean_reuse, "clean packet records reuse readiness");
    require(clean_entry.upload_rgba_bytes == 0U, "clean reuse packet does not claim upload bytes");
    require(real_entry.used_real_backend, "real-backend flag is preserved on resource packets");
    require(packets.pages.size() == 1U, "page summary groups packets by atlas page");
    require(packets.pages.front().ready_packet_count == 3U, "page summary counts ready packets");
    require(packets.policy.sampler_count == 1U, "policy counts referenced text atlas sampler");
}

void test_resource_packets_report_missing_upload_handoff_and_rejected_upload()
{
    using namespace quiz_vulkan::render;

    const render_text_frame_draw_packet_snapshot draw =
        ready_draw_packet("draw-missing-upload", U'A', 0);
    const render_text_frame_resource_packet_materialization missing =
        materialize(draw_plan_for({draw}), handoff_for({}));

    require(!missing.ok(), "missing upload handoff blocks resource materialization");
    require(missing.policy.missing_upload_handoff_count == 1U, "missing handoff is counted");
    require(
        missing.entries.front().status
            == render_text_frame_resource_packet_materialization_status::blocked_missing_upload_handoff,
        "missing upload handoff status is explicit");
    require(!missing.entries.front().blocker_summary.empty(), "missing handoff carries blocker summary");

    const render_text_frame_draw_packet_snapshot rejected_draw =
        ready_draw_packet("draw-rejected", U'B', 1);
    const render_text_frame_resource_packet_materialization rejected =
        materialize(
            draw_plan_for({rejected_draw}),
            handoff_for({handoff_packet_for_draw(
                rejected_draw,
                render_text_frame_upload_handoff_packet_status::blocked_upload_rejected,
                "upload request id missing")}));

    require(!rejected.ok(), "rejected upload blocks resource materialization");
    require(rejected.policy.rejected_upload_count == 1U, "rejected upload is counted");
    require(
        rejected.entries.front().status
            == render_text_frame_resource_packet_materialization_status::blocked_upload_rejected,
        "rejected upload status is explicit");
    require(rejected.entries.front().upload_rejected, "entry preserves rejected upload flag");
}

void test_resource_packets_report_draw_and_page_blockers()
{
    using namespace quiz_vulkan::render;

    render_text_frame_draw_packet_snapshot frame_not_ready =
        ready_draw_packet("draw-pending-frame", U'A', 0);
    frame_not_ready.status = render_text_frame_draw_packet_status::frame_not_ready;
    frame_not_ready.frame_ready_for_renderer = false;
    render_text_frame_resource_packet_materialization pending =
        materialize(
            draw_plan_for({frame_not_ready}, false),
            handoff_for({handoff_packet_for_draw(
                frame_not_ready,
                render_text_frame_upload_handoff_packet_status::ready_uploaded)},
                false));
    require(!pending.ok(), "frame-not-ready draw packet blocks resource materialization");
    require(pending.policy.frame_not_ready_count == 1U, "frame-not-ready blocker is counted");
    require(
        pending.entries.front().status
            == render_text_frame_resource_packet_materialization_status::blocked_frame_not_ready,
        "frame-not-ready status is explicit");

    render_text_frame_draw_packet_snapshot missing_page =
        ready_draw_packet("draw-missing-page", U'B', 1);
    missing_page.page_id = 0;
    render_text_frame_resource_packet_materialization missing_page_packets =
        materialize(
            draw_plan_for({missing_page}),
            handoff_for({handoff_packet_for_draw(
                missing_page,
                render_text_frame_upload_handoff_packet_status::ready_uploaded)}));
    require(missing_page_packets.policy.missing_atlas_page_count == 1U, "missing atlas page is counted");
    require(
        missing_page_packets.entries.front().status
            == render_text_frame_resource_packet_materialization_status::blocked_missing_atlas_page,
        "missing atlas page status is explicit");

    render_text_frame_draw_packet_snapshot missing_bounds =
        ready_draw_packet("draw-missing-bounds", U'C', 2);
    missing_bounds.has_atlas_bounds = false;
    render_text_frame_resource_packet_materialization missing_bounds_packets =
        materialize(
            draw_plan_for({missing_bounds}),
            handoff_for({handoff_packet_for_draw(
                missing_bounds,
                render_text_frame_upload_handoff_packet_status::ready_uploaded)}));
    require(missing_bounds_packets.policy.missing_atlas_bounds_count == 1U, "missing atlas bounds is counted");
    require(
        missing_bounds_packets.entries.front().status
            == render_text_frame_resource_packet_materialization_status::blocked_missing_atlas_bounds,
        "missing atlas bounds status is explicit");

    render_text_frame_draw_packet_snapshot missing_extent =
        ready_draw_packet("draw-missing-extent", U'D', 3);
    missing_extent.page_width = 0;
    missing_extent.uv_bounds.valid = false;
    render_text_frame_resource_packet_materialization missing_extent_packets =
        materialize(
            draw_plan_for({missing_extent}),
            handoff_for({handoff_packet_for_draw(
                missing_extent,
                render_text_frame_upload_handoff_packet_status::ready_uploaded)}));
    require(missing_extent_packets.policy.missing_page_extent_count == 1U, "missing page extent is counted");
    require(
        missing_extent_packets.entries.front().status
            == render_text_frame_resource_packet_materialization_status::blocked_missing_page_extent,
        "missing page extent status is explicit");
}

void test_resource_packets_report_missing_draw_and_duplicate_packet_ids()
{
    using namespace quiz_vulkan::render;

    const render_text_frame_draw_packet_snapshot draw =
        ready_draw_packet("draw-duplicate", U'A', 0);
    const render_text_frame_resource_packet_materialization duplicates =
        materialize(
            draw_plan_for({draw, draw}),
            handoff_for({handoff_packet_for_draw(
                draw,
                render_text_frame_upload_handoff_packet_status::ready_uploaded)}));

    require(!duplicates.ok(), "duplicate draw packet ids block resource materialization");
    require(duplicates.policy.duplicate_packet_id_count == 1U, "duplicate packet id is counted");
    require(duplicates.duplicate_packet_ids.size() == 1U, "duplicate packet id is exposed");
    require(
        duplicates.entries.back().status
            == render_text_frame_resource_packet_materialization_status::duplicate_packet_id,
        "duplicate packet status is explicit");

    render_text_frame_upload_handoff_packet_snapshot orphan =
        handoff_packet_for_draw(draw, render_text_frame_upload_handoff_packet_status::ready_uploaded);
    orphan.handoff_id = "handoff-orphan";
    orphan.draw_packet_id = {};
    orphan.missing_draw_packet = true;
    orphan.handoff_status = render_text_frame_upload_handoff_packet_status::blocked_missing_draw_packet;
    orphan.ready = false;
    orphan.blocked = true;
    orphan.uploaded = false;
    orphan.upload_rgba_bytes = 0U;
    const render_text_frame_resource_packet_materialization missing_draw =
        materialize(draw_plan_for({}), handoff_for({orphan}));

    require(!missing_draw.ok(), "orphan upload handoff blocks resource materialization");
    require(missing_draw.policy.missing_draw_packet_count == 1U, "missing draw packet is counted");
    require(
        missing_draw.entries.front().status
            == render_text_frame_resource_packet_materialization_status::blocked_missing_draw_packet,
        "missing draw packet status is explicit");
    require(!missing_draw.entries.front().has_draw_packet, "orphan entry records missing draw packet");
}

void test_resource_packet_consumption_diff_reports_stable_no_change()
{
    using namespace quiz_vulkan::render;

    const render_text_frame_draw_packet_snapshot draw =
        ready_draw_packet("draw-stable", U'A', 0);
    const render_text_frame_resource_packet_materialization packets =
        materialize(
            draw_plan_for({draw}),
            handoff_for({handoff_packet_for_draw(
                draw,
                render_text_frame_upload_handoff_packet_status::ready_uploaded)}));

    const render_text_frame_resource_packet_consumption_diff_snapshot diff =
        diff_render_text_frame_resource_packet_materializations(packets, packets);

    require(!diff.has_changes(), "identical resource packet materializations report no changes");
    require(diff.stable_no_change(), "identical resource packets mark stable no-change");
    require(diff.policy.unchanged_packet_count == packets.entries.size(), "unchanged packet count is preserved");
    require(diff.policy.added_packet_count == 0U, "stable diff has no added packets");
    require(diff.policy.removed_packet_count == 0U, "stable diff has no removed packets");
    require(diff.policy.changed_packet_count == 0U, "stable diff has no changed packets");
    require(diff.unchanged_resource_packet_ids.size() == 1U, "stable diff exposes unchanged packet identity");
    require(!diff.summary.empty(), "stable diff carries compact summary text");
}

void test_resource_packet_consumption_diff_reports_added_removed_and_changed_identity_fields()
{
    using namespace quiz_vulkan::render;

    const render_text_frame_draw_packet_snapshot removed_draw =
        ready_draw_packet("draw-removed", U'A', 0);
    const render_text_frame_draw_packet_snapshot kept_draw =
        ready_draw_packet("draw-kept", U'C', 1);
    const render_text_frame_draw_packet_snapshot added_draw =
        ready_draw_packet("draw-added", U'R', 2);

    const render_text_frame_resource_packet_materialization before =
        materialize(
            draw_plan_for({removed_draw, kept_draw}),
            handoff_for({
                handoff_packet_for_draw(
                    removed_draw,
                    render_text_frame_upload_handoff_packet_status::ready_uploaded),
                handoff_packet_for_draw(
                    kept_draw,
                    render_text_frame_upload_handoff_packet_status::ready_uploaded),
            }));
    render_text_frame_resource_packet_materialization after =
        materialize(
            draw_plan_for({kept_draw, added_draw}),
            handoff_for({
                handoff_packet_for_draw(
                    kept_draw,
                    render_text_frame_upload_handoff_packet_status::ready_uploaded),
                handoff_packet_for_draw(
                    added_draw,
                    render_text_frame_upload_handoff_packet_status::ready_uploaded),
            }));
    render_text_frame_resource_packet_materialization_entry& kept_after =
        after.entries.front();
    kept_after.stable_packet_key += ":changed";
    kept_after.page_id = 9;
    kept_after.page_revision = 11;
    kept_after.page_width = 128;
    kept_after.page_height = 128;
    kept_after.sampler_key += ":changed";
    kept_after.uv_bounds.u1 = 0.5f;
    kept_after.layout_bounds.x += 3.0f;
    kept_after.upload_request_id += ":changed";
    kept_after.upload_operation_id += ":changed";
    kept_after.upload_rgba_bytes += 64U;
    kept_after.used_deterministic_fallback = !kept_after.used_deterministic_fallback;
    kept_after.used_real_backend = !kept_after.used_real_backend;
    after.pages.front().page_width = 128U;
    after.pages.front().page_height = 128U;
    after.pages.front().sampler_key += ":page-changed";

    const render_text_frame_resource_packet_consumption_diff_snapshot diff =
        diff_render_text_frame_resource_packet_materializations(before, after);

    require(diff.has_changes(), "changed resource packet materialization reports changes");
    require(diff.policy.added_packet_count == 1U, "diff counts added resource packet");
    require(diff.policy.removed_packet_count == 1U, "diff counts removed resource packet");
    require(diff.policy.changed_packet_count == 1U, "diff counts changed resource packet");
    require(diff.policy.unchanged_packet_count == 0U, "diff has no unchanged matched packet after mutation");
    require(diff.added_resource_packet_ids.size() == 1U, "diff exposes added packet identity");
    require(diff.removed_resource_packet_ids.size() == 1U, "diff exposes removed packet identity");
    require(diff.changed_resource_packet_ids.size() == 1U, "diff exposes changed packet identity");
    require(diff.policy.stable_packet_key_changed_count == 1U, "diff counts stable packet key changes");
    require(diff.policy.page_id_changed_count == 1U, "diff counts page id changes");
    require(diff.policy.page_revision_changed_count == 1U, "diff counts page revision changes");
    require(diff.policy.page_extent_changed_count == 1U, "diff counts page extent changes");
    require(diff.policy.sampler_key_changed_count == 1U, "diff counts sampler key changes");
    require(diff.policy.uv_bounds_changed_count == 1U, "diff counts UV bound changes");
    require(diff.policy.layout_bounds_changed_count == 1U, "diff counts layout bound changes");
    require(diff.policy.upload_request_id_changed_count == 1U, "diff counts upload request id changes");
    require(diff.policy.upload_operation_id_changed_count == 1U, "diff counts upload operation id changes");
    require(diff.policy.uploaded_byte_count_changed_count == 1U, "diff counts upload byte changes");
    require(diff.policy.fallback_flag_changed_count == 1U, "diff counts deterministic fallback flag changes");
    require(diff.policy.backend_flag_changed_count == 1U, "diff counts real backend flag changes");
    require(diff.policy.changed_page_count == 1U, "diff counts changed page summaries");
    require(diff.changed_page_ids.size() == 1U, "diff exposes changed page id");
}

void test_resource_packet_consumption_diff_reports_readiness_regressions_and_improvements()
{
    using namespace quiz_vulkan::render;

    const render_text_frame_draw_packet_snapshot draw =
        ready_draw_packet("draw-readiness", U'A', 0);
    const render_text_frame_resource_packet_materialization ready_packets =
        materialize(
            draw_plan_for({draw}),
            handoff_for({handoff_packet_for_draw(
                draw,
                render_text_frame_upload_handoff_packet_status::ready_uploaded)}));
    render_text_frame_resource_packet_materialization blocked_packets =
        ready_packets;
    render_text_frame_resource_packet_materialization_entry& blocked_entry =
        blocked_packets.entries.front();
    blocked_entry.ready = false;
    blocked_entry.blocked = true;
    blocked_entry.renderer_boundary_ready = false;
    blocked_entry.status =
        render_text_frame_resource_packet_materialization_status::blocked_upload_rejected;
    blocked_entry.blocker_summary = "upload rejected after renderer boundary packet was planned";
    blocked_packets.policy.has_blockers = true;

    const render_text_frame_resource_packet_consumption_diff_snapshot regression =
        diff_render_text_frame_resource_packet_materializations(ready_packets, blocked_packets);
    const render_text_frame_resource_packet_consumption_diff_snapshot improvement =
        diff_render_text_frame_resource_packet_materializations(blocked_packets, ready_packets);

    require(regression.policy.readiness_regression_count == 1U, "diff counts ready-to-blocked regression");
    require(regression.policy.readiness_or_blocker_status_changed_count == 1U, "diff counts readiness/blocker status change");
    require(regression.readiness_regressed_resource_packet_ids.size() == 1U, "diff exposes regressed packet identity");
    require(improvement.policy.readiness_improvement_count == 1U, "diff counts blocked-to-ready improvement");
    require(improvement.readiness_improved_resource_packet_ids.size() == 1U, "diff exposes improved packet identity");
}

void test_resource_packet_consumption_diff_reports_duplicate_and_missing_identities()
{
    using namespace quiz_vulkan::render;

    const render_text_frame_draw_packet_snapshot draw =
        ready_draw_packet("draw-identity", U'A', 0);
    const render_text_frame_resource_packet_materialization before =
        materialize(
            draw_plan_for({draw}),
            handoff_for({handoff_packet_for_draw(
                draw,
                render_text_frame_upload_handoff_packet_status::ready_uploaded)}));
    render_text_frame_resource_packet_materialization duplicate_after = before;
    duplicate_after.entries.push_back(duplicate_after.entries.front());

    const render_text_frame_resource_packet_consumption_diff_snapshot duplicate_diff =
        diff_render_text_frame_resource_packet_materializations(before, duplicate_after);

    require(duplicate_diff.policy.duplicate_identity_count >= 1U, "diff counts duplicate resource packet identities");
    require(!duplicate_diff.duplicate_identity_resource_packet_ids.empty(), "diff exposes duplicate resource packet id");

    render_text_frame_resource_packet_materialization missing_after = before;
    missing_after.entries.front().resource_packet_id.clear();

    const render_text_frame_resource_packet_consumption_diff_snapshot missing_diff =
        diff_render_text_frame_resource_packet_materializations(before, missing_after);

    require(missing_diff.policy.missing_identity_count == 1U, "diff counts missing resource packet identity");
    require(missing_diff.policy.added_packet_count == 1U, "missing current identity is treated as an unmatched packet");
    require(missing_diff.policy.removed_packet_count == 1U, "previous stable identity is reported removed");
}

}  // namespace

int main()
{
    test_resource_packets_materialize_upload_and_clean_reuse();
    test_resource_packets_report_missing_upload_handoff_and_rejected_upload();
    test_resource_packets_report_draw_and_page_blockers();
    test_resource_packets_report_missing_draw_and_duplicate_packet_ids();
    test_resource_packet_consumption_diff_reports_stable_no_change();
    test_resource_packet_consumption_diff_reports_added_removed_and_changed_identity_fields();
    test_resource_packet_consumption_diff_reports_readiness_regressions_and_improvements();
    test_resource_packet_consumption_diff_reports_duplicate_and_missing_identities();
    return 0;
}
