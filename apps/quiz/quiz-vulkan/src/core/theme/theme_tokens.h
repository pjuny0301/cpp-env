#pragma once

#include <string>
#include <vector>

namespace quiz_vulkan::theme {

using theme_token_id = std::string;

struct theme_color {
    float red = 1.0f;
    float green = 1.0f;
    float blue = 1.0f;
    float alpha = 1.0f;

    [[nodiscard]] bool visible() const noexcept
    {
        return alpha > 0.0f;
    }
};

struct color_token {
    theme_token_id id;
    theme_color color;
};

struct text_style_token {
    theme_token_id id;
    std::string font_family;
    float font_size = 16.0f;
    float line_height = 0.0f;
    int font_weight = 400;
    bool italic = false;
};

struct spacing_token {
    theme_token_id id;
    float value = 0.0f;
};

enum class motion_easing {
    linear,
    ease_in,
    ease_out,
    ease_in_out,
};

struct motion_token {
    theme_token_id id;
    float duration_ms = 0.0f;
    motion_easing easing = motion_easing::linear;
    bool enabled = true;
};

struct theme_manifest {
    std::string schema_version = "quiz-theme-v1";
    std::string id;
    std::vector<color_token> colors;
    std::vector<text_style_token> text_styles;
    std::vector<spacing_token> spacing;
    std::vector<motion_token> motion;

    [[nodiscard]] bool valid() const noexcept
    {
        return schema_version == "quiz-theme-v1" && !id.empty();
    }
};

class theme_resolver_interface {
public:
    virtual ~theme_resolver_interface() = default;

    virtual const color_token* find_color(const theme_token_id& id) const = 0;
    virtual const text_style_token* find_text_style(const theme_token_id& id) const = 0;
    virtual const spacing_token* find_spacing(const theme_token_id& id) const = 0;
    virtual const motion_token* find_motion(const theme_token_id& id) const = 0;
};

} // namespace quiz_vulkan::theme
