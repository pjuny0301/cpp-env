#pragma once

#include "core/ui/ui_renderer.h"

#include <cstddef>
#include <vector>

namespace quiz_vulkan::render {

struct vulkan_renderer_frame_stats {
    std::size_t command_count = 0;
    std::size_t quad_count = 0;
    std::size_t text_count = 0;
    std::size_t image_count = 0;
    std::size_t push_clip_count = 0;
    std::size_t pop_clip_count = 0;
    std::size_t debug_bounds_count = 0;

    bool empty() const
    {
        return command_count == 0;
    }
};

class vulkan_renderer {
public:
    vulkan_renderer() = default;

    void submit(const ui::ui_draw_list& draw_list);
    void submit(const std::vector<ui::ui_draw_command>& commands);
    void clear();

    const ui::ui_draw_list& last_draw_list() const;
    const vulkan_renderer_frame_stats& last_frame_stats() const;

private:
    static vulkan_renderer_frame_stats count_commands(const ui::ui_draw_list& draw_list);

    ui::ui_draw_list last_draw_list_;
    vulkan_renderer_frame_stats last_frame_stats_;
};

} // namespace quiz_vulkan::render
