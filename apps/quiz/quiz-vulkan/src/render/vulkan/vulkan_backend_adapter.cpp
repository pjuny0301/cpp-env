#include "render/vulkan/vulkan_backend_adapter.h"

namespace quiz_vulkan::render::vulkan_backend {
namespace {

bool has_visible_area(const render_rect& rect)
{
    return rect.width > 0.0f && rect.height > 0.0f;
}

vulkan_backend_fallback_reason first_unready_reason(
    const vulkan_backend_lifecycle_readiness& lifecycle)
{
    if (!lifecycle.instance_ready) {
        return vulkan_backend_fallback_reason::instance_unavailable;
    }
    if (!lifecycle.device_ready) {
        return vulkan_backend_fallback_reason::device_unavailable;
    }
    if (!lifecycle.swapchain_ready) {
        return vulkan_backend_fallback_reason::swapchain_unavailable;
    }
    if (!lifecycle.pipeline_ready) {
        return vulkan_backend_fallback_reason::pipeline_unavailable;
    }
    if (!lifecycle.command_recorder_ready) {
        return vulkan_backend_fallback_reason::command_recorder_unavailable;
    }

    return vulkan_backend_fallback_reason::none;
}

vulkan_recorded_draw_batch make_recorded_draw_batch(
    const vulkan_draw_batch& batch,
    std::size_t recording_index)
{
    return vulkan_recorded_draw_batch{
        .kind = batch.kind,
        .command_index = batch.command_index,
        .recording_index = recording_index,
        .bounds = batch.bounds,
        .clipped_bounds = batch.clipped_bounds,
        .scissor = batch.scissor,
    };
}

} // namespace

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
    case vulkan_backend_fallback_reason::record_commands_failed:
        return "record_commands_failed";
    case vulkan_backend_fallback_reason::submit_frame_failed:
        return "submit_frame_failed";
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

vulkan_backend_lifecycle_readiness null_vulkan_backend_device::current_lifecycle_readiness() const
{
    return {};
}

vulkan_surface_extent null_vulkan_backend_device::current_surface_extent() const
{
    return {};
}

bool null_vulkan_backend_device::begin_frame(vulkan_surface_extent surface)
{
    static_cast<void>(surface);
    return false;
}

bool null_vulkan_backend_device::record_frame_commands(const vulkan_frame_plan& plan)
{
    static_cast<void>(plan);
    return false;
}

bool null_vulkan_backend_device::submit_frame()
{
    return false;
}

bool null_vulkan_backend_device::present_frame()
{
    return false;
}

diagnostic_vulkan_command_recorder::diagnostic_vulkan_command_recorder(bool ready)
    : ready_(ready)
{
    state_.ready = ready_;
}

bool diagnostic_vulkan_command_recorder::begin_recording(
    vulkan_surface_extent surface,
    std::size_t planned_batch_count)
{
    state_ = {};
    state_.ready = ready_;
    state_.planned_batch_count = planned_batch_count;
    if (!ready_ || !surface.valid()) {
        return false;
    }

    state_.frame_open = true;
    return true;
}

bool diagnostic_vulkan_command_recorder::record_draw_batch(const vulkan_draw_batch& batch)
{
    if (!state_.ready || !state_.frame_open || state_.command_buffer_recorded) {
        return false;
    }

    state_.recorded_batches.push_back(make_recorded_draw_batch(batch, state_.recorded_batch_count));
    state_.recorded_batch_count = state_.recorded_batches.size();
    return state_.recorded_batch_count <= state_.planned_batch_count;
}

bool diagnostic_vulkan_command_recorder::finish_recording()
{
    if (!state_.ready || !state_.frame_open || state_.command_buffer_recorded) {
        return false;
    }
    if (state_.recorded_batch_count != state_.planned_batch_count) {
        return false;
    }

    state_.command_buffer_recorded = true;
    return true;
}

const vulkan_backend_command_recorder_state& diagnostic_vulkan_command_recorder::recorder_state() const
{
    return state_;
}

vulkan_backend_frame_result submit_vulkan_backend_frame(
    vulkan_backend_device_interface& device,
    const render_draw_list& draw_list,
    render_rect viewport)
{
    diagnostic_vulkan_command_recorder command_recorder;
    return submit_vulkan_backend_frame(device, command_recorder, draw_list, viewport);
}

vulkan_backend_frame_result submit_vulkan_backend_frame(
    vulkan_backend_device_interface& device,
    vulkan_command_recorder_interface& command_recorder,
    const render_draw_list& draw_list,
    render_rect viewport)
{
    vulkan_backend_frame_result result;
    result.attempted = true;
    result.reached_stage = vulkan_backend_frame_stage::backend_attempted;
    result.lifecycle = device.current_lifecycle_readiness();
    result.command_recorder.ready = result.lifecycle.command_recorder_ready;
    result.fallback_reason = first_unready_reason(result.lifecycle);
    if (result.fallback_reason != vulkan_backend_fallback_reason::none) {
        return result;
    }
    result.lifecycle_ready = true;
    result.reached_stage = vulkan_backend_frame_stage::lifecycle_ready;

    result.surface = device.current_surface_extent();
    if (!result.surface.valid()) {
        result.fallback_reason = vulkan_backend_fallback_reason::surface_unavailable;
        return result;
    }
    result.reached_stage = vulkan_backend_frame_stage::surface_extent_ready;
    if (!has_visible_area(viewport)) {
        result.fallback_reason = vulkan_backend_fallback_reason::viewport_unavailable;
        return result;
    }
    result.surface_ready = true;

    const vulkan_frame_plan plan = build_vulkan_frame_plan(
        draw_list,
        vulkan_frame_plan_options{
            .viewport = viewport,
            .surface_width = result.surface.width,
            .surface_height = result.surface.height,
        });
    result.planned_batch_count = plan.batches.size();
    result.command_recorder.planned_batch_count = plan.batches.size();
    result.clipped_draw_call_count = plan.clipped_draw_call_count;
    result.discarded_draw_call_count = plan.discarded_draw_call_count;
    result.reached_stage = vulkan_backend_frame_stage::frame_plan_ready;

    result.frame_begun = device.begin_frame(result.surface);
    if (!result.frame_begun) {
        result.fallback_reason = vulkan_backend_fallback_reason::begin_frame_failed;
        return result;
    }
    result.reached_stage = vulkan_backend_frame_stage::frame_begun;

    if (!command_recorder.begin_recording(result.surface, plan.batches.size())) {
        result.command_recorder = command_recorder.recorder_state();
        result.fallback_reason = vulkan_backend_fallback_reason::record_commands_failed;
        return result;
    }
    for (const vulkan_draw_batch& batch : plan.batches) {
        if (!command_recorder.record_draw_batch(batch)) {
            result.command_recorder = command_recorder.recorder_state();
            result.fallback_reason = vulkan_backend_fallback_reason::record_commands_failed;
            return result;
        }
    }
    if (!command_recorder.finish_recording()) {
        result.command_recorder = command_recorder.recorder_state();
        result.fallback_reason = vulkan_backend_fallback_reason::record_commands_failed;
        return result;
    }
    result.command_recorder = command_recorder.recorder_state();

    result.commands_recorded = device.record_frame_commands(plan);
    if (!result.commands_recorded) {
        result.fallback_reason = vulkan_backend_fallback_reason::record_commands_failed;
        return result;
    }
    result.recorded_batch_count = plan.batches.size();
    result.reached_stage = vulkan_backend_frame_stage::commands_recorded;

    result.frame_submitted = device.submit_frame();
    if (!result.frame_submitted) {
        result.fallback_reason = vulkan_backend_fallback_reason::submit_frame_failed;
        return result;
    }
    result.reached_stage = vulkan_backend_frame_stage::frame_submitted;

    result.frame_presented = device.present_frame();
    if (!result.frame_presented) {
        result.fallback_reason = vulkan_backend_fallback_reason::present_frame_failed;
        return result;
    }
    result.reached_stage = vulkan_backend_frame_stage::frame_presented;

    result.fallback_required = false;
    result.fallback_reason = vulkan_backend_fallback_reason::none;
    return result;
}

} // namespace quiz_vulkan::render::vulkan_backend
