#ifndef QUIZ_VULKAN_BACKEND_NATIVE_FRAME_OPERATION_DEPENDENCIES_READY
#include "render/vulkan/vulkan_backend_adapter.h"
#else
#ifndef QUIZ_VULKAN_BACKEND_NATIVE_FRAME_OPERATION_H
#define QUIZ_VULKAN_BACKEND_NATIVE_FRAME_OPERATION_H

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace quiz_vulkan::render::vulkan_backend {

enum class vulkan_native_frame_operation_stage {
    not_started,
    native_function_table,
    swapchain_create,
    swapchain_images,
    acquire,
    command_recording,
    submit,
    present,
    frame_completion,
};

std::string_view native_frame_operation_stage_name(
    vulkan_native_frame_operation_stage stage);

enum class vulkan_native_frame_operation_status {
    not_checked,
    ready,
    native_function_table_unavailable,
    swapchain_create_unavailable,
    swapchain_images_unavailable,
    acquire_unavailable,
    command_recording_unavailable,
    submit_unavailable,
    present_unavailable,
    frame_completion_unavailable,
    recoverable_failure,
    fatal_failure,
};

std::string_view native_frame_operation_status_name(
    vulkan_native_frame_operation_status status);

struct vulkan_native_frame_operation_stage_summary {
    vulkan_native_frame_operation_stage stage =
        vulkan_native_frame_operation_stage::not_started;
    bool checked = false;
    bool ready = false;
    bool blocked = false;
    vulkan_backend_fallback_reason fallback_reason =
        vulkan_backend_fallback_reason::not_requested;
    std::string diagnostic;

    bool completed() const
    {
        return checked && ready && !blocked
            && fallback_reason == vulkan_backend_fallback_reason::none;
    }
};

struct vulkan_native_frame_operation_request {
    vulkan_native_function_table_diagnostics native_functions;
    vulkan_native_swapchain_create_operation_result swapchain_create;
    vulkan_native_swapchain_images_operation_result swapchain_images;
    vulkan_native_swapchain_acquire_operation_result acquire_operation;
    vulkan_command_buffer_record_result command_buffer_recording;
    vulkan_submit_batch_plan_result submit_batch;
    vulkan_native_queue_present_operation_result present_operation;
    bool cpu_fallback_available = true;
};

struct vulkan_native_frame_operation_summary {
    bool checked = false;
    vulkan_native_frame_operation_status status =
        vulkan_native_frame_operation_status::not_checked;
    vulkan_native_frame_operation_stage reached_stage =
        vulkan_native_frame_operation_stage::not_started;
    vulkan_native_frame_operation_stage blocker_stage =
        vulkan_native_frame_operation_stage::not_started;
    vulkan_backend_fallback_reason fallback_reason =
        vulkan_backend_fallback_reason::not_requested;
    bool native_function_table_checked = false;
    bool native_function_table_ready = false;
    bool swapchain_create_checked = false;
    bool swapchain_create_ready = false;
    bool swapchain_images_checked = false;
    bool swapchain_images_ready = false;
    bool acquire_checked = false;
    bool acquire_ready = false;
    bool command_recording_checked = false;
    bool command_recording_ready = false;
    bool submit_batch_checked = false;
    bool submit_batch_ready = false;
    bool present_operation_checked = false;
    bool present_operation_ready = false;
    bool frame_completion_ready = false;
    bool recoverable_failure = false;
    bool fatal_failure = false;
    bool swapchain_out_of_date = false;
    bool suboptimal = false;
    bool cpu_fallback_available = true;
    bool cpu_fallback_should_remain_active = true;
    vulkan_native_function_table_status native_function_table_status =
        vulkan_native_function_table_status::not_checked;
    std::string missing_required_extension;
    std::string missing_symbol_name;
    std::vector<vulkan_native_frame_operation_stage_summary> stages;
    std::string diagnostic;

    bool completed() const
    {
        return checked && status == vulkan_native_frame_operation_status::ready
            && fallback_reason == vulkan_backend_fallback_reason::none
            && native_function_table_ready && swapchain_create_ready
            && swapchain_images_ready && acquire_ready && command_recording_ready
            && submit_batch_ready && present_operation_ready && frame_completion_ready
            && !recoverable_failure && !fatal_failure
            && !cpu_fallback_should_remain_active;
    }
};

struct vulkan_native_frame_operation_result {
    bool checked = false;
    vulkan_native_frame_operation_status status =
        vulkan_native_frame_operation_status::not_checked;
    vulkan_native_function_table_diagnostics native_functions;
    vulkan_native_swapchain_create_operation_result swapchain_create;
    vulkan_native_swapchain_images_operation_result swapchain_images;
    vulkan_native_swapchain_acquire_operation_result acquire_operation;
    vulkan_command_buffer_record_result command_buffer_recording;
    vulkan_submit_batch_plan_result submit_batch;
    vulkan_native_queue_present_operation_result present_operation;
    vulkan_native_frame_operation_stage reached_stage =
        vulkan_native_frame_operation_stage::not_started;
    vulkan_native_frame_operation_stage blocker_stage =
        vulkan_native_frame_operation_stage::not_started;
    vulkan_backend_fallback_reason fallback_reason =
        vulkan_backend_fallback_reason::not_requested;
    bool native_function_table_checked = false;
    bool native_function_table_ready = false;
    bool swapchain_create_checked = false;
    bool swapchain_create_ready = false;
    bool swapchain_images_checked = false;
    bool swapchain_images_ready = false;
    bool acquire_checked = false;
    bool acquire_ready = false;
    bool command_recording_checked = false;
    bool command_recording_ready = false;
    bool submit_batch_checked = false;
    bool submit_batch_ready = false;
    bool present_operation_checked = false;
    bool present_operation_ready = false;
    bool frame_completion_ready = false;
    bool recoverable_failure = false;
    bool fatal_failure = false;
    bool swapchain_out_of_date = false;
    bool suboptimal = false;
    bool cpu_fallback_available = true;
    bool cpu_fallback_should_remain_active = true;
    vulkan_native_function_table_status native_function_table_status =
        vulkan_native_function_table_status::not_checked;
    std::string missing_required_extension;
    std::string missing_symbol_name;
    std::vector<vulkan_native_frame_operation_stage_summary> stages;
    std::string diagnostic;
    vulkan_native_frame_operation_summary operation;

    bool completed() const
    {
        return checked && operation.completed();
    }

    bool blocked() const
    {
        return checked && status != vulkan_native_frame_operation_status::ready;
    }
};

struct vulkan_native_frame_operation_stage_diff_diagnostics {
    vulkan_native_frame_operation_stage stage =
        vulkan_native_frame_operation_stage::not_started;
    bool before_checked = false;
    bool after_checked = false;
    bool before_ready = false;
    bool after_ready = false;
    bool before_blocked = false;
    bool after_blocked = false;
    vulkan_backend_fallback_reason before_fallback_reason =
        vulkan_backend_fallback_reason::not_requested;
    vulkan_backend_fallback_reason after_fallback_reason =
        vulkan_backend_fallback_reason::not_requested;
    bool checked_changed = false;
    bool readiness_changed = false;
    bool became_ready = false;
    bool became_blocked = false;
    bool fallback_reason_changed = false;
    std::string before_diagnostic;
    std::string after_diagnostic;

    bool changed() const
    {
        return checked_changed || readiness_changed || fallback_reason_changed
            || before_diagnostic != after_diagnostic;
    }
};

struct vulkan_native_frame_operation_diff_diagnostics {
    bool checked = false;
    bool changed = false;
    bool before_checked = false;
    bool after_checked = false;
    vulkan_native_frame_operation_status before_status =
        vulkan_native_frame_operation_status::not_checked;
    vulkan_native_frame_operation_status after_status =
        vulkan_native_frame_operation_status::not_checked;
    bool status_changed = false;
    vulkan_native_frame_operation_stage before_reached_stage =
        vulkan_native_frame_operation_stage::not_started;
    vulkan_native_frame_operation_stage after_reached_stage =
        vulkan_native_frame_operation_stage::not_started;
    bool reached_stage_changed = false;
    vulkan_native_frame_operation_stage before_blocker_stage =
        vulkan_native_frame_operation_stage::not_started;
    vulkan_native_frame_operation_stage after_blocker_stage =
        vulkan_native_frame_operation_stage::not_started;
    bool blocker_stage_changed = false;
    bool blocker_introduced = false;
    bool blocker_cleared = false;
    bool blocker_moved_forward = false;
    bool blocker_moved_backward = false;
    vulkan_backend_fallback_reason before_fallback_reason =
        vulkan_backend_fallback_reason::not_requested;
    vulkan_backend_fallback_reason after_fallback_reason =
        vulkan_backend_fallback_reason::not_requested;
    bool fallback_reason_changed = false;
    bool before_cpu_fallback_should_remain_active = false;
    bool after_cpu_fallback_should_remain_active = false;
    bool fallback_activation_changed = false;
    bool fallback_activated = false;
    bool fallback_deactivated = false;
    bool before_recoverable_failure = false;
    bool after_recoverable_failure = false;
    bool recoverable_failure_changed = false;
    bool recoverable_failure_started = false;
    bool recoverable_failure_cleared = false;
    bool before_fatal_failure = false;
    bool after_fatal_failure = false;
    bool fatal_failure_changed = false;
    bool fatal_failure_started = false;
    bool fatal_failure_cleared = false;
    bool before_swapchain_out_of_date = false;
    bool after_swapchain_out_of_date = false;
    bool swapchain_out_of_date_changed = false;
    bool swapchain_out_of_date_started = false;
    bool swapchain_out_of_date_cleared = false;
    bool before_suboptimal = false;
    bool after_suboptimal = false;
    bool suboptimal_changed = false;
    bool suboptimal_started = false;
    bool suboptimal_cleared = false;
    bool before_frame_completion_ready = false;
    bool after_frame_completion_ready = false;
    bool frame_completion_readiness_changed = false;
    bool frame_completion_became_ready = false;
    bool frame_completion_became_unready = false;
    bool before_completed = false;
    bool after_completed = false;
    bool completion_readiness_changed = false;
    bool completion_became_ready = false;
    bool completion_became_unready = false;
    std::size_t stage_count = 0;
    std::size_t stage_checked_change_count = 0;
    std::size_t stage_readiness_change_count = 0;
    std::size_t stage_ready_gain_count = 0;
    std::size_t stage_ready_loss_count = 0;
    std::vector<vulkan_native_frame_operation_stage_diff_diagnostics> stages;
};

vulkan_native_frame_operation_result build_vulkan_native_frame_operation_summary(
    const vulkan_native_frame_operation_request& request);

vulkan_native_frame_operation_diff_diagnostics build_vulkan_native_frame_operation_summary_diff(
    const vulkan_native_frame_operation_summary& before,
    const vulkan_native_frame_operation_summary& after);

vulkan_native_frame_operation_diff_diagnostics build_vulkan_native_frame_operation_result_diff(
    const vulkan_native_frame_operation_result& before,
    const vulkan_native_frame_operation_result& after);

enum class vulkan_native_frame_execution_step {
    acquire,
    record,
    submit,
    present,
};

std::string_view native_frame_execution_step_name(
    vulkan_native_frame_execution_step step);

enum class vulkan_native_frame_execution_decision {
    not_checked,
    execute,
    skip,
    fallback,
};

std::string_view native_frame_execution_decision_name(
    vulkan_native_frame_execution_decision decision);

struct vulkan_native_frame_operation_execution_request {
    vulkan_native_frame_operation_summary summary;
    vulkan_native_frame_operation_diff_diagnostics diff;
};

struct vulkan_native_frame_operation_step_execution_decision {
    vulkan_native_frame_execution_step step =
        vulkan_native_frame_execution_step::acquire;
    vulkan_native_frame_operation_stage operation_stage =
        vulkan_native_frame_operation_stage::acquire;
    vulkan_native_frame_execution_decision decision =
        vulkan_native_frame_execution_decision::not_checked;
    bool summary_checked = false;
    bool stage_checked = false;
    bool stage_ready = false;
    bool diff_checked = false;
    bool diff_changed = false;
    bool diff_became_ready = false;
    bool diff_became_blocked = false;
    bool blocked_by_previous_step = false;
    bool cpu_fallback_available = false;
    bool cpu_fallback_required = false;
    vulkan_backend_fallback_reason fallback_reason =
        vulkan_backend_fallback_reason::not_requested;
    std::string diagnostic;

    bool should_execute() const
    {
        return decision == vulkan_native_frame_execution_decision::execute;
    }

    bool should_fallback() const
    {
        return decision == vulkan_native_frame_execution_decision::fallback;
    }
};

struct vulkan_native_frame_operation_execution_plan {
    bool checked = false;
    bool summary_checked = false;
    bool diff_checked = false;
    bool diff_changed = false;
    bool native_execution_ready = false;
    bool cpu_fallback_required = false;
    bool skip_required = false;
    bool fatal_failure = false;
    bool recoverable_failure = false;
    bool swapchain_out_of_date = false;
    bool suboptimal = false;
    vulkan_native_frame_operation_status summary_status =
        vulkan_native_frame_operation_status::not_checked;
    vulkan_native_frame_operation_stage blocker_stage =
        vulkan_native_frame_operation_stage::not_started;
    vulkan_backend_fallback_reason fallback_reason =
        vulkan_backend_fallback_reason::not_requested;
    std::size_t step_count = 0;
    std::size_t execute_step_count = 0;
    std::size_t skip_step_count = 0;
    std::size_t fallback_step_count = 0;
    std::size_t not_checked_step_count = 0;
    std::vector<vulkan_native_frame_operation_step_execution_decision> steps;
    std::string diagnostic;

    bool should_execute_native_frame() const
    {
        return checked && native_execution_ready && execute_step_count == step_count;
    }

    bool should_use_cpu_fallback() const
    {
        return checked && cpu_fallback_required;
    }
};

vulkan_native_frame_operation_execution_plan build_vulkan_native_frame_operation_execution_plan(
    const vulkan_native_frame_operation_execution_request& request);

vulkan_native_frame_operation_execution_plan build_vulkan_native_frame_operation_execution_plan(
    const vulkan_native_frame_operation_summary& summary,
    const vulkan_native_frame_operation_diff_diagnostics& diff = {});

} // namespace quiz_vulkan::render::vulkan_backend

#endif
#endif
