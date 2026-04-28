#pragma once

#include "render/render_draw_list.h"
#include "render/vulkan/vulkan_backend_adapter.h"

#include <cstddef>
#include <vector>

namespace quiz_vulkan::render {

struct vulkan_renderer_frame_stats {
    std::size_t command_count = 0;
    std::size_t draw_call_count = 0;
    std::size_t visible_draw_call_count = 0;
    std::size_t quad_count = 0;
    std::size_t text_count = 0;
    std::size_t image_count = 0;
    std::size_t push_clip_count = 0;
    std::size_t pop_clip_count = 0;
    std::size_t debug_bounds_count = 0;
    std::size_t text_run_count = 0;
    std::size_t text_character_count = 0;

    bool empty() const
    {
        return command_count == 0;
    }
};

struct vulkan_renderer_framebuffer {
    std::size_t width = 0;
    std::size_t height = 0;
    std::vector<unsigned char> rgba;
};

enum class vulkan_renderer_backend {
    cpu_fallback,
    vulkan,
};

struct vulkan_renderer_options {
    render_rect viewport{0.0f, 0.0f, 1280.0f, 720.0f};
    std::size_t fallback_surface_width = 160;
    std::size_t fallback_surface_height = 90;
    bool prefer_vulkan = true;
};

struct vulkan_renderer_frame_summary {
    vulkan_renderer_backend backend = vulkan_renderer_backend::cpu_fallback;
    render_rect viewport;
    std::size_t surface_width = 0;
    std::size_t surface_height = 0;
    std::size_t shaded_pixel_count = 0;
    std::size_t clipped_draw_call_count = 0;
    std::size_t discarded_draw_call_count = 0;
    std::size_t backend_surface_width = 0;
    std::size_t backend_surface_height = 0;
    std::size_t backend_planned_batch_count = 0;
    bool backend_surface_ready = false;
    bool backend_frame_begun = false;
    bool backend_commands_recorded = false;
    bool backend_frame_submitted = false;
    bool backend_frame_presented = false;
    bool backend_attempted = false;
    bool backend_fallback_required = true;
    vulkan_backend::vulkan_backend_fallback_reason backend_fallback_reason =
        vulkan_backend::vulkan_backend_fallback_reason::not_requested;

    bool used_cpu_fallback() const
    {
        return backend == vulkan_renderer_backend::cpu_fallback;
    }

    bool nonblank() const
    {
        return shaded_pixel_count > 0;
    }
};

class vulkan_renderer {
public:
    vulkan_renderer() = default;
    explicit vulkan_renderer(vulkan_renderer_options options);

    void submit(const render_draw_list& draw_list);
    void submit(const std::vector<render_draw_command>& commands);
    void clear();

    const render_draw_list& last_draw_list() const;
    const vulkan_renderer_frame_stats& last_frame_stats() const;
    const vulkan_renderer_frame_summary& last_frame_summary() const;
    const vulkan_backend::vulkan_backend_frame_result& last_backend_frame_result() const;
    const vulkan_renderer_framebuffer& last_framebuffer() const;

    const vulkan_renderer_options& options() const;
    void set_options(vulkan_renderer_options options);

private:
    static vulkan_renderer_frame_stats count_commands(const render_draw_list& draw_list);
    static vulkan_renderer_frame_summary summarize_cpu_fallback(
        const render_draw_list& draw_list,
        const vulkan_renderer_frame_stats& stats,
        const vulkan_backend::vulkan_backend_frame_result& backend_result,
        const vulkan_renderer_options& options);
    static vulkan_renderer_framebuffer rasterize_cpu_fallback_framebuffer(
        const render_draw_list& draw_list,
        const vulkan_renderer_options& options);

    vulkan_renderer_options options_;
    render_draw_list last_draw_list_;
    vulkan_renderer_frame_stats last_frame_stats_;
    vulkan_renderer_frame_summary last_frame_summary_;
    vulkan_backend::vulkan_backend_frame_result last_backend_frame_result_;
    vulkan_renderer_framebuffer last_framebuffer_;
};

} // namespace quiz_vulkan::render
