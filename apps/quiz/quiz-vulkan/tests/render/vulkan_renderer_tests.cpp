#include "core/ui/ui_renderer.h"
#include "render/vulkan/vulkan_renderer.h"

#include <cassert>
#include <string>
#include <utility>
#include <vector>

namespace {

quiz_vulkan::scene::placed_scene make_simple_scene_snapshot()
{
    using namespace quiz_vulkan;

    scene::placed_scene snapshot;

    scene::placed_scene_node panel;
    panel.id = "panel";
    panel.bounds = scene::scene_rect{20.0f, 16.0f, 260.0f, 120.0f};
    panel.content_bounds = scene::scene_rect{32.0f, 28.0f, 236.0f, 96.0f};
    panel.style.background_color = "#24384f";
    panel.style.foreground_color = "#f4f8ff";
    panel.style.border_radius = 6.0f;
    panel.text_runs.push_back(scene::scene_text_run{"Renderer smoke", "title"});
    snapshot.nodes.push_back(std::move(panel));

    return snapshot;
}

void test_scene_snapshot_submission()
{
    using namespace quiz_vulkan;

    const scene::placed_scene snapshot = make_simple_scene_snapshot();
    const ui::ui_draw_list expected_draw_list = ui::ui_renderer{}.build_draw_list(snapshot);
    assert(!expected_draw_list.empty());

    render::vulkan_renderer renderer;
    renderer.submit(snapshot);

    const render::vulkan_renderer_frame_stats& stats = renderer.last_frame_stats();
    assert(stats.command_count == expected_draw_list.size());
    assert(stats.draw_call_count == stats.quad_count + stats.text_count + stats.image_count + stats.debug_bounds_count);
    assert(stats.visible_draw_call_count == stats.draw_call_count);
    assert(stats.quad_count == 1);
    assert(stats.text_count == 1);
    assert(stats.text_run_count == 1);
    assert(stats.text_character_count == std::string{"Renderer smoke"}.size());

    const render::vulkan_renderer_frame_summary& summary = renderer.last_frame_summary();
    assert(summary.used_cpu_fallback());
    assert(summary.nonblank());
    assert(summary.surface_width == renderer.options().fallback_surface_width);
    assert(summary.surface_height == renderer.options().fallback_surface_height);
    assert(summary.shaded_pixel_count > 0);
    assert(summary.discarded_draw_call_count == 0);

    renderer.clear();
    assert(renderer.last_draw_list().empty());
    assert(renderer.last_frame_stats().empty());
    assert(!renderer.last_frame_summary().nonblank());
}

void test_cpu_fallback_clips_and_discards()
{
    using namespace quiz_vulkan;

    ui::ui_draw_command clip;
    clip.type = ui::ui_draw_command_type::push_clip;
    clip.node_id = "clip";
    clip.bounds = scene::scene_rect{0.0f, 0.0f, 50.0f, 50.0f};
    clip.content_bounds = clip.bounds;

    ui::ui_draw_command quad;
    quad.type = ui::ui_draw_command_type::quad;
    quad.node_id = "quad";
    quad.bounds = scene::scene_rect{25.0f, 25.0f, 50.0f, 50.0f};
    quad.content_bounds = quad.bounds;
    quad.paint = ui::ui_paint{"#00ff00", ui::ui_color{0.0f, 1.0f, 0.0f, 1.0f}};

    ui::ui_draw_command transparent;
    transparent.type = ui::ui_draw_command_type::quad;
    transparent.node_id = "transparent";
    transparent.bounds = scene::scene_rect{10.0f, 10.0f, 10.0f, 10.0f};
    transparent.content_bounds = transparent.bounds;
    transparent.paint = ui::ui_paint{"transparent", ui::ui_color{0.0f, 0.0f, 0.0f, 0.0f}};

    ui::ui_draw_command pop;
    pop.type = ui::ui_draw_command_type::pop_clip;
    pop.node_id = "clip";
    pop.bounds = clip.bounds;
    pop.content_bounds = clip.bounds;

    render::vulkan_renderer_options options;
    options.viewport = scene::scene_rect{0.0f, 0.0f, 100.0f, 100.0f};
    options.fallback_surface_width = 10;
    options.fallback_surface_height = 10;

    render::vulkan_renderer renderer(options);
    renderer.submit(std::vector<ui::ui_draw_command>{clip, quad, transparent, pop});

    const render::vulkan_renderer_frame_stats& stats = renderer.last_frame_stats();
    assert(stats.command_count == 4);
    assert(stats.draw_call_count == 2);
    assert(stats.visible_draw_call_count == 1);
    assert(stats.push_clip_count == 1);
    assert(stats.pop_clip_count == 1);

    const render::vulkan_renderer_frame_summary& summary = renderer.last_frame_summary();
    assert(summary.used_cpu_fallback());
    assert(summary.nonblank());
    assert(summary.clipped_draw_call_count == 1);
    assert(summary.discarded_draw_call_count == 1);
    assert(summary.shaded_pixel_count == 9);
}

} // namespace

int main()
{
    test_scene_snapshot_submission();
    test_cpu_fallback_clips_and_discards();
    return 0;
}
