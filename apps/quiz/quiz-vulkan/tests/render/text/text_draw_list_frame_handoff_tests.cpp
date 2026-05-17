#include "render/text/font_shaped_atlas_update.h"

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

quiz_vulkan::render::render_text_draw_list_frame_handoff_snapshot handoff_for(
    quiz_vulkan::render::render_draw_list draw_list)
{
    return quiz_vulkan::render::make_render_text_draw_list_frame_handoff(
        quiz_vulkan::render::render_text_draw_list_frame_handoff_request{
            .frame_id = "frame-1",
            .source_label = "draw-list-test",
            .draw_list = std::move(draw_list),
        });
}

void test_draw_list_handoff_keeps_only_text_commands_and_preserves_fields()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.text_styles = style_catalog();
    draw_list.commands.push_back(render_draw_command{
        .type = render_draw_command_type::quad,
        .node_id = "quad-node",
    });
    draw_list.commands.push_back(text_command(
        "text-node",
        "parent-node",
        {render_text_run{.text = "Hello", .style_token = "body"}}));
    draw_list.commands.push_back(render_draw_command{
        .type = render_draw_command_type::image,
        .node_id = "image-node",
    });

    const render_text_draw_list_frame_handoff_snapshot handoff =
        handoff_for(std::move(draw_list));

    require(handoff.ok(), "valid text command handoff has no blockers");
    require(handoff.policy.draw_command_count == 3U, "handoff counts all draw commands");
    require(handoff.policy.text_command_count == 1U, "handoff counts text commands");
    require(handoff.policy.skipped_non_text_command_count == 2U, "handoff counts skipped non-text commands");
    require(handoff.skipped_non_text_command_indices.size() == 2U, "handoff records non-text command indices");
    require(handoff.entries.size() == 1U, "only text commands become handoff entries");

    const render_text_draw_list_frame_handoff_entry& entry = handoff.entries.front();
    require(entry.draw_command_index == 1U, "entry preserves draw command index");
    require(entry.node_id == "text-node", "entry preserves node id");
    require(entry.parent_node_id == "parent-node", "entry preserves parent node id");
    require(entry.bounds.x == 1.0f && entry.bounds.width == 100.0f, "entry preserves command bounds");
    require(entry.content_bounds.x == 3.0f && entry.content_bounds.width == 90.0f, "entry preserves content bounds");
    require(entry.text_runs.size() == 1U && entry.text_runs.front().text == "Hello", "entry preserves text runs");
    require(entry.primary_requested_style_token == "body", "entry preserves requested style token");
    require(entry.primary_resolved_style_id == "body", "entry resolves style token");
    require(entry.fallback_style_id == "fallback", "entry preserves fallback style id evidence");
    require(entry.options.wrap == render_text_wrap_mode::word, "entry preserves text wrapping option");
    require(entry.options.alignment == render_text_alignment::center, "entry preserves text alignment option");
    require(entry.options.max_lines == 2U, "entry preserves max-lines option");
    require(entry.status == render_text_draw_list_frame_handoff_entry_status::ready, "entry is ready");
    require(!entry.stable_entry_id.empty(), "entry exposes stable text identity");

    const render_text_request_batch_item item =
        make_render_text_request_batch_item(entry, style_catalog());
    require(item.node_id == "text-node", "handoff entry can be converted to text batch item");
    require(item.bounds.x == entry.content_bounds.x, "handoff batch item uses content bounds");
}

void test_draw_list_handoff_reports_duplicate_and_missing_stable_id()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.text_styles = style_catalog();
    draw_list.commands.push_back(text_command(
        "duplicate-node",
        "parent",
        {render_text_run{.text = "First", .style_token = "body"}}));
    draw_list.commands.push_back(text_command(
        "duplicate-node",
        "parent",
        {render_text_run{.text = "Second", .style_token = "body"}}));
    draw_list.commands.push_back(text_command(
        "",
        "parent",
        {render_text_run{.text = "Missing id", .style_token = "body"}}));

    const render_text_draw_list_frame_handoff_snapshot handoff =
        handoff_for(std::move(draw_list));

    require(!handoff.ok(), "duplicate and missing identities block handoff");
    require(handoff.policy.duplicate_stable_id_count == 1U, "handoff counts duplicate stable id");
    require(handoff.policy.missing_stable_id_count == 1U, "handoff counts missing stable id");
    require(handoff.duplicate_stable_entry_ids.size() == 1U, "handoff exposes duplicate stable id");
    require(handoff.missing_stable_entry_command_indices.size() == 1U, "handoff exposes missing-id command index");
    require(
        handoff.entries[1].status == render_text_draw_list_frame_handoff_entry_status::duplicate_stable_id,
        "second duplicate command is deterministic blocker");
    require(
        handoff.entries[2].status == render_text_draw_list_frame_handoff_entry_status::blocked_missing_stable_id,
        "missing node id is deterministic blocker");
}

void test_draw_list_handoff_reports_empty_text_and_missing_style_fallback()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.text_styles = style_catalog();
    draw_list.commands.push_back(text_command(
        "empty-text",
        "parent",
        {render_text_run{.text = "", .style_token = "body"}}));
    draw_list.commands.push_back(text_command(
        "missing-style",
        "parent",
        {render_text_run{.text = "Fallback", .style_token = "missing-style"}}));

    render_draw_command invalid_bounds = text_command(
        "invalid-bounds",
        "parent",
        {render_text_run{.text = "Bounds", .style_token = "body"}});
    invalid_bounds.content_bounds.width = 0.0f;
    draw_list.commands.push_back(invalid_bounds);

    const render_text_draw_list_frame_handoff_snapshot handoff =
        handoff_for(std::move(draw_list));

    require(!handoff.ok(), "empty text and invalid bounds block handoff");
    require(handoff.policy.empty_text_count == 1U, "handoff counts empty text blocker");
    require(handoff.policy.invalid_bounds_count == 1U, "handoff counts invalid bounds blocker");
    require(handoff.policy.missing_style_count == 1U, "handoff counts missing style evidence");
    require(handoff.policy.fallback_style_count == 1U, "handoff counts fallback style use");
    require(handoff.policy.used_fallback_style, "handoff records fallback style use");
    require(
        handoff.entries[0].status == render_text_draw_list_frame_handoff_entry_status::blocked_empty_text,
        "empty text is deterministic blocker");
    require(
        handoff.entries[1].status == render_text_draw_list_frame_handoff_entry_status::ready_with_fallback_style,
        "missing style uses fallback without crashing");
    require(handoff.entries[1].missing_style_tokens.front() == "missing-style", "entry preserves missing style token");
    require(
        handoff.entries[2].status == render_text_draw_list_frame_handoff_entry_status::blocked_invalid_bounds,
        "invalid content bounds is deterministic blocker");
}

void test_draw_list_handoff_blocks_missing_style_without_fallback_evidence()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.text_styles.styles.push_back(render_text_style{
        .id = "body",
        .font_family = "Quiz Sans",
        .font_size = 16.0f,
    });
    draw_list.commands.push_back(text_command(
        "missing-style-no-fallback",
        "parent",
        {render_text_run{.text = "No fallback", .style_token = "missing"}}));

    const render_text_draw_list_frame_handoff_snapshot handoff =
        handoff_for(std::move(draw_list));

    require(!handoff.ok(), "missing style without fallback evidence blocks handoff");
    require(handoff.policy.missing_style_count == 1U, "missing style is counted");
    require(handoff.policy.fallback_style_count == 0U, "no fallback style use is reported");
    require(
        handoff.entries.front().status == render_text_draw_list_frame_handoff_entry_status::blocked_missing_style,
        "missing style without fallback has explicit blocker status");
}

}  // namespace

int main()
{
    test_draw_list_handoff_keeps_only_text_commands_and_preserves_fields();
    test_draw_list_handoff_reports_duplicate_and_missing_stable_id();
    test_draw_list_handoff_reports_empty_text_and_missing_style_fallback();
    test_draw_list_handoff_blocks_missing_style_without_fallback_evidence();
    return 0;
}
