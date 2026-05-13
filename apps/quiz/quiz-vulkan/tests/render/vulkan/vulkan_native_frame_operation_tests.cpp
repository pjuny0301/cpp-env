#include "render/vulkan/vulkan_backend_adapter.h"

#include <cassert>
#include <cstdio>
#include <string_view>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "Requirement failed: %s\n", message);
    }
    assert((condition) && message);
}

quiz_vulkan::render::vulkan_backend::vulkan_native_function_table_diagnostics
make_ready_native_functions()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_native_function_table_diagnostics{
        .checked = true,
        .status = vulkan_backend::vulkan_native_function_table_status::ready,
        .fallback_reason = vulkan_backend::vulkan_backend_fallback_reason::none,
        .loader = {},
        .loader_ready = true,
        .required_extensions_ready = true,
        .command_buffer_recording_ready = true,
        .queue_submit_ready = true,
        .swapchain_create_ready = true,
        .swapchain_destroy_ready = true,
        .swapchain_images_ready = true,
        .swapchain_acquire_ready = true,
        .queue_present_ready = true,
        .required_extension_count = 1,
        .available_required_extension_count = 1,
        .requested_symbol_count = 7,
        .required_symbol_count = 7,
        .available_symbol_count = 7,
        .missing_required_symbol_count = 0,
        .diagnostic = "native functions ready",
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_native_swapchain_entrypoint_readiness
make_ready_swapchain_entrypoints()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_native_swapchain_entrypoint_readiness{
        .checked = true,
        .function_table_checked = true,
        .required_extensions_ready = true,
        .create_swapchain_ready = true,
        .destroy_swapchain_ready = true,
        .get_swapchain_images_ready = true,
        .acquire_next_image_ready = true,
        .queue_present_ready = true,
        .function_table_status =
            vulkan_backend::vulkan_native_function_table_status::ready,
        .required_extension_count = 1,
        .available_required_extension_count = 1,
        .diagnostic = "swapchain entrypoints ready",
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_swapchain_surface_capabilities_snapshot
make_capabilities()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_swapchain_surface_capabilities_snapshot{
        .checked = true,
        .min_image_count = 2,
        .max_image_count = 4,
        .current_extent = {},
        .min_extent = vulkan_backend::vulkan_surface_extent{.width = 640, .height = 480},
        .max_extent = vulkan_backend::vulkan_surface_extent{.width = 1920, .height = 1080},
        .supported_transforms = {
            vulkan_backend::vulkan_swapchain_surface_transform::identity,
        },
        .current_transform = vulkan_backend::vulkan_swapchain_surface_transform::identity,
        .supported_composite_alpha = {
            vulkan_backend::vulkan_swapchain_composite_alpha::opaque,
        },
        .surface_formats = {
            vulkan_backend::vulkan_swapchain_surface_format{
                .format = vulkan_backend::vulkan_swapchain_image_format::bgra8_unorm,
                .color_space = vulkan_backend::vulkan_swapchain_color_space::srgb_nonlinear,
            },
        },
        .present_modes = {
            vulkan_backend::vulkan_swapchain_present_mode::fifo,
            vulkan_backend::vulkan_swapchain_present_mode::mailbox,
        },
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_swapchain_create_plan_intent make_intent()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_swapchain_create_plan_intent{
        .desired_vsync = false,
        .desired_extent = vulkan_backend::vulkan_surface_extent{.width = 1280, .height = 720},
        .desired_image_count = 3,
        .desired_surface_format = vulkan_backend::vulkan_swapchain_surface_format{
            .format = vulkan_backend::vulkan_swapchain_image_format::bgra8_unorm,
            .color_space = vulkan_backend::vulkan_swapchain_color_space::srgb_nonlinear,
        },
        .desired_transform = vulkan_backend::vulkan_swapchain_surface_transform::identity,
        .desired_composite_alpha = vulkan_backend::vulkan_swapchain_composite_alpha::opaque,
        .graphics_queue_family_available = true,
        .present_queue_family_available = true,
        .graphics_queue_family_index = 0,
        .present_queue_family_index = 1,
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_native_swapchain_create_operation_result
make_ready_swapchain_create()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::build_vulkan_native_swapchain_create_operation_plan(
        vulkan_backend::vulkan_native_swapchain_create_operation_request{
            .create_plan = vulkan_backend::build_vulkan_swapchain_create_plan(
                vulkan_backend::vulkan_swapchain_create_plan_request{
                    .capabilities = make_capabilities(),
                    .intent = make_intent(),
                    .recreate_reference = {},
                }),
            .native_entrypoints = make_ready_swapchain_entrypoints(),
            .device = vulkan_backend::vulkan_device_handle{.value = 77},
            .surface = vulkan_backend::vulkan_surface_handle{.value = 88},
            .old_swapchain = {},
            .require_recreate_compatibility = false,
        });
}

quiz_vulkan::render::vulkan_backend::vulkan_native_swapchain_images_operation_result
make_ready_swapchain_images()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::build_vulkan_native_swapchain_images_operation_plan(
        vulkan_backend::vulkan_native_swapchain_images_operation_request{
            .create_operation = make_ready_swapchain_create(),
            .native_entrypoints = make_ready_swapchain_entrypoints(),
            .swapchain = vulkan_backend::vulkan_swapchain_handle{.value = 90},
            .image_handle_base =
                vulkan_backend::vulkan_native_function_pointer{.value = 5000},
        });
}

quiz_vulkan::render::vulkan_backend::vulkan_swapchain_image_acquire_plan_result
make_acquire_plan(
    quiz_vulkan::render::vulkan_backend::vulkan_swapchain_acquire_status status,
    std::size_t image_id = 2)
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const bool recordable =
        status == vulkan_backend::vulkan_swapchain_acquire_status::acquired
        || status == vulkan_backend::vulkan_swapchain_acquire_status::suboptimal;
    return vulkan_backend::build_vulkan_swapchain_image_acquire_plan(
        vulkan_backend::vulkan_swapchain_image_acquire_request{
            .requested = true,
            .lifecycle_ready = true,
            .swapchain_ready = true,
            .swapchain = vulkan_backend::vulkan_swapchain_handle{.value = 90},
            .image_count = 3,
            .timeout_nanoseconds = 16000000,
            .image_available_semaphore_ready = true,
            .fence_ready = true,
            .allow_suboptimal = true,
        },
        vulkan_backend::vulkan_swapchain_acquire_result{
            .status = status,
            .image = vulkan_backend::vulkan_swapchain_image_state{
                .id = vulkan_backend::vulkan_swapchain_image_id{.value = image_id},
                .available = recordable,
                .acquired = recordable,
                .presented = false,
            },
        });
}

quiz_vulkan::render::vulkan_backend::vulkan_native_swapchain_acquire_operation_result
make_ready_acquire()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::build_vulkan_native_swapchain_acquire_operation_plan(
        vulkan_backend::vulkan_native_swapchain_acquire_operation_request{
            .images_operation = make_ready_swapchain_images(),
            .acquire_plan =
                make_acquire_plan(vulkan_backend::vulkan_swapchain_acquire_status::acquired),
            .native_entrypoints = make_ready_swapchain_entrypoints(),
        });
}

quiz_vulkan::render::vulkan_backend::vulkan_command_buffer_record_result
make_recorded_command_buffer()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_command_buffer_record_result{
        .checked = true,
        .status = vulkan_backend::vulkan_command_buffer_record_result_status::recorded,
        .fallback_reason = vulkan_backend::vulkan_backend_fallback_reason::none,
        .command_buffer = vulkan_backend::vulkan_command_buffer_id{.value = 42},
        .operation_plan_checked = true,
        .operation_plan_ready = true,
        .begin_attempted = true,
        .begin_completed = true,
        .end_attempted = true,
        .end_completed = true,
        .planned_operation_count = 4,
        .attempted_operation_count = 4,
        .recorded_operation_count = 4,
        .rect_operation_count = 1,
        .text_operation_count = 1,
        .image_operation_count = 1,
        .debug_bounds_operation_count = 1,
        .diagnostic = "recorded",
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_command_submit_sync_primitives make_sync()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_command_submit_sync_primitives{
        .image_available_semaphore =
            vulkan_backend::vulkan_command_submit_sync_handle{.value = 11},
        .render_finished_semaphore =
            vulkan_backend::vulkan_command_submit_sync_handle{.value = 12},
        .frame_fence = vulkan_backend::vulkan_command_submit_sync_handle{.value = 13},
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_submit_batch_plan_result
make_ready_submit_batch()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_command_submit_sync_primitives sync = make_sync();
    return vulkan_backend::vulkan_submit_batch_plan_result{
        .checked = true,
        .status = vulkan_backend::vulkan_submit_batch_plan_status::ready,
        .fallback_reason = vulkan_backend::vulkan_backend_fallback_reason::none,
        .command_buffer_recording_checked = true,
        .command_buffer_recording_ready = true,
        .command_submit_readiness_checked = true,
        .command_submit_readiness_ready = true,
        .command_buffer_available = true,
        .sync_primitives_available = true,
        .submit_queue_available = true,
        .present_target_available = true,
        .submit_ready = true,
        .present_ready = true,
        .recorded_operation_count = 4,
        .submit_batch_count = 1,
        .wait_intent_count = 1,
        .signal_intent_count = 2,
        .present_intent_count = 1,
        .recorded_command_buffer = vulkan_backend::vulkan_command_buffer_id{.value = 42},
        .submit_command_buffer =
            vulkan_backend::vulkan_command_recording_command_buffer_handle{.value = 99},
        .submit_queue = vulkan_backend::vulkan_queue_handle{.value = 7},
        .sync_primitives = sync,
        .image_id = vulkan_backend::vulkan_swapchain_image_id{.value = 2},
        .submit_batches = {
            vulkan_backend::vulkan_submit_batch_record{
                .batch_index = 0,
                .recorded_command_buffer = vulkan_backend::vulkan_command_buffer_id{.value = 42},
                .submit_command_buffer =
                    vulkan_backend::vulkan_command_recording_command_buffer_handle{.value = 99},
                .submit_queue = vulkan_backend::vulkan_queue_handle{.value = 7},
                .recorded_operation_count = 4,
                .wait_intent_count = 1,
                .signal_intent_count = 2,
                .present_intent_count = 1,
            },
        },
        .wait_intents = {
            vulkan_backend::vulkan_submit_batch_sync_intent{
                .kind = vulkan_backend::vulkan_submit_batch_sync_intent_kind::wait_image_available,
                .handle = sync.image_available_semaphore,
                .required = true,
                .available = true,
            },
        },
        .signal_intents = {
            vulkan_backend::vulkan_submit_batch_sync_intent{
                .kind = vulkan_backend::vulkan_submit_batch_sync_intent_kind::signal_render_finished,
                .handle = sync.render_finished_semaphore,
                .required = true,
                .available = true,
            },
            vulkan_backend::vulkan_submit_batch_sync_intent{
                .kind = vulkan_backend::vulkan_submit_batch_sync_intent_kind::signal_frame_fence,
                .handle = sync.frame_fence,
                .required = true,
                .available = true,
            },
        },
        .present_intents = {
            vulkan_backend::vulkan_submit_batch_present_intent{
                .requested = true,
                .target_available = true,
                .image_id = vulkan_backend::vulkan_swapchain_image_id{.value = 2},
                .wait_render_finished_semaphore = sync.render_finished_semaphore,
            },
        },
        .diagnostic = "submit batch ready",
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_queue_submit_present_result
make_completed_queue_present()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_queue_submit_present_result{
        .checked = true,
        .status = vulkan_backend::vulkan_queue_submit_present_status::submitted_and_presented,
        .submit_call = vulkan_backend::vulkan_queue_submit_adapter_submit_call{
            .queue = vulkan_backend::vulkan_queue_handle{.value = 7},
            .command_buffer =
                vulkan_backend::vulkan_command_recording_command_buffer_handle{.value = 99},
            .sync_primitives = make_sync(),
            .batch_count = 1,
        },
        .present_call = vulkan_backend::vulkan_queue_submit_adapter_present_call{
            .queue = vulkan_backend::vulkan_queue_handle{.value = 101},
            .swapchain = vulkan_backend::vulkan_swapchain_handle{.value = 90},
            .image_id = vulkan_backend::vulkan_swapchain_image_id{.value = 2},
            .wait_render_finished_semaphore =
                vulkan_backend::vulkan_command_submit_sync_handle{.value = 12},
        },
        .submit_result = vulkan_backend::vulkan_queue_submit_adapter_operation_result{
            .checked = true,
            .status = vulkan_backend::vulkan_queue_submit_adapter_call_status::completed,
            .diagnostic = "submitted",
        },
        .present_result = vulkan_backend::vulkan_queue_submit_adapter_operation_result{
            .checked = true,
            .status = vulkan_backend::vulkan_queue_submit_adapter_call_status::completed,
            .diagnostic = "presented",
        },
        .submit_called = true,
        .present_called = true,
        .submit_order = 1,
        .present_order = 2,
        .diagnostic = "submitted and presented",
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_native_queue_present_operation_result
make_ready_present_operation()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_submit_batch_plan_result submit_batch =
        make_ready_submit_batch();
    const vulkan_backend::vulkan_present_completion_plan_result present_completion =
        vulkan_backend::build_vulkan_present_completion_plan(
            submit_batch,
            make_completed_queue_present());
    return vulkan_backend::build_vulkan_native_queue_present_operation_plan(
        vulkan_backend::vulkan_native_queue_present_operation_request{
            .acquire_operation = make_ready_acquire(),
            .submit_batch = submit_batch,
            .present_completion = present_completion,
            .native_entrypoints = make_ready_swapchain_entrypoints(),
            .present_result = vulkan_backend::vulkan_swapchain_present_result{
                .status = vulkan_backend::vulkan_swapchain_present_status::presented,
                .image_id = vulkan_backend::vulkan_swapchain_image_id{.value = 2},
            },
            .allow_suboptimal = true,
        });
}

quiz_vulkan::render::vulkan_backend::vulkan_native_frame_operation_request
make_ready_frame_request()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_native_frame_operation_request{
        .native_functions = make_ready_native_functions(),
        .swapchain_create = make_ready_swapchain_create(),
        .swapchain_images = make_ready_swapchain_images(),
        .acquire_operation = make_ready_acquire(),
        .command_buffer_recording = make_recorded_command_buffer(),
        .submit_batch = make_ready_submit_batch(),
        .present_operation = make_ready_present_operation(),
        .cpu_fallback_available = true,
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_native_frame_operation_result
make_ready_frame()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::build_vulkan_native_frame_operation_summary(
        make_ready_frame_request());
}

quiz_vulkan::render::vulkan_backend::vulkan_native_frame_operation_result
make_swapchain_create_blocked_frame()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::vulkan_native_frame_operation_request request =
        make_ready_frame_request();
    request.swapchain_create.status =
        vulkan_backend::vulkan_native_swapchain_create_operation_status::create_plan_unavailable;
    request.swapchain_create.vk_create_swapchain_callable = false;
    request.swapchain_create.operation.status =
        vulkan_backend::vulkan_native_swapchain_create_operation_status::create_plan_unavailable;
    request.swapchain_create.operation.vk_create_swapchain_callable = false;
    request.swapchain_create.diagnostic = "create unavailable";
    return vulkan_backend::build_vulkan_native_frame_operation_summary(request);
}

quiz_vulkan::render::vulkan_backend::vulkan_native_frame_operation_result
make_submit_blocked_frame()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::vulkan_native_frame_operation_request request =
        make_ready_frame_request();
    request.submit_batch.status =
        vulkan_backend::vulkan_submit_batch_plan_status::submit_queue_unavailable;
    request.submit_batch.fallback_reason =
        vulkan_backend::vulkan_backend_fallback_reason::submit_frame_failed;
    request.submit_batch.diagnostic = "submit unavailable";
    return vulkan_backend::build_vulkan_native_frame_operation_summary(request);
}

quiz_vulkan::render::vulkan_backend::vulkan_native_frame_operation_result
make_out_of_date_acquire_frame()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::vulkan_native_frame_operation_request request =
        make_ready_frame_request();
    request.acquire_operation =
        vulkan_backend::build_vulkan_native_swapchain_acquire_operation_plan(
            vulkan_backend::vulkan_native_swapchain_acquire_operation_request{
                .images_operation = make_ready_swapchain_images(),
                .acquire_plan = make_acquire_plan(
                    vulkan_backend::vulkan_swapchain_acquire_status::out_of_date,
                    0),
                .native_entrypoints = make_ready_swapchain_entrypoints(),
            });
    return vulkan_backend::build_vulkan_native_frame_operation_summary(request);
}

quiz_vulkan::render::vulkan_backend::vulkan_native_frame_operation_result
make_suboptimal_acquire_frame()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::vulkan_native_frame_operation_request request =
        make_ready_frame_request();
    request.acquire_operation =
        vulkan_backend::build_vulkan_native_swapchain_acquire_operation_plan(
            vulkan_backend::vulkan_native_swapchain_acquire_operation_request{
                .images_operation = make_ready_swapchain_images(),
                .acquire_plan = make_acquire_plan(
                    vulkan_backend::vulkan_swapchain_acquire_status::suboptimal),
                .native_entrypoints = make_ready_swapchain_entrypoints(),
            });
    return vulkan_backend::build_vulkan_native_frame_operation_summary(request);
}

quiz_vulkan::render::vulkan_backend::vulkan_native_frame_operation_result
make_recoverable_present_failure_frame()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::vulkan_native_frame_operation_request request =
        make_ready_frame_request();
    request.present_operation.status =
        vulkan_backend::vulkan_native_queue_present_operation_status::present_failed_recoverable;
    request.present_operation.recoverable_failure = true;
    request.present_operation.frame_lifecycle_may_complete = false;
    request.present_operation.operation.status =
        vulkan_backend::vulkan_native_queue_present_operation_status::present_failed_recoverable;
    request.present_operation.operation.recoverable_failure = true;
    request.present_operation.operation.frame_lifecycle_may_complete = false;
    request.present_operation.diagnostic = "recoverable present";
    return vulkan_backend::build_vulkan_native_frame_operation_summary(request);
}

quiz_vulkan::render::vulkan_backend::vulkan_native_frame_operation_result
make_fatal_present_failure_frame()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::vulkan_native_frame_operation_request request =
        make_ready_frame_request();
    request.present_operation.status =
        vulkan_backend::vulkan_native_queue_present_operation_status::present_failed_fatal;
    request.present_operation.fatal_failure = true;
    request.present_operation.frame_lifecycle_may_complete = false;
    request.present_operation.operation.status =
        vulkan_backend::vulkan_native_queue_present_operation_status::present_failed_fatal;
    request.present_operation.operation.fatal_failure = true;
    request.present_operation.operation.frame_lifecycle_may_complete = false;
    request.present_operation.diagnostic = "fatal present";
    return vulkan_backend::build_vulkan_native_frame_operation_summary(request);
}

const quiz_vulkan::render::vulkan_backend::vulkan_native_frame_operation_stage_diff_diagnostics*
find_stage_diff(
    const quiz_vulkan::render::vulkan_backend::vulkan_native_frame_operation_diff_diagnostics& diff,
    quiz_vulkan::render::vulkan_backend::vulkan_native_frame_operation_stage stage)
{
    for (const auto& stage_diff : diff.stages) {
        if (stage_diff.stage == stage) {
            return &stage_diff;
        }
    }

    return nullptr;
}

const quiz_vulkan::render::vulkan_backend::vulkan_native_frame_operation_step_execution_decision*
find_execution_step(
    const quiz_vulkan::render::vulkan_backend::vulkan_native_frame_operation_execution_plan& plan,
    quiz_vulkan::render::vulkan_backend::vulkan_native_frame_execution_step step)
{
    for (const auto& step_decision : plan.steps) {
        if (step_decision.step == step) {
            return &step_decision;
        }
    }

    return nullptr;
}

void test_native_frame_operation_names_are_stable()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    require(
        vulkan_backend::native_frame_operation_stage_name(
            vulkan_backend::vulkan_native_frame_operation_stage::acquire)
            == std::string_view{"acquire"},
        "native frame operation acquire stage name is stable");
    require(
        vulkan_backend::native_frame_operation_stage_name(
            vulkan_backend::vulkan_native_frame_operation_stage::frame_completion)
            == std::string_view{"frame_completion"},
        "native frame operation completion stage name is stable");
    require(
        vulkan_backend::native_frame_operation_status_name(
            vulkan_backend::vulkan_native_frame_operation_status::ready)
            == std::string_view{"ready"},
        "native frame operation ready status name is stable");
    require(
        vulkan_backend::native_frame_operation_status_name(
            vulkan_backend::vulkan_native_frame_operation_status::present_unavailable)
            == std::string_view{"present_unavailable"},
        "native frame operation present unavailable status name is stable");
    require(
        vulkan_backend::native_frame_execution_step_name(
            vulkan_backend::vulkan_native_frame_execution_step::record)
            == std::string_view{"record"},
        "native frame execution record step name is stable");
    require(
        vulkan_backend::native_frame_execution_decision_name(
            vulkan_backend::vulkan_native_frame_execution_decision::fallback)
            == std::string_view{"fallback"},
        "native frame execution fallback decision name is stable");
}

void test_native_frame_operation_reports_ready_frame_completion()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_native_frame_operation_result frame =
        vulkan_backend::build_vulkan_native_frame_operation_summary(
            make_ready_frame_request());

    require(frame.checked, "native frame operation is checked");
    require(frame.completed(), "native frame operation completes");
    require(!frame.blocked(), "ready native frame operation is not blocked");
    require(
        frame.status == vulkan_backend::vulkan_native_frame_operation_status::ready,
        "native frame operation status is ready");
    require(
        frame.reached_stage
            == vulkan_backend::vulkan_native_frame_operation_stage::frame_completion,
        "native frame operation reaches frame completion");
    require(
        frame.blocker_stage == vulkan_backend::vulkan_native_frame_operation_stage::not_started,
        "ready native frame operation has no blocker stage");
    require(
        frame.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::none,
        "ready native frame operation has no fallback");
    require(frame.native_function_table_ready, "native function table is ready");
    require(frame.swapchain_create_ready, "swapchain create operation is ready");
    require(frame.swapchain_images_ready, "swapchain images operation is ready");
    require(frame.acquire_ready, "acquire operation is ready");
    require(frame.command_recording_ready, "command recording operation is ready");
    require(frame.submit_batch_ready, "submit batch operation is ready");
    require(frame.present_operation_ready, "present operation is ready");
    require(frame.frame_completion_ready, "frame completion is ready");
    require(!frame.cpu_fallback_should_remain_active, "ready native frame disables active fallback");
    require(frame.stages.size() == 8, "native frame records all operation stages");
    require(frame.stages.front().completed(), "first native frame stage completes");
    require(frame.operation.completed(), "native frame summary completes");
}

void test_native_frame_operation_blocks_major_prerequisite_stages()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::vulkan_native_frame_operation_request request =
        make_ready_frame_request();
    request.swapchain_create.status =
        vulkan_backend::vulkan_native_swapchain_create_operation_status::create_plan_unavailable;
    request.swapchain_create.vk_create_swapchain_callable = false;
    request.swapchain_create.operation.status =
        vulkan_backend::vulkan_native_swapchain_create_operation_status::create_plan_unavailable;
    request.swapchain_create.operation.vk_create_swapchain_callable = false;
    request.swapchain_create.diagnostic = "create unavailable";
    const vulkan_backend::vulkan_native_frame_operation_result create_blocked =
        vulkan_backend::build_vulkan_native_frame_operation_summary(request);
    require(
        create_blocked.status
            == vulkan_backend::vulkan_native_frame_operation_status::swapchain_create_unavailable,
        "native frame blocks unavailable swapchain create");
    require(
        create_blocked.blocker_stage
            == vulkan_backend::vulkan_native_frame_operation_stage::swapchain_create,
        "native frame records create blocker stage");
    require(create_blocked.cpu_fallback_should_remain_active, "create blocker keeps CPU fallback active");

    request = make_ready_frame_request();
    request.swapchain_images.status =
        vulkan_backend::vulkan_native_swapchain_images_operation_status::missing_images_symbol;
    request.swapchain_images.vk_get_swapchain_images_callable = false;
    request.swapchain_images.operation.status =
        vulkan_backend::vulkan_native_swapchain_images_operation_status::missing_images_symbol;
    request.swapchain_images.operation.vk_get_swapchain_images_callable = false;
    request.swapchain_images.diagnostic = "images unavailable";
    const vulkan_backend::vulkan_native_frame_operation_result images_blocked =
        vulkan_backend::build_vulkan_native_frame_operation_summary(request);
    require(
        images_blocked.status
            == vulkan_backend::vulkan_native_frame_operation_status::swapchain_images_unavailable,
        "native frame blocks unavailable swapchain images");

    request = make_ready_frame_request();
    request.acquire_operation =
        vulkan_backend::build_vulkan_native_swapchain_acquire_operation_plan(
            vulkan_backend::vulkan_native_swapchain_acquire_operation_request{
                .images_operation = make_ready_swapchain_images(),
                .acquire_plan = make_acquire_plan(
                    vulkan_backend::vulkan_swapchain_acquire_status::out_of_date,
                    0),
                .native_entrypoints = make_ready_swapchain_entrypoints(),
            });
    const vulkan_backend::vulkan_native_frame_operation_result acquire_blocked =
        vulkan_backend::build_vulkan_native_frame_operation_summary(request);
    require(
        acquire_blocked.status
            == vulkan_backend::vulkan_native_frame_operation_status::acquire_unavailable,
        "native frame blocks unavailable acquire operation");
    require(acquire_blocked.swapchain_out_of_date, "native frame records out-of-date swapchain");

    request = make_ready_frame_request();
    request.command_buffer_recording.status =
        vulkan_backend::vulkan_command_buffer_record_result_status::operation_failed;
    request.command_buffer_recording.fallback_reason =
        vulkan_backend::vulkan_backend_fallback_reason::record_commands_failed;
    request.command_buffer_recording.diagnostic = "recording failed";
    const vulkan_backend::vulkan_native_frame_operation_result recording_blocked =
        vulkan_backend::build_vulkan_native_frame_operation_summary(request);
    require(
        recording_blocked.status
            == vulkan_backend::vulkan_native_frame_operation_status::command_recording_unavailable,
        "native frame blocks unavailable command recording");

    request = make_ready_frame_request();
    request.submit_batch.status =
        vulkan_backend::vulkan_submit_batch_plan_status::submit_queue_unavailable;
    request.submit_batch.fallback_reason =
        vulkan_backend::vulkan_backend_fallback_reason::submit_frame_failed;
    request.submit_batch.diagnostic = "submit unavailable";
    const vulkan_backend::vulkan_native_frame_operation_result submit_blocked =
        vulkan_backend::build_vulkan_native_frame_operation_summary(request);
    require(
        submit_blocked.status
            == vulkan_backend::vulkan_native_frame_operation_status::submit_unavailable,
        "native frame blocks unavailable submit batch");
    require(
        submit_blocked.fallback_reason
            == vulkan_backend::vulkan_backend_fallback_reason::submit_frame_failed,
        "native frame maps submit blocker to submit fallback");
}

void test_native_frame_operation_blocks_missing_native_function_table()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::vulkan_native_frame_operation_request request =
        make_ready_frame_request();
    request.native_functions.status =
        vulkan_backend::vulkan_native_function_table_status::missing_queue_present_symbol;
    request.native_functions.queue_present_ready = false;
    request.native_functions.missing_required_symbol_count = 1;
    request.native_functions.missing_symbol_stage =
        vulkan_backend::vulkan_native_entrypoint_stage::queue_present;
    request.native_functions.missing_symbol_name = "vkQueuePresentKHR";
    request.native_functions.diagnostic = "missing vkQueuePresentKHR";

    const vulkan_backend::vulkan_native_frame_operation_result frame =
        vulkan_backend::build_vulkan_native_frame_operation_summary(request);

    require(
        frame.status
            == vulkan_backend::vulkan_native_frame_operation_status::native_function_table_unavailable,
        "native frame blocks unavailable native function table");
    require(
        frame.blocker_stage
            == vulkan_backend::vulkan_native_frame_operation_stage::native_function_table,
        "native frame records native function table blocker stage");
    require(
        frame.fallback_reason
            == vulkan_backend::vulkan_backend_fallback_reason::present_frame_failed,
        "native frame maps queue present symbol to present fallback");
    require(
        frame.missing_symbol_name == "vkQueuePresentKHR",
        "native frame records missing native symbol");
    require(frame.cpu_fallback_should_remain_active, "native function blocker keeps CPU fallback active");
}

void test_native_frame_operation_classifies_recoverable_and_fatal_present_failures()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::vulkan_native_frame_operation_request request =
        make_ready_frame_request();
    request.present_operation.status =
        vulkan_backend::vulkan_native_queue_present_operation_status::present_failed_recoverable;
    request.present_operation.recoverable_failure = true;
    request.present_operation.frame_lifecycle_may_complete = false;
    request.present_operation.operation.status =
        vulkan_backend::vulkan_native_queue_present_operation_status::present_failed_recoverable;
    request.present_operation.operation.recoverable_failure = true;
    request.present_operation.operation.frame_lifecycle_may_complete = false;
    request.present_operation.diagnostic = "recoverable present";
    const vulkan_backend::vulkan_native_frame_operation_result recoverable =
        vulkan_backend::build_vulkan_native_frame_operation_summary(request);
    require(
        recoverable.status
            == vulkan_backend::vulkan_native_frame_operation_status::recoverable_failure,
        "native frame classifies recoverable present failure");
    require(recoverable.recoverable_failure, "native frame records recoverable failure");
    require(!recoverable.fatal_failure, "recoverable frame is not fatal");
    require(recoverable.cpu_fallback_should_remain_active, "recoverable failure keeps CPU fallback active");

    request = make_ready_frame_request();
    request.present_operation.status =
        vulkan_backend::vulkan_native_queue_present_operation_status::present_failed_fatal;
    request.present_operation.fatal_failure = true;
    request.present_operation.frame_lifecycle_may_complete = false;
    request.present_operation.operation.status =
        vulkan_backend::vulkan_native_queue_present_operation_status::present_failed_fatal;
    request.present_operation.operation.fatal_failure = true;
    request.present_operation.operation.frame_lifecycle_may_complete = false;
    request.present_operation.diagnostic = "fatal present";
    const vulkan_backend::vulkan_native_frame_operation_result fatal =
        vulkan_backend::build_vulkan_native_frame_operation_summary(request);
    require(
        fatal.status == vulkan_backend::vulkan_native_frame_operation_status::fatal_failure,
        "native frame classifies fatal present failure");
    require(fatal.fatal_failure, "native frame records fatal failure");
    require(!fatal.recoverable_failure, "fatal frame is not recoverable");
    require(fatal.cpu_fallback_should_remain_active, "fatal failure keeps CPU fallback active");
}

void test_native_frame_operation_diff_reports_stage_and_fallback_changes()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_native_frame_operation_result ready =
        make_ready_frame();
    const vulkan_backend::vulkan_native_frame_operation_result submit_blocked =
        make_submit_blocked_frame();

    const vulkan_backend::vulkan_native_frame_operation_diff_diagnostics result_diff =
        vulkan_backend::build_vulkan_native_frame_operation_result_diff(
            ready,
            submit_blocked);
    const vulkan_backend::vulkan_native_frame_operation_diff_diagnostics summary_diff =
        vulkan_backend::build_vulkan_native_frame_operation_summary_diff(
            ready.operation,
            submit_blocked.operation);

    require(result_diff.checked, "native frame result diff is checked");
    require(result_diff.changed, "native frame result diff detects changes");
    require(summary_diff.changed, "native frame summary diff detects changes");
    require(result_diff.status_changed, "native frame diff reports status change");
    require(result_diff.blocker_introduced, "native frame diff reports introduced blocker");
    require(
        result_diff.after_blocker_stage
            == vulkan_backend::vulkan_native_frame_operation_stage::submit,
        "native frame diff reports submit blocker");
    require(result_diff.fallback_activation_changed, "native frame diff reports fallback change");
    require(result_diff.fallback_activated, "native frame diff reports fallback activation");
    require(
        result_diff.after_fallback_reason
            == vulkan_backend::vulkan_backend_fallback_reason::submit_frame_failed,
        "native frame diff reports submit fallback reason");
    require(
        result_diff.completion_readiness_changed,
        "native frame diff reports completion readiness delta");
    require(
        result_diff.completion_became_unready,
        "native frame diff reports completion becoming unready");
    require(result_diff.stage_count == 8, "native frame diff reports all operation stages");
    require(
        result_diff.stage_readiness_change_count == 1,
        "native frame diff reports one stage readiness change");
    require(
        result_diff.stage_ready_loss_count == 1,
        "native frame diff reports one stage readiness loss");

    const auto* submit_stage = find_stage_diff(
        result_diff,
        vulkan_backend::vulkan_native_frame_operation_stage::submit);
    require(submit_stage != nullptr, "native frame diff includes submit stage");
    require(submit_stage->readiness_changed, "native frame diff marks submit readiness change");
    require(submit_stage->became_blocked, "native frame diff marks submit stage blocked");
    require(
        submit_stage->after_fallback_reason
            == vulkan_backend::vulkan_backend_fallback_reason::submit_frame_failed,
        "native frame diff carries submit stage fallback");
}

void test_native_frame_operation_diff_reports_blocker_movement()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_native_frame_operation_diff_diagnostics diff =
        vulkan_backend::build_vulkan_native_frame_operation_result_diff(
            make_swapchain_create_blocked_frame(),
            make_submit_blocked_frame());

    require(diff.changed, "native frame diff detects blocker movement");
    require(diff.blocker_stage_changed, "native frame diff reports changed blocker");
    require(diff.blocker_moved_forward, "native frame diff reports blocker moving forward");
    require(!diff.blocker_moved_backward, "native frame diff does not report backward movement");
    require(
        diff.before_blocker_stage
            == vulkan_backend::vulkan_native_frame_operation_stage::swapchain_create,
        "native frame diff records original blocker");
    require(
        diff.after_blocker_stage
            == vulkan_backend::vulkan_native_frame_operation_stage::submit,
        "native frame diff records new blocker");
    require(diff.fallback_reason_changed, "native frame diff reports fallback reason movement");
    require(
        diff.stage_readiness_change_count == 2,
        "native frame diff reports readiness change at old and new blocker stages");
    require(diff.stage_ready_gain_count == 1, "native frame diff reports old blocker ready gain");
    require(diff.stage_ready_loss_count == 1, "native frame diff reports new blocker ready loss");
}

void test_native_frame_operation_diff_reports_swapchain_readiness_changes()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_native_frame_operation_diff_diagnostics diff =
        vulkan_backend::build_vulkan_native_frame_operation_result_diff(
            make_out_of_date_acquire_frame(),
            make_suboptimal_acquire_frame());

    require(diff.changed, "native frame diff detects swapchain state changes");
    require(
        diff.swapchain_out_of_date_changed,
        "native frame diff reports out-of-date transition");
    require(
        diff.swapchain_out_of_date_cleared,
        "native frame diff reports out-of-date clearing");
    require(diff.suboptimal_changed, "native frame diff reports suboptimal transition");
    require(diff.suboptimal_started, "native frame diff reports suboptimal starting");
    require(diff.fallback_deactivated, "native frame diff reports fallback deactivation");
    require(
        diff.completion_became_ready,
        "native frame diff reports completion becoming ready");

    const auto* acquire_stage = find_stage_diff(
        diff,
        vulkan_backend::vulkan_native_frame_operation_stage::acquire);
    require(acquire_stage != nullptr, "native frame diff includes acquire stage");
    require(acquire_stage->became_ready, "native frame diff marks acquire becoming ready");
}

void test_native_frame_operation_diff_reports_failure_transitions()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_native_frame_operation_diff_diagnostics ready_to_recoverable =
        vulkan_backend::build_vulkan_native_frame_operation_result_diff(
            make_ready_frame(),
            make_recoverable_present_failure_frame());
    require(
        ready_to_recoverable.frame_completion_became_unready,
        "native frame diff reports frame completion readiness becoming unready");

    const vulkan_backend::vulkan_native_frame_operation_diff_diagnostics diff =
        vulkan_backend::build_vulkan_native_frame_operation_result_diff(
            make_recoverable_present_failure_frame(),
            make_fatal_present_failure_frame());

    require(diff.changed, "native frame diff detects failure transitions");
    require(
        diff.recoverable_failure_changed,
        "native frame diff reports recoverable failure transition");
    require(
        diff.recoverable_failure_cleared,
        "native frame diff reports recoverable failure clearing");
    require(diff.fatal_failure_changed, "native frame diff reports fatal failure transition");
    require(diff.fatal_failure_started, "native frame diff reports fatal failure starting");
    require(!diff.fallback_activation_changed, "native frame diff leaves active fallback stable");
    require(
        diff.after_status == vulkan_backend::vulkan_native_frame_operation_status::fatal_failure,
        "native frame diff records fatal status");
}

void test_native_frame_operation_execution_plan_executes_ready_lifecycle()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_native_frame_operation_result ready =
        make_ready_frame();
    const vulkan_backend::vulkan_native_frame_operation_execution_plan plan =
        vulkan_backend::build_vulkan_native_frame_operation_execution_plan(
            ready.operation);

    require(plan.checked, "native frame execution plan is checked");
    require(plan.summary_checked, "native frame execution plan observes checked summary");
    require(plan.native_execution_ready, "native frame execution plan is ready");
    require(plan.should_execute_native_frame(), "native frame execution plan executes native frame");
    require(!plan.should_use_cpu_fallback(), "ready native frame does not fallback");
    require(!plan.skip_required, "ready native frame does not skip");
    require(plan.step_count == 4, "native frame execution plan records lifecycle steps");
    require(plan.execute_step_count == 4, "native frame execution plan executes all steps");
    require(plan.fallback_step_count == 0, "native frame execution plan has no fallback steps");
    require(plan.skip_step_count == 0, "native frame execution plan has no skip steps");

    const auto* acquire = find_execution_step(
        plan,
        vulkan_backend::vulkan_native_frame_execution_step::acquire);
    const auto* record = find_execution_step(
        plan,
        vulkan_backend::vulkan_native_frame_execution_step::record);
    const auto* submit = find_execution_step(
        plan,
        vulkan_backend::vulkan_native_frame_execution_step::submit);
    const auto* present = find_execution_step(
        plan,
        vulkan_backend::vulkan_native_frame_execution_step::present);
    require(acquire != nullptr && acquire->should_execute(), "ready plan executes acquire");
    require(record != nullptr && record->should_execute(), "ready plan executes record");
    require(submit != nullptr && submit->should_execute(), "ready plan executes submit");
    require(present != nullptr && present->should_execute(), "ready plan executes present");
}

void test_native_frame_operation_execution_plan_falls_back_at_submit_blocker()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_native_frame_operation_result ready =
        make_ready_frame();
    const vulkan_backend::vulkan_native_frame_operation_result submit_blocked =
        make_submit_blocked_frame();
    const vulkan_backend::vulkan_native_frame_operation_diff_diagnostics diff =
        vulkan_backend::build_vulkan_native_frame_operation_result_diff(
            ready,
            submit_blocked);
    const vulkan_backend::vulkan_native_frame_operation_execution_plan plan =
        vulkan_backend::build_vulkan_native_frame_operation_execution_plan(
            submit_blocked.operation,
            diff);

    require(plan.checked, "submit-blocked execution plan is checked");
    require(plan.diff_checked, "submit-blocked execution plan consumes diff");
    require(plan.diff_changed, "submit-blocked execution plan records diff change");
    require(!plan.native_execution_ready, "submit-blocked execution plan is not native-ready");
    require(plan.should_use_cpu_fallback(), "submit-blocked execution plan uses fallback");
    require(plan.execute_step_count == 2, "submit-blocked plan executes acquire and record");
    require(plan.fallback_step_count == 1, "submit-blocked plan has one fallback step");
    require(plan.skip_step_count == 1, "submit-blocked plan skips after fallback");
    require(
        plan.fallback_reason
            == vulkan_backend::vulkan_backend_fallback_reason::submit_frame_failed,
        "submit-blocked plan carries submit fallback reason");

    const auto* submit = find_execution_step(
        plan,
        vulkan_backend::vulkan_native_frame_execution_step::submit);
    const auto* present = find_execution_step(
        plan,
        vulkan_backend::vulkan_native_frame_execution_step::present);
    require(submit != nullptr && submit->should_fallback(), "submit-blocked plan falls back at submit");
    require(submit->diff_became_blocked, "submit-blocked plan consumes submit diff blocker");
    require(
        submit->fallback_reason
            == vulkan_backend::vulkan_backend_fallback_reason::submit_frame_failed,
        "submit-blocked step carries submit fallback reason");
    require(
        present != nullptr
            && present->decision == vulkan_backend::vulkan_native_frame_execution_decision::skip,
        "submit-blocked plan skips present after fallback");
    require(present->blocked_by_previous_step, "present skip is caused by prior fallback");
}

void test_native_frame_operation_execution_plan_falls_back_at_present_failure()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_native_frame_operation_result ready =
        make_ready_frame();
    const vulkan_backend::vulkan_native_frame_operation_result recoverable =
        make_recoverable_present_failure_frame();
    const vulkan_backend::vulkan_native_frame_operation_execution_plan plan =
        vulkan_backend::build_vulkan_native_frame_operation_execution_plan(
            recoverable.operation,
            vulkan_backend::build_vulkan_native_frame_operation_result_diff(
                ready,
                recoverable));

    require(plan.recoverable_failure, "present failure plan records recoverable failure");
    require(plan.execute_step_count == 3, "present failure plan executes through submit");
    require(plan.fallback_step_count == 1, "present failure plan falls back once");
    require(plan.skip_step_count == 0, "present failure plan does not skip after terminal present");

    const auto* present = find_execution_step(
        plan,
        vulkan_backend::vulkan_native_frame_execution_step::present);
    require(present != nullptr && present->should_fallback(), "present failure plan falls back at present");
    require(
        present->operation_stage == vulkan_backend::vulkan_native_frame_operation_stage::present,
        "present failure plan maps present step to present stage");
    require(
        present->fallback_reason
            == vulkan_backend::vulkan_backend_fallback_reason::present_frame_failed,
        "present failure plan carries present fallback");
}

void test_native_frame_operation_execution_plan_skips_unchecked_summary()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_native_frame_operation_execution_plan plan =
        vulkan_backend::build_vulkan_native_frame_operation_execution_plan(
            vulkan_backend::vulkan_native_frame_operation_summary{});

    require(plan.checked, "unchecked summary execution plan is checked");
    require(!plan.summary_checked, "unchecked summary execution plan records unchecked summary");
    require(!plan.native_execution_ready, "unchecked summary execution plan is not native-ready");
    require(!plan.should_use_cpu_fallback(), "unchecked summary execution plan does not fallback");
    require(plan.not_checked_step_count == 4, "unchecked summary marks all steps unchecked");
    require(plan.execute_step_count == 0, "unchecked summary executes no steps");
    require(plan.fallback_step_count == 0, "unchecked summary has no fallback steps");
    require(plan.skip_step_count == 0, "unchecked summary has no explicit skip steps");
}

} // namespace

int main()
{
    test_native_frame_operation_names_are_stable();
    test_native_frame_operation_reports_ready_frame_completion();
    test_native_frame_operation_blocks_major_prerequisite_stages();
    test_native_frame_operation_blocks_missing_native_function_table();
    test_native_frame_operation_classifies_recoverable_and_fatal_present_failures();
    test_native_frame_operation_diff_reports_stage_and_fallback_changes();
    test_native_frame_operation_diff_reports_blocker_movement();
    test_native_frame_operation_diff_reports_swapchain_readiness_changes();
    test_native_frame_operation_diff_reports_failure_transitions();
    test_native_frame_operation_execution_plan_executes_ready_lifecycle();
    test_native_frame_operation_execution_plan_falls_back_at_submit_blocker();
    test_native_frame_operation_execution_plan_falls_back_at_present_failure();
    test_native_frame_operation_execution_plan_skips_unchecked_summary();
    return 0;
}
