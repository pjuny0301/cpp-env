#pragma once

#include "core/ui/ui_renderer.h"

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

enum class vulkan_renderer_backend {
    cpu_fallback,
    vulkan,
};

struct vulkan_renderer_options {
    scene::scene_rect viewport{0.0f, 0.0f, 1280.0f, 720.0f};
    std::size_t fallback_surface_width = 160;
    std::size_t fallback_surface_height = 90;
    bool prefer_vulkan = true;
};

struct vulkan_renderer_frame_summary {
    vulkan_renderer_backend backend = vulkan_renderer_backend::cpu_fallback;
    scene::scene_rect viewport;
    std::size_t surface_width = 0;
    std::size_t surface_height = 0;
    std::size_t shaded_pixel_count = 0;
    std::size_t clipped_draw_call_count = 0;
    std::size_t discarded_draw_call_count = 0;

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

    void submit(const scene::placed_scene& placed_scene);
    void submit(const ui::ui_draw_list& draw_list);
    void submit(const std::vector<ui::ui_draw_command>& commands);
    void clear();

    const ui::ui_draw_list& last_draw_list() const;
    const vulkan_renderer_frame_stats& last_frame_stats() const;
    const vulkan_renderer_frame_summary& last_frame_summary() const;

    const vulkan_renderer_options& options() const;
    void set_options(vulkan_renderer_options options);

private:
    static vulkan_renderer_frame_stats count_commands(const ui::ui_draw_list& draw_list);
    static vulkan_renderer_frame_summary summarize_cpu_fallback(
        const ui::ui_draw_list& draw_list,
        const vulkan_renderer_frame_stats& stats,
        const vulkan_renderer_options& options);

    vulkan_renderer_options options_;
    ui::ui_draw_list last_draw_list_;
    vulkan_renderer_frame_stats last_frame_stats_;
    vulkan_renderer_frame_summary last_frame_summary_;
};

} // namespace quiz_vulkan::render
