#pragma once

#include "core/layout/layout_placer.h"
#include "render/render_draw_list.h"

namespace quiz_vulkan::ui {

using ui_color = render::render_color;
using ui_paint = render::render_paint;
using ui_rect = render::render_rect;
using ui_text_run = render::render_text_run;
using ui_image_ref = render::render_image_ref;
using ui_draw_command_type = render::render_draw_command_type;
using ui_draw_command = render::render_draw_command;
using ui_draw_list = render::render_draw_list;

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
