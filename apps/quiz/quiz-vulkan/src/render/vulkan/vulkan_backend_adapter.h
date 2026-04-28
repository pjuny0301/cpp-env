#pragma once

#include "render/render_draw_list.h"
#include "render/vulkan/vulkan_frame_plan.h"

#include <cstddef>

namespace quiz_vulkan::render::vulkan_backend {

enum class vulkan_backend_fallback_reason {
    none,
    not_requested,
    surface_unavailable,
    viewport_unavailable,
    begin_frame_failed,
    record_commands_failed,
    submit_frame_failed,
    present_frame_failed,
};

struct vulkan_surface_extent {
    std::size_t width = 0;
    std::size_t height = 0;

    bool valid() const
    {
        return width > 0 && height > 0;
    }
};

struct vulkan_backend_frame_result {
    vulkan_surface_extent surface;
    bool surface_ready = false;
    bool frame_begun = false;
    bool commands_recorded = false;
    bool frame_submitted = false;
    bool frame_presented = false;
    bool attempted = false;
    bool fallback_required = true;
    vulkan_backend_fallback_reason fallback_reason = vulkan_backend_fallback_reason::not_requested;
    std::size_t planned_batch_count = 0;
    std::size_t clipped_draw_call_count = 0;
    std::size_t discarded_draw_call_count = 0;

    bool completed() const
    {
        return surface_ready && frame_begun && commands_recorded
            && frame_submitted && frame_presented && !fallback_required;
    }
};

class vulkan_backend_device_interface {
public:
    virtual ~vulkan_backend_device_interface() = default;

    virtual vulkan_surface_extent current_surface_extent() const = 0;
    virtual bool begin_frame(vulkan_surface_extent surface) = 0;
    virtual bool record_frame_commands(const vulkan_frame_plan& plan) = 0;
    virtual bool submit_frame() = 0;
    virtual bool present_frame() = 0;
};

class null_vulkan_backend_device final : public vulkan_backend_device_interface {
public:
    vulkan_surface_extent current_surface_extent() const override;
    bool begin_frame(vulkan_surface_extent surface) override;
    bool record_frame_commands(const vulkan_frame_plan& plan) override;
    bool submit_frame() override;
    bool present_frame() override;
};

vulkan_backend_frame_result submit_vulkan_backend_frame(
    vulkan_backend_device_interface& device,
    const render_draw_list& draw_list,
    render_rect viewport);

} // namespace quiz_vulkan::render::vulkan_backend
