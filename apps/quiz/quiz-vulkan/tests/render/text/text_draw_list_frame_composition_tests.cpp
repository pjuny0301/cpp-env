#include "render/text/text_frame_snapshot.h"

#include <cassert>
#include <cstdio>
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

bool rect_matches(
    const quiz_vulkan::render::render_rect& lhs,
    const quiz_vulkan::render::render_rect& rhs)
{
    return lhs.x == rhs.x
        && lhs.y == rhs.y
        && lhs.width == rhs.width
        && lhs.height == rhs.height;
}

quiz_vulkan::render::render_text_style_catalog style_catalog()
{
    using namespace quiz_vulkan::render;

    render_text_style_catalog catalog;
    catalog.fallback_style = render_text_style{
        .id = "fallback",
        .font_family = "Fallback Sans",
        .font_size = 14.0f,
        .line_height = 18.0f,
        .font_weight = 400,
    };
    catalog.styles.push_back(render_text_style{
        .id = "body",
        .font_family = "Quiz Sans",
        .font_size = 16.0f,
        .line_height = 20.0f,
        .font_weight = 500,
    });
    return catalog;
}

quiz_vulkan::render::font_face_catalog font_catalog()
{
    using namespace quiz_vulkan::render;

    font_face_catalog catalog;
    catalog.add_face(font_face_descriptor{
        .id = 7,
        .family = "Quiz Sans",
        .source_uri = "fonts/quiz-sans.ttf",
        .coverage = {
            font_codepoint_range{.first = U'A', .last = U'Z'},
            font_codepoint_range{.first = U'a', .last = U'z'},
            font_codepoint_range{.first = U'0', .last = U'9'},
            font_codepoint_range{.first = U' ', .last = U' '},
        },
        .weight = 500,
    });
    catalog.add_face(font_face_descriptor{
        .id = 8,
        .family = "Fallback Sans",
        .source_uri = "fonts/fallback.ttf",
        .coverage = {},
        .weight = 400,
        .fallback = true,
    });
    return catalog;
}

quiz_vulkan::render::render_draw_command text_command(
    quiz_vulkan::render::render_node_id node_id,
    quiz_vulkan::render::render_node_id parent_node_id,
    std::vector<quiz_vulkan::render::render_text_run> runs)
{
    using namespace quiz_vulkan::render;

    return render_draw_command{
        .type = render_draw_command_type::text,
        .node_id = std::move(node_id),
        .parent_node_id = std::move(parent_node_id),
        .depth = 2,
        .bounds = render_rect{1.0f, 2.0f, 100.0f, 30.0f},
        .content_bounds = render_rect{3.0f, 4.0f, 90.0f, 20.0f},
        .text_runs = std::move(runs),
        .text_options = render_text_options{
            .wrap = render_text_wrap_mode::word,
            .alignment = render_text_alignment::center,
            .max_lines = 2,
        },
    };
}

quiz_vulkan::render::render_text_glyph_atlas_materialization_snapshot
upload_ready_materialization(const std::uint32_t glyph_id = U'H')
{
    using namespace quiz_vulkan::render;

    return make_render_text_glyph_atlas_materialization(
        render_text_glyph_atlas_materialization_request{
            .cluster_index = 0,
            .run_index = 0,
            .cluster_byte_offset = 0,
            .cluster_byte_count = 1,
            .codepoint = glyph_id,
            .shaped_glyph_ids = {glyph_id},
            .resolved_glyph_id = glyph_id,
            .resolved_face_id = 7,
            .cache_key = glyph_atlas_key{.face_id = 7, .glyph_id = glyph_id, .pixel_size = 16},
            .has_cache_key = true,
            .glyph_supported = true,
            .glyph_id_from_selection = true,
            .glyph_id_matches_codepoint = true,
            .layout_bounds = render_rect{3.0f, 4.0f, 8.0f, 16.0f},
            .has_layout_bounds = true,
            .shaping_font_backend_label = "deterministic fake",
            .shaping_font_backend_used_deterministic_fallback = true,
            .raster_font_backend_label = "deterministic fake",
            .raster_font_backend_used_deterministic_fallback = true,
            .rasterizer_status = render_text_font_rasterizer_status::rasterized,
            .raster_payload_matches_cache_key = true,
            .rasterized_payload_skipped = false,
            .payload_upload_ready = true,
            .payload_alpha_bytes = 4,
            .payload_rgba_bytes = 16,
            .has_atlas_placement = true,
            .page = render_text_atlas_page{.id = 2, .revision = 3, .width = 128, .height = 128},
            .atlas_bounds = render_rect{16.0f, 16.0f, 2.0f, 2.0f},
            .has_atlas_update = true,
            .atlas_update_bounds = render_rect{16.0f, 16.0f, 2.0f, 2.0f},
            .atlas_update_rgba_bytes = 16,
        });
}

quiz_vulkan::render::render_text_draw_list_frame_composition_snapshot compose(
    quiz_vulkan::render::render_draw_list draw_list,
    std::vector<std::vector<quiz_vulkan::render::render_text_glyph_atlas_materialization_snapshot>>
        materializations_by_entry = {},
    const bool consume_uploads = false)
{
    return quiz_vulkan::render::compose_render_text_draw_list_frame(
        quiz_vulkan::render::render_text_draw_list_frame_composition_request{
            .frame_id = "frame-1",
            .source_label = "draw-list-composition-test",
            .draw_list = std::move(draw_list),
            .font_catalog = font_catalog(),
            .materializations_by_handoff_entry_index = std::move(materializations_by_entry),
            .consume_queued_atlas_uploads = consume_uploads,
        });
}

void test_ready_draw_list_text_command_composes_to_frame_evidence()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.text_styles = style_catalog();
    draw_list.commands.push_back(render_draw_command{
        .type = render_draw_command_type::quad,
        .node_id = "quad-node",
    });
    const render_draw_command command = text_command(
        "text-node",
        "parent-node",
        {render_text_run{.text = "Hello", .style_token = "body"}});
    draw_list.commands.push_back(command);

    const render_text_draw_list_frame_composition_snapshot snapshot =
        compose(std::move(draw_list), {{upload_ready_materialization()}}, true);

    require(snapshot.ok(), "ready composition has no blockers");
    require(snapshot.ready_for_renderer(), "consumed upload-ready materialization produces ready frame evidence");
    require(snapshot.policy.draw_command_count == 2U, "composition counts draw commands");
    require(snapshot.policy.skipped_non_text_command_count == 1U, "composition preserves non-text skip count");
    require(snapshot.policy.handoff_entry_count == 1U, "composition preserves handoff entry count");
    require(snapshot.policy.composed_entry_count == 1U, "composition counts composed entries");
    require(snapshot.policy.request_batch_item_count == 1U, "composition creates one request batch item");
    require(snapshot.policy.layout_request_count == 1U, "composition creates one layout request");
    require(snapshot.policy.materialization_count == 1U, "composition forwards materialization evidence");
    require(snapshot.policy.atlas_update_request_count == 1U, "composition forwards atlas update evidence");
    require(snapshot.policy.upload_request_count == 1U, "composition forwards upload-ready evidence");
    require(snapshot.policy.fallback_chain_missing_glyph_count == 0U, "font catalog covers ready text");
    require(snapshot.policy.deterministic_fallback_used, "deterministic fake backend evidence is preserved");
    require(snapshot.composed_command_indices.size() == 1U, "composition records composed command index");
    require(snapshot.composed_command_indices.front() == 1U, "composition preserves draw command index");

    const render_text_draw_list_frame_handoff_entry& entry = snapshot.handoff.entries.front();
    require(entry.draw_command_index == 1U, "handoff entry preserves draw command index");
    require(entry.node_id == command.node_id, "handoff entry preserves node id");
    require(entry.parent_node_id == command.parent_node_id, "handoff entry preserves parent node id");
    require(rect_matches(entry.bounds, command.bounds), "handoff entry preserves command bounds");
    require(rect_matches(entry.content_bounds, command.content_bounds), "handoff entry preserves content bounds");
    require(entry.primary_requested_style_token == "body", "handoff entry preserves requested style token");
    require(entry.primary_resolved_style_id == "body", "handoff entry preserves resolved style id");
    require(!entry.used_fallback_style, "ready handoff entry does not invent fallback style use");
    require(entry.status == render_text_draw_list_frame_handoff_entry_status::ready, "handoff entry is ready");
    require(entry.ok(), "ready handoff entry is ok");

    const render_text_request_batch_item& item = snapshot.request_items.front();
    require(item.item_index == 0U, "request item index is assigned after handoff filtering");
    require(item.node_id == "text-node", "request item preserves node id");
    require(item.source_label == entry.stable_entry_id, "request item uses handoff identity");
    require(rect_matches(item.bounds, entry.content_bounds), "request item uses handoff content bounds");
    require(item.text_runs.size() == 1U && item.text_runs.front().text == "Hello", "request item preserves text runs");
    require(item.style_catalog.find("body") != nullptr, "request item preserves style catalog");
    require(item.options.alignment == render_text_alignment::center, "request item preserves text options");
    require(item.options.max_lines == 2, "request item preserves max line option");
    require(item.materializations.size() == 1U, "request item carries per-entry materialization evidence");

    require(snapshot.batch_plan.policy.item_count == 1U, "batch plan sees composed request item");
    require(snapshot.batch_plan.style_keys.size() == 1U, "batch plan records style evidence");
    require(
        snapshot.batch_plan.style_keys.front().requested_style_token == "body",
        "batch plan preserves requested style token");
    require(
        snapshot.batch_plan.style_keys.front().resolved_style_id == "body",
        "batch plan preserves resolved style id");
    require(snapshot.batch_plan.layout_requests.front().source_label == entry.stable_entry_id,
        "layout request keeps handoff identity");
    require(rect_matches(snapshot.batch_plan.layout_requests.front().bounds, entry.content_bounds),
        "layout request keeps content bounds");

    require(snapshot.frame.layout_requests.size() == 1U, "frame snapshot exposes layout request");
    require(snapshot.frame.layout_requests.front().node_id == "text-node", "frame layout request preserves node id");
    require(snapshot.frame.layout_requests.front().source_label == entry.stable_entry_id,
        "frame layout request preserves handoff identity");
    require(snapshot.frame.atlas_uploads.size() == 1U, "frame snapshot exposes upload handoff");
    require(snapshot.frame.status == render_text_frame_snapshot_status::ready, "frame is ready after consuming upload");
}

void test_blocked_handoff_entry_stays_blocked_before_layout()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.text_styles = style_catalog();
    draw_list.commands.push_back(text_command(
        "empty-text",
        "parent",
        {render_text_run{.text = "", .style_token = "body"}}));
    draw_list.commands.push_back(text_command(
        "ready-text",
        "parent",
        {render_text_run{.text = "Ready", .style_token = "body"}}));

    const render_text_draw_list_frame_composition_snapshot snapshot = compose(std::move(draw_list));

    require(!snapshot.ok(), "blocked handoff entry makes composition non-ok");
    require(snapshot.has_blockers(), "composition records blocked entries");
    require(snapshot.policy.blocked_entry_count == 1U, "composition counts one blocked entry");
    require(snapshot.blocked_command_indices.size() == 1U, "composition records blocked command index");
    require(snapshot.blocked_command_indices.front() == 0U, "blocked command index is preserved");
    require(snapshot.handoff.entries.front().status == render_text_draw_list_frame_handoff_entry_status::blocked_empty_text,
        "blocked handoff entry keeps explicit blocker status");
    require(snapshot.handoff.entries.front().blocked, "blocked handoff entry remains blocked");
    require(!snapshot.handoff.entries.front().blocker_reason.empty(), "blocked handoff entry keeps diagnostics");
    require(snapshot.handoff.entries.back().status == render_text_draw_list_frame_handoff_entry_status::ready,
        "ready handoff entry remains ready");
    require(snapshot.policy.composed_entry_count == 1U, "ready entry still composes");
    require(snapshot.request_items.size() == 1U, "blocked entry does not become request item");
    require(snapshot.request_items.front().node_id == "ready-text", "only ready command enters request batch");
    require(snapshot.batch_plan.policy.item_count == 1U, "batch plan excludes blocked handoff entry");
    require(snapshot.frame.layout_requests.size() == 1U, "blocked entry does not enter layout requests");
    require(snapshot.frame.layout_requests.front().node_id == "ready-text", "layout request belongs to ready command");
}

void test_multiple_text_commands_preserve_command_order()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.text_styles = style_catalog();
    draw_list.commands.push_back(text_command(
        "first-text",
        "root",
        {render_text_run{.text = "First", .style_token = "body"}}));
    draw_list.commands.push_back(render_draw_command{
        .type = render_draw_command_type::image,
        .node_id = "image-node",
    });
    draw_list.commands.push_back(text_command(
        "second-text",
        "root",
        {render_text_run{.text = "Second", .style_token = "body"}}));

    const render_text_draw_list_frame_composition_snapshot snapshot = compose(std::move(draw_list));

    require(snapshot.policy.skipped_non_text_command_count == 1U, "composition counts non-text commands");
    require(snapshot.policy.composed_entry_count == 2U, "composition includes both text commands");
    require(snapshot.composed_command_indices.size() == 2U, "composition records both command indices");
    require(snapshot.composed_command_indices[0] == 0U, "first text command order is preserved");
    require(snapshot.composed_command_indices[1] == 2U, "second text command order is preserved");
    require(snapshot.request_items[0].node_id == "first-text", "first request item follows draw order");
    require(snapshot.request_items[1].node_id == "second-text", "second request item follows draw order");
    require(snapshot.frame.layout_requests[0].node_id == "first-text", "first layout request follows draw order");
    require(snapshot.frame.layout_requests[1].node_id == "second-text", "second layout request follows draw order");
}

void test_non_text_draw_commands_do_not_enter_text_frame_path()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.text_styles = style_catalog();
    draw_list.commands.push_back(render_draw_command{
        .type = render_draw_command_type::quad,
        .node_id = "quad-node",
    });
    draw_list.commands.push_back(render_draw_command{
        .type = render_draw_command_type::image,
        .node_id = "image-node",
    });

    const render_text_draw_list_frame_composition_snapshot snapshot = compose(std::move(draw_list));

    require(snapshot.ok(), "non-text-only composition is a clean no-op");
    require(!snapshot.policy.has_text_frame, "non-text-only composition has no text frame entries");
    require(snapshot.policy.skipped_non_text_command_count == 2U, "all commands are skipped as non-text");
    require(snapshot.request_items.empty(), "non-text commands do not become request items");
    require(snapshot.frame.layout_requests.empty(), "non-text commands do not become layout requests");
    require(snapshot.frame.atlas_uploads.empty(), "non-text commands do not become atlas uploads");
}

} // namespace

int main()
{
    test_ready_draw_list_text_command_composes_to_frame_evidence();
    test_blocked_handoff_entry_stays_blocked_before_layout();
    test_multiple_text_commands_preserve_command_order();
    test_non_text_draw_commands_do_not_enter_text_frame_path();
    return 0;
}
