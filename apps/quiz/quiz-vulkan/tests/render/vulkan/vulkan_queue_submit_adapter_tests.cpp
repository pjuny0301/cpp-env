#include "render/vulkan/vulkan_backend_adapter.h"
#include "render/vulkan/vulkan_backend_command_recording.h"
#include "render/vulkan/vulkan_backend_command_submit.h"
#include "render/vulkan/vulkan_backend_device.h"
#include "render/vulkan/vulkan_backend_instance.h"
#include "render/vulkan/vulkan_backend_loader.h"
#include "render/vulkan/vulkan_backend_queue_submit.h"
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

quiz_vulkan::render::vulkan_backend::vulkan_draw_batch make_batch()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_draw_batch{
        .kind = vulkan_backend::vulkan_batch_kind::quad,
        .command_index = 0,
        .node_id = "batch",
        .bounds = quiz_vulkan::render::render_rect{0.0f, 0.0f, 64.0f, 64.0f},
        .clipped_bounds = quiz_vulkan::render::render_rect{0.0f, 0.0f, 64.0f, 64.0f},
        .paint = quiz_vulkan::render::render_paint{},
        .scissor = vulkan_backend::vulkan_scissor_rect{.x = 0, .y = 0, .width = 64, .height = 64},
        .vertices = {},
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_frame_plan make_frame_plan()
{
    return quiz_vulkan::render::vulkan_backend::vulkan_frame_plan{
        .viewport = quiz_vulkan::render::render_rect{0.0f, 0.0f, 1280.0f, 720.0f},
        .surface_width = 1280,
        .surface_height = 720,
        .clipped_draw_call_count = 0,
        .discarded_draw_call_count = 0,
        .batches = {make_batch()},
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_command_recording_readiness_result
make_ready_command_recording()
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
                vulkan_backend::vulkan_command_recording_command_buffer_handle{.value = 500},
        });

    return vulkan_backend::check_vulkan_command_recording_readiness(
        factory,
        make_ready_render_pass(),
        vulkan_backend::vulkan_command_recording_readiness_request{
            .frame_plan = make_frame_plan(),
        });
}

quiz_vulkan::render::vulkan_backend::vulkan_command_submit_readiness_result
make_ready_command_submit()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_command_submit_readiness_factory factory;
    return vulkan_backend::check_vulkan_command_submit_readiness(
        factory,
        make_ready_command_recording(),
        vulkan_backend::vulkan_command_submit_readiness_request{
            .sync_policy = {},
            .require_present_target = true,
            .present_target_available = true,
            .image_id = vulkan_backend::vulkan_swapchain_image_id{.value = 9},
        });
}

quiz_vulkan::render::vulkan_backend::vulkan_native_swapchain_acquire_operation_result
make_ready_native_acquire_operation()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_native_swapchain_acquire_operation_result{
        .checked = true,
        .status = vulkan_backend::vulkan_native_swapchain_acquire_operation_status::ready,
        .device = vulkan_backend::vulkan_device_handle{.value = 80},
        .swapchain = vulkan_backend::vulkan_swapchain_handle{.value = 90},
        .selected_image_index = 2,
        .image_id = vulkan_backend::vulkan_swapchain_image_id{.value = 9},
        .image_handle = vulkan_backend::vulkan_swapchain_image_handle{.value = 9009},
        .image_binding_ready = true,
        .image_available = true,
        .image_acquired = true,
        .vk_acquire_next_image_callable = true,
        .command_recording_may_consume_acquired_image = true,
    };
}

void test_queue_submit_adapter_submits_before_presenting()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_queue_submit_adapter fake;
    const vulkan_backend::vulkan_queue_submit_present_request present_request =
        vulkan_backend::make_vulkan_queue_submit_present_request_from_acquire(
            make_ready_native_acquire_operation());
    const vulkan_backend::vulkan_queue_submit_present_result result =
        vulkan_backend::submit_and_present_vulkan_queue_frame(
            fake.adapter(),
            make_ready_command_submit(),
            present_request);

    require(result.checked, "queue submit adapter result is checked");
    require(
        result.status == vulkan_backend::vulkan_queue_submit_present_status::submitted_and_presented,
        "queue submit adapter reports submitted and presented");
    require(result.completed(), "queue submit adapter result completes");
    require(result.submit_before_present(), "queue submit adapter result records submit before present");
    require(result.present_execution_ready, "queue submit adapter records present execution readiness");
    require(result.command_submit_ready, "queue submit adapter records command submit readiness");
    require(result.submitted_frame_ready, "queue submit adapter records submitted frame readiness");
    require(result.present_wait_intent_ready, "queue submit adapter records present wait intent");
    require(result.submit_signal_intent_ready, "queue submit adapter records submit signal intent");
    require(result.present_queue_family_ready, "queue submit adapter records present queue family evidence");
    require(result.present_queue_family_index == 1, "queue submit adapter records present family index");
    require(result.acquired_image_index == 2, "queue submit adapter records acquired image index");
    require(result.acquired_image_index_ready, "queue submit adapter records acquired index readiness");
    require(result.image_handle.value == 9009, "queue submit adapter records acquired image handle");
    require(result.acquired_image_handle_ready, "queue submit adapter records acquired handle readiness");
    require(fake.state().submit_before_present(), "fake adapter observes submit before present");
    require(fake.state().last_submit_call.queue.value == 100, "fake adapter receives submit queue");
    require(fake.state().last_submit_call.wait_intent_count == 1, "fake adapter receives wait intent count");
    require(fake.state().last_submit_call.signal_intent_count == 2, "fake adapter receives signal intent count");
    require(fake.state().last_submit_call.command_submit_ready, "fake adapter receives command submit gate");
    require(fake.state().last_present_call.queue.value == 101, "fake adapter receives present queue");
    require(fake.state().last_present_call.queue_family_index == 1, "fake adapter receives present family index");
    require(fake.state().last_present_call.queue_family_ready, "fake adapter receives present family readiness");
    require(fake.state().last_submit_call.command_buffer.value == 500, "fake adapter receives command buffer");
    require(fake.state().last_present_call.image_id.value == 9, "fake adapter receives present image");
    require(fake.state().last_present_call.acquired_image_index == 2, "fake adapter receives acquired image index");
    require(fake.state().last_present_call.image_handle.value == 9009, "fake adapter receives acquired image handle");
    require(fake.state().last_present_call.submitted_frame_ready, "fake adapter receives submitted frame gate");
}

void test_queue_submit_adapter_maps_recoverable_and_fatal_submit_failures()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_queue_submit_adapter recoverable_fake(
        vulkan_backend::fake_vulkan_queue_submit_adapter_options{
            .submit_status =
                vulkan_backend::vulkan_queue_submit_adapter_call_status::failed_recoverable,
        });
    const vulkan_backend::vulkan_queue_submit_present_result recoverable =
        vulkan_backend::submit_and_present_vulkan_queue_frame(
            recoverable_fake.adapter(),
            make_ready_command_submit());
    require(
        recoverable.status
            == vulkan_backend::vulkan_queue_submit_present_status::submit_failed_recoverable,
        "queue submit adapter maps recoverable submit failure");
    require(recoverable.recoverable_failure(), "recoverable submit failure is classified");
    require(!recoverable.present_called, "queue submit adapter does not present after submit failure");
    require(recoverable_fake.state().present_call_count == 0, "fake adapter present is not called after submit failure");
    require(
        recoverable.command_submit.frame_result.recoverable_failure(),
        "recoverable submit failure maps into frame result");

    vulkan_backend::fake_vulkan_queue_submit_adapter fatal_fake(
        vulkan_backend::fake_vulkan_queue_submit_adapter_options{
            .submit_status =
                vulkan_backend::vulkan_queue_submit_adapter_call_status::failed_fatal,
        });
    const vulkan_backend::vulkan_queue_submit_present_result fatal =
        vulkan_backend::submit_and_present_vulkan_queue_frame(
            fatal_fake.adapter(),
            make_ready_command_submit());
    require(
        fatal.status == vulkan_backend::vulkan_queue_submit_present_status::submit_failed_fatal,
        "queue submit adapter maps fatal submit failure");
    require(fatal.fatal_failure(), "fatal submit failure is classified");
    require(!fatal.present_called, "queue submit adapter does not present after fatal submit failure");
    require(
        fatal.command_submit.frame_result.fatal_failure(),
        "fatal submit failure maps into frame result");
}

void test_queue_submit_adapter_maps_recoverable_and_fatal_present_failures()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_queue_submit_adapter recoverable_fake(
        vulkan_backend::fake_vulkan_queue_submit_adapter_options{
            .present_status =
                vulkan_backend::vulkan_queue_submit_adapter_call_status::failed_recoverable,
        });
    const vulkan_backend::vulkan_queue_submit_present_result recoverable =
        vulkan_backend::submit_and_present_vulkan_queue_frame(
            recoverable_fake.adapter(),
            make_ready_command_submit());
    require(
        recoverable.status
            == vulkan_backend::vulkan_queue_submit_present_status::present_failed_recoverable,
        "queue submit adapter maps recoverable present failure");
    require(recoverable.recoverable_failure(), "recoverable present failure is classified");
    require(recoverable.submit_before_present(), "present failure still occurs after submit");
    require(
        recoverable.present_execution_ready,
        "recoverable present failure still records a complete present attempt");
    require(
        recoverable.submitted_frame_ready,
        "recoverable present failure records submitted frame readiness");

    vulkan_backend::fake_vulkan_queue_submit_adapter fatal_fake(
        vulkan_backend::fake_vulkan_queue_submit_adapter_options{
            .present_status =
                vulkan_backend::vulkan_queue_submit_adapter_call_status::failed_fatal,
        });
    const vulkan_backend::vulkan_queue_submit_present_result fatal =
        vulkan_backend::submit_and_present_vulkan_queue_frame(
            fatal_fake.adapter(),
            make_ready_command_submit());
    require(
        fatal.status == vulkan_backend::vulkan_queue_submit_present_status::present_failed_fatal,
        "queue submit adapter maps fatal present failure");
    require(fatal.fatal_failure(), "fatal present failure is classified");
    require(fatal.submit_before_present(), "fatal present failure still occurs after submit");
}

void test_queue_submit_adapter_reports_missing_submit_queue_and_present_target()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::vulkan_command_submit_readiness_result missing_queue =
        make_ready_command_submit();
    missing_queue.submit_queue_available = false;
    missing_queue.submit_queue = {};
    vulkan_backend::fake_vulkan_queue_submit_adapter fake_for_missing_queue;
    const vulkan_backend::vulkan_queue_submit_present_result queue_result =
        vulkan_backend::submit_and_present_vulkan_queue_frame(
            fake_for_missing_queue.adapter(),
            missing_queue);
    require(
        queue_result.status
            == vulkan_backend::vulkan_queue_submit_present_status::submit_queue_unavailable,
        "queue submit adapter reports missing submit queue");
    require(!queue_result.submit_called, "queue submit adapter does not call submit without queue");
    require(fake_for_missing_queue.state().submit_call_count == 0, "fake adapter submit is not called without queue");

    vulkan_backend::vulkan_command_submit_readiness_result missing_present =
        make_ready_command_submit();
    missing_present.present_target_available = false;
    vulkan_backend::fake_vulkan_queue_submit_adapter fake_for_missing_present;
    const vulkan_backend::vulkan_queue_submit_present_result present_result =
        vulkan_backend::submit_and_present_vulkan_queue_frame(
            fake_for_missing_present.adapter(),
            missing_present);
    require(
        present_result.status
            == vulkan_backend::vulkan_queue_submit_present_status::present_target_unavailable,
        "queue submit adapter reports missing present target");
    require(!present_result.submit_called, "queue submit adapter does not submit without present target");
    require(fake_for_missing_present.state().submit_call_count == 0, "fake adapter submit is not called without present target");
}

void test_queue_submit_adapter_reports_missing_present_queue_after_submit()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::vulkan_command_submit_readiness_result missing_present_queue =
        make_ready_command_submit();
    missing_present_queue.command_recording.render_pass.swapchain.device.selected_queues = {
        vulkan_backend::vulkan_device_queue_selection{
            .capability = vulkan_backend::vulkan_device_queue_capability::graphics,
            .family_index = 0,
            .queue = vulkan_backend::vulkan_queue_handle{.value = 100},
        },
    };

    vulkan_backend::fake_vulkan_queue_submit_adapter fake;
    const vulkan_backend::vulkan_queue_submit_present_result result =
        vulkan_backend::submit_and_present_vulkan_queue_frame(
            fake.adapter(),
            missing_present_queue);

    require(
        result.status
            == vulkan_backend::vulkan_queue_submit_present_status::present_target_unavailable,
        "queue submit adapter reports missing present queue as missing present target");
    require(result.submit_called, "queue submit adapter submits before discovering missing present queue");
    require(!result.present_called, "queue submit adapter does not call present without present queue");
    require(fake.state().submit_call_count == 1, "fake adapter submit is called once");
    require(fake.state().present_call_count == 0, "fake adapter present is not called without present queue");
}

void test_queue_submit_adapter_result_wires_backend_frame_summary()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_queue_submit_adapter fake;
    const vulkan_backend::vulkan_queue_submit_present_result queue_result =
        vulkan_backend::submit_and_present_vulkan_queue_frame(
            fake.adapter(),
            make_ready_command_submit(),
            vulkan_backend::vulkan_queue_submit_present_request{
                .require_present = true,
                .image_id = vulkan_backend::vulkan_swapchain_image_id{.value = 9},
            });
    const vulkan_backend::vulkan_backend_queue_submit_adapter_summary summary =
        vulkan_backend::summarize_vulkan_queue_submit_adapter_result(queue_result);
    const vulkan_backend::vulkan_backend_frame_result frame =
        vulkan_backend::apply_vulkan_queue_submit_adapter_result_to_frame(
            {},
            queue_result);

    require(summary.checked, "backend queue submit summary is checked");
    require(
        summary.status
            == vulkan_backend::vulkan_queue_submit_present_status::submitted_and_presented,
        "backend queue submit summary stores success status");
    require(summary.submit_called, "backend queue submit summary records submit call");
    require(summary.present_called, "backend queue submit summary records present call");
    require(
        summary.submit_before_present,
        "backend queue submit summary records submit-before-present ordering");
    require(!summary.recoverable_failure, "backend queue submit summary has no recoverable failure");
    require(!summary.fatal_failure, "backend queue submit summary has no fatal failure");
    require(summary.completed(), "backend queue submit summary completes for adapter success");

    require(frame.queue_submit.checked, "frame stores queue submit adapter result");
    require(frame.queue_submit_adapter.checked, "frame stores queue submit adapter summary");
    require(
        frame.queue_submit_adapter.submit_before_present,
        "frame summary exposes submit-before-present ordering");
    require(
        frame.command_submit.frame_result.completed(),
        "frame command submit diagnostics reflect adapter success");
}

void test_queue_submit_adapter_failures_wire_backend_frame_summary()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_queue_submit_adapter recoverable_fake(
        vulkan_backend::fake_vulkan_queue_submit_adapter_options{
            .submit_status =
                vulkan_backend::vulkan_queue_submit_adapter_call_status::failed_recoverable,
        });
    const vulkan_backend::vulkan_backend_frame_result recoverable_frame =
        vulkan_backend::apply_vulkan_queue_submit_adapter_result_to_frame(
            {},
            vulkan_backend::submit_and_present_vulkan_queue_frame(
                recoverable_fake.adapter(),
                make_ready_command_submit()));
    require(
        recoverable_frame.queue_submit_adapter.status
            == vulkan_backend::vulkan_queue_submit_present_status::submit_failed_recoverable,
        "frame summary stores recoverable submit failure status");
    require(
        recoverable_frame.queue_submit_adapter.submit_called,
        "frame summary records recoverable submit call");
    require(
        !recoverable_frame.queue_submit_adapter.present_called,
        "frame summary records no present after recoverable submit failure");
    require(
        recoverable_frame.queue_submit_adapter.recoverable_failure,
        "frame summary exposes recoverable adapter failure");
    require(
        !recoverable_frame.queue_submit_adapter.fatal_failure,
        "frame summary does not mark recoverable failure fatal");
    require(
        recoverable_frame.queue_submit_adapter.failed(),
        "frame summary classifies recoverable adapter failure as failed");
    require(
        recoverable_frame.command_submit.frame_result.recoverable_failure(),
        "frame command submit diagnostics expose recoverable failure");

    vulkan_backend::fake_vulkan_queue_submit_adapter fatal_fake(
        vulkan_backend::fake_vulkan_queue_submit_adapter_options{
            .present_status =
                vulkan_backend::vulkan_queue_submit_adapter_call_status::failed_fatal,
        });
    const vulkan_backend::vulkan_backend_frame_result fatal_frame =
        vulkan_backend::apply_vulkan_queue_submit_adapter_result_to_frame(
            {},
            vulkan_backend::submit_and_present_vulkan_queue_frame(
                fatal_fake.adapter(),
                make_ready_command_submit()));
    require(
        fatal_frame.queue_submit_adapter.status
            == vulkan_backend::vulkan_queue_submit_present_status::present_failed_fatal,
        "frame summary stores fatal present failure status");
    require(
        fatal_frame.queue_submit_adapter.submit_called,
        "frame summary records submit before fatal present failure");
    require(
        fatal_frame.queue_submit_adapter.present_called,
        "frame summary records fatal present call");
    require(
        fatal_frame.queue_submit_adapter.submit_before_present,
        "frame summary preserves ordering for fatal present failure");
    require(
        fatal_frame.queue_submit_adapter.fatal_failure,
        "frame summary exposes fatal adapter failure");
    require(
        !fatal_frame.queue_submit_adapter.recoverable_failure,
        "frame summary does not mark fatal failure recoverable");
    require(
        fatal_frame.queue_submit.fatal_failure(),
        "frame queue submit diagnostics expose fatal failure");
}

void test_queue_submit_adapter_names_are_stable()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    require(
        vulkan_backend::queue_submit_adapter_call_status_name(
            vulkan_backend::vulkan_queue_submit_adapter_call_status::completed)
            == std::string_view{"completed"},
        "queue submit adapter call status name is stable");
    require(
        vulkan_backend::queue_submit_present_status_name(
            vulkan_backend::vulkan_queue_submit_present_status::submitted_and_presented)
            == std::string_view{"submitted_and_presented"},
        "queue submit present status name is stable");
    require(
        vulkan_backend::queue_submit_present_status_name(
            vulkan_backend::vulkan_queue_submit_present_status::present_failed_fatal)
            == std::string_view{"present_failed_fatal"},
        "queue submit present fatal status name is stable");
}

} // namespace

int main()
{
    test_queue_submit_adapter_submits_before_presenting();
    test_queue_submit_adapter_maps_recoverable_and_fatal_submit_failures();
    test_queue_submit_adapter_maps_recoverable_and_fatal_present_failures();
    test_queue_submit_adapter_reports_missing_submit_queue_and_present_target();
    test_queue_submit_adapter_reports_missing_present_queue_after_submit();
    test_queue_submit_adapter_result_wires_backend_frame_summary();
    test_queue_submit_adapter_failures_wire_backend_frame_summary();
    test_queue_submit_adapter_names_are_stable();
    return 0;
}
