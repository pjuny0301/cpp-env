#include "render/vulkan/vulkan_backend_adapter.h"
#include "render/vulkan/vulkan_backend_device.h"
#include "render/vulkan/vulkan_backend_instance.h"
#include "render/vulkan/vulkan_backend_loader.h"
#include "render/vulkan/vulkan_backend_swapchain.h"

#include <cassert>
#include <cstdio>
#include <string>
#include <string_view>

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

quiz_vulkan::render::vulkan_backend::vulkan_device_create_result make_unavailable_device()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_device_create_result{
        .checked = true,
        .status = vulkan_backend::vulkan_device_create_status::missing_required_queue,
        .instance = make_created_instance(),
        .handle = {},
        .selected_extensions = {"VK_KHR_swapchain"},
        .selected_queues = {
            vulkan_backend::vulkan_device_queue_selection{
                .capability = vulkan_backend::vulkan_device_queue_capability::graphics,
                .family_index = 0,
                .queue = vulkan_backend::vulkan_queue_handle{.value = 100},
            },
        },
        .diagnostic = "missing required device queue: present",
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_swapchain_create_request make_swapchain_request(
    quiz_vulkan::render::vulkan_backend::vulkan_swapchain_present_mode present_mode)
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_swapchain_create_request{
        .requested_extent = vulkan_backend::vulkan_surface_extent{.width = 1280, .height = 720},
        .requested_present_mode = present_mode,
        .min_image_count = 3,
    };
}

quiz_vulkan::render::vulkan_backend::fake_vulkan_swapchain_factory make_swapchain_factory()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::fake_vulkan_swapchain_factory(
        vulkan_backend::fake_vulkan_swapchain_factory_options{
            .supported_present_modes = {
                vulkan_backend::vulkan_swapchain_present_mode::fifo,
                vulkan_backend::vulkan_swapchain_present_mode::mailbox,
            },
            .min_extent = vulkan_backend::vulkan_surface_extent{.width = 1, .height = 1},
            .max_extent = vulkan_backend::vulkan_surface_extent{.width = 4096, .height = 4096},
            .fail_creation = false,
            .handle = vulkan_backend::vulkan_swapchain_handle{.value = 90},
        });
}

quiz_vulkan::render::vulkan_backend::vulkan_swapchain_image_acquire_request
make_ready_acquire_request()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_swapchain_image_acquire_request{
        .requested = true,
        .lifecycle_ready = true,
        .swapchain_ready = true,
        .swapchain = vulkan_backend::vulkan_swapchain_handle{.value = 90},
        .image_count = 3,
        .timeout_nanoseconds = 0,
        .image_available_semaphore_ready = true,
        .fence_ready = true,
        .allow_suboptimal = true,
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_swapchain_acquire_result make_acquired_image(
    quiz_vulkan::render::vulkan_backend::vulkan_swapchain_acquire_status status =
        quiz_vulkan::render::vulkan_backend::vulkan_swapchain_acquire_status::acquired)
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_swapchain_acquire_result{
        .status = status,
        .image = vulkan_backend::vulkan_swapchain_image_state{
            .id = vulkan_backend::vulkan_swapchain_image_id{.value = 2},
            .available = true,
            .acquired = true,
            .presented = false,
        },
    };
}

void test_swapchain_factory_marks_ready_device_swapchain_ready()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_swapchain_factory factory = make_swapchain_factory();
    const vulkan_backend::vulkan_swapchain_create_result result =
        vulkan_backend::create_vulkan_swapchain(
            factory,
            make_ready_device(),
            make_swapchain_request(vulkan_backend::vulkan_swapchain_present_mode::mailbox));

    require(result.checked, "swapchain result records that creation was checked");
    require(
        result.status == vulkan_backend::vulkan_swapchain_create_status::created,
        "swapchain result reports created status");
    require(result.ready_for_frame(), "created swapchain reaches the frame gate");
    require(result.device.ready_for_backend(), "created swapchain preserves ready device");
    require(result.handle.valid(), "created swapchain returns a valid opaque handle");
    require(result.handle.value == 90, "created swapchain returns configured fake handle");
    require(result.requested_extent.width == 1280, "created swapchain records requested width");
    require(result.requested_extent.height == 720, "created swapchain records requested height");
    require(result.selected_extent.width == 1280, "created swapchain records selected width");
    require(result.selected_extent.height == 720, "created swapchain records selected height");
    require(
        result.requested_present_mode == vulkan_backend::vulkan_swapchain_present_mode::mailbox,
        "created swapchain records requested mailbox mode");
    require(
        result.selected_present_mode == vulkan_backend::vulkan_swapchain_present_mode::mailbox,
        "created swapchain selects requested mailbox mode when supported");
    require(result.requested_present_mode_supported, "created swapchain marks requested mode supported");
    require(!result.fallback_to_fifo, "created swapchain does not fallback when requested mode is supported");
    require(result.min_image_count == 3, "created swapchain records requested image count");

    vulkan_backend::null_vulkan_backend_device device(result);
    const vulkan_backend::vulkan_backend_lifecycle_readiness lifecycle =
        device.current_lifecycle_readiness();
    require(lifecycle.instance_ready, "swapchain lifecycle keeps instance ready");
    require(lifecycle.device_ready, "swapchain lifecycle keeps device ready");
    require(lifecycle.swapchain_ready, "swapchain lifecycle marks swapchain ready");
    require(lifecycle.effective_swapchain_ready(), "swapchain lifecycle effective swapchain readiness is true");
    require(device.current_surface_extent().width == 1280, "null backend exposes selected swapchain width");
    require(device.current_surface_extent().height == 720, "null backend exposes selected swapchain height");
}

void test_swapchain_factory_maps_unavailable_device_to_swapchain_unavailable()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_swapchain_factory factory = make_swapchain_factory();
    const vulkan_backend::vulkan_swapchain_create_result result =
        vulkan_backend::create_vulkan_swapchain(
            factory,
            make_unavailable_device(),
            make_swapchain_request(vulkan_backend::vulkan_swapchain_present_mode::fifo));

    require(result.checked, "device-unavailable swapchain result is checked");
    require(
        result.status == vulkan_backend::vulkan_swapchain_create_status::device_unavailable,
        "device-unavailable swapchain result reports unavailable device status");
    require(!result.ready_for_frame(), "device-unavailable swapchain result does not reach frame gate");
    require(!result.handle.valid(), "device-unavailable swapchain result has no swapchain handle");
    require(!result.selected_extent.valid(), "device-unavailable swapchain result selects no extent");
    require(
        result.diagnostic == "Vulkan device is not ready for swapchain creation",
        "device-unavailable swapchain result records device diagnostic");
}

void test_swapchain_factory_reports_invalid_extent()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_swapchain_factory factory = make_swapchain_factory();
    const vulkan_backend::vulkan_swapchain_create_result result =
        vulkan_backend::create_vulkan_swapchain(
            factory,
            make_ready_device(),
            vulkan_backend::vulkan_swapchain_create_request{
                .requested_extent = vulkan_backend::vulkan_surface_extent{.width = 0, .height = 720},
                .requested_present_mode = vulkan_backend::vulkan_swapchain_present_mode::fifo,
                .min_image_count = 2,
            });

    require(result.checked, "invalid-extent swapchain result is checked");
    require(
        result.status == vulkan_backend::vulkan_swapchain_create_status::invalid_surface_extent,
        "invalid-extent swapchain result reports invalid surface extent");
    require(!result.ready_for_frame(), "invalid-extent swapchain result does not reach frame gate");
    require(!result.handle.valid(), "invalid-extent swapchain result has no handle");
    require(!result.selected_extent.valid(), "invalid-extent swapchain result selects no extent");
    require(
        result.diagnostic == "Vulkan swapchain surface extent is invalid",
        "invalid-extent swapchain result records diagnostic");
}

void test_swapchain_factory_falls_back_to_fifo_present_mode()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_swapchain_factory factory(
        vulkan_backend::fake_vulkan_swapchain_factory_options{
            .supported_present_modes = {vulkan_backend::vulkan_swapchain_present_mode::fifo},
            .min_extent = vulkan_backend::vulkan_surface_extent{.width = 1, .height = 1},
            .max_extent = vulkan_backend::vulkan_surface_extent{.width = 4096, .height = 4096},
            .fail_creation = false,
            .handle = vulkan_backend::vulkan_swapchain_handle{.value = 91},
        });

    const vulkan_backend::vulkan_swapchain_create_result result =
        vulkan_backend::create_vulkan_swapchain(
            factory,
            make_ready_device(),
            make_swapchain_request(vulkan_backend::vulkan_swapchain_present_mode::mailbox));

    require(result.checked, "fallback-present-mode swapchain result is checked");
    require(
        result.status == vulkan_backend::vulkan_swapchain_create_status::created,
        "fallback-present-mode swapchain result still creates swapchain");
    require(result.ready_for_frame(), "fallback-present-mode swapchain reaches frame gate");
    require(
        result.requested_present_mode == vulkan_backend::vulkan_swapchain_present_mode::mailbox,
        "fallback-present-mode swapchain records requested mailbox mode");
    require(
        result.selected_present_mode == vulkan_backend::vulkan_swapchain_present_mode::fifo,
        "fallback-present-mode swapchain selects fifo fallback");
    require(!result.requested_present_mode_supported, "fallback-present-mode swapchain marks request unsupported");
    require(result.fallback_to_fifo, "fallback-present-mode swapchain records fifo fallback");
}

void test_swapchain_image_acquire_plan_accepts_ready_image()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_swapchain_image_acquire_plan_result plan =
        vulkan_backend::build_vulkan_swapchain_image_acquire_plan(
            make_ready_acquire_request(),
            make_acquired_image());

    require(plan.checked, "swapchain image acquire plan is checked");
    require(
        plan.status == vulkan_backend::vulkan_swapchain_image_acquire_plan_status::ready,
        "swapchain image acquire plan reports ready");
    require(plan.ready_for_command_recording(), "acquired swapchain image reaches command recording gate");
    require(!plan.blocked(), "acquired swapchain image does not block command recording");
    require(plan.selected_image_index == 2, "swapchain image acquire plan stores selected image index");
    require(plan.image_id.value == 2, "swapchain image acquire plan stores selected image id");
    require(plan.sync_primitives_ready, "swapchain image acquire plan records sync readiness");
}

void test_swapchain_image_acquire_plan_reports_readiness_gates()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::vulkan_swapchain_image_acquire_request lifecycle_unavailable =
        make_ready_acquire_request();
    lifecycle_unavailable.lifecycle_ready = false;
    const vulkan_backend::vulkan_swapchain_image_acquire_plan_result lifecycle_plan =
        vulkan_backend::build_vulkan_swapchain_image_acquire_plan(
            lifecycle_unavailable,
            make_acquired_image());
    require(
        lifecycle_plan.status
            == vulkan_backend::vulkan_swapchain_image_acquire_plan_status::lifecycle_unavailable,
        "swapchain image acquire plan blocks when lifecycle is unavailable");

    vulkan_backend::vulkan_swapchain_image_acquire_request swapchain_unavailable =
        make_ready_acquire_request();
    swapchain_unavailable.swapchain_ready = false;
    const vulkan_backend::vulkan_swapchain_image_acquire_plan_result swapchain_plan =
        vulkan_backend::build_vulkan_swapchain_image_acquire_plan(
            swapchain_unavailable,
            make_acquired_image());
    require(
        swapchain_plan.status
            == vulkan_backend::vulkan_swapchain_image_acquire_plan_status::swapchain_unavailable,
        "swapchain image acquire plan blocks when swapchain is unavailable");

    vulkan_backend::vulkan_swapchain_image_acquire_request sync_unavailable =
        make_ready_acquire_request();
    sync_unavailable.fence_ready = false;
    const vulkan_backend::vulkan_swapchain_image_acquire_plan_result sync_plan =
        vulkan_backend::build_vulkan_swapchain_image_acquire_plan(
            sync_unavailable,
            make_acquired_image());
    require(
        sync_plan.status
            == vulkan_backend::vulkan_swapchain_image_acquire_plan_status::sync_unavailable,
        "swapchain image acquire plan blocks when acquire sync is unavailable");

    vulkan_backend::vulkan_swapchain_image_acquire_request no_images =
        make_ready_acquire_request();
    no_images.image_count = 0;
    const vulkan_backend::vulkan_swapchain_image_acquire_plan_result no_images_plan =
        vulkan_backend::build_vulkan_swapchain_image_acquire_plan(
            no_images,
            make_acquired_image());
    require(
        no_images_plan.status
            == vulkan_backend::vulkan_swapchain_image_acquire_plan_status::no_images_available,
        "swapchain image acquire plan blocks when no images are available");
}

void test_swapchain_image_acquire_plan_models_timeout_out_of_date_suboptimal_and_error()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_swapchain_image_acquire_plan_result timeout =
        vulkan_backend::build_vulkan_swapchain_image_acquire_plan(
            make_ready_acquire_request(),
            vulkan_backend::vulkan_swapchain_acquire_result{
                .status = vulkan_backend::vulkan_swapchain_acquire_status::timeout,
                .image = {},
            });
    require(
        timeout.status == vulkan_backend::vulkan_swapchain_image_acquire_plan_status::timeout,
        "swapchain image acquire plan reports timeout");
    require(timeout.timed_out, "swapchain image acquire plan records timeout flag");
    require(timeout.blocked(), "timed-out swapchain image acquire blocks command recording");

    const vulkan_backend::vulkan_swapchain_image_acquire_plan_result out_of_date =
        vulkan_backend::build_vulkan_swapchain_image_acquire_plan(
            make_ready_acquire_request(),
            vulkan_backend::vulkan_swapchain_acquire_result{
                .status = vulkan_backend::vulkan_swapchain_acquire_status::out_of_date,
                .image = {},
            });
    require(
        out_of_date.status
            == vulkan_backend::vulkan_swapchain_image_acquire_plan_status::out_of_date,
        "swapchain image acquire plan reports out-of-date swapchain");
    require(out_of_date.out_of_date, "swapchain image acquire plan records out-of-date flag");
    require(out_of_date.blocked(), "out-of-date swapchain image acquire blocks command recording");

    const vulkan_backend::vulkan_swapchain_image_acquire_plan_result suboptimal =
        vulkan_backend::build_vulkan_swapchain_image_acquire_plan(
            make_ready_acquire_request(),
            make_acquired_image(vulkan_backend::vulkan_swapchain_acquire_status::suboptimal));
    require(
        suboptimal.status == vulkan_backend::vulkan_swapchain_image_acquire_plan_status::suboptimal,
        "swapchain image acquire plan reports suboptimal status");
    require(suboptimal.suboptimal, "swapchain image acquire plan records suboptimal flag");
    require(
        suboptimal.ready_for_command_recording(),
        "suboptimal swapchain image acquire can still proceed with a recordable image");

    const vulkan_backend::vulkan_swapchain_image_acquire_plan_result error =
        vulkan_backend::build_vulkan_swapchain_image_acquire_plan(
            make_ready_acquire_request(),
            vulkan_backend::vulkan_swapchain_acquire_result{
                .status = vulkan_backend::vulkan_swapchain_acquire_status::error,
                .image = {},
            });
    require(
        error.status == vulkan_backend::vulkan_swapchain_image_acquire_plan_status::error,
        "swapchain image acquire plan reports error status");
    require(error.error, "swapchain image acquire plan records error flag");
    require(error.blocked(), "errored swapchain image acquire blocks command recording");
}

void test_lifecycle_fallback_moves_to_swapchain_only_after_device_is_ready()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_swapchain_factory factory = make_swapchain_factory();
    const vulkan_backend::vulkan_swapchain_create_request request =
        make_swapchain_request(vulkan_backend::vulkan_swapchain_present_mode::fifo);

    const vulkan_backend::vulkan_swapchain_create_result device_unavailable =
        vulkan_backend::create_vulkan_swapchain(factory, make_unavailable_device(), request);
    vulkan_backend::null_vulkan_backend_device device_unavailable_backend(device_unavailable);
    const quiz_vulkan::render::render_draw_list draw_list;
    const vulkan_backend::vulkan_backend_frame_result device_unavailable_frame =
        vulkan_backend::submit_vulkan_backend_frame(
            device_unavailable_backend,
            draw_list,
            quiz_vulkan::render::render_rect{0.0f, 0.0f, 64.0f, 64.0f});
    require(
        device_unavailable_frame.fallback_reason
            == vulkan_backend::vulkan_backend_fallback_reason::device_unavailable,
        "device-unavailable swapchain result stays at device fallback gate");

    const vulkan_backend::vulkan_swapchain_create_result swapchain_unavailable =
        vulkan_backend::create_vulkan_swapchain(
            factory,
            make_ready_device(),
            vulkan_backend::vulkan_swapchain_create_request{
                .requested_extent = vulkan_backend::vulkan_surface_extent{.width = 0, .height = 720},
                .requested_present_mode = vulkan_backend::vulkan_swapchain_present_mode::fifo,
                .min_image_count = 2,
            });
    vulkan_backend::null_vulkan_backend_device swapchain_unavailable_backend(swapchain_unavailable);
    const vulkan_backend::vulkan_backend_frame_result swapchain_unavailable_frame =
        vulkan_backend::submit_vulkan_backend_frame(
            swapchain_unavailable_backend,
            draw_list,
            quiz_vulkan::render::render_rect{0.0f, 0.0f, 64.0f, 64.0f});
    require(
        swapchain_unavailable_frame.lifecycle.device_ready,
        "swapchain-unavailable frame keeps device ready");
    require(
        !swapchain_unavailable_frame.lifecycle.swapchain_ready,
        "swapchain-unavailable frame keeps swapchain unavailable");
    require(
        swapchain_unavailable_frame.fallback_reason
            == vulkan_backend::vulkan_backend_fallback_reason::swapchain_unavailable,
        "swapchain-unavailable frame moves fallback to swapchain gate after device is ready");
}

void test_vulkan_swapchain_create_status_names_are_stable()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    require(
        vulkan_backend::swapchain_create_status_name(
            vulkan_backend::vulkan_swapchain_create_status::not_requested)
            == std::string_view{"not_requested"},
        "swapchain create status name for not requested is stable");
    require(
        vulkan_backend::swapchain_create_status_name(
            vulkan_backend::vulkan_swapchain_create_status::created)
            == std::string_view{"created"},
        "swapchain create status name for created is stable");
    require(
        vulkan_backend::swapchain_create_status_name(
            vulkan_backend::vulkan_swapchain_create_status::device_unavailable)
            == std::string_view{"device_unavailable"},
        "swapchain create status name for device unavailable is stable");
    require(
        vulkan_backend::swapchain_create_status_name(
            vulkan_backend::vulkan_swapchain_create_status::invalid_surface_extent)
            == std::string_view{"invalid_surface_extent"},
        "swapchain create status name for invalid surface extent is stable");
    require(
        vulkan_backend::swapchain_create_status_name(
            vulkan_backend::vulkan_swapchain_create_status::missing_present_queue)
            == std::string_view{"missing_present_queue"},
        "swapchain create status name for missing present queue is stable");
    require(
        vulkan_backend::swapchain_create_status_name(
            vulkan_backend::vulkan_swapchain_create_status::unsupported_present_mode)
            == std::string_view{"unsupported_present_mode"},
        "swapchain create status name for unsupported present mode is stable");
    require(
        vulkan_backend::swapchain_create_status_name(
            vulkan_backend::vulkan_swapchain_create_status::creation_failed)
            == std::string_view{"creation_failed"},
        "swapchain create status name for creation failed is stable");
    require(
        vulkan_backend::swapchain_acquire_status_name(
            vulkan_backend::vulkan_swapchain_acquire_status::timeout)
            == std::string_view{"timeout"},
        "swapchain acquire status name for timeout is stable");
    require(
        vulkan_backend::swapchain_acquire_status_name(
            vulkan_backend::vulkan_swapchain_acquire_status::out_of_date)
            == std::string_view{"out_of_date"},
        "swapchain acquire status name for out of date is stable");
    require(
        vulkan_backend::swapchain_acquire_status_name(
            vulkan_backend::vulkan_swapchain_acquire_status::suboptimal)
            == std::string_view{"suboptimal"},
        "swapchain acquire status name for suboptimal is stable");
    require(
        vulkan_backend::swapchain_acquire_status_name(
            vulkan_backend::vulkan_swapchain_acquire_status::error)
            == std::string_view{"error"},
        "swapchain acquire status name for error is stable");
    require(
        vulkan_backend::swapchain_image_acquire_plan_status_name(
            vulkan_backend::vulkan_swapchain_image_acquire_plan_status::ready)
            == std::string_view{"ready"},
        "swapchain image acquire plan status name for ready is stable");
    require(
        vulkan_backend::swapchain_image_acquire_plan_status_name(
            vulkan_backend::vulkan_swapchain_image_acquire_plan_status::out_of_date)
            == std::string_view{"out_of_date"},
        "swapchain image acquire plan status name for out of date is stable");
    require(
        vulkan_backend::swapchain_image_acquire_plan_status_name(
            vulkan_backend::vulkan_swapchain_image_acquire_plan_status::suboptimal)
            == std::string_view{"suboptimal"},
        "swapchain image acquire plan status name for suboptimal is stable");
}

} // namespace

int main()
{
    test_swapchain_factory_marks_ready_device_swapchain_ready();
    test_swapchain_factory_maps_unavailable_device_to_swapchain_unavailable();
    test_swapchain_factory_reports_invalid_extent();
    test_swapchain_factory_falls_back_to_fifo_present_mode();
    test_swapchain_image_acquire_plan_accepts_ready_image();
    test_swapchain_image_acquire_plan_reports_readiness_gates();
    test_swapchain_image_acquire_plan_models_timeout_out_of_date_suboptimal_and_error();
    test_lifecycle_fallback_moves_to_swapchain_only_after_device_is_ready();
    test_vulkan_swapchain_create_status_names_are_stable();
    return 0;
}
