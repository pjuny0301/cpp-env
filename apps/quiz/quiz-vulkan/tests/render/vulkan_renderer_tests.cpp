#include "render/vulkan/vulkan_renderer.h"
#include "render/vulkan/vulkan_backend_adapter.h"
#include "render/vulkan/vulkan_frame_plan.h"

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <string_view>
#include <string>
#include <utility>
#include <vector>

namespace {

class fake_vulkan_backend_device final : public quiz_vulkan::render::vulkan_backend::vulkan_backend_device_interface {
public:
    explicit fake_vulkan_backend_device(quiz_vulkan::render::vulkan_backend::vulkan_surface_extent surface)
        : surface_(surface)
    {
    }

    quiz_vulkan::render::vulkan_backend::vulkan_surface_extent current_surface_extent() const override
    {
        return surface_;
    }

    bool begin_frame(quiz_vulkan::render::vulkan_backend::vulkan_surface_extent surface) override
    {
        calls.push_back("begin");
        begun_surface = surface;
        return begin_succeeds;
    }

    bool record_frame_commands(const quiz_vulkan::render::vulkan_backend::vulkan_frame_plan& plan) override
    {
        calls.push_back("record");
        recorded_plan = plan;
        return record_succeeds;
    }

    bool submit_frame() override
    {
        calls.push_back("submit");
        return submit_succeeds;
    }

    bool present_frame() override
    {
        calls.push_back("present");
        return present_succeeds;
    }

    bool begin_succeeds = true;
    bool record_succeeds = true;
    bool submit_succeeds = true;
    bool present_succeeds = true;
    quiz_vulkan::render::vulkan_backend::vulkan_surface_extent begun_surface;
    quiz_vulkan::render::vulkan_backend::vulkan_frame_plan recorded_plan;
    std::vector<std::string> calls;

private:
    quiz_vulkan::render::vulkan_backend::vulkan_surface_extent surface_;
};

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

void test_vulkan_backend_fallback_reason_names_are_stable()
{
    using namespace quiz_vulkan::render;

    require(
        vulkan_backend::fallback_reason_name(vulkan_backend::vulkan_backend_fallback_reason::none)
            == std::string_view{"none"},
        "fallback reason name for none is stable");
    require(
        vulkan_backend::fallback_reason_name(vulkan_backend::vulkan_backend_fallback_reason::not_requested)
            == std::string_view{"not_requested"},
        "fallback reason name for not requested is stable");
    require(
        vulkan_backend::fallback_reason_name(vulkan_backend::vulkan_backend_fallback_reason::surface_unavailable)
            == std::string_view{"surface_unavailable"},
        "fallback reason name for surface unavailable is stable");
    require(
        vulkan_backend::fallback_reason_name(vulkan_backend::vulkan_backend_fallback_reason::viewport_unavailable)
            == std::string_view{"viewport_unavailable"},
        "fallback reason name for viewport unavailable is stable");
    require(
        vulkan_backend::fallback_reason_name(vulkan_backend::vulkan_backend_fallback_reason::begin_frame_failed)
            == std::string_view{"begin_frame_failed"},
        "fallback reason name for begin frame failure is stable");
    require(
        vulkan_backend::fallback_reason_name(vulkan_backend::vulkan_backend_fallback_reason::record_commands_failed)
            == std::string_view{"record_commands_failed"},
        "fallback reason name for command recording failure is stable");
    require(
        vulkan_backend::fallback_reason_name(vulkan_backend::vulkan_backend_fallback_reason::submit_frame_failed)
            == std::string_view{"submit_frame_failed"},
        "fallback reason name for submit failure is stable");
    require(
        vulkan_backend::fallback_reason_name(vulkan_backend::vulkan_backend_fallback_reason::present_frame_failed)
            == std::string_view{"present_frame_failed"},
        "fallback reason name for present failure is stable");
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
    require(summary.backend_fallback_required, "renderer summary exposes backend fallback requirement");
    require(!summary.backend_surface_ready, "renderer summary exposes missing backend surface");
    require(!summary.backend_frame_begun, "renderer summary exposes frame begin status");
    require(!summary.backend_commands_recorded, "renderer summary exposes command recording status");
    require(!summary.backend_frame_submitted, "renderer summary exposes frame submit status");
    require(!summary.backend_frame_presented, "renderer summary exposes frame present status");
    require(summary.backend_attempted, "renderer summary exposes backend attempt status");
    require(summary.backend_planned_batch_count == 0, "renderer summary exposes backend planned batch count");
    require(summary.backend_recorded_batch_count == 0, "renderer summary exposes backend recorded batch count");
    require(summary.backend_surface_width == 0, "renderer summary exposes backend surface width");
    require(summary.backend_surface_height == 0, "renderer summary exposes backend surface height");
    require(
        summary.backend_fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::surface_unavailable,
        "renderer summary exposes backend fallback reason");

    const vulkan_backend::vulkan_backend_frame_result& backend_result = renderer.last_backend_frame_result();
    require(backend_result.fallback_required, "renderer retains backend fallback requirement");
    require(backend_result.attempted, "renderer retains backend attempt status");
    require(!backend_result.surface_ready, "renderer retains backend surface readiness");
    require(!backend_result.frame_begun, "renderer retains backend begin status");
    require(!backend_result.commands_recorded, "renderer retains backend record status");
    require(!backend_result.frame_submitted, "renderer retains backend submit status");
    require(!backend_result.frame_presented, "renderer retains backend present status");
    require(backend_result.planned_batch_count == 0, "renderer retains backend planned batch count");
    require(backend_result.recorded_batch_count == 0, "renderer retains backend recorded batch count");
    require(
        backend_result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::surface_unavailable,
        "renderer retains backend fallback reason");
    require(count_nonzero_framebuffer_pixels(renderer.last_framebuffer()) > 0, "fallback framebuffer receives pixels");

    renderer.clear();
    require(renderer.last_draw_list().empty(), "clear drops the retained draw list");
    require(renderer.last_frame_stats().empty(), "clear drops frame stats");
    require(!renderer.last_frame_summary().nonblank(), "clear drops summary coverage");
    require(renderer.last_frame_summary().backend_fallback_required, "clear resets backend summary diagnostics");
    require(!renderer.last_frame_summary().backend_attempted, "clear resets backend attempt diagnostics");
    require(
        renderer.last_frame_summary().backend_fallback_reason
            == vulkan_backend::vulkan_backend_fallback_reason::not_requested,
        "clear resets backend fallback reason");
    require(renderer.last_backend_frame_result().fallback_required, "clear resets retained backend result");
    require(!renderer.last_backend_frame_result().attempted, "clear resets retained backend attempt status");
    require(
        renderer.last_backend_frame_result().fallback_reason
            == vulkan_backend::vulkan_backend_fallback_reason::not_requested,
        "clear resets retained backend fallback reason");
    require(renderer.last_framebuffer().rgba.empty(), "clear drops framebuffer bytes");
}

void test_renderer_backend_diagnostics_report_vulkan_not_requested()
{
    using namespace quiz_vulkan::render;

    vulkan_renderer renderer(vulkan_renderer_options{
        .viewport = render_rect{0.0f, 0.0f, 100.0f, 100.0f},
        .fallback_surface_width = 10,
        .fallback_surface_height = 10,
        .prefer_vulkan = false,
    });
    renderer.submit(std::vector<render_draw_command>{make_quad_command(
        "quad",
        render_rect{0.0f, 0.0f, 100.0f, 100.0f},
        render_color{1.0f, 0.0f, 0.0f, 1.0f})});

    const vulkan_renderer_frame_summary& summary = renderer.last_frame_summary();
    require(summary.used_cpu_fallback(), "renderer keeps CPU fallback when Vulkan is not requested");
    require(summary.nonblank(), "renderer still shades fallback pixels when Vulkan is not requested");
    require(!summary.backend_attempted, "renderer summary records that Vulkan was not attempted");
    require(summary.backend_fallback_required, "renderer summary still requires fallback when Vulkan is not requested");
    require(
        summary.backend_fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::not_requested,
        "renderer summary records Vulkan not requested fallback reason");

    const vulkan_backend::vulkan_backend_frame_result& backend_result = renderer.last_backend_frame_result();
    require(!backend_result.attempted, "renderer backend result records Vulkan was not attempted");
    require(backend_result.fallback_required, "renderer backend result keeps fallback required");
    require(
        backend_result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::not_requested,
        "renderer backend result records Vulkan not requested fallback reason");
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

void test_vulkan_frame_plan_builds_scissored_batches_from_render_contracts()
{
    using namespace quiz_vulkan::render;

    render_draw_command clip{
        .type = render_draw_command_type::push_clip,
        .node_id = "clip",
        .parent_node_id = {},
        .depth = 0,
        .bounds = render_rect{10.0f, 20.0f, 50.0f, 40.0f},
        .content_bounds = render_rect{10.0f, 20.0f, 50.0f, 40.0f},
        .paint = {},
        .border_radius = 0.0f,
        .text_runs = {},
        .image = {},
    };
    render_draw_command quad = make_quad_command(
        "quad",
        render_rect{0.0f, 0.0f, 30.0f, 40.0f},
        render_color{1.0f, 0.0f, 0.0f, 1.0f});
    render_draw_command text = make_text_command(
        "text",
        render_rect{20.0f, 30.0f, 10.0f, 10.0f},
        "batch");
    render_draw_command transparent = make_quad_command(
        "transparent",
        render_rect{20.0f, 30.0f, 10.0f, 10.0f},
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

    render_draw_list draw_list;
    draw_list.commands = {clip, quad, text, transparent, pop};

    const vulkan_backend::vulkan_frame_plan plan = vulkan_backend::build_vulkan_frame_plan(
        draw_list,
        vulkan_backend::vulkan_frame_plan_options{
            .viewport = render_rect{0.0f, 0.0f, 100.0f, 100.0f},
            .surface_width = 10,
            .surface_height = 10,
        });

    require(plan.batches.size() == 2, "frame plan emits drawable quad and text batches");
    require(plan.clipped_draw_call_count == 1, "frame plan records clipped backend batch");
    require(plan.discarded_draw_call_count == 1, "frame plan discards transparent backend batch");

    const vulkan_backend::vulkan_draw_batch& quad_batch = plan.batches[0];
    require(quad_batch.kind == vulkan_backend::vulkan_batch_kind::quad, "first backend batch is a quad");
    require(quad_batch.command_index == 1, "quad backend batch keeps source command index");
    require(quad_batch.clipped_bounds.x == 10.0f, "quad clipped x");
    require(quad_batch.clipped_bounds.y == 20.0f, "quad clipped y");
    require(quad_batch.clipped_bounds.width == 20.0f, "quad clipped width");
    require(quad_batch.clipped_bounds.height == 20.0f, "quad clipped height");
    require(quad_batch.scissor.x == 1, "quad scissor x maps to surface");
    require(quad_batch.scissor.y == 2, "quad scissor y maps to surface");
    require(quad_batch.scissor.width == 2, "quad scissor width maps to surface");
    require(quad_batch.scissor.height == 2, "quad scissor height maps to surface");
    require(quad_batch.vertices[0].x == 10.0f, "quad vertex 0 x uses clipped bounds");
    require(quad_batch.vertices[0].y == 20.0f, "quad vertex 0 y uses clipped bounds");
    require(quad_batch.vertices[2].x == 30.0f, "quad vertex 2 x uses clipped bounds");
    require(quad_batch.vertices[2].y == 40.0f, "quad vertex 2 y uses clipped bounds");

    const vulkan_backend::vulkan_draw_batch& text_batch = plan.batches[1];
    require(text_batch.kind == vulkan_backend::vulkan_batch_kind::text, "text command becomes a text backend batch");
    require(text_batch.command_index == 2, "text backend batch keeps source command index");
    require(text_batch.scissor.x == 2, "text scissor x maps to surface");
    require(text_batch.scissor.y == 3, "text scissor y maps to surface");
    require(text_batch.scissor.width == 1, "text scissor width maps to surface");
    require(text_batch.scissor.height == 1, "text scissor height maps to surface");
}

void test_vulkan_backend_adapter_completes_fake_device_lifecycle()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{8.0f, 16.0f, 16.0f, 16.0f},
        render_color{0.25f, 0.5f, 0.75f, 1.0f}));

    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 64, .height = 32});
    const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
        device,
        draw_list,
        render_rect{0.0f, 0.0f, 64.0f, 64.0f});

    require(result.completed(), "fake backend completes frame lifecycle");
    require(result.attempted, "fake backend records lifecycle attempt");
    require(!result.fallback_required, "completed fake backend frame does not require fallback");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::none,
        "completed fake backend frame has no fallback reason");
    require(result.surface_ready, "fake backend surface is ready");
    require(result.frame_begun, "fake backend begins a frame");
    require(result.commands_recorded, "fake backend records frame commands");
    require(result.frame_submitted, "fake backend submits frame");
    require(result.frame_presented, "fake backend presents frame");
    require(result.planned_batch_count == 1, "fake backend receives one planned batch");
    require(result.recorded_batch_count == 1, "fake backend records one batch");
    require(result.clipped_draw_call_count == 0, "unclipped fake backend batch is not clipped");
    require(result.discarded_draw_call_count == 0, "visible fake backend batch is not discarded");

    require(device.calls.size() == 4, "fake backend lifecycle call count");
    require(device.calls[0] == "begin", "fake backend begins before recording");
    require(device.calls[1] == "record", "fake backend records before submit");
    require(device.calls[2] == "submit", "fake backend submits before present");
    require(device.calls[3] == "present", "fake backend presents last");
    require(device.begun_surface.width == 64, "fake backend begin receives surface width");
    require(device.begun_surface.height == 32, "fake backend begin receives surface height");

    require(device.recorded_plan.surface_width == 64, "recorded frame plan uses device surface width");
    require(device.recorded_plan.surface_height == 32, "recorded frame plan uses device surface height");
    require(device.recorded_plan.batches.size() == 1, "recorded frame plan keeps one batch");
    const vulkan_backend::vulkan_draw_batch& batch = device.recorded_plan.batches.front();
    require(batch.kind == vulkan_backend::vulkan_batch_kind::quad, "recorded batch remains a quad");
    require(batch.scissor.x == 8, "recorded batch scissor x maps to device surface");
    require(batch.scissor.y == 8, "recorded batch scissor y maps to device surface");
    require(batch.scissor.width == 16, "recorded batch scissor width maps to device surface");
    require(batch.scissor.height == 8, "recorded batch scissor height maps to device surface");
}

void test_vulkan_backend_adapter_preserves_plan_diagnostics()
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

    render_draw_list draw_list;
    draw_list.commands = {clip, quad, transparent, pop};

    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 10, .height = 10});
    const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
        device,
        draw_list,
        render_rect{0.0f, 0.0f, 100.0f, 100.0f});

    require(result.completed(), "backend completes clipped diagnostic frame");
    require(result.planned_batch_count == 1, "backend reports drawable clipped batch count");
    require(result.recorded_batch_count == 1, "backend reports recorded clipped batch count");
    require(result.clipped_draw_call_count == 1, "backend reports clipped draw call count");
    require(result.discarded_draw_call_count == 1, "backend reports discarded draw call count");
    require(device.recorded_plan.batches.size() == 1, "recorded plan contains only visible clipped batch");
    require(device.recorded_plan.clipped_draw_call_count == 1, "recorded plan preserves clipped draw count");
    require(device.recorded_plan.discarded_draw_call_count == 1, "recorded plan preserves discarded draw count");

    const vulkan_backend::vulkan_draw_batch& batch = device.recorded_plan.batches.front();
    require(batch.command_index == 1, "recorded clipped batch keeps source command index");
    require(batch.clipped_bounds.x == 25.0f, "recorded clipped batch x");
    require(batch.clipped_bounds.y == 25.0f, "recorded clipped batch y");
    require(batch.clipped_bounds.width == 25.0f, "recorded clipped batch width");
    require(batch.clipped_bounds.height == 25.0f, "recorded clipped batch height");
    require(batch.scissor.x == 2, "recorded clipped batch scissor x");
    require(batch.scissor.y == 2, "recorded clipped batch scissor y");
    require(batch.scissor.width == 3, "recorded clipped batch scissor width");
    require(batch.scissor.height == 3, "recorded clipped batch scissor height");
}

void test_vulkan_backend_adapter_completes_empty_frame()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 16, .height = 16});
    const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
        device,
        draw_list,
        render_rect{0.0f, 0.0f, 100.0f, 100.0f});

    require(result.completed(), "backend completes empty frame lifecycle");
    require(result.attempted, "backend records attempted empty frame");
    require(!result.fallback_required, "empty frame does not require fallback with fake device");
    require(result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::none, "empty frame has no fallback reason");
    require(result.surface_ready, "backend surface is ready for empty frame");
    require(result.frame_begun, "backend begins empty frame");
    require(result.commands_recorded, "backend records empty frame commands");
    require(result.frame_submitted, "backend submits empty frame");
    require(result.frame_presented, "backend presents empty frame");
    require(result.planned_batch_count == 0, "empty frame has no planned batches");
    require(result.recorded_batch_count == 0, "empty frame has no recorded batches");
    require(result.clipped_draw_call_count == 0, "empty frame has no clipped draw calls");
    require(result.discarded_draw_call_count == 0, "empty frame has no discarded draw calls");
    require(device.recorded_plan.empty(), "empty frame records empty plan");
    require(device.calls.size() == 4, "empty frame still runs lifecycle");
}

void test_vulkan_backend_adapter_completes_all_discarded_frame()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "transparent",
        render_rect{0.0f, 0.0f, 100.0f, 100.0f},
        render_color{1.0f, 1.0f, 1.0f, 0.0f}));

    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 16, .height = 16});
    const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
        device,
        draw_list,
        render_rect{0.0f, 0.0f, 100.0f, 100.0f});

    require(result.completed(), "backend completes all-discarded frame lifecycle");
    require(!result.fallback_required, "all-discarded frame does not require fallback with fake device");
    require(result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::none, "all-discarded frame has no fallback reason");
    require(result.planned_batch_count == 0, "all-discarded frame has no planned batches");
    require(result.recorded_batch_count == 0, "all-discarded frame has no recorded batches");
    require(result.clipped_draw_call_count == 0, "all-discarded frame has no clipped draw calls");
    require(result.discarded_draw_call_count == 1, "all-discarded frame reports discarded draw call");
    require(device.recorded_plan.empty(), "all-discarded frame records empty plan");
    require(device.recorded_plan.discarded_draw_call_count == 1, "recorded all-discarded plan preserves discarded count");
    require(device.calls.size() == 4, "all-discarded frame still runs lifecycle");
}

void test_vulkan_backend_adapter_falls_back_without_surface()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{0.0f, 0.0f, 100.0f, 100.0f},
        render_color{1.0f, 1.0f, 1.0f, 1.0f}));

    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{});
    const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
        device,
        draw_list,
        render_rect{0.0f, 0.0f, 100.0f, 100.0f});

    require(!result.completed(), "backend cannot complete without a surface");
    require(result.attempted, "backend records attempted frame without surface");
    require(result.fallback_required, "backend requires fallback without a surface");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::surface_unavailable,
        "backend reports surface unavailable fallback reason");
    require(!result.surface_ready, "zero surface is not ready");
    require(!result.frame_begun, "backend does not begin frame without surface");
    require(!result.commands_recorded, "backend does not record without surface");
    require(!result.frame_submitted, "backend does not submit without surface");
    require(!result.frame_presented, "backend does not present without surface");
    require(result.planned_batch_count == 0, "backend does not build batches without surface");
    require(result.recorded_batch_count == 0, "backend does not record batches without surface");
    require(device.calls.empty(), "backend does not call device lifecycle without surface");
}

void test_vulkan_backend_adapter_falls_back_without_viewport()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{0.0f, 0.0f, 100.0f, 100.0f},
        render_color{1.0f, 1.0f, 1.0f, 1.0f}));

    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 16, .height = 16});
    const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
        device,
        draw_list,
        render_rect{0.0f, 0.0f, 0.0f, 100.0f});

    require(!result.completed(), "backend cannot complete without a viewport");
    require(result.attempted, "backend records attempted frame without viewport");
    require(result.fallback_required, "backend requires fallback without viewport");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::viewport_unavailable,
        "backend reports viewport unavailable fallback reason");
    require(result.surface.width == 16, "backend records available surface width without viewport");
    require(result.surface.height == 16, "backend records available surface height without viewport");
    require(!result.surface_ready, "backend surface is not frame-ready without viewport");
    require(!result.frame_begun, "backend does not begin frame without viewport");
    require(!result.commands_recorded, "backend does not record without viewport");
    require(!result.frame_submitted, "backend does not submit without viewport");
    require(!result.frame_presented, "backend does not present without viewport");
    require(result.planned_batch_count == 0, "backend does not build batches without viewport");
    require(result.recorded_batch_count == 0, "backend does not record batches without viewport");
    require(device.calls.empty(), "backend does not call device lifecycle without viewport");
}

void test_vulkan_backend_adapter_falls_back_when_begin_fails()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{0.0f, 0.0f, 100.0f, 100.0f},
        render_color{1.0f, 1.0f, 1.0f, 1.0f}));

    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 16, .height = 16});
    device.begin_succeeds = false;
    const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
        device,
        draw_list,
        render_rect{0.0f, 0.0f, 100.0f, 100.0f});

    require(!result.completed(), "backend cannot complete when frame begin fails");
    require(result.attempted, "backend records attempted frame when begin fails");
    require(result.fallback_required, "backend requires fallback when begin fails");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::begin_frame_failed,
        "backend reports begin fallback reason");
    require(result.surface_ready, "backend had a surface before begin failed");
    require(!result.frame_begun, "backend reports failed frame begin");
    require(!result.commands_recorded, "backend does not record commands after failed begin");
    require(!result.frame_submitted, "backend does not submit after failed begin");
    require(!result.frame_presented, "backend does not present after failed begin");
    require(result.planned_batch_count == 1, "backend reports planned batch count before begin failure");
    require(result.recorded_batch_count == 0, "backend does not record batches after begin failure");
    require(device.calls.size() == 1, "backend stops lifecycle after failed begin");
    require(device.calls[0] == "begin", "backend calls begin before stopping");
}

void test_vulkan_backend_adapter_falls_back_when_recording_fails()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{0.0f, 0.0f, 100.0f, 100.0f},
        render_color{1.0f, 1.0f, 1.0f, 1.0f}));

    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 16, .height = 16});
    device.record_succeeds = false;
    const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
        device,
        draw_list,
        render_rect{0.0f, 0.0f, 100.0f, 100.0f});

    require(!result.completed(), "backend cannot complete when command recording fails");
    require(result.attempted, "backend records attempted frame when command recording fails");
    require(result.fallback_required, "backend requires fallback when command recording fails");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::record_commands_failed,
        "backend reports command recording fallback reason");
    require(result.surface_ready, "backend had a surface before recording failed");
    require(result.frame_begun, "backend began frame before recording failed");
    require(!result.commands_recorded, "backend reports failed command recording");
    require(!result.frame_submitted, "backend does not submit after failed recording");
    require(!result.frame_presented, "backend does not present after failed recording");
    require(result.planned_batch_count == 1, "backend still reports planned batch count before failure");
    require(result.recorded_batch_count == 0, "backend does not count batches after failed recording");
    require(device.calls.size() == 2, "backend stops lifecycle after failed recording");
    require(device.calls[0] == "begin", "backend begins before failed recording");
    require(device.calls[1] == "record", "backend records before stopping");
}

void test_vulkan_backend_adapter_falls_back_when_submit_fails()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{0.0f, 0.0f, 100.0f, 100.0f},
        render_color{1.0f, 1.0f, 1.0f, 1.0f}));

    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 16, .height = 16});
    device.submit_succeeds = false;
    const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
        device,
        draw_list,
        render_rect{0.0f, 0.0f, 100.0f, 100.0f});

    require(!result.completed(), "backend cannot complete when frame submit fails");
    require(result.attempted, "backend records attempted frame when submit fails");
    require(result.fallback_required, "backend requires fallback when submit fails");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::submit_frame_failed,
        "backend reports submit fallback reason");
    require(result.surface_ready, "backend had a surface before submit failed");
    require(result.frame_begun, "backend began frame before submit failed");
    require(result.commands_recorded, "backend recorded commands before submit failed");
    require(!result.frame_submitted, "backend reports failed frame submit");
    require(!result.frame_presented, "backend does not present after failed submit");
    require(result.planned_batch_count == 1, "backend reports planned batch count before submit failure");
    require(result.recorded_batch_count == 1, "backend reports recorded batch count before submit failure");
    require(device.calls.size() == 3, "backend stops lifecycle after failed submit");
    require(device.calls[0] == "begin", "backend begins before failed submit");
    require(device.calls[1] == "record", "backend records before failed submit");
    require(device.calls[2] == "submit", "backend submits before stopping");
}

void test_vulkan_backend_adapter_falls_back_when_present_fails()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{0.0f, 0.0f, 100.0f, 100.0f},
        render_color{1.0f, 1.0f, 1.0f, 1.0f}));

    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 16, .height = 16});
    device.present_succeeds = false;
    const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
        device,
        draw_list,
        render_rect{0.0f, 0.0f, 100.0f, 100.0f});

    require(!result.completed(), "backend cannot complete when presentation fails");
    require(result.attempted, "backend records attempted frame when presentation fails");
    require(result.fallback_required, "backend requires fallback when presentation fails");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::present_frame_failed,
        "backend reports presentation fallback reason");
    require(result.surface_ready, "backend had a surface before presentation failed");
    require(result.frame_begun, "backend began frame before presentation failed");
    require(result.commands_recorded, "backend recorded commands before presentation failed");
    require(result.frame_submitted, "backend submitted frame before presentation failed");
    require(!result.frame_presented, "backend reports failed presentation");
    require(result.planned_batch_count == 1, "backend reports planned batch count before presentation failure");
    require(result.recorded_batch_count == 1, "backend reports recorded batch count before presentation failure");
    require(device.calls.size() == 4, "backend reaches present call before failing");
    require(device.calls[0] == "begin", "backend begins before presentation failure");
    require(device.calls[1] == "record", "backend records before presentation failure");
    require(device.calls[2] == "submit", "backend submits before presentation failure");
    require(device.calls[3] == "present", "backend presents before reporting failure");
}

} // namespace

int main()
{
    test_vulkan_backend_fallback_reason_names_are_stable();
    test_draw_list_submission_counts_generic_work();
    test_renderer_backend_diagnostics_report_vulkan_not_requested();
    test_cpu_fallback_clips_and_discards();
    test_degenerate_surface_discards_draw_calls();
    test_vulkan_frame_plan_builds_scissored_batches_from_render_contracts();
    test_vulkan_backend_adapter_completes_fake_device_lifecycle();
    test_vulkan_backend_adapter_preserves_plan_diagnostics();
    test_vulkan_backend_adapter_completes_empty_frame();
    test_vulkan_backend_adapter_completes_all_discarded_frame();
    test_vulkan_backend_adapter_falls_back_without_surface();
    test_vulkan_backend_adapter_falls_back_without_viewport();
    test_vulkan_backend_adapter_falls_back_when_begin_fails();
    test_vulkan_backend_adapter_falls_back_when_recording_fails();
    test_vulkan_backend_adapter_falls_back_when_submit_fails();
    test_vulkan_backend_adapter_falls_back_when_present_fails();
    return 0;
}
