#include "render/vulkan/vulkan_backend_adapter.h"

#include <cassert>
#include <cstdio>
#include <string>
#include <string_view>
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

    quiz_vulkan::render::vulkan_backend::vulkan_swapchain_acquire_result acquire_next_image(
        quiz_vulkan::render::vulkan_backend::vulkan_surface_extent surface) override
    {
        calls.push_back("acquire");
        acquired_surface = surface;
        if (!acquire_succeeds) {
            return quiz_vulkan::render::vulkan_backend::vulkan_swapchain_acquire_result{
                .status = quiz_vulkan::render::vulkan_backend::vulkan_swapchain_acquire_status::failed,
                .image = {},
            };
        }

        return quiz_vulkan::render::vulkan_backend::vulkan_swapchain_acquire_result{
            .status = quiz_vulkan::render::vulkan_backend::vulkan_swapchain_acquire_status::acquired,
            .image = quiz_vulkan::render::vulkan_backend::vulkan_swapchain_image_state{
                .id = next_image_id,
                .available = true,
                .acquired = true,
                .presented = false,
            },
        };
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

    quiz_vulkan::render::vulkan_backend::vulkan_swapchain_present_result present_image(
        quiz_vulkan::render::vulkan_backend::vulkan_swapchain_image_id image_id) override
    {
        calls.push_back("present_image");
        presented_image_id = image_id;
        return quiz_vulkan::render::vulkan_backend::vulkan_swapchain_present_result{
            .status = present_image_succeeds
                ? quiz_vulkan::render::vulkan_backend::vulkan_swapchain_present_status::presented
                : quiz_vulkan::render::vulkan_backend::vulkan_swapchain_present_status::failed,
            .image_id = image_id,
        };
    }

    bool present_frame() override
    {
        calls.push_back("present");
        return present_frame_succeeds;
    }

    bool acquire_succeeds = true;
    bool begin_succeeds = true;
    bool record_succeeds = true;
    bool submit_succeeds = true;
    bool present_image_succeeds = true;
    bool present_frame_succeeds = true;
    quiz_vulkan::render::vulkan_backend::vulkan_swapchain_image_id next_image_id{.value = 7};
    quiz_vulkan::render::vulkan_backend::vulkan_backend_lifecycle_readiness lifecycle{
        .instance_ready = true,
        .device_ready = true,
        .swapchain_ready = true,
        .pipeline_ready = true,
        .command_recorder_ready = true,
    };
    quiz_vulkan::render::vulkan_backend::vulkan_surface_extent begun_surface;
    quiz_vulkan::render::vulkan_backend::vulkan_surface_extent acquired_surface;
    quiz_vulkan::render::vulkan_backend::vulkan_swapchain_image_id presented_image_id;
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

quiz_vulkan::render::render_draw_command make_quad_command(
    std::string node_id,
    quiz_vulkan::render::render_rect bounds)
{
    using namespace quiz_vulkan::render;

    return render_draw_command{
        .type = render_draw_command_type::quad,
        .node_id = std::move(node_id),
        .parent_node_id = {},
        .depth = 0,
        .bounds = bounds,
        .content_bounds = bounds,
        .paint = render_paint{.source = "test", .color = render_color{0.2f, 0.4f, 0.8f, 1.0f}},
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
        .paint = render_paint{.source = "text", .color = render_color{1.0f, 1.0f, 1.0f, 1.0f}},
        .border_radius = 0.0f,
        .text_runs = {render_text_run{.text = std::move(text), .style_token = "body"}},
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

quiz_vulkan::render::render_draw_list make_quad_draw_list()
{
    quiz_vulkan::render::render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        quiz_vulkan::render::render_rect{0.0f, 0.0f, 24.0f, 24.0f}));
    return draw_list;
}

quiz_vulkan::render::vulkan_backend::vulkan_loader_readiness_state make_ready_loader()
{
    return quiz_vulkan::render::vulkan_backend::vulkan_loader_readiness_state{
        .checked = true,
        .status = quiz_vulkan::render::vulkan_backend::vulkan_loader_readiness_status::ready,
        .probe_status = quiz_vulkan::render::vulkan_backend::vulkan_loader_probe_status::available,
        .loader_library_available = true,
        .instance_proc_address_available = true,
        .instance_ready = true,
        .loaded_library_name = "fake-vulkan-loader",
        .required_symbol_name = "vkGetInstanceProcAddr",
        .attempted_library_count = 1,
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_instance_create_result make_ready_instance()
{
    return quiz_vulkan::render::vulkan_backend::vulkan_instance_create_result{
        .checked = true,
        .status = quiz_vulkan::render::vulkan_backend::vulkan_instance_create_status::created,
        .loader = make_ready_loader(),
        .handle = quiz_vulkan::render::vulkan_backend::vulkan_instance_handle{.value = 1},
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_device_create_result make_ready_device()
{
    return quiz_vulkan::render::vulkan_backend::vulkan_device_create_result{
        .checked = true,
        .status = quiz_vulkan::render::vulkan_backend::vulkan_device_create_status::created,
        .instance = make_ready_instance(),
        .handle = quiz_vulkan::render::vulkan_backend::vulkan_device_handle{.value = 2},
        .selected_queues = {
            quiz_vulkan::render::vulkan_backend::vulkan_device_queue_selection{
                .capability = quiz_vulkan::render::vulkan_backend::vulkan_device_queue_capability::graphics,
                .family_index = 0,
                .queue = quiz_vulkan::render::vulkan_backend::vulkan_queue_handle{.value = 3},
            },
        },
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_swapchain_create_result make_ready_swapchain()
{
    return quiz_vulkan::render::vulkan_backend::vulkan_swapchain_create_result{
        .checked = true,
        .status = quiz_vulkan::render::vulkan_backend::vulkan_swapchain_create_status::created,
        .device = make_ready_device(),
        .handle = quiz_vulkan::render::vulkan_backend::vulkan_swapchain_handle{.value = 4},
        .requested_extent = quiz_vulkan::render::vulkan_backend::vulkan_surface_extent{.width = 64, .height = 64},
        .selected_extent = quiz_vulkan::render::vulkan_backend::vulkan_surface_extent{.width = 64, .height = 64},
        .requested_present_mode = quiz_vulkan::render::vulkan_backend::vulkan_swapchain_present_mode::fifo,
        .selected_present_mode = quiz_vulkan::render::vulkan_backend::vulkan_swapchain_present_mode::fifo,
        .requested_present_mode_supported = true,
        .fallback_to_fifo = false,
        .min_image_count = 2,
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_render_pass_create_result make_ready_render_pass()
{
    return quiz_vulkan::render::vulkan_backend::vulkan_render_pass_create_result{
        .checked = true,
        .status = quiz_vulkan::render::vulkan_backend::vulkan_render_pass_create_status::created,
        .swapchain = make_ready_swapchain(),
        .render_pass = quiz_vulkan::render::vulkan_backend::vulkan_render_pass_handle{.value = 5},
        .framebuffer = quiz_vulkan::render::vulkan_backend::vulkan_framebuffer_handle{.value = 6},
        .requested_extent = quiz_vulkan::render::vulkan_backend::vulkan_surface_extent{.width = 64, .height = 64},
        .selected_extent = quiz_vulkan::render::vulkan_backend::vulkan_surface_extent{.width = 64, .height = 64},
        .requested_attachments = {
            quiz_vulkan::render::vulkan_backend::vulkan_render_pass_attachment_request{},
        },
        .selected_attachments = {
            quiz_vulkan::render::vulkan_backend::vulkan_render_pass_attachment_request{},
        },
    };
}

void require_handoff_fallback(
    const quiz_vulkan::render::vulkan_backend::vulkan_backend_frame_pipeline_handoff& handoff,
    quiz_vulkan::render::vulkan_backend::vulkan_backend_fallback_reason reason,
    quiz_vulkan::render::vulkan_backend::vulkan_backend_frame_pipeline_handoff_status status,
    quiz_vulkan::render::vulkan_backend::vulkan_backend_frame_stage reached_stage)
{
    require(handoff.checked, "frame pipeline handoff summary is checked");
    require(handoff.blocked(), "frame pipeline handoff reports blocked fallback");
    require(!handoff.completed(), "blocked frame pipeline handoff does not complete");
    require(handoff.cpu_fallback_available, "blocked frame pipeline handoff keeps CPU fallback available");
    require(handoff.fallback_reason == reason, "frame pipeline handoff records fallback reason");
    require(handoff.status == status, "frame pipeline handoff records status");
    require(handoff.reached_stage == reached_stage, "frame pipeline handoff records reached stage");
}

void test_vulkan_frame_pipeline_handoff_status_names_are_stable()
{
    using namespace quiz_vulkan::render;

    require(
        vulkan_backend::frame_pipeline_handoff_status_name(
            vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::not_checked)
            == std::string_view{"not_checked"},
        "frame handoff status name for not checked is stable");
    require(
        vulkan_backend::frame_pipeline_handoff_status_name(
            vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::ready)
            == std::string_view{"ready"},
        "frame handoff status name for ready is stable");
    require(
        vulkan_backend::frame_pipeline_handoff_status_name(
            vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::pipeline_unavailable)
            == std::string_view{"pipeline_unavailable"},
        "frame handoff status name for pipeline unavailable is stable");
    require(
        vulkan_backend::frame_pipeline_handoff_status_name(
            vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::resource_binding_unavailable)
            == std::string_view{"resource_binding_unavailable"},
        "frame handoff status name for resource binding unavailable is stable");
    require(
        vulkan_backend::frame_pipeline_handoff_status_name(
            vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::command_recording_unavailable)
            == std::string_view{"command_recording_unavailable"},
        "frame handoff status name for command recording unavailable is stable");
    require(
        vulkan_backend::frame_pipeline_handoff_status_name(
            vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::frame_lifecycle_unavailable)
            == std::string_view{"frame_lifecycle_unavailable"},
        "frame handoff status name for frame lifecycle unavailable is stable");
    require(
        vulkan_backend::frame_pipeline_handoff_status_name(
            vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::submit_unavailable)
            == std::string_view{"submit_unavailable"},
        "frame handoff status name for submit unavailable is stable");
    require(
        vulkan_backend::frame_pipeline_handoff_status_name(
            vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::present_unavailable)
            == std::string_view{"present_unavailable"},
        "frame handoff status name for present unavailable is stable");
}

void test_vulkan_frame_pipeline_handoff_maps_lifecycle_prerequisites()
{
    using namespace quiz_vulkan::render;

    {
        fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 64, .height = 64});
        device.lifecycle.instance_ready = false;
        const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
            device,
            make_quad_draw_list(),
            render_rect{0.0f, 0.0f, 64.0f, 64.0f});
        require_handoff_fallback(
            result.pipeline_handoff,
            vulkan_backend::vulkan_backend_fallback_reason::instance_unavailable,
            vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::instance_unavailable,
            vulkan_backend::vulkan_backend_frame_stage::backend_attempted);
    }

    {
        fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 64, .height = 64});
        device.lifecycle.device_ready = false;
        const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
            device,
            make_quad_draw_list(),
            render_rect{0.0f, 0.0f, 64.0f, 64.0f});
        require_handoff_fallback(
            result.pipeline_handoff,
            vulkan_backend::vulkan_backend_fallback_reason::device_unavailable,
            vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::device_unavailable,
            vulkan_backend::vulkan_backend_frame_stage::backend_attempted);
    }

    {
        fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 64, .height = 64});
        device.lifecycle.swapchain_ready = false;
        const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
            device,
            make_quad_draw_list(),
            render_rect{0.0f, 0.0f, 64.0f, 64.0f});
        require_handoff_fallback(
            result.pipeline_handoff,
            vulkan_backend::vulkan_backend_fallback_reason::swapchain_unavailable,
            vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::swapchain_unavailable,
            vulkan_backend::vulkan_backend_frame_stage::backend_attempted);
    }

    {
        fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 64, .height = 64});
        device.lifecycle.render_pass = make_ready_render_pass();
        device.lifecycle.render_pass.status = vulkan_backend::vulkan_render_pass_create_status::invalid_format;
        device.lifecycle.render_pass.render_pass = {};
        device.lifecycle.render_pass_ready = false;
        const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
            device,
            make_quad_draw_list(),
            render_rect{0.0f, 0.0f, 64.0f, 64.0f});
        require_handoff_fallback(
            result.pipeline_handoff,
            vulkan_backend::vulkan_backend_fallback_reason::render_pass_unavailable,
            vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::render_pass_unavailable,
            vulkan_backend::vulkan_backend_frame_stage::backend_attempted);
    }

    {
        fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 64, .height = 64});
        device.lifecycle.pipeline_ready = false;
        const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
            device,
            make_quad_draw_list(),
            render_rect{0.0f, 0.0f, 64.0f, 64.0f});
        require_handoff_fallback(
            result.pipeline_handoff,
            vulkan_backend::vulkan_backend_fallback_reason::pipeline_unavailable,
            vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::pipeline_unavailable,
            vulkan_backend::vulkan_backend_frame_stage::backend_attempted);
    }

    {
        fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 64, .height = 64});
        device.lifecycle.command_recorder_ready = false;
        const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
            device,
            make_quad_draw_list(),
            render_rect{0.0f, 0.0f, 64.0f, 64.0f});
        require_handoff_fallback(
            result.pipeline_handoff,
            vulkan_backend::vulkan_backend_fallback_reason::command_recorder_unavailable,
            vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::command_recording_unavailable,
            vulkan_backend::vulkan_backend_frame_stage::backend_attempted);
    }
}

void test_vulkan_frame_pipeline_handoff_maps_surface_and_frame_lifecycle_failures()
{
    using namespace quiz_vulkan::render;

    {
        fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{});
        const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
            device,
            make_quad_draw_list(),
            render_rect{0.0f, 0.0f, 64.0f, 64.0f});
        require_handoff_fallback(
            result.pipeline_handoff,
            vulkan_backend::vulkan_backend_fallback_reason::surface_unavailable,
            vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::surface_unavailable,
            vulkan_backend::vulkan_backend_frame_stage::lifecycle_ready);
    }

    {
        fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 64, .height = 64});
        const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
            device,
            make_quad_draw_list(),
            render_rect{0.0f, 0.0f, 0.0f, 64.0f});
        require_handoff_fallback(
            result.pipeline_handoff,
            vulkan_backend::vulkan_backend_fallback_reason::viewport_unavailable,
            vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::viewport_unavailable,
            vulkan_backend::vulkan_backend_frame_stage::surface_extent_ready);
    }

    {
        fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 64, .height = 64});
        device.acquire_succeeds = false;
        const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
            device,
            make_quad_draw_list(),
            render_rect{0.0f, 0.0f, 64.0f, 64.0f});
        require_handoff_fallback(
            result.pipeline_handoff,
            vulkan_backend::vulkan_backend_fallback_reason::acquire_image_failed,
            vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::frame_lifecycle_unavailable,
            vulkan_backend::vulkan_backend_frame_stage::frame_plan_ready);
        require(result.pipeline_handoff.frame_lifecycle_attempted_step_count == 1, "handoff records one attempted lifecycle step");
        require(result.pipeline_handoff.frame_lifecycle_completed_step_count == 0, "handoff records no completed lifecycle steps");
    }

    {
        fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 64, .height = 64});
        device.submit_succeeds = false;
        const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
            device,
            make_quad_draw_list(),
            render_rect{0.0f, 0.0f, 64.0f, 64.0f});
        require_handoff_fallback(
            result.pipeline_handoff,
            vulkan_backend::vulkan_backend_fallback_reason::submit_frame_failed,
            vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::submit_unavailable,
            vulkan_backend::vulkan_backend_frame_stage::commands_recorded);
        require(result.pipeline_handoff.command_recording_ready, "handoff records command recording ready before submit failure");
        require(!result.pipeline_handoff.frame_submit_completed, "handoff records incomplete frame submit");
    }

    {
        fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 64, .height = 64});
        device.present_image_succeeds = false;
        const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
            device,
            make_quad_draw_list(),
            render_rect{0.0f, 0.0f, 64.0f, 64.0f});
        require_handoff_fallback(
            result.pipeline_handoff,
            vulkan_backend::vulkan_backend_fallback_reason::present_image_failed,
            vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::present_unavailable,
            vulkan_backend::vulkan_backend_frame_stage::frame_submitted);
        require(result.pipeline_handoff.frame_submit_completed, "handoff records completed frame submit before present failure");
        require(!result.pipeline_handoff.present_completed, "handoff records incomplete present");
    }
}

void test_vulkan_frame_pipeline_handoff_requires_pipeline_before_resource_binding()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_image_command(
        "image",
        render_rect{0.0f, 0.0f, 32.0f, 32.0f},
        "fixture://renderer/image.png"));

    vulkan_backend::diagnostic_vulkan_pipeline_cache pipeline_cache(
        vulkan_backend::diagnostic_vulkan_pipeline_cache_options{
            .default_available = true,
            .overrides = {
                vulkan_backend::vulkan_pipeline_capability{
                    .kind = vulkan_backend::vulkan_batch_kind::image,
                    .available = false,
                },
            },
        });
    vulkan_backend::diagnostic_vulkan_command_recorder command_recorder;
    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 64, .height = 64});

    const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
        device,
        pipeline_cache,
        command_recorder,
        draw_list,
        render_rect{0.0f, 0.0f, 64.0f, 64.0f});

    require_handoff_fallback(
        result.pipeline_handoff,
        vulkan_backend::vulkan_backend_fallback_reason::pipeline_unavailable,
        vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::pipeline_unavailable,
        vulkan_backend::vulkan_backend_frame_stage::frame_plan_ready);
    require(result.pipeline_handoff.pipeline_required, "handoff records pipeline required");
    require(result.pipeline_handoff.pipeline_checked, "handoff records pipeline checked");
    require(!result.pipeline_handoff.pipeline_completed, "handoff records incomplete pipeline");
    require(!result.pipeline_handoff.resource_bindings_checked, "handoff does not check resource binding after pipeline failure");
    require(!result.pipeline_handoff.command_recorder_gate_checked, "handoff does not check command recorder gate after pipeline failure");
    require(!result.pipeline_handoff.command_recording_ready, "handoff does not consider command recording ready without pipeline");
    require(device.calls.empty(), "pipeline failure stops before device lifecycle calls");
}

void test_vulkan_frame_pipeline_handoff_requires_resource_binding_before_command_recording()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_image_command(
        "image",
        render_rect{0.0f, 0.0f, 32.0f, 32.0f},
        ""));

    vulkan_backend::diagnostic_vulkan_pipeline_cache pipeline_cache;
    vulkan_backend::diagnostic_vulkan_command_recorder command_recorder;
    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 64, .height = 64});

    const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
        device,
        pipeline_cache,
        command_recorder,
        draw_list,
        render_rect{0.0f, 0.0f, 64.0f, 64.0f});

    require_handoff_fallback(
        result.pipeline_handoff,
        vulkan_backend::vulkan_backend_fallback_reason::resource_binding_unavailable,
        vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::resource_binding_unavailable,
        vulkan_backend::vulkan_backend_frame_stage::frame_plan_ready);
    require(result.pipeline_handoff.pipeline_completed, "handoff records completed pipeline before resource failure");
    require(result.pipeline_handoff.resource_bindings_checked, "handoff checks resource bindings");
    require(!result.pipeline_handoff.resource_bindings_completed, "handoff records incomplete resource binding");
    require(result.pipeline_handoff.resource_registry_checked, "handoff checks resource registry");
    require(!result.pipeline_handoff.resource_registry_completed, "handoff records incomplete resource registry");
    require(result.pipeline_handoff.command_recorder_gate_checked, "handoff checks command recorder gate");
    require(!result.pipeline_handoff.command_recorder_gate_allowed, "handoff blocks command recorder gate");
    require(!result.pipeline_handoff.command_recording_ready, "handoff does not consider command recording ready without resources");
    require(device.calls.empty(), "resource binding failure stops before device lifecycle calls");
}

void test_vulkan_frame_pipeline_handoff_accepts_draw_list_render_data()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{0.0f, 0.0f, 16.0f, 16.0f}));
    draw_list.commands.push_back(make_text_command(
        "text",
        render_rect{16.0f, 0.0f, 24.0f, 16.0f},
        "frame text"));
    draw_list.commands.push_back(make_image_command(
        "image",
        render_rect{40.0f, 0.0f, 16.0f, 16.0f},
        "fixture://renderer/image.png"));

    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 64, .height = 64});
    const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
        device,
        draw_list,
        render_rect{0.0f, 0.0f, 64.0f, 64.0f});

    const vulkan_backend::vulkan_backend_frame_pipeline_handoff& handoff = result.pipeline_handoff;
    require(result.completed(), "frame result completes with generic draw-list render data");
    require(handoff.completed(), "frame pipeline handoff completes");
    require(
        handoff.status == vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::ready,
        "frame pipeline handoff reports ready");
    require(!handoff.cpu_fallback_available, "ready handoff does not require CPU fallback");
    require(handoff.planned_batch_count == 3, "handoff records three planned render batches");
    require(handoff.recorded_batch_count == 3, "handoff records three recorded render batches");
    require(handoff.quad_batch_count == 1, "handoff counts quad batch");
    require(handoff.text_batch_count == 1, "handoff counts text batch as render data");
    require(handoff.image_batch_count == 1, "handoff counts image batch as render data");
    require(handoff.pipeline_completed, "handoff requires completed pipeline before command recording");
    require(handoff.resource_bindings_completed, "handoff requires completed resource binding before command recording");
    require(handoff.resource_registry_completed, "handoff requires completed resource registry before command recording");
    require(handoff.command_packets_checked, "handoff checks command packets before command recording");
    require(handoff.command_packets_completed, "handoff requires completed command packets before command recording");
    require(handoff.command_packet_execution_checked, "handoff checks command packet execution before command recording");
    require(handoff.command_packet_execution_completed, "handoff requires completed packet execution before command recording");
    require(handoff.command_recorder_gate_allowed, "handoff allows command recorder after prerequisites");
    require(handoff.command_recording_ready, "handoff records command recording ready");
    require(handoff.frame_submit_completed, "handoff records frame submit completed");
    require(handoff.present_completed, "handoff records present completed");
    require(handoff.frame_lifecycle_checked, "handoff records frame lifecycle policy checked");
    require(handoff.frame_lifecycle_completed, "handoff records frame lifecycle completed");
    require(handoff.frame_lifecycle_attempted_step_count == 5, "handoff records five lifecycle attempts");
    require(handoff.frame_lifecycle_completed_step_count == 5, "handoff records five completed lifecycle steps");
    require(device.calls.size() == 6, "backend executes stable frame lifecycle call count");
    require(device.calls[0] == "acquire", "backend acquires before frame begin");
    require(device.calls[1] == "begin", "backend begins after acquire");
    require(device.calls[2] == "record", "backend records after begin");
    require(device.calls[3] == "submit", "backend submits after recording");
    require(device.calls[4] == "present_image", "backend presents image after submit");
    require(device.calls[5] == "present", "backend presents frame last");
}

} // namespace

int main()
{
    test_vulkan_frame_pipeline_handoff_status_names_are_stable();
    test_vulkan_frame_pipeline_handoff_maps_lifecycle_prerequisites();
    test_vulkan_frame_pipeline_handoff_maps_surface_and_frame_lifecycle_failures();
    test_vulkan_frame_pipeline_handoff_requires_pipeline_before_resource_binding();
    test_vulkan_frame_pipeline_handoff_requires_resource_binding_before_command_recording();
    test_vulkan_frame_pipeline_handoff_accepts_draw_list_render_data();
    return 0;
}
