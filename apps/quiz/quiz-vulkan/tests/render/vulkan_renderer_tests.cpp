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

    quiz_vulkan::render::vulkan_backend::vulkan_backend_lifecycle_readiness current_lifecycle_readiness() const override
    {
        return lifecycle;
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
    quiz_vulkan::render::vulkan_backend::vulkan_backend_lifecycle_readiness lifecycle{
        .instance_ready = true,
        .device_ready = true,
        .swapchain_ready = true,
        .pipeline_ready = true,
        .command_recorder_ready = true,
    };
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
        vulkan_backend::fallback_reason_name(vulkan_backend::vulkan_backend_fallback_reason::instance_unavailable)
            == std::string_view{"instance_unavailable"},
        "fallback reason name for unavailable instance is stable");
    require(
        vulkan_backend::fallback_reason_name(vulkan_backend::vulkan_backend_fallback_reason::device_unavailable)
            == std::string_view{"device_unavailable"},
        "fallback reason name for unavailable device is stable");
    require(
        vulkan_backend::fallback_reason_name(vulkan_backend::vulkan_backend_fallback_reason::swapchain_unavailable)
            == std::string_view{"swapchain_unavailable"},
        "fallback reason name for unavailable swapchain is stable");
    require(
        vulkan_backend::fallback_reason_name(vulkan_backend::vulkan_backend_fallback_reason::pipeline_unavailable)
            == std::string_view{"pipeline_unavailable"},
        "fallback reason name for unavailable pipeline is stable");
    require(
        vulkan_backend::fallback_reason_name(
            vulkan_backend::vulkan_backend_fallback_reason::command_recorder_unavailable)
            == std::string_view{"command_recorder_unavailable"},
        "fallback reason name for unavailable command recorder is stable");
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

void test_vulkan_backend_frame_stage_names_are_stable()
{
    using namespace quiz_vulkan::render;

    require(
        vulkan_backend::frame_stage_name(vulkan_backend::vulkan_backend_frame_stage::not_started)
            == std::string_view{"not_started"},
        "frame stage name for not started is stable");
    require(
        vulkan_backend::frame_stage_name(vulkan_backend::vulkan_backend_frame_stage::backend_attempted)
            == std::string_view{"backend_attempted"},
        "frame stage name for backend attempted is stable");
    require(
        vulkan_backend::frame_stage_name(vulkan_backend::vulkan_backend_frame_stage::lifecycle_ready)
            == std::string_view{"lifecycle_ready"},
        "frame stage name for lifecycle ready is stable");
    require(
        vulkan_backend::frame_stage_name(vulkan_backend::vulkan_backend_frame_stage::surface_extent_ready)
            == std::string_view{"surface_extent_ready"},
        "frame stage name for surface extent ready is stable");
    require(
        vulkan_backend::frame_stage_name(vulkan_backend::vulkan_backend_frame_stage::frame_plan_ready)
            == std::string_view{"frame_plan_ready"},
        "frame stage name for frame plan ready is stable");
    require(
        vulkan_backend::frame_stage_name(vulkan_backend::vulkan_backend_frame_stage::frame_begun)
            == std::string_view{"frame_begun"},
        "frame stage name for frame begun is stable");
    require(
        vulkan_backend::frame_stage_name(vulkan_backend::vulkan_backend_frame_stage::commands_recorded)
            == std::string_view{"commands_recorded"},
        "frame stage name for commands recorded is stable");
    require(
        vulkan_backend::frame_stage_name(vulkan_backend::vulkan_backend_frame_stage::frame_submitted)
            == std::string_view{"frame_submitted"},
        "frame stage name for frame submitted is stable");
    require(
        vulkan_backend::frame_stage_name(vulkan_backend::vulkan_backend_frame_stage::frame_presented)
            == std::string_view{"frame_presented"},
        "frame stage name for frame presented is stable");
}

void test_vulkan_command_recorder_failure_stage_names_are_stable()
{
    using namespace quiz_vulkan::render;

    require(
        vulkan_backend::command_recorder_failure_stage_name(
            vulkan_backend::vulkan_command_recorder_failure_stage::none)
            == std::string_view{"none"},
        "command recorder failure stage name for none is stable");
    require(
        vulkan_backend::command_recorder_failure_stage_name(
            vulkan_backend::vulkan_command_recorder_failure_stage::begin_recording)
            == std::string_view{"begin_recording"},
        "command recorder failure stage name for begin is stable");
    require(
        vulkan_backend::command_recorder_failure_stage_name(
            vulkan_backend::vulkan_command_recorder_failure_stage::record_draw_batch)
            == std::string_view{"record_draw_batch"},
        "command recorder failure stage name for record is stable");
    require(
        vulkan_backend::command_recorder_failure_stage_name(
            vulkan_backend::vulkan_command_recorder_failure_stage::finish_recording)
            == std::string_view{"finish_recording"},
        "command recorder failure stage name for finish is stable");
}

void test_vulkan_shader_stage_names_are_stable()
{
    using namespace quiz_vulkan::render;

    require(
        vulkan_backend::shader_stage_name(vulkan_backend::vulkan_shader_stage::vertex)
            == std::string_view{"vertex"},
        "shader stage name for vertex is stable");
    require(
        vulkan_backend::shader_stage_name(vulkan_backend::vulkan_shader_stage::fragment)
            == std::string_view{"fragment"},
        "shader stage name for fragment is stable");
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
        summary.backend_reached_stage == vulkan_backend::vulkan_backend_frame_stage::backend_attempted,
        "renderer summary exposes reached backend stage");
    require(!summary.backend_instance_ready, "renderer summary exposes backend instance readiness");
    require(!summary.backend_device_ready, "renderer summary exposes backend device readiness");
    require(!summary.backend_swapchain_ready, "renderer summary exposes backend swapchain readiness");
    require(!summary.backend_pipeline_ready, "renderer summary exposes backend pipeline readiness");
    require(!summary.backend_command_recorder_ready, "renderer summary exposes backend command recorder readiness");
    require(!summary.backend_command_recorder_frame_open, "renderer summary exposes command recorder frame-open state");
    require(!summary.backend_command_buffer_recorded, "renderer summary exposes command buffer recorded state");
    require(!summary.backend_lifecycle_ready, "renderer summary exposes aggregate lifecycle readiness");
    require(
        summary.backend_fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::instance_unavailable,
        "renderer summary exposes backend fallback reason");

    const vulkan_backend::vulkan_backend_frame_result& backend_result = renderer.last_backend_frame_result();
    require(backend_result.fallback_required, "renderer retains backend fallback requirement");
    require(backend_result.attempted, "renderer retains backend attempt status");
    require(!backend_result.lifecycle.ready_for_frame(), "renderer retains backend lifecycle readiness");
    require(!backend_result.command_recorder.ready, "renderer retains command recorder readiness");
    require(backend_result.command_recorder.empty(), "renderer retains empty command recorder state");
    require(!backend_result.lifecycle_ready, "renderer retains aggregate lifecycle readiness");
    require(!backend_result.surface_ready, "renderer retains backend surface readiness");
    require(!backend_result.frame_begun, "renderer retains backend begin status");
    require(!backend_result.commands_recorded, "renderer retains backend record status");
    require(!backend_result.frame_submitted, "renderer retains backend submit status");
    require(!backend_result.frame_presented, "renderer retains backend present status");
    require(backend_result.planned_batch_count == 0, "renderer retains backend planned batch count");
    require(backend_result.recorded_batch_count == 0, "renderer retains backend recorded batch count");
    require(
        backend_result.reached_stage == vulkan_backend::vulkan_backend_frame_stage::backend_attempted,
        "renderer retains reached backend stage");
    require(
        backend_result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::instance_unavailable,
        "renderer retains backend fallback reason");
    require(count_nonzero_framebuffer_pixels(renderer.last_framebuffer()) > 0, "fallback framebuffer receives pixels");

    renderer.clear();
    require(renderer.last_draw_list().empty(), "clear drops the retained draw list");
    require(renderer.last_frame_stats().empty(), "clear drops frame stats");
    require(!renderer.last_frame_summary().nonblank(), "clear drops summary coverage");
    require(renderer.last_frame_summary().backend_fallback_required, "clear resets backend summary diagnostics");
    require(!renderer.last_frame_summary().backend_attempted, "clear resets backend attempt diagnostics");
    require(
        renderer.last_frame_summary().backend_reached_stage
            == vulkan_backend::vulkan_backend_frame_stage::not_started,
        "clear resets backend reached stage");
    require(
        renderer.last_frame_summary().backend_fallback_reason
            == vulkan_backend::vulkan_backend_fallback_reason::not_requested,
        "clear resets backend fallback reason");
    require(renderer.last_backend_frame_result().fallback_required, "clear resets retained backend result");
    require(!renderer.last_backend_frame_result().attempted, "clear resets retained backend attempt status");
    require(
        renderer.last_backend_frame_result().reached_stage
            == vulkan_backend::vulkan_backend_frame_stage::not_started,
        "clear resets retained backend reached stage");
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
        summary.backend_reached_stage == vulkan_backend::vulkan_backend_frame_stage::not_started,
        "renderer summary records no reached Vulkan stage when not requested");
    require(
        summary.backend_fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::not_requested,
        "renderer summary records Vulkan not requested fallback reason");

    const vulkan_backend::vulkan_backend_frame_result& backend_result = renderer.last_backend_frame_result();
    require(!backend_result.attempted, "renderer backend result records Vulkan was not attempted");
    require(backend_result.fallback_required, "renderer backend result keeps fallback required");
    require(
        backend_result.reached_stage == vulkan_backend::vulkan_backend_frame_stage::not_started,
        "renderer backend result records no reached Vulkan stage when not requested");
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

void test_vulkan_frame_plan_applies_nested_clips()
{
    using namespace quiz_vulkan::render;

    render_draw_command outer_clip{
        .type = render_draw_command_type::push_clip,
        .node_id = "outer_clip",
        .parent_node_id = {},
        .depth = 0,
        .bounds = render_rect{0.0f, 0.0f, 80.0f, 80.0f},
        .content_bounds = render_rect{0.0f, 0.0f, 80.0f, 80.0f},
        .paint = {},
        .border_radius = 0.0f,
        .text_runs = {},
        .image = {},
    };
    render_draw_command inner_clip{
        .type = render_draw_command_type::push_clip,
        .node_id = "inner_clip",
        .parent_node_id = {},
        .depth = 0,
        .bounds = render_rect{20.0f, 20.0f, 40.0f, 40.0f},
        .content_bounds = render_rect{20.0f, 20.0f, 40.0f, 40.0f},
        .paint = {},
        .border_radius = 0.0f,
        .text_runs = {},
        .image = {},
    };
    render_draw_command quad = make_quad_command(
        "nested_quad",
        render_rect{10.0f, 10.0f, 60.0f, 60.0f},
        render_color{0.4f, 0.6f, 0.8f, 1.0f});
    render_draw_command pop_inner{
        .type = render_draw_command_type::pop_clip,
        .node_id = "inner_clip",
        .parent_node_id = {},
        .depth = 0,
        .bounds = inner_clip.bounds,
        .content_bounds = inner_clip.content_bounds,
        .paint = {},
        .border_radius = 0.0f,
        .text_runs = {},
        .image = {},
    };
    render_draw_command pop_outer{
        .type = render_draw_command_type::pop_clip,
        .node_id = "outer_clip",
        .parent_node_id = {},
        .depth = 0,
        .bounds = outer_clip.bounds,
        .content_bounds = outer_clip.content_bounds,
        .paint = {},
        .border_radius = 0.0f,
        .text_runs = {},
        .image = {},
    };

    render_draw_list draw_list;
    draw_list.commands = {outer_clip, inner_clip, quad, pop_inner, pop_outer};

    const vulkan_backend::vulkan_frame_plan plan = vulkan_backend::build_vulkan_frame_plan(
        draw_list,
        vulkan_backend::vulkan_frame_plan_options{
            .viewport = render_rect{0.0f, 0.0f, 100.0f, 100.0f},
            .surface_width = 10,
            .surface_height = 10,
        });

    require(plan.batches.size() == 1, "nested clips produce one drawable batch");
    require(plan.clipped_draw_call_count == 1, "nested clips mark clipped draw call");
    require(plan.discarded_draw_call_count == 0, "nested clips do not discard visible batch");

    const vulkan_backend::vulkan_draw_batch& batch = plan.batches.front();
    require(batch.command_index == 2, "nested clipped batch keeps source command index");
    require(batch.clipped_bounds.x == 20.0f, "nested clipped batch x");
    require(batch.clipped_bounds.y == 20.0f, "nested clipped batch y");
    require(batch.clipped_bounds.width == 40.0f, "nested clipped batch width");
    require(batch.clipped_bounds.height == 40.0f, "nested clipped batch height");
    require(batch.scissor.x == 2, "nested clipped batch scissor x");
    require(batch.scissor.y == 2, "nested clipped batch scissor y");
    require(batch.scissor.width == 4, "nested clipped batch scissor width");
    require(batch.scissor.height == 4, "nested clipped batch scissor height");
}

void test_vulkan_frame_plan_ignores_unmatched_pop_clip()
{
    using namespace quiz_vulkan::render;

    render_draw_command unmatched_pop{
        .type = render_draw_command_type::pop_clip,
        .node_id = "unmatched_pop",
        .parent_node_id = {},
        .depth = 0,
        .bounds = {},
        .content_bounds = {},
        .paint = {},
        .border_radius = 0.0f,
        .text_runs = {},
        .image = {},
    };
    render_draw_command quad = make_quad_command(
        "quad",
        render_rect{10.0f, 20.0f, 30.0f, 40.0f},
        render_color{0.8f, 0.7f, 0.6f, 1.0f});

    render_draw_list draw_list;
    draw_list.commands = {unmatched_pop, quad};

    const vulkan_backend::vulkan_frame_plan plan = vulkan_backend::build_vulkan_frame_plan(
        draw_list,
        vulkan_backend::vulkan_frame_plan_options{
            .viewport = render_rect{0.0f, 0.0f, 100.0f, 100.0f},
            .surface_width = 10,
            .surface_height = 10,
        });

    require(plan.batches.size() == 1, "unmatched pop clip does not discard later batch");
    require(plan.clipped_draw_call_count == 0, "unmatched pop clip does not clip later batch");
    require(plan.discarded_draw_call_count == 0, "unmatched pop clip does not discard later batch");

    const vulkan_backend::vulkan_draw_batch& batch = plan.batches.front();
    require(batch.command_index == 1, "batch after unmatched pop keeps source command index");
    require(batch.clipped_bounds.x == 10.0f, "batch after unmatched pop x");
    require(batch.clipped_bounds.y == 20.0f, "batch after unmatched pop y");
    require(batch.clipped_bounds.width == 30.0f, "batch after unmatched pop width");
    require(batch.clipped_bounds.height == 40.0f, "batch after unmatched pop height");
    require(batch.scissor.x == 1, "batch after unmatched pop scissor x");
    require(batch.scissor.y == 2, "batch after unmatched pop scissor y");
    require(batch.scissor.width == 3, "batch after unmatched pop scissor width");
    require(batch.scissor.height == 4, "batch after unmatched pop scissor height");
}

void test_vulkan_diagnostic_command_recorder_records_planned_batches()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{10.0f, 20.0f, 30.0f, 40.0f},
        render_color{0.8f, 0.7f, 0.6f, 1.0f}));
    draw_list.commands.push_back(make_image_command(
        "image",
        render_rect{50.0f, 60.0f, 20.0f, 20.0f},
        "fixture://renderer/image.png"));

    const vulkan_backend::vulkan_frame_plan plan = vulkan_backend::build_vulkan_frame_plan(
        draw_list,
        vulkan_backend::vulkan_frame_plan_options{
            .viewport = render_rect{0.0f, 0.0f, 100.0f, 100.0f},
            .surface_width = 10,
            .surface_height = 10,
        });

    vulkan_backend::diagnostic_vulkan_command_recorder recorder;
    require(
        recorder.begin_recording(
            vulkan_backend::vulkan_surface_extent{.width = 10, .height = 10},
            plan.batches.size()),
        "diagnostic recorder begins command buffer recording");
    for (const vulkan_backend::vulkan_draw_batch& batch : plan.batches) {
        require(recorder.record_draw_batch(batch), "diagnostic recorder records each planned batch");
    }
    require(recorder.finish_recording(), "diagnostic recorder finishes command buffer recording");

    const vulkan_backend::vulkan_backend_command_recorder_state& state = recorder.recorder_state();
    require(state.completed(), "diagnostic recorder reports completed command buffer state");
    require(!state.failed(), "diagnostic recorder reports no failure after successful recording");
    require(
        state.failure_stage == vulkan_backend::vulkan_command_recorder_failure_stage::none,
        "diagnostic recorder keeps none failure stage after successful recording");
    require(state.planned_batch_count == 2, "diagnostic recorder stores planned batch count");
    require(state.recorded_batch_count == 2, "diagnostic recorder stores recorded batch count");
    require(state.recorded_batches.size() == 2, "diagnostic recorder stores recorded batches");

    const vulkan_backend::vulkan_recorded_draw_batch& first = state.recorded_batches[0];
    require(first.kind == vulkan_backend::vulkan_batch_kind::quad, "diagnostic recorder stores first batch kind");
    require(first.command_index == 0, "diagnostic recorder stores first source command index");
    require(first.recording_index == 0, "diagnostic recorder stores first recording order");
    require(first.clipped_bounds.x == 10.0f, "diagnostic recorder stores first clipped x");
    require(first.scissor.x == 1, "diagnostic recorder stores first scissor x");
    require(first.scissor.y == 2, "diagnostic recorder stores first scissor y");
    require(first.scissor.width == 3, "diagnostic recorder stores first scissor width");
    require(first.scissor.height == 4, "diagnostic recorder stores first scissor height");

    const vulkan_backend::vulkan_recorded_draw_batch& second = state.recorded_batches[1];
    require(second.kind == vulkan_backend::vulkan_batch_kind::image, "diagnostic recorder stores second batch kind");
    require(second.command_index == 1, "diagnostic recorder stores second source command index");
    require(second.recording_index == 1, "diagnostic recorder stores second recording order");
    require(second.scissor.x == 5, "diagnostic recorder stores second scissor x");
    require(second.scissor.y == 6, "diagnostic recorder stores second scissor y");
    require(second.scissor.width == 2, "diagnostic recorder stores second scissor width");
    require(second.scissor.height == 2, "diagnostic recorder stores second scissor height");
}

void test_vulkan_diagnostic_command_recorder_reports_begin_failure()
{
    using namespace quiz_vulkan::render;

    vulkan_backend::diagnostic_vulkan_command_recorder recorder(
        vulkan_backend::diagnostic_vulkan_command_recorder_options{
            .ready = true,
            .fail_at = vulkan_backend::vulkan_command_recorder_failure_stage::begin_recording,
        });

    require(
        !recorder.begin_recording(
            vulkan_backend::vulkan_surface_extent{.width = 10, .height = 10},
            1),
        "diagnostic recorder can deterministically fail begin");

    const vulkan_backend::vulkan_backend_command_recorder_state& state = recorder.recorder_state();
    require(state.ready, "begin failure recorder remains logically ready");
    require(!state.frame_open, "begin failure does not open recorder frame");
    require(!state.command_buffer_recorded, "begin failure does not record command buffer");
    require(state.failed(), "begin failure marks recorder state failed");
    require(
        state.failure_stage == vulkan_backend::vulkan_command_recorder_failure_stage::begin_recording,
        "begin failure records begin failure stage");
    require(state.failure_recording_index == 0, "begin failure records deterministic failure index");
    require(state.planned_batch_count == 1, "begin failure preserves planned batch count");
    require(state.recorded_batch_count == 0, "begin failure records no batches");
    require(state.recorded_batches.empty(), "begin failure stores no recorded batches");
}

void test_vulkan_diagnostic_command_recorder_reports_record_failure()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "first",
        render_rect{0.0f, 0.0f, 40.0f, 40.0f},
        render_color{1.0f, 0.0f, 0.0f, 1.0f}));
    draw_list.commands.push_back(make_quad_command(
        "second",
        render_rect{50.0f, 50.0f, 20.0f, 20.0f},
        render_color{0.0f, 1.0f, 0.0f, 1.0f}));

    const vulkan_backend::vulkan_frame_plan plan = vulkan_backend::build_vulkan_frame_plan(
        draw_list,
        vulkan_backend::vulkan_frame_plan_options{
            .viewport = render_rect{0.0f, 0.0f, 100.0f, 100.0f},
            .surface_width = 10,
            .surface_height = 10,
        });

    vulkan_backend::diagnostic_vulkan_command_recorder recorder(
        vulkan_backend::diagnostic_vulkan_command_recorder_options{
            .ready = true,
            .fail_at = vulkan_backend::vulkan_command_recorder_failure_stage::record_draw_batch,
            .fail_recording_index = 1,
        });

    require(
        recorder.begin_recording(
            vulkan_backend::vulkan_surface_extent{.width = 10, .height = 10},
            plan.batches.size()),
        "record failure recorder begins successfully");
    require(recorder.record_draw_batch(plan.batches[0]), "record failure recorder records first batch");
    require(!recorder.record_draw_batch(plan.batches[1]), "record failure recorder fails second batch");
    require(!recorder.finish_recording(), "record failure recorder does not finish after failed batch");

    const vulkan_backend::vulkan_backend_command_recorder_state& state = recorder.recorder_state();
    require(state.ready, "record failure recorder remains ready");
    require(state.frame_open, "record failure keeps frame open");
    require(!state.command_buffer_recorded, "record failure does not finish command buffer");
    require(state.failed(), "record failure marks recorder state failed");
    require(
        state.failure_stage == vulkan_backend::vulkan_command_recorder_failure_stage::record_draw_batch,
        "record failure records record failure stage");
    require(state.failure_recording_index == 1, "record failure records failed batch index");
    require(state.planned_batch_count == 2, "record failure preserves planned batch count");
    require(state.recorded_batch_count == 1, "record failure preserves successfully recorded count");
    require(state.recorded_batches.size() == 1, "record failure stores only successful batches");
    require(state.recorded_batches.front().recording_index == 0, "record failure preserves first recording index");
}

void test_vulkan_diagnostic_command_recorder_reports_finish_failure()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{0.0f, 0.0f, 100.0f, 100.0f},
        render_color{1.0f, 1.0f, 1.0f, 1.0f}));

    const vulkan_backend::vulkan_frame_plan plan = vulkan_backend::build_vulkan_frame_plan(
        draw_list,
        vulkan_backend::vulkan_frame_plan_options{
            .viewport = render_rect{0.0f, 0.0f, 100.0f, 100.0f},
            .surface_width = 10,
            .surface_height = 10,
        });

    vulkan_backend::diagnostic_vulkan_command_recorder recorder(
        vulkan_backend::diagnostic_vulkan_command_recorder_options{
            .ready = true,
            .fail_at = vulkan_backend::vulkan_command_recorder_failure_stage::finish_recording,
        });

    require(
        recorder.begin_recording(
            vulkan_backend::vulkan_surface_extent{.width = 10, .height = 10},
            plan.batches.size()),
        "finish failure recorder begins successfully");
    require(recorder.record_draw_batch(plan.batches.front()), "finish failure recorder records planned batch");
    require(!recorder.finish_recording(), "finish failure recorder fails finish");

    const vulkan_backend::vulkan_backend_command_recorder_state& state = recorder.recorder_state();
    require(state.ready, "finish failure recorder remains ready");
    require(state.frame_open, "finish failure keeps frame open");
    require(!state.command_buffer_recorded, "finish failure does not record command buffer");
    require(state.failed(), "finish failure marks recorder state failed");
    require(
        state.failure_stage == vulkan_backend::vulkan_command_recorder_failure_stage::finish_recording,
        "finish failure records finish failure stage");
    require(state.failure_recording_index == 1, "finish failure records failure after one recorded batch");
    require(state.planned_batch_count == 1, "finish failure preserves planned batch count");
    require(state.recorded_batch_count == 1, "finish failure preserves recorded batch count");
    require(state.recorded_batches.size() == 1, "finish failure preserves recorded batches");
}

void test_vulkan_diagnostic_pipeline_cache_reports_batch_capabilities()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{10.0f, 10.0f, 20.0f, 20.0f},
        render_color{1.0f, 0.0f, 0.0f, 1.0f}));
    draw_list.commands.push_back(make_image_command(
        "image",
        render_rect{40.0f, 40.0f, 20.0f, 20.0f},
        "fixture://renderer/image.png"));

    const vulkan_backend::vulkan_frame_plan plan = vulkan_backend::build_vulkan_frame_plan(
        draw_list,
        vulkan_backend::vulkan_frame_plan_options{
            .viewport = render_rect{0.0f, 0.0f, 100.0f, 100.0f},
            .surface_width = 10,
            .surface_height = 10,
        });

    vulkan_backend::diagnostic_vulkan_pipeline_cache cache;
    require(cache.ensure_pipeline(plan.batches[0]), "diagnostic pipeline cache provides quad pipeline");
    require(cache.ensure_pipeline(plan.batches[1]), "diagnostic pipeline cache provides image pipeline");

    const vulkan_backend::vulkan_backend_pipeline_state& state = cache.pipeline_state();
    require(state.cache_checked, "pipeline cache records that pipelines were checked");
    require(state.ready, "pipeline cache remains ready when all requested pipelines exist");
    require(state.completed(), "pipeline cache reports completed state when all requested pipelines exist");
    require(!state.missing_pipeline, "pipeline cache reports no missing pipeline");
    require(state.requested_pipeline_count == 2, "pipeline cache records requested pipeline count");
    require(state.capabilities.size() == 4, "pipeline cache exposes one capability per batch kind");
    require(state.cache_entries.size() == 4, "pipeline cache exposes one cache entry per batch kind");
    require(state.pipeline_descriptors.size() == 4, "pipeline cache exposes one descriptor per batch kind");
    require(state.shader_registry.registered_shader_count == 8, "pipeline cache registers vertex and fragment shaders");
    require(state.shader_registry.registry_checked, "pipeline cache verifies shaders while checking pipelines");
    require(!state.shader_registry.missing_shader, "pipeline cache reports no missing shader on happy path");
    require(
        state.descriptor_for(vulkan_backend::vulkan_batch_kind::quad) != nullptr,
        "pipeline cache exposes quad descriptor");
    require(
        state.descriptor_for(vulkan_backend::vulkan_batch_kind::image) != nullptr,
        "pipeline cache exposes image descriptor");
    require(state.supports(vulkan_backend::vulkan_batch_kind::quad), "pipeline cache supports quad pipeline");
    require(state.supports(vulkan_backend::vulkan_batch_kind::text), "pipeline cache supports text pipeline");
    require(state.supports(vulkan_backend::vulkan_batch_kind::image), "pipeline cache supports image pipeline");
    require(state.supports(vulkan_backend::vulkan_batch_kind::debug_bounds), "pipeline cache supports debug pipeline");

    require(state.cache_entries[0].kind == vulkan_backend::vulkan_batch_kind::quad, "first pipeline cache entry is quad");
    require(state.cache_entries[0].requested, "quad pipeline cache entry is requested");
    require(state.cache_entries[0].request_count == 1, "quad pipeline cache entry records request count");
    require(state.cache_entries[0].last_command_index == 0, "quad pipeline cache entry records source command index");
    require(state.cache_entries[1].kind == vulkan_backend::vulkan_batch_kind::text, "second pipeline cache entry is text");
    require(!state.cache_entries[1].requested, "text pipeline cache entry is not requested");
    require(state.cache_entries[2].kind == vulkan_backend::vulkan_batch_kind::image, "third pipeline cache entry is image");
    require(state.cache_entries[2].requested, "image pipeline cache entry is requested");
    require(state.cache_entries[2].request_count == 1, "image pipeline cache entry records request count");
    require(state.cache_entries[2].last_command_index == 1, "image pipeline cache entry records source command index");
    require(
        state.cache_entries[3].kind == vulkan_backend::vulkan_batch_kind::debug_bounds,
        "fourth pipeline cache entry is debug bounds");
    require(!state.cache_entries[3].requested, "debug pipeline cache entry is not requested");
}

void test_vulkan_diagnostic_pipeline_cache_identifies_missing_batch_pipeline()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{10.0f, 10.0f, 20.0f, 20.0f},
        render_color{1.0f, 0.0f, 0.0f, 1.0f}));
    draw_list.commands.push_back(make_image_command(
        "image",
        render_rect{40.0f, 40.0f, 20.0f, 20.0f},
        "fixture://renderer/image.png"));

    const vulkan_backend::vulkan_frame_plan plan = vulkan_backend::build_vulkan_frame_plan(
        draw_list,
        vulkan_backend::vulkan_frame_plan_options{
            .viewport = render_rect{0.0f, 0.0f, 100.0f, 100.0f},
            .surface_width = 10,
            .surface_height = 10,
        });

    vulkan_backend::diagnostic_vulkan_pipeline_cache cache(
        vulkan_backend::diagnostic_vulkan_pipeline_cache_options{
            .default_available = true,
            .overrides = {vulkan_backend::vulkan_pipeline_capability{
                .kind = vulkan_backend::vulkan_batch_kind::image,
                .available = false,
            }},
        });

    require(cache.ensure_pipeline(plan.batches[0]), "pipeline cache provides available quad pipeline");
    require(!cache.ensure_pipeline(plan.batches[1]), "pipeline cache reports missing image pipeline");

    const vulkan_backend::vulkan_backend_pipeline_state& state = cache.pipeline_state();
    require(state.cache_checked, "missing pipeline cache records that pipelines were checked");
    require(!state.ready, "missing pipeline cache is not ready");
    require(!state.completed(), "missing pipeline cache does not complete");
    require(state.missing_pipeline, "missing pipeline cache records missing pipeline");
    require(
        state.missing_batch_kind == vulkan_backend::vulkan_batch_kind::image,
        "missing pipeline cache records missing image pipeline");
    require(state.missing_command_index == 1, "missing pipeline cache records source command index");
    require(state.requested_pipeline_count == 2, "missing pipeline cache records request count through failure");
    require(state.supports(vulkan_backend::vulkan_batch_kind::quad), "missing pipeline cache still supports quad");
    require(!state.supports(vulkan_backend::vulkan_batch_kind::image), "missing pipeline cache reports unsupported image");
    require(state.cache_entries[0].requested, "missing pipeline cache records quad request");
    require(state.cache_entries[2].requested, "missing pipeline cache records image request");
    require(!state.cache_entries[2].available, "missing pipeline cache entry records unavailable image pipeline");
    require(state.cache_entries[2].last_command_index == 1, "missing pipeline cache entry records failed command index");
}

void test_vulkan_diagnostic_pipeline_cache_identifies_missing_vertex_shader()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{10.0f, 10.0f, 20.0f, 20.0f},
        render_color{1.0f, 0.0f, 0.0f, 1.0f}));

    const vulkan_backend::vulkan_frame_plan plan = vulkan_backend::build_vulkan_frame_plan(
        draw_list,
        vulkan_backend::vulkan_frame_plan_options{
            .viewport = render_rect{0.0f, 0.0f, 100.0f, 100.0f},
            .surface_width = 10,
            .surface_height = 10,
        });

    vulkan_backend::diagnostic_vulkan_pipeline_cache cache(
        vulkan_backend::diagnostic_vulkan_pipeline_cache_options{
            .use_default_shader_modules = false,
            .shader_modules = {vulkan_backend::vulkan_shader_module_descriptor{
                .id = vulkan_backend::vulkan_shader_module_id{.value = "quad.fragment"},
                .stage = vulkan_backend::vulkan_shader_stage::fragment,
            }},
        });

    require(!cache.ensure_pipeline(plan.batches.front()), "pipeline cache reports missing vertex shader");

    const vulkan_backend::vulkan_backend_pipeline_state& state = cache.pipeline_state();
    require(state.missing_pipeline, "missing vertex shader marks pipeline missing");
    require(!state.missing_descriptor, "missing vertex shader keeps descriptor available");
    require(state.missing_shader, "missing vertex shader marks shader missing");
    require(
        state.missing_shader_stage == vulkan_backend::vulkan_shader_stage::vertex,
        "missing vertex shader records vertex stage");
    require(state.missing_shader_id.value == "quad.vertex", "missing vertex shader records shader module id");
    require(state.missing_batch_kind == vulkan_backend::vulkan_batch_kind::quad, "missing vertex shader records batch kind");
    require(state.missing_command_index == 0, "missing vertex shader records command index");
    require(state.shader_registry.registry_checked, "missing vertex shader checks shader registry");
    require(state.shader_registry.missing_shader, "shader registry records missing vertex shader");
    require(
        state.shader_registry.missing_stage == vulkan_backend::vulkan_shader_stage::vertex,
        "shader registry records missing vertex stage");
    require(state.shader_registry.missing_shader_id.value == "quad.vertex", "shader registry records vertex id");
}

void test_vulkan_diagnostic_pipeline_cache_identifies_missing_fragment_shader()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_image_command(
        "image",
        render_rect{10.0f, 10.0f, 20.0f, 20.0f},
        "fixture://renderer/image.png"));

    const vulkan_backend::vulkan_frame_plan plan = vulkan_backend::build_vulkan_frame_plan(
        draw_list,
        vulkan_backend::vulkan_frame_plan_options{
            .viewport = render_rect{0.0f, 0.0f, 100.0f, 100.0f},
            .surface_width = 10,
            .surface_height = 10,
        });

    vulkan_backend::diagnostic_vulkan_pipeline_cache cache(
        vulkan_backend::diagnostic_vulkan_pipeline_cache_options{
            .use_default_shader_modules = false,
            .shader_modules = {vulkan_backend::vulkan_shader_module_descriptor{
                .id = vulkan_backend::vulkan_shader_module_id{.value = "image.vertex"},
                .stage = vulkan_backend::vulkan_shader_stage::vertex,
            }},
        });

    require(!cache.ensure_pipeline(plan.batches.front()), "pipeline cache reports missing fragment shader");

    const vulkan_backend::vulkan_backend_pipeline_state& state = cache.pipeline_state();
    require(state.missing_pipeline, "missing fragment shader marks pipeline missing");
    require(!state.missing_descriptor, "missing fragment shader keeps descriptor available");
    require(state.missing_shader, "missing fragment shader marks shader missing");
    require(
        state.missing_shader_stage == vulkan_backend::vulkan_shader_stage::fragment,
        "missing fragment shader records fragment stage");
    require(state.missing_shader_id.value == "image.fragment", "missing fragment shader records shader module id");
    require(state.missing_batch_kind == vulkan_backend::vulkan_batch_kind::image, "missing fragment shader records batch kind");
    require(state.missing_command_index == 0, "missing fragment shader records command index");
    require(state.shader_registry.registry_checked, "missing fragment shader checks shader registry");
    require(state.shader_registry.missing_shader, "shader registry records missing fragment shader");
    require(
        state.shader_registry.missing_stage == vulkan_backend::vulkan_shader_stage::fragment,
        "shader registry records missing fragment stage");
    require(state.shader_registry.missing_shader_id.value == "image.fragment", "shader registry records fragment id");
}

void test_vulkan_diagnostic_pipeline_cache_identifies_missing_batch_descriptor()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_text_command(
        "text",
        render_rect{10.0f, 10.0f, 20.0f, 20.0f},
        "missing descriptor"));

    const vulkan_backend::vulkan_frame_plan plan = vulkan_backend::build_vulkan_frame_plan(
        draw_list,
        vulkan_backend::vulkan_frame_plan_options{
            .viewport = render_rect{0.0f, 0.0f, 100.0f, 100.0f},
            .surface_width = 10,
            .surface_height = 10,
        });

    vulkan_backend::diagnostic_vulkan_pipeline_cache cache(
        vulkan_backend::diagnostic_vulkan_pipeline_cache_options{
            .use_default_pipeline_descriptors = false,
            .pipeline_descriptors = {vulkan_backend::vulkan_pipeline_descriptor{
                .kind = vulkan_backend::vulkan_batch_kind::quad,
                .vertex_shader = vulkan_backend::vulkan_shader_module_id{.value = "quad.vertex"},
                .fragment_shader = vulkan_backend::vulkan_shader_module_id{.value = "quad.fragment"},
            }},
        });

    require(!cache.ensure_pipeline(plan.batches.front()), "pipeline cache reports missing text descriptor");

    const vulkan_backend::vulkan_backend_pipeline_state& state = cache.pipeline_state();
    require(state.missing_pipeline, "missing descriptor marks pipeline missing");
    require(state.missing_descriptor, "missing descriptor flag is set");
    require(!state.missing_shader, "missing descriptor does not report missing shader");
    require(
        state.missing_batch_kind == vulkan_backend::vulkan_batch_kind::text,
        "missing descriptor records text batch kind");
    require(state.missing_command_index == 0, "missing descriptor records command index");
    require(state.pipeline_descriptors.size() == 1, "pipeline cache uses injected descriptor set");
    require(
        state.descriptor_for(vulkan_backend::vulkan_batch_kind::text) == nullptr,
        "pipeline cache exposes no text descriptor");
    require(!state.shader_registry.registry_checked, "missing descriptor stops before shader registry lookup");
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
    vulkan_backend::diagnostic_vulkan_command_recorder recorder;
    const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
        device,
        recorder,
        draw_list,
        render_rect{0.0f, 0.0f, 64.0f, 64.0f});

    require(result.completed(), "fake backend completes frame lifecycle");
    require(result.attempted, "fake backend records lifecycle attempt");
    require(!result.fallback_required, "completed fake backend frame does not require fallback");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::none,
        "completed fake backend frame has no fallback reason");
    require(
        result.reached_stage == vulkan_backend::vulkan_backend_frame_stage::frame_presented,
        "completed fake backend reports presented frame stage");
    require(result.lifecycle.ready_for_frame(), "fake backend reports lifecycle resources ready");
    require(result.lifecycle_ready, "fake backend lifecycle is frame-ready");
    require(result.surface_ready, "fake backend surface is ready");
    require(result.frame_begun, "fake backend begins a frame");
    require(result.commands_recorded, "fake backend records frame commands");
    require(result.frame_submitted, "fake backend submits frame");
    require(result.frame_presented, "fake backend presents frame");
    require(result.planned_batch_count == 1, "fake backend receives one planned batch");
    require(result.recorded_batch_count == 1, "fake backend records one batch");
    require(result.clipped_draw_call_count == 0, "unclipped fake backend batch is not clipped");
    require(result.discarded_draw_call_count == 0, "visible fake backend batch is not discarded");
    require(result.pipeline.completed(), "fake backend pipeline cache reports completed state");
    require(result.pipeline.supports(vulkan_backend::vulkan_batch_kind::quad), "fake backend pipeline cache supports quads");
    require(result.pipeline.requested_pipeline_count == 1, "fake backend pipeline cache records one pipeline request");
    require(result.pipeline.cache_entries[0].requested, "fake backend pipeline cache records quad request");
    require(result.command_recorder.ready, "fake backend command recorder is ready");
    require(result.command_recorder.frame_open, "fake backend command recorder opens a frame");
    require(result.command_recorder.command_buffer_recorded, "fake backend command buffer is recorded");
    require(result.command_recorder.planned_batch_count == 1, "fake backend command recorder sees planned batch count");
    require(result.command_recorder.recorded_batch_count == 1, "fake backend command recorder sees recorded batch count");
    require(result.command_recorder.completed(), "fake backend command recorder reports completed state");
    require(!result.command_recorder.failed(), "fake backend command recorder reports no failure");
    require(recorder.recorder_state().completed(), "adapter drives injected command recorder to completion");
    require(result.command_recorder.recorded_batches.size() == 1, "fake backend command recorder stores one batch");
    require(
        result.command_recorder.recorded_batches.front().command_index == 0,
        "fake backend command recorder stores source command index");
    require(
        result.command_recorder.recorded_batches.front().recording_index == 0,
        "fake backend command recorder stores recording order");

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
    require(result.command_recorder.ready, "empty frame has a ready command recorder");
    require(result.command_recorder.frame_open, "empty frame opens command recorder state");
    require(result.command_recorder.command_buffer_recorded, "empty frame records an empty command buffer");
    require(result.command_recorder.empty(), "empty frame command recorder has no batches");
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
    require(result.command_recorder.ready, "all-discarded frame has a ready command recorder");
    require(result.command_recorder.frame_open, "all-discarded frame opens command recorder state");
    require(result.command_recorder.command_buffer_recorded, "all-discarded frame records an empty command buffer");
    require(result.command_recorder.empty(), "all-discarded frame command recorder has no batches");
    require(result.clipped_draw_call_count == 0, "all-discarded frame has no clipped draw calls");
    require(result.discarded_draw_call_count == 1, "all-discarded frame reports discarded draw call");
    require(device.recorded_plan.empty(), "all-discarded frame records empty plan");
    require(device.recorded_plan.discarded_draw_call_count == 1, "recorded all-discarded plan preserves discarded count");
    require(device.calls.size() == 4, "all-discarded frame still runs lifecycle");
}

void test_vulkan_backend_adapter_falls_back_when_command_recorder_is_unready()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{0.0f, 0.0f, 100.0f, 100.0f},
        render_color{1.0f, 1.0f, 1.0f, 1.0f}));

    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 16, .height = 16});
    device.lifecycle.command_recorder_ready = false;
    const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
        device,
        draw_list,
        render_rect{0.0f, 0.0f, 100.0f, 100.0f});

    require(!result.completed(), "backend cannot complete without a command recorder");
    require(result.attempted, "backend records attempted frame with unavailable command recorder");
    require(result.fallback_required, "backend requires fallback without command recorder readiness");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::command_recorder_unavailable,
        "backend reports command recorder readiness fallback reason");
    require(
        result.reached_stage == vulkan_backend::vulkan_backend_frame_stage::backend_attempted,
        "backend stops at attempted stage without command recorder readiness");
    require(result.lifecycle.instance_ready, "backend records ready instance");
    require(result.lifecycle.device_ready, "backend records ready device");
    require(result.lifecycle.swapchain_ready, "backend records ready swapchain");
    require(result.lifecycle.pipeline_ready, "backend records ready pipeline");
    require(!result.lifecycle.command_recorder_ready, "backend records unavailable command recorder");
    require(!result.lifecycle.ready_for_frame(), "backend lifecycle is not frame-ready without command recorder");
    require(!result.command_recorder.ready, "backend command recorder state is not ready");
    require(!result.command_recorder.frame_open, "backend command recorder does not open frame when unready");
    require(!result.command_recorder.command_buffer_recorded, "backend command recorder does not record when unready");
    require(!result.command_recorder.failed(), "backend command recorder state does not fail before lifecycle readiness");
    require(result.command_recorder.empty(), "backend command recorder state remains empty when unready");
    require(!result.lifecycle_ready, "backend aggregate lifecycle readiness fails");
    require(!result.surface.valid(), "backend does not query a surface before lifecycle readiness passes");
    require(!result.surface_ready, "backend surface is not ready when lifecycle readiness fails");
    require(!result.frame_begun, "backend does not begin frame without command recorder readiness");
    require(!result.commands_recorded, "backend does not record without command recorder readiness");
    require(!result.frame_submitted, "backend does not submit without command recorder readiness");
    require(!result.frame_presented, "backend does not present without command recorder readiness");
    require(result.planned_batch_count == 0, "backend does not plan batches without command recorder readiness");
    require(result.recorded_batch_count == 0, "backend does not record batches without command recorder readiness");
    require(device.calls.empty(), "backend does not call frame lifecycle without command recorder readiness");
}

void test_vulkan_backend_adapter_falls_back_when_injected_recorder_rejects_frame()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{0.0f, 0.0f, 100.0f, 100.0f},
        render_color{1.0f, 1.0f, 1.0f, 1.0f}));

    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 16, .height = 16});
    vulkan_backend::diagnostic_vulkan_command_recorder recorder(false);
    const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
        device,
        recorder,
        draw_list,
        render_rect{0.0f, 0.0f, 100.0f, 100.0f});

    require(!result.completed(), "backend cannot complete when injected recorder rejects frame");
    require(result.lifecycle_ready, "backend lifecycle is ready before injected recorder rejects frame");
    require(result.surface_ready, "backend surface is ready before injected recorder rejects frame");
    require(result.frame_begun, "backend begins frame before injected recorder rejects frame");
    require(!result.commands_recorded, "backend does not mark commands recorded after recorder rejection");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::record_commands_failed,
        "backend reports record fallback when injected recorder rejects frame");
    require(
        result.reached_stage == vulkan_backend::vulkan_backend_frame_stage::frame_begun,
        "backend stops at frame begun stage when injected recorder rejects frame");
    require(!result.command_recorder.ready, "backend records rejected command recorder readiness");
    require(!result.command_recorder.frame_open, "backend does not open rejected command recorder frame");
    require(!result.command_recorder.command_buffer_recorded, "backend does not record rejected command buffer");
    require(result.command_recorder.failed(), "backend records rejected recorder failure");
    require(
        result.command_recorder.failure_stage == vulkan_backend::vulkan_command_recorder_failure_stage::begin_recording,
        "backend records begin failure stage from rejected recorder");
    require(result.command_recorder.failure_recording_index == 0, "backend records begin failure index");
    require(result.command_recorder.planned_batch_count == 1, "backend preserves planned count from rejected recorder");
    require(result.command_recorder.recorded_batches.empty(), "backend has no recorded batches after recorder rejection");
    require(device.calls.size() == 1, "backend stops device lifecycle after begin when recorder rejects frame");
    require(device.calls[0] == "begin", "backend begins before injected recorder rejects frame");
    require(!recorder.recorder_state().ready, "injected recorder exposes rejected state");
}

void test_vulkan_backend_adapter_falls_back_when_batch_pipeline_is_missing()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{0.0f, 0.0f, 40.0f, 40.0f},
        render_color{1.0f, 0.0f, 0.0f, 1.0f}));
    draw_list.commands.push_back(make_image_command(
        "image",
        render_rect{50.0f, 50.0f, 20.0f, 20.0f},
        "fixture://renderer/image.png"));

    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 16, .height = 16});
    vulkan_backend::diagnostic_vulkan_pipeline_cache pipeline_cache(
        vulkan_backend::diagnostic_vulkan_pipeline_cache_options{
            .default_available = true,
            .overrides = {vulkan_backend::vulkan_pipeline_capability{
                .kind = vulkan_backend::vulkan_batch_kind::image,
                .available = false,
            }},
        });
    vulkan_backend::diagnostic_vulkan_command_recorder recorder;
    const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
        device,
        pipeline_cache,
        recorder,
        draw_list,
        render_rect{0.0f, 0.0f, 100.0f, 100.0f});

    require(!result.completed(), "backend cannot complete when a required batch pipeline is missing");
    require(result.lifecycle_ready, "backend lifecycle is ready before missing pipeline fallback");
    require(result.surface_ready, "backend surface is ready before missing pipeline fallback");
    require(!result.frame_begun, "backend does not begin frame when a batch pipeline is missing");
    require(!result.commands_recorded, "backend does not record commands when a batch pipeline is missing");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::pipeline_unavailable,
        "backend reports pipeline unavailable when a batch pipeline is missing");
    require(
        result.reached_stage == vulkan_backend::vulkan_backend_frame_stage::frame_plan_ready,
        "backend stops after frame plan when a batch pipeline is missing");
    require(result.planned_batch_count == 2, "backend reports planned batches before missing pipeline fallback");
    require(result.recorded_batch_count == 0, "backend records no batches when a pipeline is missing");
    require(result.pipeline.cache_checked, "backend records pipeline cache check before missing pipeline fallback");
    require(!result.pipeline.ready, "backend records unready pipeline cache");
    require(result.pipeline.missing_pipeline, "backend records missing pipeline");
    require(
        result.pipeline.missing_batch_kind == vulkan_backend::vulkan_batch_kind::image,
        "backend records missing image pipeline");
    require(result.pipeline.missing_command_index == 1, "backend records command index for missing pipeline");
    require(result.pipeline.requested_pipeline_count == 2, "backend records pipeline requests through missing pipeline");
    require(result.pipeline.cache_entries[0].requested, "backend records successful quad pipeline request");
    require(result.pipeline.cache_entries[2].requested, "backend records failed image pipeline request");
    require(!result.pipeline.cache_entries[2].available, "backend records unavailable image pipeline");
    require(result.command_recorder.ready, "backend keeps command recorder ready before missing pipeline fallback");
    require(!result.command_recorder.frame_open, "backend does not open command recorder when pipeline is missing");
    require(result.command_recorder.planned_batch_count == 2, "backend preserves planned batch count before recorder");
    require(device.calls.empty(), "backend does not call device lifecycle when a batch pipeline is missing");
    require(!recorder.recorder_state().frame_open, "injected recorder is not opened when a batch pipeline is missing");
}

void test_vulkan_backend_adapter_falls_back_when_injected_recorder_fails_record_batch()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "first",
        render_rect{0.0f, 0.0f, 40.0f, 40.0f},
        render_color{1.0f, 0.0f, 0.0f, 1.0f}));
    draw_list.commands.push_back(make_quad_command(
        "second",
        render_rect{50.0f, 50.0f, 20.0f, 20.0f},
        render_color{0.0f, 1.0f, 0.0f, 1.0f}));

    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 16, .height = 16});
    vulkan_backend::diagnostic_vulkan_command_recorder recorder(
        vulkan_backend::diagnostic_vulkan_command_recorder_options{
            .ready = true,
            .fail_at = vulkan_backend::vulkan_command_recorder_failure_stage::record_draw_batch,
            .fail_recording_index = 1,
        });
    const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
        device,
        recorder,
        draw_list,
        render_rect{0.0f, 0.0f, 100.0f, 100.0f});

    require(!result.completed(), "backend cannot complete when injected recorder fails a batch");
    require(result.frame_begun, "backend begins frame before injected record failure");
    require(!result.commands_recorded, "backend does not call device recording after injected record failure");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::record_commands_failed,
        "backend reports record fallback when injected recorder fails a batch");
    require(
        result.reached_stage == vulkan_backend::vulkan_backend_frame_stage::frame_begun,
        "backend stops at frame begun stage when injected recorder fails a batch");
    require(result.command_recorder.ready, "backend records ready injected recorder on record failure");
    require(result.command_recorder.frame_open, "backend records open frame on record failure");
    require(!result.command_recorder.command_buffer_recorded, "backend records unfinished buffer on record failure");
    require(result.command_recorder.failed(), "backend records injected recorder record failure");
    require(
        result.command_recorder.failure_stage
            == vulkan_backend::vulkan_command_recorder_failure_stage::record_draw_batch,
        "backend records record failure stage from injected recorder");
    require(result.command_recorder.failure_recording_index == 1, "backend records failed batch index");
    require(result.command_recorder.planned_batch_count == 2, "backend preserves planned count on record failure");
    require(result.command_recorder.recorded_batch_count == 1, "backend preserves recorded count before failure");
    require(result.command_recorder.recorded_batches.size() == 1, "backend preserves successful batches before failure");
    require(device.calls.size() == 1, "backend stops device lifecycle after begin on injected record failure");
    require(device.calls[0] == "begin", "backend begins before injected recorder fails a batch");
}

void test_vulkan_backend_adapter_falls_back_when_injected_recorder_fails_finish()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{0.0f, 0.0f, 100.0f, 100.0f},
        render_color{1.0f, 1.0f, 1.0f, 1.0f}));

    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 16, .height = 16});
    vulkan_backend::diagnostic_vulkan_command_recorder recorder(
        vulkan_backend::diagnostic_vulkan_command_recorder_options{
            .ready = true,
            .fail_at = vulkan_backend::vulkan_command_recorder_failure_stage::finish_recording,
        });
    const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
        device,
        recorder,
        draw_list,
        render_rect{0.0f, 0.0f, 100.0f, 100.0f});

    require(!result.completed(), "backend cannot complete when injected recorder fails finish");
    require(result.frame_begun, "backend begins frame before injected finish failure");
    require(!result.commands_recorded, "backend does not call device recording after injected finish failure");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::record_commands_failed,
        "backend reports record fallback when injected recorder fails finish");
    require(
        result.reached_stage == vulkan_backend::vulkan_backend_frame_stage::frame_begun,
        "backend stops at frame begun stage when injected recorder fails finish");
    require(result.command_recorder.ready, "backend records ready injected recorder on finish failure");
    require(result.command_recorder.frame_open, "backend records open frame on finish failure");
    require(!result.command_recorder.command_buffer_recorded, "backend records unfinished buffer on finish failure");
    require(result.command_recorder.failed(), "backend records injected recorder finish failure");
    require(
        result.command_recorder.failure_stage
            == vulkan_backend::vulkan_command_recorder_failure_stage::finish_recording,
        "backend records finish failure stage from injected recorder");
    require(result.command_recorder.failure_recording_index == 1, "backend records finish failure index");
    require(result.command_recorder.planned_batch_count == 1, "backend preserves planned count on finish failure");
    require(result.command_recorder.recorded_batch_count == 1, "backend preserves recorded count before finish failure");
    require(result.command_recorder.recorded_batches.size() == 1, "backend preserves batches before finish failure");
    require(device.calls.size() == 1, "backend stops device lifecycle after begin on injected finish failure");
    require(device.calls[0] == "begin", "backend begins before injected recorder fails finish");
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
    require(
        result.reached_stage == vulkan_backend::vulkan_backend_frame_stage::lifecycle_ready,
        "backend reaches lifecycle-ready stage before missing surface fallback");
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
    require(
        result.reached_stage == vulkan_backend::vulkan_backend_frame_stage::surface_extent_ready,
        "backend reaches surface extent stage before missing viewport fallback");
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
    require(
        result.reached_stage == vulkan_backend::vulkan_backend_frame_stage::frame_plan_ready,
        "backend reaches frame plan stage before begin failure");
    require(result.surface_ready, "backend had a surface before begin failed");
    require(!result.frame_begun, "backend reports failed frame begin");
    require(!result.commands_recorded, "backend does not record commands after failed begin");
    require(!result.frame_submitted, "backend does not submit after failed begin");
    require(!result.frame_presented, "backend does not present after failed begin");
    require(result.planned_batch_count == 1, "backend reports planned batch count before begin failure");
    require(result.recorded_batch_count == 0, "backend does not record batches after begin failure");
    require(result.command_recorder.ready, "command recorder was ready before begin failure");
    require(!result.command_recorder.frame_open, "command recorder does not open frame after failed begin");
    require(!result.command_recorder.command_buffer_recorded, "command recorder does not record after failed begin");
    require(result.command_recorder.planned_batch_count == 1, "command recorder records planned count before begin failure");
    require(result.command_recorder.recorded_batch_count == 0, "command recorder has no recorded count after begin failure");
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
    require(
        result.reached_stage == vulkan_backend::vulkan_backend_frame_stage::frame_begun,
        "backend reaches frame begun stage before recording failure");
    require(result.surface_ready, "backend had a surface before recording failed");
    require(result.frame_begun, "backend began frame before recording failed");
    require(!result.commands_recorded, "backend reports failed command recording");
    require(!result.frame_submitted, "backend does not submit after failed recording");
    require(!result.frame_presented, "backend does not present after failed recording");
    require(result.planned_batch_count == 1, "backend still reports planned batch count before failure");
    require(result.recorded_batch_count == 0, "backend does not count batches after failed recording");
    require(result.command_recorder.ready, "command recorder was ready before recording failure");
    require(result.command_recorder.frame_open, "command recorder frame was open before device recording failure");
    require(result.command_recorder.command_buffer_recorded, "command recorder records buffer before device recording failure");
    require(result.command_recorder.planned_batch_count == 1, "command recorder tracks planned count before device failure");
    require(result.command_recorder.recorded_batch_count == 1, "command recorder tracks recorded count before device failure");
    require(result.command_recorder.recorded_batches.size() == 1, "command recorder stores batch before device failure");
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
    require(
        result.reached_stage == vulkan_backend::vulkan_backend_frame_stage::commands_recorded,
        "backend reaches commands recorded stage before submit failure");
    require(result.surface_ready, "backend had a surface before submit failed");
    require(result.frame_begun, "backend began frame before submit failed");
    require(result.commands_recorded, "backend recorded commands before submit failed");
    require(!result.frame_submitted, "backend reports failed frame submit");
    require(!result.frame_presented, "backend does not present after failed submit");
    require(result.planned_batch_count == 1, "backend reports planned batch count before submit failure");
    require(result.recorded_batch_count == 1, "backend reports recorded batch count before submit failure");
    require(result.command_recorder.frame_open, "command recorder frame remains tracked before submit failure");
    require(result.command_recorder.command_buffer_recorded, "command recorder reports recorded buffer before submit failure");
    require(result.command_recorder.recorded_batch_count == 1, "command recorder tracks recorded count before submit failure");
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
    require(
        result.reached_stage == vulkan_backend::vulkan_backend_frame_stage::frame_submitted,
        "backend reaches frame submitted stage before presentation failure");
    require(result.surface_ready, "backend had a surface before presentation failed");
    require(result.frame_begun, "backend began frame before presentation failed");
    require(result.commands_recorded, "backend recorded commands before presentation failed");
    require(result.frame_submitted, "backend submitted frame before presentation failed");
    require(!result.frame_presented, "backend reports failed presentation");
    require(result.planned_batch_count == 1, "backend reports planned batch count before presentation failure");
    require(result.recorded_batch_count == 1, "backend reports recorded batch count before presentation failure");
    require(result.command_recorder.frame_open, "command recorder frame remains tracked before presentation failure");
    require(result.command_recorder.command_buffer_recorded, "command recorder reports recorded buffer before presentation failure");
    require(result.command_recorder.recorded_batch_count == 1, "command recorder tracks recorded count before presentation failure");
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
    test_vulkan_backend_frame_stage_names_are_stable();
    test_vulkan_command_recorder_failure_stage_names_are_stable();
    test_vulkan_shader_stage_names_are_stable();
    test_draw_list_submission_counts_generic_work();
    test_renderer_backend_diagnostics_report_vulkan_not_requested();
    test_cpu_fallback_clips_and_discards();
    test_degenerate_surface_discards_draw_calls();
    test_vulkan_frame_plan_builds_scissored_batches_from_render_contracts();
    test_vulkan_frame_plan_applies_nested_clips();
    test_vulkan_frame_plan_ignores_unmatched_pop_clip();
    test_vulkan_diagnostic_command_recorder_records_planned_batches();
    test_vulkan_diagnostic_command_recorder_reports_begin_failure();
    test_vulkan_diagnostic_command_recorder_reports_record_failure();
    test_vulkan_diagnostic_command_recorder_reports_finish_failure();
    test_vulkan_diagnostic_pipeline_cache_reports_batch_capabilities();
    test_vulkan_diagnostic_pipeline_cache_identifies_missing_batch_pipeline();
    test_vulkan_diagnostic_pipeline_cache_identifies_missing_vertex_shader();
    test_vulkan_diagnostic_pipeline_cache_identifies_missing_fragment_shader();
    test_vulkan_diagnostic_pipeline_cache_identifies_missing_batch_descriptor();
    test_vulkan_backend_adapter_completes_fake_device_lifecycle();
    test_vulkan_backend_adapter_preserves_plan_diagnostics();
    test_vulkan_backend_adapter_completes_empty_frame();
    test_vulkan_backend_adapter_completes_all_discarded_frame();
    test_vulkan_backend_adapter_falls_back_when_command_recorder_is_unready();
    test_vulkan_backend_adapter_falls_back_when_injected_recorder_rejects_frame();
    test_vulkan_backend_adapter_falls_back_when_batch_pipeline_is_missing();
    test_vulkan_backend_adapter_falls_back_when_injected_recorder_fails_record_batch();
    test_vulkan_backend_adapter_falls_back_when_injected_recorder_fails_finish();
    test_vulkan_backend_adapter_falls_back_without_surface();
    test_vulkan_backend_adapter_falls_back_without_viewport();
    test_vulkan_backend_adapter_falls_back_when_begin_fails();
    test_vulkan_backend_adapter_falls_back_when_recording_fails();
    test_vulkan_backend_adapter_falls_back_when_submit_fails();
    test_vulkan_backend_adapter_falls_back_when_present_fails();
    return 0;
}
