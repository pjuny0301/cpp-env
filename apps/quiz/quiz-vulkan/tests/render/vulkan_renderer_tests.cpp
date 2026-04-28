#include "render/vulkan/vulkan_renderer.h"

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "Requirement failed: %s\n", message);
    }
    assert((condition) && message);
}

std::size_t framebuffer_pixel_offset(
    const quiz_vulkan::render::vulkan_renderer_framebuffer& framebuffer,
    std::size_t x,
    std::size_t y)
{
    return (y * framebuffer.width + x) * 4;
}

std::size_t count_nonzero_framebuffer_pixels(
    const quiz_vulkan::render::vulkan_renderer_framebuffer& framebuffer)
{
    std::size_t count = 0;
    for (std::size_t offset = 0; offset + 3 < framebuffer.rgba.size(); offset += 4) {
        if (framebuffer.rgba[offset] != 0 || framebuffer.rgba[offset + 1] != 0
            || framebuffer.rgba[offset + 2] != 0 || framebuffer.rgba[offset + 3] != 0) {
            ++count;
        }
    }
    return count;
}

bool framebuffer_pixel_is(
    const quiz_vulkan::render::vulkan_renderer_framebuffer& framebuffer,
    std::size_t x,
    std::size_t y,
    unsigned char red,
    unsigned char green,
    unsigned char blue,
    unsigned char alpha)
{
    if (x >= framebuffer.width || y >= framebuffer.height) {
        return false;
    }

    const std::size_t offset = framebuffer_pixel_offset(framebuffer, x, y);
    return offset + 3 < framebuffer.rgba.size()
        && framebuffer.rgba[offset] == red
        && framebuffer.rgba[offset + 1] == green
        && framebuffer.rgba[offset + 2] == blue
        && framebuffer.rgba[offset + 3] == alpha;
}

quiz_vulkan::render::render_draw_command make_quad_command(
    std::string node_id,
    quiz_vulkan::render::render_rect bounds,
    quiz_vulkan::render::render_color color)
{
    using namespace quiz_vulkan::render;

    return render_draw_command{
        .type = render_draw_command_type::quad,
        .node_id = std::move(node_id),
        .parent_node_id = {},
        .depth = 0,
        .bounds = bounds,
        .content_bounds = bounds,
        .paint = render_paint{.source = "test", .color = color},
        .border_radius = 0.0f,
        .text_runs = {},
        .image = {},
    };
}

quiz_vulkan::render::render_draw_command make_text_command(
    std::string node_id,
    quiz_vulkan::render::render_rect bounds,
    std::string text)
{
    using namespace quiz_vulkan::render;

    return render_draw_command{
        .type = render_draw_command_type::text,
        .node_id = std::move(node_id),
        .parent_node_id = {},
        .depth = 0,
        .bounds = bounds,
        .content_bounds = bounds,
        .paint = render_paint{.source = "white", .color = render_color{1.0f, 1.0f, 1.0f, 1.0f}},
        .border_radius = 0.0f,
        .text_runs = {render_text_run{.text = std::move(text), .style_token = "title"}},
        .image = {},
    };
}

quiz_vulkan::render::render_draw_command make_image_command(
    std::string node_id,
    quiz_vulkan::render::render_rect bounds,
    std::string uri)
{
    using namespace quiz_vulkan::render;

    return render_draw_command{
        .type = render_draw_command_type::image,
        .node_id = std::move(node_id),
        .parent_node_id = {},
        .depth = 0,
        .bounds = bounds,
        .content_bounds = bounds,
        .paint = render_paint{.source = "image", .color = render_color{1.0f, 1.0f, 1.0f, 1.0f}},
        .border_radius = 0.0f,
        .text_runs = {},
        .image = render_image_ref{.uri = std::move(uri), .alt_text = "fixture", .aspect_ratio = 1.0f},
    };
}

void test_draw_list_submission_counts_generic_work()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "panel",
        render_rect{20.0f, 16.0f, 260.0f, 120.0f},
        render_color{0.14f, 0.22f, 0.31f, 1.0f}));
    draw_list.commands.push_back(make_text_command(
        "panel_title",
        render_rect{32.0f, 28.0f, 236.0f, 24.0f},
        "Renderer smoke"));
    draw_list.commands.push_back(make_image_command(
        "panel_image",
        render_rect{32.0f, 56.0f, 96.0f, 64.0f},
        "fixture://renderer/card.png"));

    vulkan_renderer renderer;
    renderer.submit(draw_list);

    const vulkan_renderer_frame_stats& stats = renderer.last_frame_stats();
    require(stats.command_count == 3, "renderer consumed explicit render command count");
    require(stats.draw_call_count == 3, "quad, text, and image are draw calls");
    require(stats.visible_draw_call_count == 3, "all test draw calls are visible");
    require(stats.quad_count == 1, "renderer counts generic quads");
    require(stats.text_count == 1, "renderer counts generic text commands");
    require(stats.image_count == 1, "renderer counts generic image commands");
    require(stats.text_run_count == 1, "renderer counts text runs without owning text layout");
    require(stats.text_character_count == std::string{"Renderer smoke"}.size(), "renderer counts text payload bytes");

    const vulkan_renderer_frame_summary& summary = renderer.last_frame_summary();
    require(summary.used_cpu_fallback(), "current backend reports CPU fallback");
    require(summary.nonblank(), "generic draw list shades fallback pixels");
    require(summary.surface_width == renderer.options().fallback_surface_width, "surface width is recorded");
    require(summary.surface_height == renderer.options().fallback_surface_height, "surface height is recorded");
    require(summary.discarded_draw_call_count == 0, "all explicit draw calls are drawable");
    require(count_nonzero_framebuffer_pixels(renderer.last_framebuffer()) > 0, "fallback framebuffer receives pixels");

    renderer.clear();
    require(renderer.last_draw_list().empty(), "clear drops the retained draw list");
    require(renderer.last_frame_stats().empty(), "clear drops frame stats");
    require(!renderer.last_frame_summary().nonblank(), "clear drops summary coverage");
    require(renderer.last_framebuffer().rgba.empty(), "clear drops framebuffer bytes");
}

void test_cpu_fallback_clips_and_discards()
{
    using namespace quiz_vulkan::render;

    render_draw_command clip{
        .type = render_draw_command_type::push_clip,
        .node_id = "clip",
        .parent_node_id = {},
        .depth = 0,
        .bounds = render_rect{0.0f, 0.0f, 50.0f, 50.0f},
        .content_bounds = render_rect{0.0f, 0.0f, 50.0f, 50.0f},
        .paint = {},
        .border_radius = 0.0f,
        .text_runs = {},
        .image = {},
    };
    render_draw_command quad = make_quad_command(
        "quad",
        render_rect{25.0f, 25.0f, 50.0f, 50.0f},
        render_color{0.0f, 1.0f, 0.0f, 1.0f});
    render_draw_command transparent = make_quad_command(
        "transparent",
        render_rect{10.0f, 10.0f, 10.0f, 10.0f},
        render_color{0.0f, 0.0f, 0.0f, 0.0f});
    render_draw_command pop{
        .type = render_draw_command_type::pop_clip,
        .node_id = "clip",
        .parent_node_id = {},
        .depth = 0,
        .bounds = clip.bounds,
        .content_bounds = clip.content_bounds,
        .paint = {},
        .border_radius = 0.0f,
        .text_runs = {},
        .image = {},
    };

    vulkan_renderer_options options{
        .viewport = render_rect{0.0f, 0.0f, 100.0f, 100.0f},
        .fallback_surface_width = 10,
        .fallback_surface_height = 10,
        .prefer_vulkan = true,
    };

    vulkan_renderer renderer(options);
    renderer.submit(std::vector<render_draw_command>{clip, quad, transparent, pop});

    const vulkan_renderer_frame_stats& stats = renderer.last_frame_stats();
    require(stats.command_count == 4, "clip test command count");
    require(stats.draw_call_count == 2, "only quads are draw calls");
    require(stats.visible_draw_call_count == 1, "transparent quad is not visible");
    require(stats.push_clip_count == 1, "push clip counted");
    require(stats.pop_clip_count == 1, "pop clip counted");

    const vulkan_renderer_frame_summary& summary = renderer.last_frame_summary();
    require(summary.used_cpu_fallback(), "clip test uses CPU fallback");
    require(summary.nonblank(), "visible clipped quad shades pixels");
    require(summary.clipped_draw_call_count == 1, "quad is clipped by clip scope");
    require(summary.discarded_draw_call_count == 1, "transparent quad is discarded");
    require(summary.shaded_pixel_count == 9, "clipped quad shades expected pixel area");

    const vulkan_renderer_framebuffer& framebuffer = renderer.last_framebuffer();
    require(framebuffer.width == options.fallback_surface_width, "framebuffer records fallback width");
    require(framebuffer.height == options.fallback_surface_height, "framebuffer records fallback height");
    require(framebuffer.rgba.size() == framebuffer.width * framebuffer.height * 4, "framebuffer stores RGBA bytes");
    require(count_nonzero_framebuffer_pixels(framebuffer) == 9, "only clipped visible quad pixels are colored");
    require(framebuffer_pixel_is(framebuffer, 2, 2, 0, 255, 0, 255), "visible clipped quad writes green RGBA");
    require(framebuffer_pixel_is(framebuffer, 1, 1, 0, 0, 0, 0), "transparent command does not color pixels");
    require(framebuffer_pixel_is(framebuffer, 5, 5, 0, 0, 0, 0), "clipped command does not color out-of-clip pixels");
}

void test_degenerate_surface_discards_draw_calls()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{0.0f, 0.0f, 100.0f, 100.0f},
        render_color{1.0f, 0.0f, 0.0f, 1.0f}));

    vulkan_renderer renderer(vulkan_renderer_options{
        .viewport = render_rect{0.0f, 0.0f, 0.0f, 100.0f},
        .fallback_surface_width = 10,
        .fallback_surface_height = 10,
        .prefer_vulkan = true,
    });
    renderer.submit(draw_list);

    require(renderer.last_frame_stats().draw_call_count == 1, "degenerate viewport still counts submitted draw call");
    require(!renderer.last_frame_summary().nonblank(), "degenerate viewport cannot shade pixels");
    require(renderer.last_frame_summary().discarded_draw_call_count == 1, "degenerate viewport discards draw call");
    require(count_nonzero_framebuffer_pixels(renderer.last_framebuffer()) == 0, "degenerate viewport leaves framebuffer blank");
}

} // namespace

int main()
{
    test_draw_list_submission_counts_generic_work();
    test_cpu_fallback_clips_and_discards();
    test_degenerate_surface_discards_draw_calls();
    return 0;
}
