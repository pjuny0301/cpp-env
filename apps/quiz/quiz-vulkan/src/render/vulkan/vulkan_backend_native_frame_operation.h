#ifndef QUIZ_VULKAN_BACKEND_NATIVE_FRAME_OPERATION_DEPENDENCIES_READY
#include "render/vulkan/vulkan_backend_adapter.h"
#else
#ifndef QUIZ_VULKAN_BACKEND_NATIVE_FRAME_OPERATION_H
#define QUIZ_VULKAN_BACKEND_NATIVE_FRAME_OPERATION_H

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

vulkan_native_frame_operation_result build_vulkan_native_frame_operation_summary(
    const vulkan_native_frame_operation_request& request);

} // namespace quiz_vulkan::render::vulkan_backend

#endif
#endif
