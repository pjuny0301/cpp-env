#include "render/vulkan/vulkan_renderer.h"

namespace quiz_vulkan::render {

void vulkan_renderer::submit(const ui::ui_draw_list& draw_list)
{
    last_draw_list_ = draw_list;
    last_frame_stats_ = count_commands(last_draw_list_);
}

void vulkan_renderer::submit(const std::vector<ui::ui_draw_command>& commands)
{
    ui::ui_draw_list draw_list;
    draw_list.commands = commands;
    submit(draw_list);
}

void vulkan_renderer::clear()
{
    last_draw_list_.clear();
    last_frame_stats_ = {};
}

const ui::ui_draw_list& vulkan_renderer::last_draw_list() const
{
    return last_draw_list_;
}

const vulkan_renderer_frame_stats& vulkan_renderer::last_frame_stats() const
{
    return last_frame_stats_;
}

vulkan_renderer_frame_stats vulkan_renderer::count_commands(const ui::ui_draw_list& draw_list)
{
    vulkan_renderer_frame_stats stats;
    stats.command_count = draw_list.commands.size();

    for (const ui::ui_draw_command& command : draw_list.commands) {
        switch (command.type) {
        case ui::ui_draw_command_type::quad:
            ++stats.quad_count;
            break;
        case ui::ui_draw_command_type::text:
            ++stats.text_count;
            break;
        case ui::ui_draw_command_type::image:
            ++stats.image_count;
            break;
        case ui::ui_draw_command_type::push_clip:
            ++stats.push_clip_count;
            break;
        case ui::ui_draw_command_type::pop_clip:
            ++stats.pop_clip_count;
            break;
        case ui::ui_draw_command_type::debug_bounds:
            ++stats.debug_bounds_count;
            break;
        }
    }

    return stats;
}

} // namespace quiz_vulkan::render
