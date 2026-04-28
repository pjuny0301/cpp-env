#pragma once

#include "render/image/image_types.h"

#include <cstddef>
#include <string>
#include <vector>

namespace quiz_vulkan::render {

using render_node_id = std::string;
using render_style_id = std::string;

struct render_rect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    float right() const
    {
        return x + width;
    }

    float bottom() const
    {
        return y + height;
    }
};

struct render_color {
    float red = 1.0f;
    float green = 1.0f;
    float blue = 1.0f;
    float alpha = 1.0f;

    bool visible() const
    {
        return alpha > 0.0f;
    }
};

struct render_paint {
    std::string source;
    render_color color;
};

struct render_text_run {
    std::string text;
    render_style_id style_token;
};

enum class render_text_wrap_mode {
    no_wrap,
    word,
};

enum class render_text_alignment {
    start,
    center,
    end,
};

struct render_text_style {
    render_style_id id;
    std::string font_family;
    float font_size = 16.0f;
    float line_height = 0.0f;
    float letter_spacing = 0.0f;
    int font_weight = 400;
    bool italic = false;
};

struct render_text_style_catalog {
    render_text_style fallback_style;
    std::vector<render_text_style> styles;

    const render_text_style* find(const render_style_id& id) const
    {
        for (const render_text_style& style : styles) {
            if (style.id == id) {
                return &style;
            }
        }
        return nullptr;
    }

    const render_text_style& resolve(const render_style_id& id) const
    {
        const render_text_style* style = find(id);
        return style == nullptr ? fallback_style : *style;
    }
};

struct render_text_options {
    render_text_wrap_mode wrap = render_text_wrap_mode::no_wrap;
    render_text_alignment alignment = render_text_alignment::start;
    std::size_t max_lines = 0;
};

struct render_image_ref {
    std::string uri;
    std::string alt_text;
    float aspect_ratio = 0.0f;
    render_image_sampler_policy sampler;
};

enum class render_draw_command_type {
    quad,
    text,
    image,
    push_clip,
    pop_clip,
    debug_bounds,
};

struct render_draw_command {
    render_draw_command_type type = render_draw_command_type::quad;
    render_node_id node_id;
    render_node_id parent_node_id;
    std::size_t depth = 0;
    render_rect bounds;
    render_rect content_bounds;
    render_paint paint;
    float border_radius = 0.0f;
    std::vector<render_text_run> text_runs;
    render_image_ref image;
    render_text_options text_options;
};

struct render_draw_list {
    std::vector<render_draw_command> commands;
    render_text_style_catalog text_styles;

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

} // namespace quiz_vulkan::render
