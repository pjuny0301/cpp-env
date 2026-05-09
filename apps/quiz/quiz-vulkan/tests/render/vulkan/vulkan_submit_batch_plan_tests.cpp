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

quiz_vulkan::render::vulkan_backend::vulkan_command_buffer_record_result
make_recorded_command_buffer()
{
    return quiz_vulkan::render::vulkan_backend::vulkan_command_buffer_record_result{
        .checked = true,
        .status = quiz_vulkan::render::vulkan_backend::vulkan_command_buffer_record_result_status::recorded,
        .fallback_reason = quiz_vulkan::render::vulkan_backend::vulkan_backend_fallback_reason::none,
        .command_buffer = quiz_vulkan::render::vulkan_backend::vulkan_command_buffer_id{.value = 42},
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
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_command_submit_readiness_result
make_ready_submit_readiness()
{
    return quiz_vulkan::render::vulkan_backend::vulkan_command_submit_readiness_result{
        .checked = true,
        .status = quiz_vulkan::render::vulkan_backend::vulkan_command_submit_readiness_status::ready,
        .command_buffer = quiz_vulkan::render::vulkan_backend::vulkan_command_recording_command_buffer_handle{.value = 99},
        .submit_queue = quiz_vulkan::render::vulkan_backend::vulkan_queue_handle{.value = 7},
        .sync_primitives = quiz_vulkan::render::vulkan_backend::vulkan_command_submit_sync_primitives{
            .image_available_semaphore =
                quiz_vulkan::render::vulkan_backend::vulkan_command_submit_sync_handle{.value = 11},
            .render_finished_semaphore =
                quiz_vulkan::render::vulkan_backend::vulkan_command_submit_sync_handle{.value = 12},
            .frame_fence =
                quiz_vulkan::render::vulkan_backend::vulkan_command_submit_sync_handle{.value = 13},
        },
        .image_id = quiz_vulkan::render::vulkan_backend::vulkan_swapchain_image_id{.value = 5},
        .planned_batch_count = 4,
        .submitted_batch_count = 4,
        .command_recording_available = true,
        .command_buffer_available = true,
        .sync_primitives_available = true,
        .submit_queue_available = true,
        .present_target_available = true,
        .submit_attempted = true,
        .failure_mode = quiz_vulkan::render::vulkan_backend::vulkan_command_submit_failure_mode::none,
        .frame_result = quiz_vulkan::render::vulkan_backend::vulkan_command_submit_frame_result{
            .checked = true,
            .status = quiz_vulkan::render::vulkan_backend::vulkan_command_submit_frame_status::submitted,
            .submit_attempted = true,
            .submitted = true,
            .present_ready = true,
            .failure_mode = quiz_vulkan::render::vulkan_backend::vulkan_command_submit_failure_mode::none,
            .diagnostic = "ready",
        },
        .diagnostic = "ready",
    };
}

void test_vulkan_submit_batch_plan_names_are_stable()
{
    using namespace quiz_vulkan::render;

    require(
        vulkan_backend::submit_batch_plan_status_name(
            vulkan_backend::vulkan_submit_batch_plan_status::not_checked)
            == std::string_view{"not_checked"},
        "submit batch plan status name for not checked is stable");
    require(
        vulkan_backend::submit_batch_plan_status_name(
            vulkan_backend::vulkan_submit_batch_plan_status::ready)
            == std::string_view{"ready"},
        "submit batch plan status name for ready is stable");
    require(
        vulkan_backend::submit_batch_plan_status_name(
            vulkan_backend::vulkan_submit_batch_plan_status::command_buffer_recording_unavailable)
            == std::string_view{"command_buffer_recording_unavailable"},
        "submit batch plan status name for recording unavailable is stable");
    require(
        vulkan_backend::submit_batch_plan_status_name(
            vulkan_backend::vulkan_submit_batch_plan_status::command_submit_unavailable)
            == std::string_view{"command_submit_unavailable"},
        "submit batch plan status name for command submit unavailable is stable");
    require(
        vulkan_backend::submit_batch_plan_status_name(
            vulkan_backend::vulkan_submit_batch_plan_status::sync_primitives_unavailable)
            == std::string_view{"sync_primitives_unavailable"},
        "submit batch plan status name for sync unavailable is stable");
    require(
        vulkan_backend::submit_batch_plan_status_name(
            vulkan_backend::vulkan_submit_batch_plan_status::present_target_unavailable)
            == std::string_view{"present_target_unavailable"},
        "submit batch plan status name for present target unavailable is stable");
    require(
        vulkan_backend::submit_batch_sync_intent_kind_name(
            vulkan_backend::vulkan_submit_batch_sync_intent_kind::wait_image_available)
            == std::string_view{"wait_image_available"},
        "submit batch wait intent name is stable");
    require(
        vulkan_backend::submit_batch_sync_intent_kind_name(
            vulkan_backend::vulkan_submit_batch_sync_intent_kind::signal_render_finished)
            == std::string_view{"signal_render_finished"},
        "submit batch render-finished signal intent name is stable");
    require(
        vulkan_backend::submit_batch_sync_intent_kind_name(
            vulkan_backend::vulkan_submit_batch_sync_intent_kind::signal_frame_fence)
            == std::string_view{"signal_frame_fence"},
        "submit batch fence signal intent name is stable");
}

void test_vulkan_submit_batch_plan_records_ready_submit_intents()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_submit_batch_plan_result plan =
        vulkan_backend::build_vulkan_submit_batch_plan(
            make_recorded_command_buffer(),
            make_ready_submit_readiness());

    require(plan.checked, "submit batch plan is checked");
    require(plan.completed(), "submit batch plan completes");
    require(!plan.blocked(), "submit batch plan does not block");
    require(
        plan.status == vulkan_backend::vulkan_submit_batch_plan_status::ready,
        "submit batch plan status is ready");
    require(
        plan.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::none,
        "submit batch plan has no fallback");
    require(plan.command_buffer_recording_ready, "submit batch plan records ready command buffer recording");
    require(plan.command_submit_readiness_checked, "submit batch plan consumes checked submit readiness");
    require(plan.command_submit_readiness_ready, "submit batch plan records ready submit readiness");
    require(plan.command_buffer_available, "submit batch plan records command buffer availability");
    require(plan.sync_primitives_available, "submit batch plan records sync availability");
    require(plan.submit_queue_available, "submit batch plan records queue availability");
    require(plan.present_target_available, "submit batch plan records present target availability");
    require(plan.submit_ready, "submit batch plan is ready for queue submit");
    require(plan.present_ready, "submit batch plan is ready for present intent");
    require(plan.recorded_operation_count == 4, "submit batch plan records operation count");
    require(plan.submit_batch_count == 1, "submit batch plan creates one submit batch");
    require(plan.wait_intent_count == 1, "submit batch plan creates one wait intent");
    require(plan.signal_intent_count == 2, "submit batch plan creates two signal intents");
    require(plan.present_intent_count == 1, "submit batch plan creates one present intent");
    require(plan.recorded_command_buffer.value == 42, "submit batch plan preserves recorded command buffer");
    require(plan.submit_command_buffer.value == 99, "submit batch plan uses submit-readiness command buffer");
    require(plan.submit_queue.value == 7, "submit batch plan preserves submit queue");
    require(plan.image_id.value == 5, "submit batch plan preserves present image id");
    require(plan.submit_batches.size() == 1, "submit batch plan stores submit batch record");
    require(plan.submit_batches[0].completed(), "submit batch record completes");
    require(plan.submit_batches[0].recorded_operation_count == 4, "submit batch record counts operations");
    require(plan.wait_intents[0].completed(), "submit batch wait intent completes");
    require(plan.signal_intents[0].completed(), "submit batch render signal intent completes");
    require(plan.signal_intents[1].completed(), "submit batch fence signal intent completes");
    require(plan.present_intents[0].completed(), "submit batch present intent completes");
}

void test_vulkan_submit_batch_plan_accepts_unchecked_legacy_submit_readiness()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_submit_batch_plan_result plan =
        vulkan_backend::build_vulkan_submit_batch_plan(
            make_recorded_command_buffer(),
            vulkan_backend::vulkan_command_submit_readiness_result{});

    require(plan.checked, "legacy submit batch plan is checked");
    require(plan.completed(), "legacy submit batch plan completes");
    require(!plan.command_submit_readiness_checked, "legacy plan records unchecked submit readiness");
    require(plan.command_submit_readiness_ready, "legacy plan preserves existing fake submit path");
    require(plan.submit_command_buffer.value == 42, "legacy plan derives submit command buffer from recording");
    require(plan.submit_queue.value == 1, "legacy plan uses deterministic fake submit queue");
    require(plan.sync_primitives.image_available_semaphore.value == 1, "legacy plan uses deterministic wait semaphore");
    require(plan.sync_primitives.render_finished_semaphore.value == 2, "legacy plan uses deterministic present wait");
    require(plan.sync_primitives.frame_fence.value == 3, "legacy plan uses deterministic fence");
}

void test_vulkan_submit_batch_plan_blocks_without_recorded_command_buffer()
{
    using namespace quiz_vulkan::render;

    vulkan_backend::vulkan_command_buffer_record_result recording = make_recorded_command_buffer();
    recording.status = vulkan_backend::vulkan_command_buffer_record_result_status::operation_failed;
    recording.fallback_reason = vulkan_backend::vulkan_backend_fallback_reason::record_commands_failed;
    const vulkan_backend::vulkan_submit_batch_plan_result plan =
        vulkan_backend::build_vulkan_submit_batch_plan(recording, make_ready_submit_readiness());

    require(plan.checked, "recording-blocked submit batch plan is checked");
    require(!plan.completed(), "recording-blocked submit batch plan does not complete");
    require(plan.blocked(), "recording-blocked submit batch plan reports blocked");
    require(
        plan.status
            == vulkan_backend::vulkan_submit_batch_plan_status::command_buffer_recording_unavailable,
        "submit batch plan records unavailable command buffer recording");
    require(
        plan.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::record_commands_failed,
        "submit batch plan preserves recording fallback");
}

void test_vulkan_submit_batch_plan_blocks_on_missing_submit_prerequisites()
{
    using namespace quiz_vulkan::render;

    vulkan_backend::vulkan_command_submit_readiness_result missing_sync = make_ready_submit_readiness();
    missing_sync.sync_primitives_available = false;
    missing_sync.status = vulkan_backend::vulkan_command_submit_readiness_status::sync_primitives_unavailable;
    missing_sync.frame_result.status = vulkan_backend::vulkan_command_submit_frame_status::not_requested;
    const vulkan_backend::vulkan_submit_batch_plan_result sync_plan =
        vulkan_backend::build_vulkan_submit_batch_plan(make_recorded_command_buffer(), missing_sync);
    require(
        sync_plan.status == vulkan_backend::vulkan_submit_batch_plan_status::sync_primitives_unavailable,
        "submit batch plan blocks missing sync primitives");
    require(
        sync_plan.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::submit_frame_failed,
        "submit batch plan maps missing sync to submit fallback");

    vulkan_backend::vulkan_command_submit_readiness_result missing_queue = make_ready_submit_readiness();
    missing_queue.submit_queue_available = false;
    missing_queue.status = vulkan_backend::vulkan_command_submit_readiness_status::submit_queue_unavailable;
    missing_queue.frame_result.status = vulkan_backend::vulkan_command_submit_frame_status::not_requested;
    const vulkan_backend::vulkan_submit_batch_plan_result queue_plan =
        vulkan_backend::build_vulkan_submit_batch_plan(make_recorded_command_buffer(), missing_queue);
    require(
        queue_plan.status == vulkan_backend::vulkan_submit_batch_plan_status::submit_queue_unavailable,
        "submit batch plan blocks missing submit queue");

    vulkan_backend::vulkan_command_submit_readiness_result missing_present = make_ready_submit_readiness();
    missing_present.present_target_available = false;
    missing_present.frame_result.present_ready = false;
    missing_present.status = vulkan_backend::vulkan_command_submit_readiness_status::present_target_unavailable;
    const vulkan_backend::vulkan_submit_batch_plan_result present_plan =
        vulkan_backend::build_vulkan_submit_batch_plan(make_recorded_command_buffer(), missing_present);
    require(
        present_plan.status == vulkan_backend::vulkan_submit_batch_plan_status::present_target_unavailable,
        "submit batch plan blocks missing present target");
    require(
        present_plan.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::present_frame_failed,
        "submit batch plan maps missing present target to present fallback");
}

void test_vulkan_submit_batch_plan_maps_submit_failure_modes()
{
    using namespace quiz_vulkan::render;

    vulkan_backend::vulkan_command_submit_readiness_result recoverable = make_ready_submit_readiness();
    recoverable.status = vulkan_backend::vulkan_command_submit_readiness_status::submit_failed_recoverable;
    recoverable.failure_mode = vulkan_backend::vulkan_command_submit_failure_mode::recoverable;
    recoverable.frame_result.status =
        vulkan_backend::vulkan_command_submit_frame_status::submit_failed_recoverable;
    recoverable.frame_result.submitted = false;
    recoverable.frame_result.failure_mode = vulkan_backend::vulkan_command_submit_failure_mode::recoverable;
    const vulkan_backend::vulkan_submit_batch_plan_result recoverable_plan =
        vulkan_backend::build_vulkan_submit_batch_plan(make_recorded_command_buffer(), recoverable);
    require(
        recoverable_plan.status
            == vulkan_backend::vulkan_submit_batch_plan_status::submit_failed_recoverable,
        "submit batch plan maps recoverable submit failure");

    vulkan_backend::vulkan_command_submit_readiness_result fatal = make_ready_submit_readiness();
    fatal.status = vulkan_backend::vulkan_command_submit_readiness_status::submit_failed_fatal;
    fatal.failure_mode = vulkan_backend::vulkan_command_submit_failure_mode::fatal;
    fatal.frame_result.status =
        vulkan_backend::vulkan_command_submit_frame_status::submit_failed_fatal;
    fatal.frame_result.submitted = false;
    fatal.frame_result.failure_mode = vulkan_backend::vulkan_command_submit_failure_mode::fatal;
    const vulkan_backend::vulkan_submit_batch_plan_result fatal_plan =
        vulkan_backend::build_vulkan_submit_batch_plan(make_recorded_command_buffer(), fatal);
    require(
        fatal_plan.status == vulkan_backend::vulkan_submit_batch_plan_status::submit_failed_fatal,
        "submit batch plan maps fatal submit failure");
}

} // namespace

int main()
{
    test_vulkan_submit_batch_plan_names_are_stable();
    test_vulkan_submit_batch_plan_records_ready_submit_intents();
    test_vulkan_submit_batch_plan_accepts_unchecked_legacy_submit_readiness();
    test_vulkan_submit_batch_plan_blocks_without_recorded_command_buffer();
    test_vulkan_submit_batch_plan_blocks_on_missing_submit_prerequisites();
    test_vulkan_submit_batch_plan_maps_submit_failure_modes();
    return 0;
}
