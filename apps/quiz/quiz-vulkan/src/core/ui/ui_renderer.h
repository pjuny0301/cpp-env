#pragma once

#include "core/layout/layout_placer.h"

#include <cstddef>
#include <string>
#include <vector>

namespace quiz_vulkan::ui {

struct ui_color {
    float red = 1.0f;
    float green = 1.0f;
    float blue = 1.0f;
    float alpha = 1.0f;

    bool visible() const
    {
        return alpha > 0.0f;
    }
};

struct ui_paint {
    std::string source;
    ui_color color;
};

enum class ui_draw_command_type {
    quad,
    text,
    image,
    push_clip,
    pop_clip,
    debug_bounds,
};

struct ui_draw_command {
    ui_draw_command_type type = ui_draw_command_type::quad;
    scene::scene_node_id node_id;
    scene::scene_node_id parent_node_id;
    std::size_t depth = 0;
    scene::scene_rect bounds;
    scene::scene_rect content_bounds;
    ui_paint paint;
    float border_radius = 0.0f;
    std::vector<scene::scene_text_run> text_runs;
    scene::scene_image_ref image;
};

struct ui_draw_list {
    std::vector<ui_draw_command> commands;

    bool empty() const
    {
        return commands.empty();
    }

    std::size_t size() const
    {
        return commands.size();
    }

    void clear()
    {
        commands.clear();
    }
};

struct ui_renderer_options {
    bool include_debug_bounds = false;
    ui_color default_text_color{1.0f, 1.0f, 1.0f, 1.0f};
    ui_color unresolved_color{1.0f, 0.0f, 1.0f, 1.0f};
    ui_color debug_bounds_color{0.0f, 1.0f, 1.0f, 0.65f};
};

class ui_renderer {
public:
    ui_renderer() = default;
    explicit ui_renderer(ui_renderer_options options);

    ui_draw_list build_draw_list(const scene::placed_scene& placed_scene) const;

    const ui_renderer_options& options() const;
    void set_options(ui_renderer_options options);

private:
    ui_renderer_options options_;
};

} // namespace quiz_vulkan::ui
