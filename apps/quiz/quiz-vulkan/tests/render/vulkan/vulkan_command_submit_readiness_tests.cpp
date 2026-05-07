#include "render/vulkan/vulkan_backend_adapter.h"
#include "render/vulkan/vulkan_backend_command_recording.h"
#include "render/vulkan/vulkan_backend_command_submit.h"
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

quiz_vulkan::render::vulkan_backend::vulkan_render_pass_attachment_request make_color_attachment()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_render_pass_attachment_request{
        .role = vulkan_backend::vulkan_render_pass_attachment_role::color,
        .format = vulkan_backend::vulkan_render_pass_attachment_format::bgra8_unorm,
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_render_pass_create_result make_ready_render_pass()
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
        .requested_attachments = {make_color_attachment()},
        .selected_attachments = {make_color_attachment()},
        .diagnostic = "Vulkan render pass and framebuffer created",
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_draw_batch make_batch(
    quiz_vulkan::render::vulkan_backend::vulkan_batch_kind kind,
    std::size_t command_index)
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_draw_batch{
        .kind = kind,
        .command_index = command_index,
        .node_id = "batch",
        .bounds = quiz_vulkan::render::render_rect{0.0f, 0.0f, 64.0f, 64.0f},
        .clipped_bounds = quiz_vulkan::render::render_rect{0.0f, 0.0f, 64.0f, 64.0f},
        .paint = quiz_vulkan::render::render_paint{},
        .scissor = vulkan_backend::vulkan_scissor_rect{.x = 0, .y = 0, .width = 64, .height = 64},
        .vertices = {},
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_frame_plan make_frame_plan(
    std::vector<quiz_vulkan::render::vulkan_backend::vulkan_draw_batch> batches)
{
    return quiz_vulkan::render::vulkan_backend::vulkan_frame_plan{
        .viewport = quiz_vulkan::render::render_rect{0.0f, 0.0f, 1280.0f, 720.0f},
        .surface_width = 1280,
        .surface_height = 720,
        .clipped_draw_call_count = 0,
        .discarded_draw_call_count = 0,
        .batches = std::move(batches),
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_command_recording_readiness_result
make_ready_command_recording()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_command_recording_readiness_factory factory(
        vulkan_backend::fake_vulkan_command_recording_readiness_factory_options{
            .supported_batch_kinds = {
                vulkan_backend::vulkan_batch_kind::quad,
                vulkan_backend::vulkan_batch_kind::text,
                vulkan_backend::vulkan_batch_kind::image,
                vulkan_backend::vulkan_batch_kind::debug_bounds,
            },
            .compatible_attachment_formats = {
                vulkan_backend::vulkan_render_pass_attachment_format::bgra8_unorm,
            },
            .command_buffer_available = true,
            .command_buffer =
                vulkan_backend::vulkan_command_recording_command_buffer_handle{.value = 500},
        });

    return vulkan_backend::check_vulkan_command_recording_readiness(
        factory,
        make_ready_render_pass(),
        vulkan_backend::vulkan_command_recording_readiness_request{
            .frame_plan = make_frame_plan({
                make_batch(vulkan_backend::vulkan_batch_kind::quad, 0),
            }),
        });
}

quiz_vulkan::render::vulkan_backend::vulkan_command_submit_readiness_result
check_submit_readiness(
    const quiz_vulkan::render::vulkan_backend::vulkan_command_recording_readiness_result& recording,
    quiz_vulkan::render::vulkan_backend::fake_vulkan_command_submit_readiness_factory_options options = {},
    quiz_vulkan::render::vulkan_backend::vulkan_command_submit_readiness_request request = {})
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_command_submit_readiness_factory factory(std::move(options));
    return vulkan_backend::check_vulkan_command_submit_readiness(factory, recording, request);
}

void test_submit_readiness_accepts_recorded_command_buffer_and_present_preconditions()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_command_submit_readiness_result result =
        check_submit_readiness(make_ready_command_recording());

    require(result.checked, "command submit readiness result is checked");
    require(
        result.status == vulkan_backend::vulkan_command_submit_readiness_status::ready,
        "command submit readiness reports ready");
    require(result.preconditions_ready(), "command submit preconditions are ready");
    require(result.ready_for_submit(), "command submit readiness reaches submit gate");
    require(result.ready_for_present(), "command submit readiness reaches present gate");
    require(result.command_buffer.value == 500, "command submit uses recorded command buffer handle");
    require(result.submit_queue.value == 100, "command submit selects graphics queue");
    require(result.planned_batch_count == 1, "command submit records planned batch count");
    require(result.submitted_batch_count == 1, "command submit records submitted batch count");
    require(result.frame_result.completed(), "command submit frame result completes");
    require(
        result.diagnostic == "Vulkan command buffer submit is ready and frame submit completed",
        "command submit readiness records success diagnostic");
}

void test_submit_readiness_reports_command_recording_and_command_buffer_failures()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::vulkan_command_recording_readiness_result recording_unavailable =
        make_ready_command_recording();
    recording_unavailable.status =
        vulkan_backend::vulkan_command_recording_readiness_status::pipeline_incompatible;
    recording_unavailable.pipeline_compatible = false;
    const vulkan_backend::vulkan_command_submit_readiness_result missing_recording =
        check_submit_readiness(recording_unavailable);

    require(
        missing_recording.status
            == vulkan_backend::vulkan_command_submit_readiness_status::command_recording_unavailable,
        "command submit readiness reports command recording unavailable");
    require(!missing_recording.ready_for_submit(), "missing command recording cannot submit");
    require(
        missing_recording.diagnostic
            == "Vulkan command recording is not ready for command buffer submit",
        "missing command recording diagnostic is explicit");

    vulkan_backend::vulkan_command_recording_readiness_result command_buffer_unavailable =
        make_ready_command_recording();
    command_buffer_unavailable.command_buffer = {};
    command_buffer_unavailable.command_buffer_available = false;
    const vulkan_backend::vulkan_command_submit_readiness_result missing_command_buffer =
        check_submit_readiness(command_buffer_unavailable);

    require(
        missing_command_buffer.status
            == vulkan_backend::vulkan_command_submit_readiness_status::command_buffer_unavailable,
        "command submit readiness reports command buffer unavailable");
    require(!missing_command_buffer.ready_for_submit(), "missing command buffer cannot submit");
    require(
        missing_command_buffer.diagnostic == "Vulkan command buffer is not available for submit",
        "missing command buffer diagnostic is explicit");
}

void test_submit_readiness_reports_sync_queue_and_present_precondition_failures()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_command_submit_readiness_result missing_sync =
        check_submit_readiness(
            make_ready_command_recording(),
            vulkan_backend::fake_vulkan_command_submit_readiness_factory_options{
                .sync_primitives_available = false,
            });
    require(
        missing_sync.status
            == vulkan_backend::vulkan_command_submit_readiness_status::sync_primitives_unavailable,
        "command submit readiness reports missing sync primitives");
    require(
        missing_sync.diagnostic
            == "Vulkan frame sync primitives are not available for command buffer submit",
        "missing sync primitive diagnostic is explicit");

    const vulkan_backend::vulkan_command_submit_readiness_result missing_queue =
        check_submit_readiness(
            make_ready_command_recording(),
            vulkan_backend::fake_vulkan_command_submit_readiness_factory_options{
                .submit_queue_available = false,
            });
    require(
        missing_queue.status
            == vulkan_backend::vulkan_command_submit_readiness_status::submit_queue_unavailable,
        "command submit readiness reports missing submit queue");
    require(
        missing_queue.diagnostic == "Vulkan submit queue is not available for command buffer submit",
        "missing submit queue diagnostic is explicit");

    const vulkan_backend::vulkan_command_submit_readiness_result missing_present =
        check_submit_readiness(
            make_ready_command_recording(),
            {},
            vulkan_backend::vulkan_command_submit_readiness_request{
                .sync_policy = {},
                .require_present_target = true,
                .present_target_available = false,
                .image_id = vulkan_backend::vulkan_swapchain_image_id{.value = 7},
            });
    require(
        missing_present.status
            == vulkan_backend::vulkan_command_submit_readiness_status::present_target_unavailable,
        "command submit readiness reports missing present target");
    require(
        missing_present.diagnostic
            == "Vulkan present target is not available after command buffer submit",
        "missing present target diagnostic is explicit");
}

void test_submit_readiness_classifies_recoverable_and_fatal_submit_failures()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_command_submit_readiness_result recoverable =
        check_submit_readiness(
            make_ready_command_recording(),
            vulkan_backend::fake_vulkan_command_submit_readiness_factory_options{
                .failure_mode = vulkan_backend::vulkan_command_submit_failure_mode::recoverable,
            });
    require(
        recoverable.status
            == vulkan_backend::vulkan_command_submit_readiness_status::submit_failed_recoverable,
        "command submit readiness reports recoverable submit failure");
    require(recoverable.preconditions_ready(), "recoverable submit failure keeps preconditions ready");
    require(recoverable.recoverable_submit_failure(), "recoverable submit failure is classified");
    require(!recoverable.fatal_submit_failure(), "recoverable submit failure is not fatal");
    require(
        recoverable.frame_result.diagnostic == "Vulkan command buffer submit failed recoverably",
        "recoverable submit failure diagnostic is explicit");

    const vulkan_backend::vulkan_command_submit_readiness_result fatal =
        check_submit_readiness(
            make_ready_command_recording(),
            vulkan_backend::fake_vulkan_command_submit_readiness_factory_options{
                .failure_mode = vulkan_backend::vulkan_command_submit_failure_mode::fatal,
            });
    require(
        fatal.status == vulkan_backend::vulkan_command_submit_readiness_status::submit_failed_fatal,
        "command submit readiness reports fatal submit failure");
    require(fatal.preconditions_ready(), "fatal submit failure keeps preconditions ready");
    require(fatal.fatal_submit_failure(), "fatal submit failure is classified");
    require(!fatal.recoverable_submit_failure(), "fatal submit failure is not recoverable");
    require(
        fatal.frame_result.diagnostic == "Vulkan command buffer submit failed fatally",
        "fatal submit failure diagnostic is explicit");
}

void test_submit_readiness_updates_lifecycle_and_frame_fallback()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_command_submit_readiness_result missing_sync =
        check_submit_readiness(
            make_ready_command_recording(),
            vulkan_backend::fake_vulkan_command_submit_readiness_factory_options{
                .sync_primitives_available = false,
            });
    const vulkan_backend::vulkan_backend_lifecycle_readiness lifecycle =
        vulkan_backend::apply_vulkan_command_submit_readiness_to_lifecycle({}, missing_sync);
    require(lifecycle.command_submit.checked, "lifecycle stores command submit readiness");
    require(lifecycle.effective_command_recorder_ready(), "lifecycle keeps command recorder ready");
    require(!lifecycle.effective_command_submit_ready(), "lifecycle blocks missing sync submit");
    require(!lifecycle.ready_for_frame(), "lifecycle does not reach frame readiness without submit sync");

    vulkan_backend::null_vulkan_backend_device device(missing_sync);
    const vulkan_backend::vulkan_backend_frame_result frame =
        vulkan_backend::submit_vulkan_backend_frame(
            device,
            quiz_vulkan::render::render_draw_list{},
            quiz_vulkan::render::render_rect{0.0f, 0.0f, 64.0f, 64.0f});
    require(
        frame.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::submit_frame_failed,
        "frame fallback reports submit failure for missing sync readiness");
    require(
        frame.command_submit.status
            == vulkan_backend::vulkan_command_submit_readiness_status::sync_primitives_unavailable,
        "frame result carries command submit readiness diagnostic");

    const vulkan_backend::vulkan_command_submit_readiness_result missing_present =
        check_submit_readiness(
            make_ready_command_recording(),
            {},
            vulkan_backend::vulkan_command_submit_readiness_request{
                .sync_policy = {},
                .require_present_target = true,
                .present_target_available = false,
                .image_id = vulkan_backend::vulkan_swapchain_image_id{.value = 7},
            });
    vulkan_backend::null_vulkan_backend_device present_device(missing_present);
    const vulkan_backend::vulkan_backend_frame_result present_frame =
        vulkan_backend::submit_vulkan_backend_frame(
            present_device,
            quiz_vulkan::render::render_draw_list{},
            quiz_vulkan::render::render_rect{0.0f, 0.0f, 64.0f, 64.0f});
    require(
        present_frame.fallback_reason
            == vulkan_backend::vulkan_backend_fallback_reason::present_frame_failed,
        "frame fallback reports present failure for missing present target");
}

void test_submit_readiness_names_are_stable()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    require(
        vulkan_backend::command_submit_readiness_status_name(
            vulkan_backend::vulkan_command_submit_readiness_status::ready)
            == std::string_view{"ready"},
        "command submit readiness ready status name is stable");
    require(
        vulkan_backend::command_submit_readiness_status_name(
            vulkan_backend::vulkan_command_submit_readiness_status::submit_failed_recoverable)
            == std::string_view{"submit_failed_recoverable"},
        "command submit recoverable failure status name is stable");
    require(
        vulkan_backend::command_submit_frame_status_name(
            vulkan_backend::vulkan_command_submit_frame_status::submitted)
            == std::string_view{"submitted"},
        "command submit frame status name is stable");
    require(
        vulkan_backend::command_submit_failure_mode_name(
            vulkan_backend::vulkan_command_submit_failure_mode::fatal)
            == std::string_view{"fatal"},
        "command submit failure mode name is stable");
}

} // namespace

int main()
{
    test_submit_readiness_accepts_recorded_command_buffer_and_present_preconditions();
    test_submit_readiness_reports_command_recording_and_command_buffer_failures();
    test_submit_readiness_reports_sync_queue_and_present_precondition_failures();
    test_submit_readiness_classifies_recoverable_and_fatal_submit_failures();
    test_submit_readiness_updates_lifecycle_and_frame_fallback();
    test_submit_readiness_names_are_stable();
    return 0;
}
