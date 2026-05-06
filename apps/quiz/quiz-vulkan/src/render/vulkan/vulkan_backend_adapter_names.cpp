#include "render/vulkan/vulkan_backend_adapter.h"
#include "render/vulkan/vulkan_backend_loader.h"

namespace quiz_vulkan::render::vulkan_backend {

std::string_view fallback_reason_name(vulkan_backend_fallback_reason reason)
{
    switch (reason) {
    case vulkan_backend_fallback_reason::none:
        return "none";
    case vulkan_backend_fallback_reason::not_requested:
        return "not_requested";
    case vulkan_backend_fallback_reason::instance_unavailable:
        return "instance_unavailable";
    case vulkan_backend_fallback_reason::device_unavailable:
        return "device_unavailable";
    case vulkan_backend_fallback_reason::swapchain_unavailable:
        return "swapchain_unavailable";
    case vulkan_backend_fallback_reason::pipeline_unavailable:
        return "pipeline_unavailable";
    case vulkan_backend_fallback_reason::command_recorder_unavailable:
        return "command_recorder_unavailable";
    case vulkan_backend_fallback_reason::surface_unavailable:
        return "surface_unavailable";
    case vulkan_backend_fallback_reason::viewport_unavailable:
        return "viewport_unavailable";
    case vulkan_backend_fallback_reason::begin_frame_failed:
        return "begin_frame_failed";
    case vulkan_backend_fallback_reason::acquire_image_failed:
        return "acquire_image_failed";
    case vulkan_backend_fallback_reason::resource_binding_unavailable:
        return "resource_binding_unavailable";
    case vulkan_backend_fallback_reason::record_commands_failed:
        return "record_commands_failed";
    case vulkan_backend_fallback_reason::submit_frame_failed:
        return "submit_frame_failed";
    case vulkan_backend_fallback_reason::present_image_failed:
        return "present_image_failed";
    case vulkan_backend_fallback_reason::present_frame_failed:
        return "present_frame_failed";
    }

    return "unknown";
}

std::string_view frame_stage_name(vulkan_backend_frame_stage stage)
{
    switch (stage) {
    case vulkan_backend_frame_stage::not_started:
        return "not_started";
    case vulkan_backend_frame_stage::backend_attempted:
        return "backend_attempted";
    case vulkan_backend_frame_stage::lifecycle_ready:
        return "lifecycle_ready";
    case vulkan_backend_frame_stage::surface_extent_ready:
        return "surface_extent_ready";
    case vulkan_backend_frame_stage::frame_plan_ready:
        return "frame_plan_ready";
    case vulkan_backend_frame_stage::frame_begun:
        return "frame_begun";
    case vulkan_backend_frame_stage::commands_recorded:
        return "commands_recorded";
    case vulkan_backend_frame_stage::frame_submitted:
        return "frame_submitted";
    case vulkan_backend_frame_stage::frame_presented:
        return "frame_presented";
    }

    return "unknown";
}

std::string_view frame_lifecycle_step_name(vulkan_frame_lifecycle_step step)
{
    switch (step) {
    case vulkan_frame_lifecycle_step::acquire:
        return "acquire";
    case vulkan_frame_lifecycle_step::begin:
        return "begin";
    case vulkan_frame_lifecycle_step::render:
        return "render";
    case vulkan_frame_lifecycle_step::submit:
        return "submit";
    case vulkan_frame_lifecycle_step::present:
        return "present";
    }

    return "unknown";
}

std::string_view frame_lifecycle_order_status_name(vulkan_frame_lifecycle_order_status status)
{
    switch (status) {
    case vulkan_frame_lifecycle_order_status::not_started:
        return "not_started";
    case vulkan_frame_lifecycle_order_status::started:
        return "started";
    case vulkan_frame_lifecycle_order_status::completed:
        return "completed";
    case vulkan_frame_lifecycle_order_status::skipped:
        return "skipped";
    case vulkan_frame_lifecycle_order_status::failed:
        return "failed";
    case vulkan_frame_lifecycle_order_status::out_of_order:
        return "out_of_order";
    }

    return "unknown";
}

std::string_view frame_lifecycle_failure_classification_name(
    vulkan_frame_lifecycle_failure_classification classification)
{
    switch (classification) {
    case vulkan_frame_lifecycle_failure_classification::none:
        return "none";
    case vulkan_frame_lifecycle_failure_classification::recoverable:
        return "recoverable";
    case vulkan_frame_lifecycle_failure_classification::fatal:
        return "fatal";
    }

    return "unknown";
}

std::string_view swapchain_acquire_status_name(vulkan_swapchain_acquire_status status)
{
    switch (status) {
    case vulkan_swapchain_acquire_status::not_requested:
        return "not_requested";
    case vulkan_swapchain_acquire_status::acquired:
        return "acquired";
    case vulkan_swapchain_acquire_status::backpressured:
        return "backpressured";
    case vulkan_swapchain_acquire_status::failed:
        return "failed";
    }

    return "unknown";
}

std::string_view swapchain_present_status_name(vulkan_swapchain_present_status status)
{
    switch (status) {
    case vulkan_swapchain_present_status::not_requested:
        return "not_requested";
    case vulkan_swapchain_present_status::presented:
        return "presented";
    case vulkan_swapchain_present_status::failed:
        return "failed";
    }

    return "unknown";
}

std::string_view swapchain_present_mode_name(vulkan_swapchain_present_mode mode)
{
    switch (mode) {
    case vulkan_swapchain_present_mode::immediate:
        return "immediate";
    case vulkan_swapchain_present_mode::mailbox:
        return "mailbox";
    case vulkan_swapchain_present_mode::fifo:
        return "fifo";
    case vulkan_swapchain_present_mode::fifo_relaxed:
        return "fifo_relaxed";
    }

    return "unknown";
}

std::string_view frame_acquire_policy_status_name(vulkan_frame_acquire_policy_status status)
{
    switch (status) {
    case vulkan_frame_acquire_policy_status::not_checked:
        return "not_checked";
    case vulkan_frame_acquire_policy_status::not_requested:
        return "not_requested";
    case vulkan_frame_acquire_policy_status::acquired:
        return "acquired";
    case vulkan_frame_acquire_policy_status::backpressured:
        return "backpressured";
    case vulkan_frame_acquire_policy_status::failed:
        return "failed";
    }

    return "unknown";
}

std::string_view frame_present_result_status_name(vulkan_frame_present_result_status status)
{
    switch (status) {
    case vulkan_frame_present_result_status::not_checked:
        return "not_checked";
    case vulkan_frame_present_result_status::not_requested:
        return "not_requested";
    case vulkan_frame_present_result_status::image_presented:
        return "image_presented";
    case vulkan_frame_present_result_status::frame_presented:
        return "frame_presented";
    case vulkan_frame_present_result_status::image_failed:
        return "image_failed";
    case vulkan_frame_present_result_status::frame_failed:
        return "frame_failed";
    }

    return "unknown";
}

std::string_view frame_resource_kind_name(vulkan_frame_resource_kind kind)
{
    switch (kind) {
    case vulkan_frame_resource_kind::swapchain_image:
        return "swapchain_image";
    case vulkan_frame_resource_kind::command_buffer:
        return "command_buffer";
    case vulkan_frame_resource_kind::descriptor_set:
        return "descriptor_set";
    }

    return "unknown";
}

std::string_view frame_resource_release_stage_name(vulkan_frame_resource_release_stage stage)
{
    switch (stage) {
    case vulkan_frame_resource_release_stage::none:
        return "none";
    case vulkan_frame_resource_release_stage::after_present:
        return "after_present";
    case vulkan_frame_resource_release_stage::fallback_cleanup:
        return "fallback_cleanup";
    }

    return "unknown";
}

std::string_view descriptor_validation_status_name(vulkan_descriptor_validation_status status)
{
    switch (status) {
    case vulkan_descriptor_validation_status::not_checked:
        return "not_checked";
    case vulkan_descriptor_validation_status::valid:
        return "valid";
    case vulkan_descriptor_validation_status::missing_required_resource:
        return "missing_required_resource";
    case vulkan_descriptor_validation_status::duplicate_binding:
        return "duplicate_binding";
    case vulkan_descriptor_validation_status::invalid_layout:
        return "invalid_layout";
    }

    return "unknown";
}

std::string_view command_recorder_failure_stage_name(vulkan_command_recorder_failure_stage stage)
{
    switch (stage) {
    case vulkan_command_recorder_failure_stage::none:
        return "none";
    case vulkan_command_recorder_failure_stage::begin_recording:
        return "begin_recording";
    case vulkan_command_recorder_failure_stage::record_draw_batch:
        return "record_draw_batch";
    case vulkan_command_recorder_failure_stage::finish_recording:
        return "finish_recording";
    }

    return "unknown";
}

std::string_view command_recorder_gate_status_name(vulkan_command_recorder_gate_status status)
{
    switch (status) {
    case vulkan_command_recorder_gate_status::not_checked:
        return "not_checked";
    case vulkan_command_recorder_gate_status::allowed:
        return "allowed";
    case vulkan_command_recorder_gate_status::blocked_by_descriptor_validation:
        return "blocked_by_descriptor_validation";
    case vulkan_command_recorder_gate_status::blocked_by_resource_binding:
        return "blocked_by_resource_binding";
    case vulkan_command_recorder_gate_status::blocked_by_resource_registry:
        return "blocked_by_resource_registry";
    }

    return "unknown";
}

std::string_view command_buffer_recording_status_name(
    vulkan_command_buffer_recording_status status)
{
    switch (status) {
    case vulkan_command_buffer_recording_status::not_started:
        return "not_started";
    case vulkan_command_buffer_recording_status::recording:
        return "recording";
    case vulkan_command_buffer_recording_status::recorded:
        return "recorded";
    case vulkan_command_buffer_recording_status::failed:
        return "failed";
    }

    return "unknown";
}

std::string_view frame_submit_status_name(vulkan_frame_submit_status status)
{
    switch (status) {
    case vulkan_frame_submit_status::not_requested:
        return "not_requested";
    case vulkan_frame_submit_status::pending:
        return "pending";
    case vulkan_frame_submit_status::submitted:
        return "submitted";
    case vulkan_frame_submit_status::failed:
        return "failed";
    }

    return "unknown";
}

std::string_view shader_stage_name(vulkan_shader_stage stage)
{
    switch (stage) {
    case vulkan_shader_stage::vertex:
        return "vertex";
    case vulkan_shader_stage::fragment:
        return "fragment";
    }

    return "unknown";
}

std::string_view pipeline_lifecycle_stage_name(vulkan_pipeline_lifecycle_stage stage)
{
    switch (stage) {
    case vulkan_pipeline_lifecycle_stage::render_pass:
        return "render_pass";
    case vulkan_pipeline_lifecycle_stage::shader_stages:
        return "shader_stages";
    case vulkan_pipeline_lifecycle_stage::pipeline:
        return "pipeline";
    }

    return "unknown";
}

std::string_view pipeline_lifecycle_status_name(vulkan_pipeline_lifecycle_status status)
{
    switch (status) {
    case vulkan_pipeline_lifecycle_status::not_checked:
        return "not_checked";
    case vulkan_pipeline_lifecycle_status::ready:
        return "ready";
    case vulkan_pipeline_lifecycle_status::unavailable:
        return "unavailable";
    }

    return "unknown";
}

std::string_view loader_probe_status_name(vulkan_loader_probe_status status)
{
    switch (status) {
    case vulkan_loader_probe_status::not_checked:
        return "not_checked";
    case vulkan_loader_probe_status::available:
        return "available";
    case vulkan_loader_probe_status::library_missing:
        return "library_missing";
    case vulkan_loader_probe_status::required_symbol_missing:
        return "required_symbol_missing";
    }

    return "unknown";
}

} // namespace quiz_vulkan::render::vulkan_backend
