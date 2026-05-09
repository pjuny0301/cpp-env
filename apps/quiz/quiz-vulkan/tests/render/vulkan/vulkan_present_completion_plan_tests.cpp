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

} // namespace

int main()
{
    test_vulkan_present_completion_plan_names_are_stable();
    test_vulkan_present_completion_plan_uses_queue_present_adapter_summary();
    test_vulkan_present_completion_plan_accepts_existing_frame_present_path();
    test_vulkan_present_completion_plan_blocks_without_submit_batch();
    test_vulkan_present_completion_plan_maps_present_target_unavailable();
    test_vulkan_present_completion_plan_maps_present_failures();
    return 0;
}
