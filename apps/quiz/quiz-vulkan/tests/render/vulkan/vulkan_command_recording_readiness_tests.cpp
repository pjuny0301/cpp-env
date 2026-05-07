#include "render/vulkan/vulkan_backend_adapter.h"
#include "render/vulkan/vulkan_backend_command_recording.h"
#include "render/vulkan/vulkan_backend_device.h"
#include "render/vulkan/vulkan_backend_instance.h"
#include "render/vulkan/vulkan_backend_loader.h"
#include "render/vulkan/vulkan_backend_render_pass.h"
#include "render/vulkan/vulkan_backend_swapchain.h"
#include "render/vulkan/vulkan_frame_plan.h"

#include <cassert>
#include <cstdio>
#include <string>
#include <string_view>
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

quiz_vulkan::render::vulkan_backend::vulkan_loader_readiness_state make_ready_loader()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_loader_readiness_state{
        .checked = true,
        .status = vulkan_backend::vulkan_loader_readiness_status::ready,
        .probe_status = vulkan_backend::vulkan_loader_probe_status::available,
        .loader_library_available = true,
        .instance_proc_address_available = true,
        .instance_ready = true,
        .loaded_library_name = "fake-vulkan-loader",
        .required_symbol_name = std::string{vulkan_backend::vulkan_loader_required_symbol_name()},
        .attempted_library_count = 1,
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_instance_create_result make_created_instance()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_instance_create_result{
        .checked = true,
        .status = vulkan_backend::vulkan_instance_create_status::created,
        .loader = make_ready_loader(),
        .handle = vulkan_backend::vulkan_instance_handle{.value = 70},
        .selected_extensions = {"VK_KHR_surface"},
        .enabled_layers = {},
        .diagnostic = "Vulkan instance created",
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_device_create_result make_ready_device()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_device_create_result{
        .checked = true,
        .status = vulkan_backend::vulkan_device_create_status::created,
        .instance = make_created_instance(),
        .handle = vulkan_backend::vulkan_device_handle{.value = 80},
        .selected_extensions = {"VK_KHR_swapchain"},
        .selected_queues = {
            vulkan_backend::vulkan_device_queue_selection{
                .capability = vulkan_backend::vulkan_device_queue_capability::graphics,
                .family_index = 0,
                .queue = vulkan_backend::vulkan_queue_handle{.value = 100},
            },
            vulkan_backend::vulkan_device_queue_selection{
                .capability = vulkan_backend::vulkan_device_queue_capability::present,
                .family_index = 1,
                .queue = vulkan_backend::vulkan_queue_handle{.value = 101},
            },
        },
        .diagnostic = "Vulkan device created",
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_surface_extent make_extent()
{
    return quiz_vulkan::render::vulkan_backend::vulkan_surface_extent{
        .width = 1280,
        .height = 720,
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_swapchain_create_result make_ready_swapchain()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_swapchain_create_result{
        .checked = true,
        .status = vulkan_backend::vulkan_swapchain_create_status::created,
        .device = make_ready_device(),
        .handle = vulkan_backend::vulkan_swapchain_handle{.value = 90},
        .requested_extent = make_extent(),
        .selected_extent = make_extent(),
        .requested_present_mode = vulkan_backend::vulkan_swapchain_present_mode::fifo,
        .selected_present_mode = vulkan_backend::vulkan_swapchain_present_mode::fifo,
        .requested_present_mode_supported = true,
        .fallback_to_fifo = false,
        .min_image_count = 3,
        .diagnostic = "Vulkan swapchain created",
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_render_pass_attachment_request make_color_attachment(
    quiz_vulkan::render::vulkan_backend::vulkan_render_pass_attachment_format format)
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_render_pass_attachment_request{
        .role = vulkan_backend::vulkan_render_pass_attachment_role::color,
        .format = format,
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_render_pass_create_result make_ready_render_pass(
    quiz_vulkan::render::vulkan_backend::vulkan_render_pass_attachment_format format =
        quiz_vulkan::render::vulkan_backend::vulkan_render_pass_attachment_format::bgra8_unorm)
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_render_pass_create_result{
        .checked = true,
        .status = vulkan_backend::vulkan_render_pass_create_status::created,
        .swapchain = make_ready_swapchain(),
        .render_pass = vulkan_backend::vulkan_render_pass_handle{.value = 300},
        .framebuffer = vulkan_backend::vulkan_framebuffer_handle{.value = 400},
        .requested_extent = make_extent(),
        .selected_extent = make_extent(),
        .requested_attachments = {make_color_attachment(format)},
        .selected_attachments = {make_color_attachment(format)},
        .diagnostic = "Vulkan render pass and framebuffer created",
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_frame_plan make_frame_plan(
    std::vector<quiz_vulkan::render::vulkan_backend::vulkan_draw_batch> batches,
    std::size_t clipped_draw_call_count = 0)
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_frame_plan{
        .viewport = quiz_vulkan::render::render_rect{0.0f, 0.0f, 1280.0f, 720.0f},
        .surface_width = 1280,
        .surface_height = 720,
        .clipped_draw_call_count = clipped_draw_call_count,
        .discarded_draw_call_count = 0,
        .batches = std::move(batches),
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_draw_batch make_batch(
    quiz_vulkan::render::vulkan_backend::vulkan_batch_kind kind,
    std::size_t command_index,
    bool clipped = false)
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_draw_batch{
        .kind = kind,
        .command_index = command_index,
        .node_id = "batch",
        .bounds = quiz_vulkan::render::render_rect{0.0f, 0.0f, 64.0f, 64.0f},
        .clipped_bounds = clipped
            ? quiz_vulkan::render::render_rect{}
            : quiz_vulkan::render::render_rect{0.0f, 0.0f, 64.0f, 64.0f},
        .paint = quiz_vulkan::render::render_paint{},
        .scissor = clipped
            ? vulkan_backend::vulkan_scissor_rect{}
            : vulkan_backend::vulkan_scissor_rect{.x = 0, .y = 0, .width = 64, .height = 64},
        .vertices = {},
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_command_recording_readiness_request make_request(
    quiz_vulkan::render::vulkan_backend::vulkan_frame_plan frame_plan)
{
    return quiz_vulkan::render::vulkan_backend::vulkan_command_recording_readiness_request{
        .frame_plan = std::move(frame_plan),
    };
}

quiz_vulkan::render::vulkan_backend::fake_vulkan_command_recording_readiness_factory
make_command_recording_factory()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::fake_vulkan_command_recording_readiness_factory(
        vulkan_backend::fake_vulkan_command_recording_readiness_factory_options{
            .supported_batch_kinds = {
                vulkan_backend::vulkan_batch_kind::quad,
                vulkan_backend::vulkan_batch_kind::text,
                vulkan_backend::vulkan_batch_kind::image,
                vulkan_backend::vulkan_batch_kind::debug_bounds,
            },
            .compatible_attachment_formats = {
                vulkan_backend::vulkan_render_pass_attachment_format::bgra8_unorm,
                vulkan_backend::vulkan_render_pass_attachment_format::rgba8_unorm,
            },
            .command_buffer_available = true,
            .command_buffer =
                vulkan_backend::vulkan_command_recording_command_buffer_handle{.value = 500},
        });
}

void test_command_recording_readiness_accepts_ready_render_pass_and_plan()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_command_recording_readiness_factory factory =
        make_command_recording_factory();
    const vulkan_backend::vulkan_command_recording_readiness_result result =
        vulkan_backend::check_vulkan_command_recording_readiness(
            factory,
            make_ready_render_pass(),
            make_request(make_frame_plan({
                make_batch(vulkan_backend::vulkan_batch_kind::quad, 0),
            })));

    require(result.checked, "command recording readiness result is checked");
    require(
        result.status == vulkan_backend::vulkan_command_recording_readiness_status::ready,
        "command recording readiness reports ready");
    require(result.ready_for_recording(), "command recording readiness reaches recording gate");
    require(result.render_pass_ready(), "command recording readiness keeps render pass ready");
    require(result.framebuffer_ready(), "command recording readiness keeps framebuffer ready");
    require(result.pipeline_ready(), "command recording readiness keeps pipeline compatible");
    require(result.command_buffer_ready(), "command recording readiness keeps command buffer ready");
    require(result.command_buffer.value == 500, "command recording readiness returns configured command buffer");
    require(result.planned_batch_count == 1, "command recording readiness records planned batch count");
    require(result.recordable_batch_count == 1, "command recording readiness records recordable batch count");
    require(result.batch_compatibility.size() == 1, "command recording readiness records batch compatibility");
    require(result.batch_compatibility[0].completed(), "command recording batch compatibility completes");
    require(
        result.diagnostic == "Vulkan command recording is ready",
        "command recording readiness records ready diagnostic");
}

void test_command_recording_readiness_reports_render_pass_and_framebuffer_failures()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_command_recording_readiness_factory factory =
        make_command_recording_factory();
    vulkan_backend::vulkan_render_pass_create_result render_pass_unavailable =
        make_ready_render_pass();
    render_pass_unavailable.status =
        vulkan_backend::vulkan_render_pass_create_status::creation_failed;
    render_pass_unavailable.render_pass = {};
    const vulkan_backend::vulkan_command_recording_readiness_result missing_render_pass =
        vulkan_backend::check_vulkan_command_recording_readiness(
            factory,
            render_pass_unavailable,
            make_request(make_frame_plan({})));

    require(
        missing_render_pass.status
            == vulkan_backend::vulkan_command_recording_readiness_status::render_pass_unavailable,
        "command recording readiness reports render pass unavailable");
    require(!missing_render_pass.ready_for_recording(), "missing render pass cannot record commands");
    require(
        missing_render_pass.diagnostic == "Vulkan render pass is not ready for command recording",
        "missing render pass diagnostic is explicit");

    vulkan_backend::vulkan_render_pass_create_result framebuffer_unavailable =
        make_ready_render_pass();
    framebuffer_unavailable.framebuffer = {};
    const vulkan_backend::vulkan_command_recording_readiness_result missing_framebuffer =
        vulkan_backend::check_vulkan_command_recording_readiness(
            factory,
            framebuffer_unavailable,
            make_request(make_frame_plan({})));

    require(
        missing_framebuffer.status
            == vulkan_backend::vulkan_command_recording_readiness_status::framebuffer_unavailable,
        "command recording readiness reports framebuffer unavailable");
    require(!missing_framebuffer.ready_for_recording(), "missing framebuffer cannot record commands");
    require(
        missing_framebuffer.diagnostic == "Vulkan framebuffer is not available for command recording",
        "missing framebuffer diagnostic is explicit");
}

void test_command_recording_readiness_reports_pipeline_attachment_incompatibility()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_command_recording_readiness_factory factory(
        vulkan_backend::fake_vulkan_command_recording_readiness_factory_options{
            .supported_batch_kinds = {vulkan_backend::vulkan_batch_kind::quad},
            .compatible_attachment_formats = {
                vulkan_backend::vulkan_render_pass_attachment_format::rgba8_unorm,
            },
            .command_buffer_available = true,
            .command_buffer =
                vulkan_backend::vulkan_command_recording_command_buffer_handle{.value = 501},
        });
    const vulkan_backend::vulkan_command_recording_readiness_result result =
        vulkan_backend::check_vulkan_command_recording_readiness(
            factory,
            make_ready_render_pass(
                vulkan_backend::vulkan_render_pass_attachment_format::bgra8_unorm),
            make_request(make_frame_plan({
                make_batch(vulkan_backend::vulkan_batch_kind::quad, 0),
            })));

    require(
        result.status
            == vulkan_backend::vulkan_command_recording_readiness_status::pipeline_incompatible,
        "command recording readiness reports pipeline attachment incompatibility");
    require(!result.pipeline_ready(), "pipeline incompatibility blocks command recording");
    require(result.attachment_compatibility.size() == 1, "pipeline compatibility records attachment");
    require(!result.attachment_compatibility[0].compatible, "pipeline compatibility marks attachment incompatible");
    require(
        result.diagnostic
            == "Vulkan command recording pipeline is incompatible with selected attachment format: bgra8_unorm",
        "pipeline incompatibility diagnostic names selected attachment format");
}

void test_command_recording_readiness_reports_command_buffer_unavailable()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_command_recording_readiness_factory factory(
        vulkan_backend::fake_vulkan_command_recording_readiness_factory_options{
            .supported_batch_kinds = {vulkan_backend::vulkan_batch_kind::quad},
            .compatible_attachment_formats = {
                vulkan_backend::vulkan_render_pass_attachment_format::bgra8_unorm,
            },
            .command_buffer_available = false,
            .command_buffer = {},
        });
    const vulkan_backend::vulkan_command_recording_readiness_result result =
        vulkan_backend::check_vulkan_command_recording_readiness(
            factory,
            make_ready_render_pass(),
            make_request(make_frame_plan({
                make_batch(vulkan_backend::vulkan_batch_kind::quad, 0),
            })));

    require(
        result.status
            == vulkan_backend::vulkan_command_recording_readiness_status::command_buffer_unavailable,
        "command recording readiness reports command buffer unavailable");
    require(result.pipeline_ready(), "command buffer failure preserves compatible pipeline");
    require(!result.command_buffer_ready(), "command buffer failure blocks command recording");
    require(
        result.diagnostic == "Vulkan command buffer is not available for recording",
        "command buffer unavailable diagnostic is explicit");
}

void test_command_recording_readiness_accepts_empty_and_clipped_plans()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_command_recording_readiness_factory factory =
        make_command_recording_factory();
    const vulkan_backend::vulkan_command_recording_readiness_result empty =
        vulkan_backend::check_vulkan_command_recording_readiness(
            factory,
            make_ready_render_pass(),
            make_request(make_frame_plan({})));
    require(empty.ready_for_recording(), "empty draw plan is valid for command recording");
    require(empty.empty_draw_plan, "empty draw plan is marked explicitly");
    require(empty.planned_batch_count == 0, "empty draw plan records zero planned batches");
    require(empty.recordable_batch_count == 0, "empty draw plan records zero recordable batches");
    require(
        empty.diagnostic == "Vulkan command recording is ready for an empty draw plan",
        "empty draw plan diagnostic is explicit");

    const vulkan_backend::vulkan_command_recording_readiness_result clipped =
        vulkan_backend::check_vulkan_command_recording_readiness(
            factory,
            make_ready_render_pass(),
            make_request(make_frame_plan(
                {make_batch(vulkan_backend::vulkan_batch_kind::quad, 0, true)},
                1)));
    require(clipped.ready_for_recording(), "clipped draw plan remains valid for command recording");
    require(clipped.clipped_draw_call_count == 1, "clipped draw plan records clipped draw count");
    require(clipped.batch_compatibility.size() == 1, "clipped draw plan records batch compatibility");
    require(clipped.batch_compatibility[0].clipped, "clipped draw plan marks clipped batch");
    require(
        clipped.diagnostic == "Vulkan command recording is ready with clipped draw batches",
        "clipped draw plan diagnostic is explicit");
}

void test_command_recording_readiness_reports_unsupported_draw_batch()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_command_recording_readiness_factory factory(
        vulkan_backend::fake_vulkan_command_recording_readiness_factory_options{
            .supported_batch_kinds = {vulkan_backend::vulkan_batch_kind::quad},
            .compatible_attachment_formats = {
                vulkan_backend::vulkan_render_pass_attachment_format::bgra8_unorm,
            },
            .command_buffer_available = true,
            .command_buffer =
                vulkan_backend::vulkan_command_recording_command_buffer_handle{.value = 502},
        });
    const vulkan_backend::vulkan_command_recording_readiness_result result =
        vulkan_backend::check_vulkan_command_recording_readiness(
            factory,
            make_ready_render_pass(),
            make_request(make_frame_plan({
                make_batch(vulkan_backend::vulkan_batch_kind::image, 7),
            })));

    require(
        result.status
            == vulkan_backend::vulkan_command_recording_readiness_status::unsupported_draw_batch,
        "command recording readiness reports unsupported draw batch");
    require(!result.pipeline_ready(), "unsupported draw batch blocks pipeline compatibility");
    require(result.unsupported_batch_kind == vulkan_backend::vulkan_batch_kind::image, "unsupported batch kind is recorded");
    require(result.unsupported_command_index == 7, "unsupported batch command index is recorded");
    require(
        result.diagnostic == "Vulkan command recording pipeline does not support draw batch: image",
        "unsupported draw batch diagnostic is explicit");
}

void test_command_recording_readiness_updates_lifecycle_fallback_gates()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_command_recording_readiness_factory pipeline_factory(
        vulkan_backend::fake_vulkan_command_recording_readiness_factory_options{
            .supported_batch_kinds = {vulkan_backend::vulkan_batch_kind::quad},
            .compatible_attachment_formats = {
                vulkan_backend::vulkan_render_pass_attachment_format::rgba8_unorm,
            },
            .command_buffer_available = true,
            .command_buffer =
                vulkan_backend::vulkan_command_recording_command_buffer_handle{.value = 503},
        });
    const vulkan_backend::vulkan_command_recording_readiness_result pipeline_incompatible =
        vulkan_backend::check_vulkan_command_recording_readiness(
            pipeline_factory,
            make_ready_render_pass(),
            make_request(make_frame_plan({
                make_batch(vulkan_backend::vulkan_batch_kind::quad, 0),
            })));
    vulkan_backend::null_vulkan_backend_device pipeline_device(pipeline_incompatible);
    const vulkan_backend::vulkan_backend_frame_result pipeline_frame =
        vulkan_backend::submit_vulkan_backend_frame(
            pipeline_device,
            quiz_vulkan::render::render_draw_list{},
            quiz_vulkan::render::render_rect{0.0f, 0.0f, 64.0f, 64.0f});
    require(
        pipeline_frame.fallback_reason
            == vulkan_backend::vulkan_backend_fallback_reason::pipeline_unavailable,
        "pipeline incompatibility reaches the pipeline fallback gate");
    require(!pipeline_frame.lifecycle.pipeline_ready, "pipeline incompatibility clears pipeline readiness");
    require(!pipeline_frame.lifecycle.effective_pipeline_ready(), "pipeline incompatibility clears effective pipeline readiness");

    vulkan_backend::fake_vulkan_command_recording_readiness_factory command_buffer_factory(
        vulkan_backend::fake_vulkan_command_recording_readiness_factory_options{
            .supported_batch_kinds = {vulkan_backend::vulkan_batch_kind::quad},
            .compatible_attachment_formats = {
                vulkan_backend::vulkan_render_pass_attachment_format::bgra8_unorm,
            },
            .command_buffer_available = false,
            .command_buffer = {},
        });
    const vulkan_backend::vulkan_command_recording_readiness_result command_buffer_unavailable =
        vulkan_backend::check_vulkan_command_recording_readiness(
            command_buffer_factory,
            make_ready_render_pass(),
            make_request(make_frame_plan({
                make_batch(vulkan_backend::vulkan_batch_kind::quad, 0),
            })));
    vulkan_backend::null_vulkan_backend_device command_buffer_device(command_buffer_unavailable);
    const vulkan_backend::vulkan_backend_frame_result command_buffer_frame =
        vulkan_backend::submit_vulkan_backend_frame(
            command_buffer_device,
            quiz_vulkan::render::render_draw_list{},
            quiz_vulkan::render::render_rect{0.0f, 0.0f, 64.0f, 64.0f});
    require(
        command_buffer_frame.fallback_reason
            == vulkan_backend::vulkan_backend_fallback_reason::command_recorder_unavailable,
        "command buffer unavailability reaches the command recorder fallback gate");
    require(command_buffer_frame.lifecycle.pipeline_ready, "command buffer failure keeps pipeline ready");
    require(!command_buffer_frame.lifecycle.command_recorder_ready, "command buffer failure clears recorder readiness");
}

void test_command_recording_status_names_are_stable()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    require(
        vulkan_backend::command_recording_readiness_status_name(
            vulkan_backend::vulkan_command_recording_readiness_status::not_requested)
            == std::string_view{"not_requested"},
        "command recording status name for not requested is stable");
    require(
        vulkan_backend::command_recording_readiness_status_name(
            vulkan_backend::vulkan_command_recording_readiness_status::ready)
            == std::string_view{"ready"},
        "command recording status name for ready is stable");
    require(
        vulkan_backend::command_recording_readiness_status_name(
            vulkan_backend::vulkan_command_recording_readiness_status::render_pass_unavailable)
            == std::string_view{"render_pass_unavailable"},
        "command recording status name for render pass unavailable is stable");
    require(
        vulkan_backend::command_recording_readiness_status_name(
            vulkan_backend::vulkan_command_recording_readiness_status::framebuffer_unavailable)
            == std::string_view{"framebuffer_unavailable"},
        "command recording status name for framebuffer unavailable is stable");
    require(
        vulkan_backend::command_recording_readiness_status_name(
            vulkan_backend::vulkan_command_recording_readiness_status::pipeline_incompatible)
            == std::string_view{"pipeline_incompatible"},
        "command recording status name for pipeline incompatible is stable");
    require(
        vulkan_backend::command_recording_readiness_status_name(
            vulkan_backend::vulkan_command_recording_readiness_status::command_buffer_unavailable)
            == std::string_view{"command_buffer_unavailable"},
        "command recording status name for command buffer unavailable is stable");
    require(
        vulkan_backend::command_recording_readiness_status_name(
            vulkan_backend::vulkan_command_recording_readiness_status::unsupported_draw_batch)
            == std::string_view{"unsupported_draw_batch"},
        "command recording status name for unsupported draw batch is stable");
    require(
        vulkan_backend::command_recording_batch_kind_name(vulkan_backend::vulkan_batch_kind::image)
            == std::string_view{"image"},
        "command recording batch kind name for image is stable");
}

} // namespace

int main()
{
    test_command_recording_readiness_accepts_ready_render_pass_and_plan();
    test_command_recording_readiness_reports_render_pass_and_framebuffer_failures();
    test_command_recording_readiness_reports_pipeline_attachment_incompatibility();
    test_command_recording_readiness_reports_command_buffer_unavailable();
    test_command_recording_readiness_accepts_empty_and_clipped_plans();
    test_command_recording_readiness_reports_unsupported_draw_batch();
    test_command_recording_readiness_updates_lifecycle_fallback_gates();
    test_command_recording_status_names_are_stable();
    return 0;
}
