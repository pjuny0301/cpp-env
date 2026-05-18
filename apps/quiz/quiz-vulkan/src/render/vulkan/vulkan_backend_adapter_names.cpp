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
    case vulkan_backend_fallback_reason::render_pass_unavailable:
        return "render_pass_unavailable";
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

std::string_view native_entrypoint_stage_name(vulkan_native_entrypoint_stage stage)
{
    switch (stage) {
    case vulkan_native_entrypoint_stage::command_buffer_recording:
        return "command_buffer_recording";
    case vulkan_native_entrypoint_stage::queue_submit:
        return "queue_submit";
    case vulkan_native_entrypoint_stage::swapchain_create:
        return "swapchain_create";
    case vulkan_native_entrypoint_stage::swapchain_destroy:
        return "swapchain_destroy";
    case vulkan_native_entrypoint_stage::swapchain_images:
        return "swapchain_images";
    case vulkan_native_entrypoint_stage::swapchain_acquire:
        return "swapchain_acquire";
    case vulkan_native_entrypoint_stage::image_view_create:
        return "image_view_create";
    case vulkan_native_entrypoint_stage::image_view_destroy:
        return "image_view_destroy";
    case vulkan_native_entrypoint_stage::framebuffer_create:
        return "framebuffer_create";
    case vulkan_native_entrypoint_stage::framebuffer_destroy:
        return "framebuffer_destroy";
    case vulkan_native_entrypoint_stage::queue_present:
        return "queue_present";
    }

    return "unknown";
}

std::string_view native_function_table_status_name(
    vulkan_native_function_table_status status)
{
    switch (status) {
    case vulkan_native_function_table_status::not_checked:
        return "not_checked";
    case vulkan_native_function_table_status::ready:
        return "ready";
    case vulkan_native_function_table_status::loader_unavailable:
        return "loader_unavailable";
    case vulkan_native_function_table_status::required_extension_unavailable:
        return "required_extension_unavailable";
    case vulkan_native_function_table_status::missing_command_buffer_recording_symbol:
        return "missing_command_buffer_recording_symbol";
    case vulkan_native_function_table_status::missing_queue_submit_symbol:
        return "missing_queue_submit_symbol";
    case vulkan_native_function_table_status::missing_swapchain_create_symbol:
        return "missing_swapchain_create_symbol";
    case vulkan_native_function_table_status::missing_swapchain_destroy_symbol:
        return "missing_swapchain_destroy_symbol";
    case vulkan_native_function_table_status::missing_swapchain_images_symbol:
        return "missing_swapchain_images_symbol";
    case vulkan_native_function_table_status::missing_swapchain_acquire_symbol:
        return "missing_swapchain_acquire_symbol";
    case vulkan_native_function_table_status::missing_image_view_create_symbol:
        return "missing_image_view_create_symbol";
    case vulkan_native_function_table_status::missing_image_view_destroy_symbol:
        return "missing_image_view_destroy_symbol";
    case vulkan_native_function_table_status::missing_framebuffer_create_symbol:
        return "missing_framebuffer_create_symbol";
    case vulkan_native_function_table_status::missing_framebuffer_destroy_symbol:
        return "missing_framebuffer_destroy_symbol";
    case vulkan_native_function_table_status::missing_queue_present_symbol:
        return "missing_queue_present_symbol";
    }

    return "unknown";
}

std::string_view sdk_header_probe_status_name(vulkan_sdk_header_probe_status status)
{
    switch (status) {
    case vulkan_sdk_header_probe_status::not_checked:
        return "not_checked";
    case vulkan_sdk_header_probe_status::available:
        return "available";
    case vulkan_sdk_header_probe_status::unavailable:
        return "unavailable";
    }

    return "unknown";
}

std::string_view sdk_capability_status_name(vulkan_sdk_capability_status status)
{
    switch (status) {
    case vulkan_sdk_capability_status::not_checked:
        return "not_checked";
    case vulkan_sdk_capability_status::ready:
        return "ready";
    case vulkan_sdk_capability_status::headers_unavailable:
        return "headers_unavailable";
    case vulkan_sdk_capability_status::api_version_unavailable:
        return "api_version_unavailable";
    case vulkan_sdk_capability_status::api_version_too_old:
        return "api_version_too_old";
    case vulkan_sdk_capability_status::missing_required_extension:
        return "missing_required_extension";
    case vulkan_sdk_capability_status::native_function_table_unavailable:
        return "native_function_table_unavailable";
    }

    return "unknown";
}

std::string_view sdk_adapter_fallback_status_name(
    vulkan_sdk_adapter_fallback_status status)
{
    switch (status) {
    case vulkan_sdk_adapter_fallback_status::none:
        return "none";
    case vulkan_sdk_adapter_fallback_status::headers_unavailable:
        return "headers_unavailable";
    case vulkan_sdk_adapter_fallback_status::api_version_unavailable:
        return "api_version_unavailable";
    case vulkan_sdk_adapter_fallback_status::extension_unavailable:
        return "extension_unavailable";
    case vulkan_sdk_adapter_fallback_status::native_function_table_unavailable:
        return "native_function_table_unavailable";
    }

    return "unknown";
}

std::string_view sdk_native_path_status_name(vulkan_sdk_native_path_status status)
{
    switch (status) {
    case vulkan_sdk_native_path_status::not_checked:
        return "not_checked";
    case vulkan_sdk_native_path_status::ready:
        return "ready";
    case vulkan_sdk_native_path_status::sdk_missing:
        return "sdk_missing";
    case vulkan_sdk_native_path_status::version_mismatch:
        return "version_mismatch";
    case vulkan_sdk_native_path_status::extension_missing:
        return "extension_missing";
    case vulkan_sdk_native_path_status::function_table_blocked:
        return "function_table_blocked";
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

std::string_view command_packet_category_name(vulkan_command_packet_category category)
{
    switch (category) {
    case vulkan_command_packet_category::rect:
        return "rect";
    case vulkan_command_packet_category::text:
        return "text";
    case vulkan_command_packet_category::image:
        return "image";
    case vulkan_command_packet_category::debug_bounds:
        return "debug_bounds";
    }

    return "unknown";
}

std::string_view command_packet_bridge_status_name(
    vulkan_command_packet_bridge_status status)
{
    switch (status) {
    case vulkan_command_packet_bridge_status::not_checked:
        return "not_checked";
    case vulkan_command_packet_bridge_status::ready:
        return "ready";
    case vulkan_command_packet_bridge_status::pipeline_unavailable:
        return "pipeline_unavailable";
    case vulkan_command_packet_bridge_status::resource_binding_unavailable:
        return "resource_binding_unavailable";
    }

    return "unknown";
}

std::string_view command_packet_execution_status_name(
    vulkan_command_packet_execution_status status)
{
    switch (status) {
    case vulkan_command_packet_execution_status::not_checked:
        return "not_checked";
    case vulkan_command_packet_execution_status::completed:
        return "completed";
    case vulkan_command_packet_execution_status::packet_bridge_unavailable:
        return "packet_bridge_unavailable";
    case vulkan_command_packet_execution_status::begin_failed:
        return "begin_failed";
    case vulkan_command_packet_execution_status::packet_failed:
        return "packet_failed";
    case vulkan_command_packet_execution_status::end_failed:
        return "end_failed";
    }

    return "unknown";
}

std::string_view command_packet_execution_event_name(
    vulkan_command_packet_execution_event event)
{
    switch (event) {
    case vulkan_command_packet_execution_event::begin:
        return "begin";
    case vulkan_command_packet_execution_event::packet:
        return "packet";
    case vulkan_command_packet_execution_event::end:
        return "end";
    }

    return "unknown";
}

std::string_view native_command_packet_execution_status_name(
    vulkan_native_command_packet_execution_status status)
{
    switch (status) {
    case vulkan_native_command_packet_execution_status::not_checked:
        return "not_checked";
    case vulkan_native_command_packet_execution_status::completed:
        return "completed";
    case vulkan_native_command_packet_execution_status::packet_bridge_unavailable:
        return "packet_bridge_unavailable";
    case vulkan_native_command_packet_execution_status::native_function_table_unavailable:
        return "native_function_table_unavailable";
    case vulkan_native_command_packet_execution_status::native_command_symbol_unavailable:
        return "native_command_symbol_unavailable";
    case vulkan_native_command_packet_execution_status::command_buffer_unavailable:
        return "command_buffer_unavailable";
    case vulkan_native_command_packet_execution_status::pipeline_unavailable:
        return "pipeline_unavailable";
    case vulkan_native_command_packet_execution_status::pipeline_layout_unavailable:
        return "pipeline_layout_unavailable";
    case vulkan_native_command_packet_execution_status::descriptor_sets_unavailable:
        return "descriptor_sets_unavailable";
    case vulkan_native_command_packet_execution_status::descriptor_payloads_unavailable:
        return "descriptor_payloads_unavailable";
    case vulkan_native_command_packet_execution_status::invalid_packet_data:
        return "invalid_packet_data";
    }

    return "unknown";
}

std::string_view native_command_packet_call_kind_name(
    vulkan_native_command_packet_call_kind kind)
{
    switch (kind) {
    case vulkan_native_command_packet_call_kind::bind_pipeline:
        return "bind_pipeline";
    case vulkan_native_command_packet_call_kind::bind_descriptor_sets:
        return "bind_descriptor_sets";
    case vulkan_native_command_packet_call_kind::set_viewport:
        return "set_viewport";
    case vulkan_native_command_packet_call_kind::set_scissor:
        return "set_scissor";
    case vulkan_native_command_packet_call_kind::draw:
        return "draw";
    }

    return "unknown";
}

std::string_view native_descriptor_set_allocation_status_name(
    vulkan_native_descriptor_set_allocation_status status)
{
    switch (status) {
    case vulkan_native_descriptor_set_allocation_status::not_checked:
        return "not_checked";
    case vulkan_native_descriptor_set_allocation_status::ready:
        return "ready";
    case vulkan_native_descriptor_set_allocation_status::packet_bridge_unavailable:
        return "packet_bridge_unavailable";
    case vulkan_native_descriptor_set_allocation_status::resource_binding_unavailable:
        return "resource_binding_unavailable";
    case vulkan_native_descriptor_set_allocation_status::resource_binding_mismatch:
        return "resource_binding_mismatch";
    case vulkan_native_descriptor_set_allocation_status::image_materialization_unavailable:
        return "image_materialization_unavailable";
    case vulkan_native_descriptor_set_allocation_status::image_materialization_blocked:
        return "image_materialization_blocked";
    case vulkan_native_descriptor_set_allocation_status::image_materialization_mismatch:
        return "image_materialization_mismatch";
    }

    return "unknown";
}

std::string_view native_descriptor_write_payload_status_name(
    vulkan_native_descriptor_write_payload_status status)
{
    switch (status) {
    case vulkan_native_descriptor_write_payload_status::not_checked:
        return "not_checked";
    case vulkan_native_descriptor_write_payload_status::ready:
        return "ready";
    case vulkan_native_descriptor_write_payload_status::packet_bridge_unavailable:
        return "packet_bridge_unavailable";
    case vulkan_native_descriptor_write_payload_status::descriptor_set_allocation_unavailable:
        return "descriptor_set_allocation_unavailable";
    case vulkan_native_descriptor_write_payload_status::image_descriptor_resource_unavailable:
        return "image_descriptor_resource_unavailable";
    case vulkan_native_descriptor_write_payload_status::image_descriptor_texture_mismatch:
        return "image_descriptor_texture_mismatch";
    case vulkan_native_descriptor_write_payload_status::image_descriptor_sampler_mismatch:
        return "image_descriptor_sampler_mismatch";
    case vulkan_native_descriptor_write_payload_status::image_descriptor_resource_incomplete:
        return "image_descriptor_resource_incomplete";
    case vulkan_native_descriptor_write_payload_status::duplicate_payload:
        return "duplicate_payload";
    }

    return "unknown";
}

std::string_view scoped_command_packet_execution_status_name(
    vulkan_scoped_command_packet_execution_status status)
{
    switch (status) {
    case vulkan_scoped_command_packet_execution_status::not_checked:
        return "not_checked";
    case vulkan_scoped_command_packet_execution_status::completed:
        return "completed";
    case vulkan_scoped_command_packet_execution_status::render_pass_scope_unavailable:
        return "render_pass_scope_unavailable";
    case vulkan_scoped_command_packet_execution_status::packet_bridge_unavailable:
        return "packet_bridge_unavailable";
    case vulkan_scoped_command_packet_execution_status::begin_failed:
        return "begin_failed";
    case vulkan_scoped_command_packet_execution_status::packet_failed:
        return "packet_failed";
    case vulkan_scoped_command_packet_execution_status::end_failed:
        return "end_failed";
    }

    return "unknown";
}

std::string_view command_recorder_operation_plan_status_name(
    vulkan_command_recorder_operation_plan_status status)
{
    switch (status) {
    case vulkan_command_recorder_operation_plan_status::not_checked:
        return "not_checked";
    case vulkan_command_recorder_operation_plan_status::ready:
        return "ready";
    case vulkan_command_recorder_operation_plan_status::packet_bridge_unavailable:
        return "packet_bridge_unavailable";
    case vulkan_command_recorder_operation_plan_status::packet_execution_unavailable:
        return "packet_execution_unavailable";
    }

    return "unknown";
}

std::string_view native_descriptor_payload_command_recording_status_name(
    vulkan_native_descriptor_payload_command_recording_status status)
{
    switch (status) {
    case vulkan_native_descriptor_payload_command_recording_status::not_checked:
        return "not_checked";
    case vulkan_native_descriptor_payload_command_recording_status::ready:
        return "ready";
    case vulkan_native_descriptor_payload_command_recording_status::packet_bridge_unavailable:
        return "packet_bridge_unavailable";
    case vulkan_native_descriptor_payload_command_recording_status::
        descriptor_set_allocation_unavailable:
        return "descriptor_set_allocation_unavailable";
    case vulkan_native_descriptor_payload_command_recording_status::
        descriptor_write_payload_unavailable:
        return "descriptor_write_payload_unavailable";
    case vulkan_native_descriptor_payload_command_recording_status::operation_plan_unavailable:
        return "operation_plan_unavailable";
    case vulkan_native_descriptor_payload_command_recording_status::
        missing_descriptor_write_payload:
        return "missing_descriptor_write_payload";
    case vulkan_native_descriptor_payload_command_recording_status::
        duplicate_descriptor_write_payload:
        return "duplicate_descriptor_write_payload";
    case vulkan_native_descriptor_payload_command_recording_status::
        incomplete_descriptor_write_payload:
        return "incomplete_descriptor_write_payload";
    case vulkan_native_descriptor_payload_command_recording_status::
        descriptor_write_payload_mismatch:
        return "descriptor_write_payload_mismatch";
    }

    return "unknown";
}

std::string_view command_recorder_operation_kind_name(
    vulkan_command_recorder_operation_kind kind)
{
    switch (kind) {
    case vulkan_command_recorder_operation_kind::draw_rect:
        return "draw_rect";
    case vulkan_command_recorder_operation_kind::draw_text:
        return "draw_text";
    case vulkan_command_recorder_operation_kind::draw_image:
        return "draw_image";
    case vulkan_command_recorder_operation_kind::draw_debug_bounds:
        return "draw_debug_bounds";
    }

    return "unknown";
}

std::string_view command_buffer_record_result_status_name(
    vulkan_command_buffer_record_result_status status)
{
    switch (status) {
    case vulkan_command_buffer_record_result_status::not_checked:
        return "not_checked";
    case vulkan_command_buffer_record_result_status::recorded:
        return "recorded";
    case vulkan_command_buffer_record_result_status::operation_plan_unavailable:
        return "operation_plan_unavailable";
    case vulkan_command_buffer_record_result_status::native_entrypoint_unavailable:
        return "native_entrypoint_unavailable";
    case vulkan_command_buffer_record_result_status::command_buffer_unavailable:
        return "command_buffer_unavailable";
    case vulkan_command_buffer_record_result_status::begin_failed:
        return "begin_failed";
    case vulkan_command_buffer_record_result_status::operation_failed:
        return "operation_failed";
    case vulkan_command_buffer_record_result_status::end_failed:
        return "end_failed";
    }

    return "unknown";
}

std::string_view command_buffer_record_event_kind_name(
    vulkan_command_buffer_record_event_kind kind)
{
    switch (kind) {
    case vulkan_command_buffer_record_event_kind::begin:
        return "begin";
    case vulkan_command_buffer_record_event_kind::operation:
        return "operation";
    case vulkan_command_buffer_record_event_kind::end:
        return "end";
    }

    return "unknown";
}

std::string_view submit_batch_plan_status_name(vulkan_submit_batch_plan_status status)
{
    switch (status) {
    case vulkan_submit_batch_plan_status::not_checked:
        return "not_checked";
    case vulkan_submit_batch_plan_status::ready:
        return "ready";
    case vulkan_submit_batch_plan_status::command_buffer_recording_unavailable:
        return "command_buffer_recording_unavailable";
    case vulkan_submit_batch_plan_status::native_queue_submit_unavailable:
        return "native_queue_submit_unavailable";
    case vulkan_submit_batch_plan_status::command_submit_unavailable:
        return "command_submit_unavailable";
    case vulkan_submit_batch_plan_status::command_buffer_unavailable:
        return "command_buffer_unavailable";
    case vulkan_submit_batch_plan_status::sync_primitives_unavailable:
        return "sync_primitives_unavailable";
    case vulkan_submit_batch_plan_status::submit_queue_unavailable:
        return "submit_queue_unavailable";
    case vulkan_submit_batch_plan_status::present_target_unavailable:
        return "present_target_unavailable";
    case vulkan_submit_batch_plan_status::submit_failed_recoverable:
        return "submit_failed_recoverable";
    case vulkan_submit_batch_plan_status::submit_failed_fatal:
        return "submit_failed_fatal";
    }

    return "unknown";
}

std::string_view submit_batch_sync_intent_kind_name(
    vulkan_submit_batch_sync_intent_kind kind)
{
    switch (kind) {
    case vulkan_submit_batch_sync_intent_kind::wait_image_available:
        return "wait_image_available";
    case vulkan_submit_batch_sync_intent_kind::signal_render_finished:
        return "signal_render_finished";
    case vulkan_submit_batch_sync_intent_kind::signal_frame_fence:
        return "signal_frame_fence";
    }

    return "unknown";
}

std::string_view present_completion_plan_status_name(
    vulkan_present_completion_plan_status status)
{
    switch (status) {
    case vulkan_present_completion_plan_status::not_checked:
        return "not_checked";
    case vulkan_present_completion_plan_status::ready:
        return "ready";
    case vulkan_present_completion_plan_status::submit_batch_unavailable:
        return "submit_batch_unavailable";
    case vulkan_present_completion_plan_status::native_queue_present_unavailable:
        return "native_queue_present_unavailable";
    case vulkan_present_completion_plan_status::present_request_unavailable:
        return "present_request_unavailable";
    case vulkan_present_completion_plan_status::present_adapter_unavailable:
        return "present_adapter_unavailable";
    case vulkan_present_completion_plan_status::submit_failed_recoverable:
        return "submit_failed_recoverable";
    case vulkan_present_completion_plan_status::submit_failed_fatal:
        return "submit_failed_fatal";
    case vulkan_present_completion_plan_status::present_failed_recoverable:
        return "present_failed_recoverable";
    case vulkan_present_completion_plan_status::present_failed_fatal:
        return "present_failed_fatal";
    }

    return "unknown";
}

std::string_view frame_completion_status_name(vulkan_frame_completion_status status)
{
    switch (status) {
    case vulkan_frame_completion_status::not_checked:
        return "not_checked";
    case vulkan_frame_completion_status::ready_for_present:
        return "ready_for_present";
    case vulkan_frame_completion_status::completed:
        return "completed";
    case vulkan_frame_completion_status::submit_unavailable:
        return "submit_unavailable";
    case vulkan_frame_completion_status::present_unavailable:
        return "present_unavailable";
    case vulkan_frame_completion_status::submit_failed_recoverable:
        return "submit_failed_recoverable";
    case vulkan_frame_completion_status::submit_failed_fatal:
        return "submit_failed_fatal";
    case vulkan_frame_completion_status::present_failed_recoverable:
        return "present_failed_recoverable";
    case vulkan_frame_completion_status::present_failed_fatal:
        return "present_failed_fatal";
    }

    return "unknown";
}

std::string_view native_queue_present_operation_status_name(
    vulkan_native_queue_present_operation_status status)
{
    switch (status) {
    case vulkan_native_queue_present_operation_status::not_checked:
        return "not_checked";
    case vulkan_native_queue_present_operation_status::ready:
        return "ready";
    case vulkan_native_queue_present_operation_status::acquire_operation_unavailable:
        return "acquire_operation_unavailable";
    case vulkan_native_queue_present_operation_status::submit_batch_unavailable:
        return "submit_batch_unavailable";
    case vulkan_native_queue_present_operation_status::present_completion_unavailable:
        return "present_completion_unavailable";
    case vulkan_native_queue_present_operation_status::native_entrypoints_unavailable:
        return "native_entrypoints_unavailable";
    case vulkan_native_queue_present_operation_status::required_extension_unavailable:
        return "required_extension_unavailable";
    case vulkan_native_queue_present_operation_status::missing_queue_present_symbol:
        return "missing_queue_present_symbol";
    case vulkan_native_queue_present_operation_status::present_queue_unavailable:
        return "present_queue_unavailable";
    case vulkan_native_queue_present_operation_status::swapchain_unavailable:
        return "swapchain_unavailable";
    case vulkan_native_queue_present_operation_status::acquired_image_unavailable:
        return "acquired_image_unavailable";
    case vulkan_native_queue_present_operation_status::submitted_frame_unavailable:
        return "submitted_frame_unavailable";
    case vulkan_native_queue_present_operation_status::present_result_unavailable:
        return "present_result_unavailable";
    case vulkan_native_queue_present_operation_status::out_of_date:
        return "out_of_date";
    case vulkan_native_queue_present_operation_status::suboptimal:
        return "suboptimal";
    case vulkan_native_queue_present_operation_status::present_failed_recoverable:
        return "present_failed_recoverable";
    case vulkan_native_queue_present_operation_status::present_failed_fatal:
        return "present_failed_fatal";
    }

    return "unknown";
}

std::string_view native_frame_operation_stage_name(
    vulkan_native_frame_operation_stage stage)
{
    switch (stage) {
    case vulkan_native_frame_operation_stage::not_started:
        return "not_started";
    case vulkan_native_frame_operation_stage::native_function_table:
        return "native_function_table";
    case vulkan_native_frame_operation_stage::swapchain_create:
        return "swapchain_create";
    case vulkan_native_frame_operation_stage::swapchain_images:
        return "swapchain_images";
    case vulkan_native_frame_operation_stage::acquire:
        return "acquire";
    case vulkan_native_frame_operation_stage::command_recording:
        return "command_recording";
    case vulkan_native_frame_operation_stage::submit:
        return "submit";
    case vulkan_native_frame_operation_stage::present:
        return "present";
    case vulkan_native_frame_operation_stage::frame_completion:
        return "frame_completion";
    }

    return "unknown";
}

std::string_view native_frame_operation_status_name(
    vulkan_native_frame_operation_status status)
{
    switch (status) {
    case vulkan_native_frame_operation_status::not_checked:
        return "not_checked";
    case vulkan_native_frame_operation_status::ready:
        return "ready";
    case vulkan_native_frame_operation_status::native_function_table_unavailable:
        return "native_function_table_unavailable";
    case vulkan_native_frame_operation_status::swapchain_create_unavailable:
        return "swapchain_create_unavailable";
    case vulkan_native_frame_operation_status::swapchain_images_unavailable:
        return "swapchain_images_unavailable";
    case vulkan_native_frame_operation_status::acquire_unavailable:
        return "acquire_unavailable";
    case vulkan_native_frame_operation_status::command_recording_unavailable:
        return "command_recording_unavailable";
    case vulkan_native_frame_operation_status::submit_unavailable:
        return "submit_unavailable";
    case vulkan_native_frame_operation_status::present_unavailable:
        return "present_unavailable";
    case vulkan_native_frame_operation_status::frame_completion_unavailable:
        return "frame_completion_unavailable";
    case vulkan_native_frame_operation_status::recoverable_failure:
        return "recoverable_failure";
    case vulkan_native_frame_operation_status::fatal_failure:
        return "fatal_failure";
    }

    return "unknown";
}

std::string_view native_frame_execution_step_name(
    vulkan_native_frame_execution_step step)
{
    switch (step) {
    case vulkan_native_frame_execution_step::acquire:
        return "acquire";
    case vulkan_native_frame_execution_step::record:
        return "record";
    case vulkan_native_frame_execution_step::submit:
        return "submit";
    case vulkan_native_frame_execution_step::present:
        return "present";
    }

    return "unknown";
}

std::string_view native_frame_execution_decision_name(
    vulkan_native_frame_execution_decision decision)
{
    switch (decision) {
    case vulkan_native_frame_execution_decision::not_checked:
        return "not_checked";
    case vulkan_native_frame_execution_decision::execute:
        return "execute";
    case vulkan_native_frame_execution_decision::skip:
        return "skip";
    case vulkan_native_frame_execution_decision::fallback:
        return "fallback";
    }

    return "unknown";
}

std::string_view frame_pipeline_handoff_status_name(
    vulkan_backend_frame_pipeline_handoff_status status)
{
    switch (status) {
    case vulkan_backend_frame_pipeline_handoff_status::not_checked:
        return "not_checked";
    case vulkan_backend_frame_pipeline_handoff_status::ready:
        return "ready";
    case vulkan_backend_frame_pipeline_handoff_status::instance_unavailable:
        return "instance_unavailable";
    case vulkan_backend_frame_pipeline_handoff_status::device_unavailable:
        return "device_unavailable";
    case vulkan_backend_frame_pipeline_handoff_status::swapchain_unavailable:
        return "swapchain_unavailable";
    case vulkan_backend_frame_pipeline_handoff_status::render_pass_unavailable:
        return "render_pass_unavailable";
    case vulkan_backend_frame_pipeline_handoff_status::surface_unavailable:
        return "surface_unavailable";
    case vulkan_backend_frame_pipeline_handoff_status::viewport_unavailable:
        return "viewport_unavailable";
    case vulkan_backend_frame_pipeline_handoff_status::pipeline_unavailable:
        return "pipeline_unavailable";
    case vulkan_backend_frame_pipeline_handoff_status::resource_binding_unavailable:
        return "resource_binding_unavailable";
    case vulkan_backend_frame_pipeline_handoff_status::command_recording_unavailable:
        return "command_recording_unavailable";
    case vulkan_backend_frame_pipeline_handoff_status::frame_lifecycle_unavailable:
        return "frame_lifecycle_unavailable";
    case vulkan_backend_frame_pipeline_handoff_status::submit_unavailable:
        return "submit_unavailable";
    case vulkan_backend_frame_pipeline_handoff_status::present_unavailable:
        return "present_unavailable";
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
    case vulkan_swapchain_acquire_status::timeout:
        return "timeout";
    case vulkan_swapchain_acquire_status::out_of_date:
        return "out_of_date";
    case vulkan_swapchain_acquire_status::suboptimal:
        return "suboptimal";
    case vulkan_swapchain_acquire_status::failed:
        return "failed";
    case vulkan_swapchain_acquire_status::error:
        return "error";
    }

    return "unknown";
}

std::string_view swapchain_image_acquire_plan_status_name(
    vulkan_swapchain_image_acquire_plan_status status)
{
    switch (status) {
    case vulkan_swapchain_image_acquire_plan_status::not_checked:
        return "not_checked";
    case vulkan_swapchain_image_acquire_plan_status::not_requested:
        return "not_requested";
    case vulkan_swapchain_image_acquire_plan_status::ready:
        return "ready";
    case vulkan_swapchain_image_acquire_plan_status::lifecycle_unavailable:
        return "lifecycle_unavailable";
    case vulkan_swapchain_image_acquire_plan_status::swapchain_unavailable:
        return "swapchain_unavailable";
    case vulkan_swapchain_image_acquire_plan_status::sync_unavailable:
        return "sync_unavailable";
    case vulkan_swapchain_image_acquire_plan_status::no_images_available:
        return "no_images_available";
    case vulkan_swapchain_image_acquire_plan_status::backpressured:
        return "backpressured";
    case vulkan_swapchain_image_acquire_plan_status::timeout:
        return "timeout";
    case vulkan_swapchain_image_acquire_plan_status::out_of_date:
        return "out_of_date";
    case vulkan_swapchain_image_acquire_plan_status::suboptimal:
        return "suboptimal";
    case vulkan_swapchain_image_acquire_plan_status::error:
        return "error";
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
    case vulkan_swapchain_present_status::out_of_date:
        return "out_of_date";
    case vulkan_swapchain_present_status::suboptimal:
        return "suboptimal";
    case vulkan_swapchain_present_status::failed:
        return "failed";
    case vulkan_swapchain_present_status::error:
        return "error";
    }

    return "unknown";
}

std::string_view swapchain_recreate_policy_action_name(
    vulkan_swapchain_recreate_policy_action action)
{
    switch (action) {
    case vulkan_swapchain_recreate_policy_action::not_checked:
        return "not_checked";
    case vulkan_swapchain_recreate_policy_action::keep_rendering:
        return "keep_rendering";
    case vulkan_swapchain_recreate_policy_action::recreate_immediately:
        return "recreate_immediately";
    case vulkan_swapchain_recreate_policy_action::recreate_after_frame:
        return "recreate_after_frame";
    case vulkan_swapchain_recreate_policy_action::skip_submit:
        return "skip_submit";
    case vulkan_swapchain_recreate_policy_action::fatal_error:
        return "fatal_error";
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
    case vulkan_shader_stage::compute:
        return "compute";
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

std::string_view loader_candidate_status_name(vulkan_loader_candidate_status status)
{
    switch (status) {
    case vulkan_loader_candidate_status::not_checked:
        return "not_checked";
    case vulkan_loader_candidate_status::library_missing:
        return "library_missing";
    case vulkan_loader_candidate_status::required_symbol_missing:
        return "required_symbol_missing";
    case vulkan_loader_candidate_status::usable:
        return "usable";
    }

    return "unknown";
}

std::string_view loader_readiness_status_name(vulkan_loader_readiness_status status)
{
    switch (status) {
    case vulkan_loader_readiness_status::not_checked:
        return "not_checked";
    case vulkan_loader_readiness_status::ready:
        return "ready";
    case vulkan_loader_readiness_status::library_missing:
        return "library_missing";
    case vulkan_loader_readiness_status::required_symbol_missing:
        return "required_symbol_missing";
    }

    return "unknown";
}

} // namespace quiz_vulkan::render::vulkan_backend
