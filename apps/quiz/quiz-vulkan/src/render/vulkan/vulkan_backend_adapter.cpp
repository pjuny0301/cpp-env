#include "render/vulkan/vulkan_backend_adapter.h"

namespace quiz_vulkan::render::vulkan_backend {
namespace {

bool has_visible_area(const render_rect& rect)
{
    return rect.width > 0.0f && rect.height > 0.0f;
}

} // namespace

std::string_view fallback_reason_name(vulkan_backend_fallback_reason reason)
{
    switch (reason) {
    case vulkan_backend_fallback_reason::none:
        return "none";
    case vulkan_backend_fallback_reason::not_requested:
        return "not_requested";
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

vulkan_backend_frame_result submit_vulkan_backend_frame(
    vulkan_backend_device_interface& device,
    const render_draw_list& draw_list,
    render_rect viewport)
{
    vulkan_backend_frame_result result;
    result.attempted = true;
    result.surface = device.current_surface_extent();
    if (!result.surface.valid()) {
        result.fallback_reason = vulkan_backend_fallback_reason::surface_unavailable;
        return result;
    }
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
    result.clipped_draw_call_count = plan.clipped_draw_call_count;
    result.discarded_draw_call_count = plan.discarded_draw_call_count;

    result.frame_begun = device.begin_frame(result.surface);
    if (!result.frame_begun) {
        result.fallback_reason = vulkan_backend_fallback_reason::begin_frame_failed;
        return result;
    }

    result.commands_recorded = device.record_frame_commands(plan);
    if (!result.commands_recorded) {
        result.fallback_reason = vulkan_backend_fallback_reason::record_commands_failed;
        return result;
    }
    result.recorded_batch_count = plan.batches.size();

    result.frame_submitted = device.submit_frame();
    if (!result.frame_submitted) {
        result.fallback_reason = vulkan_backend_fallback_reason::submit_frame_failed;
        return result;
    }

    result.frame_presented = device.present_frame();
    if (!result.frame_presented) {
        result.fallback_reason = vulkan_backend_fallback_reason::present_frame_failed;
        return result;
    }

    result.fallback_required = false;
    result.fallback_reason = vulkan_backend_fallback_reason::none;
    return result;
}

} // namespace quiz_vulkan::render::vulkan_backend
