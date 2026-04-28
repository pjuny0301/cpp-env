#include "core/ui/ui_renderer.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace quiz_vulkan::ui {
namespace {

struct active_clip_scope {
    scene::scene_node_id node_id;
    std::size_t depth = 0;
    ui_rect bounds;
};

float clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

bool has_visible_area(const scene::scene_rect& rect)
{
    return rect.width > 0.0f && rect.height > 0.0f;
}

ui_rect to_ui_rect(const scene::scene_rect& rect)
{
    return ui_rect{rect.x, rect.y, rect.width, rect.height};
}

std::vector<ui_text_run> to_ui_text_runs(const std::vector<scene::scene_text_run>& text_runs)
{
    std::vector<ui_text_run> converted;
    converted.reserve(text_runs.size());
    for (const scene::scene_text_run& run : text_runs) {
        converted.push_back(ui_text_run{run.text, run.style_token});
    }
    return converted;
}

ui_image_ref to_ui_image_ref(const scene::scene_image_ref& image)
{
    return ui_image_ref{image.uri, image.alt_text, image.aspect_ratio};
}

std::string_view trim(std::string_view value)
{
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.front() == '\r' || value.front() == '\n')) {
        value.remove_prefix(1);
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r' || value.back() == '\n')) {
        value.remove_suffix(1);
    }
    return value;
}

char ascii_lower(char value)
{
    if (value >= 'A' && value <= 'Z') {
        return static_cast<char>(value - 'A' + 'a');
    }
    return value;
}

std::string to_ascii_lower(std::string_view value)
{
    std::string lower;
    lower.reserve(value.size());
    for (char character : value) {
        lower.push_back(ascii_lower(character));
    }
    return lower;
}

int hex_value(char value)
{
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    return -1;
}

bool read_hex_byte(std::string_view text, std::size_t offset, float& channel)
{
    const int high = hex_value(text[offset]);
    const int low = hex_value(text[offset + 1]);
    if (high < 0 || low < 0) {
        return false;
    }

    channel = static_cast<float>((high * 16) + low) / 255.0f;
    return true;
}

bool read_hex_nibble(std::string_view text, std::size_t offset, float& channel)
{
    const int value = hex_value(text[offset]);
    if (value < 0) {
        return false;
    }

    channel = static_cast<float>(value) / 15.0f;
    return true;
}

bool parse_hex_color(std::string_view text, ui_color& color)
{
    if (text.empty() || text.front() != '#') {
        return false;
    }

    if (text.size() == 4 || text.size() == 5) {
        ui_color parsed;
        if (!read_hex_nibble(text, 1, parsed.red) || !read_hex_nibble(text, 2, parsed.green)
            || !read_hex_nibble(text, 3, parsed.blue)) {
            return false;
        }
        if (text.size() == 5 && !read_hex_nibble(text, 4, parsed.alpha)) {
            return false;
        }
        color = parsed;
        return true;
    }

    if (text.size() == 7 || text.size() == 9) {
        ui_color parsed;
        if (!read_hex_byte(text, 1, parsed.red) || !read_hex_byte(text, 3, parsed.green)
            || !read_hex_byte(text, 5, parsed.blue)) {
            return false;
        }
        if (text.size() == 9 && !read_hex_byte(text, 7, parsed.alpha)) {
            return false;
        }
        color = parsed;
        return true;
    }

    return false;
}

bool parse_named_color(std::string_view text, ui_color& color)
{
    const std::string name = to_ascii_lower(text);
    if (name == "transparent") {
        color = ui_color{0.0f, 0.0f, 0.0f, 0.0f};
        return true;
    }
    if (name == "black") {
        color = ui_color{0.0f, 0.0f, 0.0f, 1.0f};
        return true;
    }
    if (name == "white") {
        color = ui_color{1.0f, 1.0f, 1.0f, 1.0f};
        return true;
    }
    if (name == "red") {
        color = ui_color{1.0f, 0.0f, 0.0f, 1.0f};
        return true;
    }
    if (name == "green") {
        color = ui_color{0.0f, 1.0f, 0.0f, 1.0f};
        return true;
    }
    if (name == "blue") {
        color = ui_color{0.0f, 0.0f, 1.0f, 1.0f};
        return true;
    }
    return false;
}

bool parse_color(std::string_view text, ui_color& color)
{
    const std::string_view trimmed = trim(text);
    return parse_hex_color(trimmed, color) || parse_named_color(trimmed, color);
}

ui_color apply_opacity(ui_color color, float opacity)
{
    color.alpha *= clamp01(opacity);
    return color;
}

ui_paint make_paint(std::string source, ui_color color)
{
    return ui_paint{std::move(source), color};
}

ui_paint make_default_paint(ui_color color, float opacity)
{
    return make_paint({}, apply_opacity(color, opacity));
}

bool make_source_paint(const std::string& source, const ui_renderer_options& options, float opacity, ui_paint& paint)
{
    if (source.empty()) {
        return false;
    }

    ui_color color;
    if (!parse_color(source, color)) {
        color = options.unresolved_color;
    }

    paint = make_paint(source, apply_opacity(color, opacity));
    return true;
}

ui_draw_command make_command(
    ui_draw_command_type type,
    const scene::placed_scene_node& node,
    scene::scene_rect bounds,
    scene::scene_rect content_bounds)
{
    ui_draw_command command;
    command.type = type;
    command.node_id = node.id;
    command.parent_node_id = node.parent_id;
    command.depth = node.depth;
    command.bounds = to_ui_rect(bounds);
    command.content_bounds = to_ui_rect(content_bounds);
    command.border_radius = node.style.border_radius;
    return command;
}

ui_draw_command make_pop_clip_command(const active_clip_scope& scope)
{
    ui_draw_command command;
    command.type = ui_draw_command_type::pop_clip;
    command.node_id = scope.node_id;
    command.depth = scope.depth;
    command.bounds = scope.bounds;
    command.content_bounds = scope.bounds;
    return command;
}

void close_clip_scopes_for_depth(
    ui_draw_list& draw_list,
    std::vector<active_clip_scope>& active_clips,
    std::size_t next_depth)
{
    while (!active_clips.empty() && active_clips.back().depth >= next_depth) {
        draw_list.commands.push_back(make_pop_clip_command(active_clips.back()));
        active_clips.pop_back();
    }
}

void close_all_clip_scopes(ui_draw_list& draw_list, std::vector<active_clip_scope>& active_clips)
{
    while (!active_clips.empty()) {
        draw_list.commands.push_back(make_pop_clip_command(active_clips.back()));
        active_clips.pop_back();
    }
}

} // namespace

ui_renderer::ui_renderer(ui_renderer_options options)
    : options_(std::move(options))
{
}

ui_draw_list ui_renderer::build_draw_list(const scene::placed_scene& placed_scene) const
{
    ui_draw_list draw_list;
    std::vector<active_clip_scope> active_clips;

    draw_list.commands.reserve(placed_scene.nodes.size() * 2);

    for (const scene::placed_scene_node& node : placed_scene.nodes) {
        close_clip_scopes_for_depth(draw_list, active_clips, node.depth);

        if (!node.visible || !has_visible_area(node.bounds)) {
            continue;
        }

        ui_paint background_paint;
        if (make_source_paint(node.style.background_color, options_, node.style.opacity, background_paint)
            && background_paint.color.visible()) {
            ui_draw_command command = make_command(ui_draw_command_type::quad, node, node.bounds, node.content_bounds);
            command.paint = std::move(background_paint);
            draw_list.commands.push_back(std::move(command));
        }

        if (node.has_image && has_visible_area(node.content_bounds)) {
            ui_paint image_paint = make_default_paint(ui_color{1.0f, 1.0f, 1.0f, 1.0f}, node.style.opacity);
            if (image_paint.color.visible()) {
                ui_draw_command command = make_command(ui_draw_command_type::image, node, node.content_bounds, node.content_bounds);
                command.image = to_ui_image_ref(node.image);
                command.paint = std::move(image_paint);
                draw_list.commands.push_back(std::move(command));
            }
        }

        if (!node.text_runs.empty() && has_visible_area(node.content_bounds)) {
            ui_paint text_paint = make_default_paint(options_.default_text_color, node.style.opacity);
            ui_paint foreground_paint;
            if (make_source_paint(node.style.foreground_color, options_, node.style.opacity, foreground_paint)) {
                text_paint = std::move(foreground_paint);
            }

            if (text_paint.color.visible()) {
                ui_draw_command command = make_command(ui_draw_command_type::text, node, node.content_bounds, node.content_bounds);
                command.paint = std::move(text_paint);
                command.text_runs = to_ui_text_runs(node.text_runs);
                draw_list.commands.push_back(std::move(command));
            }
        }

        if (options_.include_debug_bounds) {
            ui_draw_command command = make_command(ui_draw_command_type::debug_bounds, node, node.bounds, node.content_bounds);
            command.paint = make_paint("debug_bounds", options_.debug_bounds_color);
            command.border_radius = 0.0f;
            draw_list.commands.push_back(std::move(command));
        }

        if (node.layout_rule.clip_children && has_visible_area(node.content_bounds)) {
            ui_draw_command command = make_command(ui_draw_command_type::push_clip, node, node.content_bounds, node.content_bounds);
            draw_list.commands.push_back(std::move(command));
            active_clips.push_back(active_clip_scope{node.id, node.depth, to_ui_rect(node.content_bounds)});
        }
    }

    close_all_clip_scopes(draw_list, active_clips);
    return draw_list;
}

const ui_renderer_options& ui_renderer::options() const
{
    return options_;
}

void ui_renderer::set_options(ui_renderer_options options)
{
    options_ = std::move(options);
}

} // namespace quiz_vulkan::ui
