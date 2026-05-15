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
make_ready_submit_batch_plan()
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
        .image_id = vulkan_backend::vulkan_swapchain_image_id{.value = 5},
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
                .image_id = vulkan_backend::vulkan_swapchain_image_id{.value = 5},
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
        .command_submit = {},
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
            .image_id = vulkan_backend::vulkan_swapchain_image_id{.value = 5},
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

quiz_vulkan::render::vulkan_backend::vulkan_native_swapchain_entrypoint_readiness
make_ready_native_swapchain_entrypoints()
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
        .diagnostic = "native swapchain entrypoints ready",
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_native_swapchain_acquire_operation_result
make_ready_native_acquire_operation(
    quiz_vulkan::render::vulkan_backend::vulkan_native_swapchain_acquire_operation_status status =
        quiz_vulkan::render::vulkan_backend::vulkan_native_swapchain_acquire_operation_status::ready)
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const bool recordable =
        status == vulkan_backend::vulkan_native_swapchain_acquire_operation_status::ready
        || status == vulkan_backend::vulkan_native_swapchain_acquire_operation_status::suboptimal;
    const bool out_of_date =
        status == vulkan_backend::vulkan_native_swapchain_acquire_operation_status::out_of_date;
    const bool suboptimal =
        status == vulkan_backend::vulkan_native_swapchain_acquire_operation_status::suboptimal;
    const bool error =
        status == vulkan_backend::vulkan_native_swapchain_acquire_operation_status::error;
    const vulkan_backend::vulkan_device_handle device{.value = 77};
    const vulkan_backend::vulkan_swapchain_handle swapchain{.value = 90};
    const vulkan_backend::vulkan_swapchain_image_id image_id{.value = recordable ? 5U : 0U};
    const vulkan_backend::vulkan_swapchain_image_handle image_handle{
        .value = recordable ? 5005U : 0U,
    };
    const vulkan_backend::vulkan_swapchain_acquire_status acquire_status =
        suboptimal ? vulkan_backend::vulkan_swapchain_acquire_status::suboptimal
        : out_of_date ? vulkan_backend::vulkan_swapchain_acquire_status::out_of_date
        : error ? vulkan_backend::vulkan_swapchain_acquire_status::error
                : vulkan_backend::vulkan_swapchain_acquire_status::acquired;
    const vulkan_backend::vulkan_swapchain_image_acquire_plan_status acquire_plan_status =
        suboptimal ? vulkan_backend::vulkan_swapchain_image_acquire_plan_status::suboptimal
        : out_of_date ? vulkan_backend::vulkan_swapchain_image_acquire_plan_status::out_of_date
        : error ? vulkan_backend::vulkan_swapchain_image_acquire_plan_status::error
                : vulkan_backend::vulkan_swapchain_image_acquire_plan_status::ready;

    const vulkan_backend::vulkan_native_swapchain_acquire_operation_summary summary{
        .checked = true,
        .status = status,
        .entrypoint_name = "vkAcquireNextImageKHR",
        .vk_acquire_next_image_callable = true,
        .command_recording_may_consume_acquired_image = recordable,
        .device = device,
        .swapchain = swapchain,
        .selected_image_index = image_id.value,
        .image_id = image_id,
        .image_handle = image_handle,
        .timeout_nanoseconds = 16000000,
        .acquire_status = acquire_status,
        .acquire_plan_status = acquire_plan_status,
        .images_operation_checked = true,
        .images_operation_ready = true,
        .acquire_plan_checked = true,
        .acquire_plan_ready_for_command_recording = recordable,
        .native_entrypoints_checked = true,
        .required_extensions_ready = true,
        .acquire_symbol_ready = true,
        .device_valid = true,
        .swapchain_valid = true,
        .image_binding_ready = recordable,
        .image_available = recordable,
        .image_acquired = recordable,
        .timed_out = false,
        .out_of_date = out_of_date,
        .suboptimal = suboptimal,
        .error = error,
        .diagnostic = "native acquire operation",
    };

    return vulkan_backend::vulkan_native_swapchain_acquire_operation_result{
        .checked = true,
        .status = status,
        .device = device,
        .swapchain = swapchain,
        .selected_image_index = image_id.value,
        .image_id = image_id,
        .image_handle = image_handle,
        .timeout_nanoseconds = 16000000,
        .acquire_status = acquire_status,
        .acquire_plan_status = acquire_plan_status,
        .images_operation_checked = true,
        .images_operation_ready = true,
        .acquire_plan_checked = true,
        .acquire_plan_ready_for_command_recording = recordable,
        .native_entrypoints_checked = true,
        .required_extensions_ready = true,
        .acquire_symbol_ready = true,
        .device_valid = true,
        .swapchain_valid = true,
        .image_binding_ready = recordable,
        .image_available = recordable,
        .image_acquired = recordable,
        .timed_out = false,
        .out_of_date = out_of_date,
        .suboptimal = suboptimal,
        .error = error,
        .vk_acquire_next_image_callable = true,
        .command_recording_may_consume_acquired_image = recordable,
        .diagnostic = "native acquire operation",
        .operation = summary,
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_present_completion_plan_result
make_ready_present_completion_plan()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::build_vulkan_present_completion_plan(
        make_ready_submit_batch_plan(),
        make_completed_queue_present());
}

quiz_vulkan::render::vulkan_backend::vulkan_native_queue_present_operation_request
make_ready_native_queue_present_request()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_native_queue_present_operation_request{
        .acquire_operation = make_ready_native_acquire_operation(),
        .submit_batch = make_ready_submit_batch_plan(),
        .present_completion = make_ready_present_completion_plan(),
        .native_entrypoints = make_ready_native_swapchain_entrypoints(),
        .present_result = vulkan_backend::vulkan_swapchain_present_result{
            .status = vulkan_backend::vulkan_swapchain_present_status::presented,
            .image_id = vulkan_backend::vulkan_swapchain_image_id{.value = 5},
        },
        .allow_suboptimal = true,
    };
}

void test_vulkan_present_completion_plan_names_are_stable()
{
    using namespace quiz_vulkan::render;

    require(
        vulkan_backend::present_completion_plan_status_name(
            vulkan_backend::vulkan_present_completion_plan_status::not_checked)
            == std::string_view{"not_checked"},
        "present completion status name for not checked is stable");
    require(
        vulkan_backend::present_completion_plan_status_name(
            vulkan_backend::vulkan_present_completion_plan_status::ready)
            == std::string_view{"ready"},
        "present completion status name for ready is stable");
    require(
        vulkan_backend::present_completion_plan_status_name(
            vulkan_backend::vulkan_present_completion_plan_status::submit_batch_unavailable)
            == std::string_view{"submit_batch_unavailable"},
        "present completion status name for submit batch unavailable is stable");
    require(
        vulkan_backend::present_completion_plan_status_name(
            vulkan_backend::vulkan_present_completion_plan_status::present_failed_fatal)
            == std::string_view{"present_failed_fatal"},
        "present completion status name for fatal present failure is stable");
    require(
        vulkan_backend::frame_completion_status_name(
            vulkan_backend::vulkan_frame_completion_status::completed)
            == std::string_view{"completed"},
        "frame completion status name for completed is stable");
    require(
        vulkan_backend::frame_completion_status_name(
            vulkan_backend::vulkan_frame_completion_status::present_unavailable)
            == std::string_view{"present_unavailable"},
        "frame completion status name for present unavailable is stable");
    require(
        vulkan_backend::native_queue_present_operation_status_name(
            vulkan_backend::vulkan_native_queue_present_operation_status::ready)
            == std::string_view{"ready"},
        "native present operation ready status name is stable");
    require(
        vulkan_backend::native_queue_present_operation_status_name(
            vulkan_backend::vulkan_native_queue_present_operation_status::missing_queue_present_symbol)
            == std::string_view{"missing_queue_present_symbol"},
        "native present operation missing symbol status name is stable");
    require(
        vulkan_backend::native_queue_present_operation_status_name(
            vulkan_backend::vulkan_native_queue_present_operation_status::out_of_date)
            == std::string_view{"out_of_date"},
        "native present operation out-of-date status name is stable");
}

void test_vulkan_present_completion_plan_uses_queue_present_adapter_summary()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_present_completion_plan_result plan =
        vulkan_backend::build_vulkan_present_completion_plan(
            make_ready_submit_batch_plan(),
            make_completed_queue_present());

    require(plan.checked, "present completion plan is checked");
    require(plan.completed(), "present completion plan completes");
    require(
        plan.status == vulkan_backend::vulkan_present_completion_plan_status::ready,
        "present completion plan is ready");
    require(
        plan.frame_status == vulkan_backend::vulkan_frame_completion_status::completed,
        "present completion plan marks frame completed");
    require(
        plan.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::none,
        "present completion plan has no fallback");
    require(plan.submit_batch_ready, "present completion plan consumes ready submit batch");
    require(plan.queue_present_adapter_checked, "present completion plan records checked adapter");
    require(plan.queue_present_adapter_ready, "present completion plan records ready adapter");
    require(plan.present_request_ready, "present request summary is ready");
    require(plan.present_result_ready, "present result summary is ready");
    require(plan.frame_completion_ready, "frame completion readiness is exposed");
    require(plan.request.source_adapter_checked, "present request comes from adapter");
    require(plan.request.present_queue.value == 101, "present completion preserves adapter present queue");
    require(plan.request.swapchain.value == 90, "present completion preserves adapter swapchain");
    require(plan.request.image_id.value == 5, "present completion preserves adapter image");
    require(plan.result.present_called, "present completion records present call");
    require(plan.result.submit_before_present, "present completion records submit-before-present order");
}

void test_vulkan_present_completion_plan_accepts_existing_frame_present_path()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_present_completion_plan_result plan =
        vulkan_backend::build_vulkan_present_completion_plan(
            make_ready_submit_batch_plan(),
            vulkan_backend::vulkan_queue_submit_present_result{});

    require(plan.completed(), "unchecked queue present plan completes through existing frame path");
    require(!plan.queue_present_adapter_checked, "unchecked queue present adapter is recorded");
    require(plan.queue_present_adapter_ready, "existing frame present path is adapter-ready");
    require(
        plan.frame_status == vulkan_backend::vulkan_frame_completion_status::ready_for_present,
        "existing frame path exposes readiness before present");
    require(plan.request.present_queue.value == 7, "existing frame path derives deterministic queue from submit batch");
    require(plan.request.swapchain.value == 1, "existing frame path uses deterministic fake swapchain");
    require(plan.result.completed(), "existing frame path creates deterministic present result summary");
}

void test_vulkan_present_completion_plan_blocks_without_submit_batch()
{
    using namespace quiz_vulkan::render;

    vulkan_backend::vulkan_submit_batch_plan_result submit_batch =
        make_ready_submit_batch_plan();
    submit_batch.status =
        vulkan_backend::vulkan_submit_batch_plan_status::submit_queue_unavailable;
    submit_batch.fallback_reason =
        vulkan_backend::vulkan_backend_fallback_reason::submit_frame_failed;

    const vulkan_backend::vulkan_present_completion_plan_result plan =
        vulkan_backend::build_vulkan_present_completion_plan(
            submit_batch,
            make_completed_queue_present());

    require(!plan.completed(), "present completion blocks without submit batch");
    require(plan.blocked(), "present completion reports blocked without submit batch");
    require(
        plan.status
            == vulkan_backend::vulkan_present_completion_plan_status::submit_batch_unavailable,
        "present completion maps missing submit batch");
    require(
        plan.frame_status == vulkan_backend::vulkan_frame_completion_status::submit_unavailable,
        "present completion maps submit batch failure to submit-unavailable frame status");
    require(
        plan.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::submit_frame_failed,
        "present completion preserves submit fallback");
}

void test_vulkan_present_completion_plan_maps_present_target_unavailable()
{
    using namespace quiz_vulkan::render;

    vulkan_backend::vulkan_queue_submit_present_result queue_present =
        make_completed_queue_present();
    queue_present.status =
        vulkan_backend::vulkan_queue_submit_present_status::present_target_unavailable;
    queue_present.present_called = false;
    queue_present.present_result = {};
    queue_present.diagnostic = "present target missing";

    const vulkan_backend::vulkan_present_completion_plan_result plan =
        vulkan_backend::build_vulkan_present_completion_plan(
            make_ready_submit_batch_plan(),
            queue_present);

    require(
        plan.status
            == vulkan_backend::vulkan_present_completion_plan_status::present_request_unavailable,
        "present completion maps missing present target");
    require(
        plan.frame_status == vulkan_backend::vulkan_frame_completion_status::present_unavailable,
        "present completion maps missing target to present-unavailable frame status");
    require(
        plan.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::present_frame_failed,
        "present completion maps missing target to present fallback");
}

void test_vulkan_present_completion_plan_maps_present_failures()
{
    using namespace quiz_vulkan::render;

    vulkan_backend::vulkan_queue_submit_present_result recoverable =
        make_completed_queue_present();
    recoverable.status =
        vulkan_backend::vulkan_queue_submit_present_status::present_failed_recoverable;
    recoverable.present_result =
        vulkan_backend::vulkan_queue_submit_adapter_operation_result{
            .checked = true,
            .status = vulkan_backend::vulkan_queue_submit_adapter_call_status::failed_recoverable,
            .diagnostic = "recoverable present",
        };
    const vulkan_backend::vulkan_present_completion_plan_result recoverable_plan =
        vulkan_backend::build_vulkan_present_completion_plan(
            make_ready_submit_batch_plan(),
            recoverable);
    require(
        recoverable_plan.status
            == vulkan_backend::vulkan_present_completion_plan_status::present_failed_recoverable,
        "present completion maps recoverable present failure");
    require(recoverable_plan.recoverable_failure, "recoverable present failure is classified");
    require(!recoverable_plan.fatal_failure, "recoverable present failure is not fatal");

    vulkan_backend::vulkan_queue_submit_present_result fatal =
        make_completed_queue_present();
    fatal.status = vulkan_backend::vulkan_queue_submit_present_status::present_failed_fatal;
    fatal.present_result =
        vulkan_backend::vulkan_queue_submit_adapter_operation_result{
            .checked = true,
            .status = vulkan_backend::vulkan_queue_submit_adapter_call_status::failed_fatal,
            .diagnostic = "fatal present",
        };
    const vulkan_backend::vulkan_present_completion_plan_result fatal_plan =
        vulkan_backend::build_vulkan_present_completion_plan(
            make_ready_submit_batch_plan(),
            fatal);
    require(
        fatal_plan.status
            == vulkan_backend::vulkan_present_completion_plan_status::present_failed_fatal,
        "present completion maps fatal present failure");
    require(fatal_plan.fatal_failure, "fatal present failure is classified");
    require(!fatal_plan.recoverable_failure, "fatal present failure is not recoverable");
}

void test_native_queue_present_operation_reports_ready_completion()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_native_queue_present_operation_result operation =
        vulkan_backend::build_vulkan_native_queue_present_operation_plan(
            make_ready_native_queue_present_request());

    require(operation.checked, "native present operation is checked");
    require(
        operation.status == vulkan_backend::vulkan_native_queue_present_operation_status::ready,
        "native present operation reports ready");
    require(operation.can_call_vk_queue_present(), "native present operation exposes call gate");
    require(operation.ready_for_frame_completion(), "native present operation completes frame");
    require(!operation.blocked(), "ready native present operation is not blocked");
    require(operation.required_extensions_ready, "native present records extension readiness");
    require(operation.queue_present_symbol_ready, "native present records queue-present symbol");
    require(operation.acquired_image_ready, "native present records acquired image readiness");
    require(operation.submit_batch_ready, "native present records submit batch readiness");
    require(operation.submitted_frame_ready, "native present records submitted frame readiness");
    require(operation.present_completion_ready, "native present records present completion readiness");
    require(operation.present_request_ready, "native present records present request readiness");
    require(operation.present_adapter_result_ready, "native present records adapter result readiness");
    require(operation.present_result_completed, "native present records present result completion");
    require(operation.frame_lifecycle_may_complete, "native present exposes frame completion gate");
    require(operation.device.value == 77, "native present preserves device handle");
    require(operation.swapchain.value == 90, "native present preserves swapchain handle");
    require(operation.present_queue.value == 101, "native present preserves present queue");
    require(operation.image_id.value == 5, "native present preserves image id");
    require(operation.image_handle.value == 5005, "native present preserves image handle");
    require(operation.wait_render_finished_semaphore.value == 12, "native present preserves wait semaphore");
    require(operation.submit_before_present, "native present records submit-before-present order");
    require(operation.operation.ready_for_frame_completion(), "native present summary is frame-ready");
    require(
        operation.operation.entrypoint_name == "vkQueuePresentKHR",
        "native present summary names stable entrypoint");
}

void test_native_queue_present_operation_blocks_native_extension_and_symbol()
{
    using namespace quiz_vulkan::render;

    vulkan_backend::vulkan_native_queue_present_operation_request request =
        make_ready_native_queue_present_request();
    request.native_entrypoints.required_extensions_ready = false;
    request.native_entrypoints.missing_required_extension = "VK_KHR_swapchain";
    request.native_entrypoints.diagnostic = "missing VK_KHR_swapchain";
    const vulkan_backend::vulkan_native_queue_present_operation_result missing_extension =
        vulkan_backend::build_vulkan_native_queue_present_operation_plan(request);

    require(
        missing_extension.status
            == vulkan_backend::vulkan_native_queue_present_operation_status::required_extension_unavailable,
        "native present blocks missing swapchain extension");
    require(
        missing_extension.missing_required_extension == "VK_KHR_swapchain",
        "native present records missing extension");
    require(!missing_extension.can_call_vk_queue_present(), "missing extension blocks call gate");

    request = make_ready_native_queue_present_request();
    request.native_entrypoints.queue_present_ready = false;
    request.native_entrypoints.missing_symbol_name = "vkQueuePresentKHR";
    const vulkan_backend::vulkan_native_queue_present_operation_result missing_symbol =
        vulkan_backend::build_vulkan_native_queue_present_operation_plan(request);

    require(
        missing_symbol.status
            == vulkan_backend::vulkan_native_queue_present_operation_status::missing_queue_present_symbol,
        "native present blocks missing queue-present symbol");
    require(
        missing_symbol.missing_symbol_name == "vkQueuePresentKHR",
        "native present records missing queue-present symbol");
}

void test_native_queue_present_operation_blocks_acquire_submit_and_targets()
{
    using namespace quiz_vulkan::render;

    vulkan_backend::vulkan_native_queue_present_operation_request request =
        make_ready_native_queue_present_request();
    request.acquire_operation = make_ready_native_acquire_operation(
        vulkan_backend::vulkan_native_swapchain_acquire_operation_status::out_of_date);
    const vulkan_backend::vulkan_native_queue_present_operation_result outdated_acquire =
        vulkan_backend::build_vulkan_native_queue_present_operation_plan(request);
    require(
        outdated_acquire.status
            == vulkan_backend::vulkan_native_queue_present_operation_status::out_of_date,
        "native present maps out-of-date acquire");
    require(outdated_acquire.out_of_date, "native present records acquire out-of-date flag");

    request = make_ready_native_queue_present_request();
    request.submit_batch.status =
        vulkan_backend::vulkan_submit_batch_plan_status::submit_queue_unavailable;
    request.submit_batch.diagnostic = "missing submit queue";
    const vulkan_backend::vulkan_native_queue_present_operation_result missing_submit =
        vulkan_backend::build_vulkan_native_queue_present_operation_plan(request);
    require(
        missing_submit.status
            == vulkan_backend::vulkan_native_queue_present_operation_status::submit_batch_unavailable,
        "native present blocks missing submit batch");
    require(!missing_submit.submitted_frame_ready, "missing submit batch blocks submitted frame");

    request = make_ready_native_queue_present_request();
    request.present_completion.request.present_queue = {};
    const vulkan_backend::vulkan_native_queue_present_operation_result missing_queue =
        vulkan_backend::build_vulkan_native_queue_present_operation_plan(request);
    require(
        missing_queue.status
            == vulkan_backend::vulkan_native_queue_present_operation_status::present_queue_unavailable,
        "native present blocks missing present queue");

    request = make_ready_native_queue_present_request();
    request.present_completion.request.swapchain = {};
    const vulkan_backend::vulkan_native_queue_present_operation_result missing_swapchain =
        vulkan_backend::build_vulkan_native_queue_present_operation_plan(request);
    require(
        missing_swapchain.status
            == vulkan_backend::vulkan_native_queue_present_operation_status::swapchain_unavailable,
        "native present blocks missing swapchain handle");
}

void test_native_queue_present_operation_maps_present_result_statuses()
{
    using namespace quiz_vulkan::render;

    vulkan_backend::vulkan_native_queue_present_operation_request request =
        make_ready_native_queue_present_request();
    request.present_result.status = vulkan_backend::vulkan_swapchain_present_status::suboptimal;
    const vulkan_backend::vulkan_native_queue_present_operation_result suboptimal =
        vulkan_backend::build_vulkan_native_queue_present_operation_plan(request);
    require(
        suboptimal.status
            == vulkan_backend::vulkan_native_queue_present_operation_status::suboptimal,
        "native present reports suboptimal present result");
    require(suboptimal.can_call_vk_queue_present(), "suboptimal present still records call gate");
    require(suboptimal.ready_for_frame_completion(), "allowed suboptimal present completes frame");
    require(suboptimal.suboptimal, "native present records suboptimal flag");

    request = make_ready_native_queue_present_request();
    request.present_result.status = vulkan_backend::vulkan_swapchain_present_status::out_of_date;
    const vulkan_backend::vulkan_native_queue_present_operation_result out_of_date =
        vulkan_backend::build_vulkan_native_queue_present_operation_plan(request);
    require(
        out_of_date.status
            == vulkan_backend::vulkan_native_queue_present_operation_status::out_of_date,
        "native present reports out-of-date present result");
    require(out_of_date.can_call_vk_queue_present(), "out-of-date present records call gate");
    require(!out_of_date.ready_for_frame_completion(), "out-of-date present does not complete frame");

    request = make_ready_native_queue_present_request();
    request.present_result.status = vulkan_backend::vulkan_swapchain_present_status::failed;
    const vulkan_backend::vulkan_native_queue_present_operation_result recoverable =
        vulkan_backend::build_vulkan_native_queue_present_operation_plan(request);
    require(
        recoverable.status
            == vulkan_backend::vulkan_native_queue_present_operation_status::present_failed_recoverable,
        "native present maps failed status to recoverable failure");
    require(recoverable.recoverable_failure, "native present records recoverable failure");

    request = make_ready_native_queue_present_request();
    request.present_result.status = vulkan_backend::vulkan_swapchain_present_status::error;
    const vulkan_backend::vulkan_native_queue_present_operation_result fatal =
        vulkan_backend::build_vulkan_native_queue_present_operation_plan(request);
    require(
        fatal.status
            == vulkan_backend::vulkan_native_queue_present_operation_status::present_failed_fatal,
        "native present maps error status to fatal failure");
    require(fatal.fatal_failure, "native present records fatal failure");
}

} // namespace

int main()
{
    test_vulkan_present_completion_plan_names_are_stable();
    test_vulkan_present_completion_plan_uses_queue_present_adapter_summary();
    test_vulkan_present_completion_plan_accepts_existing_frame_present_path();
    test_vulkan_present_completion_plan_blocks_without_submit_batch();
    test_vulkan_present_completion_plan_maps_present_target_unavailable();
    test_vulkan_present_completion_plan_maps_present_failures();
    test_native_queue_present_operation_reports_ready_completion();
    test_native_queue_present_operation_blocks_native_extension_and_symbol();
    test_native_queue_present_operation_blocks_acquire_submit_and_targets();
    test_native_queue_present_operation_maps_present_result_statuses();
    return 0;
}
