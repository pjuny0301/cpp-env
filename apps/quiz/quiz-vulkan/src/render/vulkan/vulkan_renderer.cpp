#include "render/vulkan/vulkan_renderer.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {
namespace {

bool has_visible_area(const scene::scene_rect& rect)
{
    return rect.width > 0.0f && rect.height > 0.0f;
}

bool is_draw_command(ui::ui_draw_command_type type)
{
    switch (type) {
    case ui::ui_draw_command_type::quad:
    case ui::ui_draw_command_type::text:
    case ui::ui_draw_command_type::image:
    case ui::ui_draw_command_type::debug_bounds:
        return true;
    case ui::ui_draw_command_type::push_clip:
    case ui::ui_draw_command_type::pop_clip:
        return false;
    }

    return false;
}

const scene::placed_scene_node* find_command_node(
    const scene::placed_scene* placed_scene,
    const ui::ui_draw_command& command)
{
    if (placed_scene == nullptr) {
        return nullptr;
    }

    return placed_scene->find_node(command.node_id);
}

bool remember_node_id(std::vector<scene::scene_node_id>& node_ids, const scene::scene_node_id& node_id)
{
    if (std::find(node_ids.begin(), node_ids.end(), node_id) != node_ids.end()) {
        return false;
    }

    node_ids.push_back(node_id);
    return true;
}

void count_input_draw_node(
    vulkan_renderer_frame_stats& stats,
    const scene::placed_scene_node& node,
    std::vector<scene::scene_node_id>& input_enabled_draw_node_ids,
    std::vector<scene::scene_node_id>& disabled_draw_node_ids)
{
    if (!node.input_enabled) {
        if (remember_node_id(disabled_draw_node_ids, node.id)) {
            ++stats.disabled_draw_node_count;
        }
        return;
    }

    if (node.has_action_binding && remember_node_id(input_enabled_draw_node_ids, node.id)) {
        ++stats.input_enabled_draw_node_count;
    }
}

void count_semantic_draw_command(
    vulkan_renderer_frame_stats& stats,
    const scene::placed_scene_node& node)
{
    switch (node.semantics.role) {
    case scene::scene_node_role::quiz_option:
        ++stats.quiz_option_draw_command_count;
        break;
    case scene::scene_node_role::quiz_feedback:
        ++stats.quiz_feedback_draw_command_count;
        break;
    case scene::scene_node_role::quiz_answer_input:
        ++stats.quiz_answer_input_draw_command_count;
        break;
    case scene::scene_node_role::quiz_answer_dock:
        ++stats.quiz_answer_dock_draw_command_count;
        break;
    case scene::scene_node_role::generic:
    case scene::scene_node_role::app_shell:
    case scene::scene_node_role::quiz_question_stage:
    case scene::scene_node_role::quiz_question_header:
    case scene::scene_node_role::quiz_question_prompt:
    case scene::scene_node_role::quiz_question_body:
    case scene::scene_node_role::quiz_question_image:
    case scene::scene_node_role::quiz_option_group:
    case scene::scene_node_role::quiz_controls:
        break;
    }
}

scene::scene_rect intersect_rect(const scene::scene_rect& left, const scene::scene_rect& right)
{
    const float x0 = std::max(left.x, right.x);
    const float y0 = std::max(left.y, right.y);
    const float x1 = std::min(left.right(), right.right());
    const float y1 = std::min(left.bottom(), right.bottom());

    if (x1 <= x0 || y1 <= y0) {
        return scene::scene_rect{x0, y0, 0.0f, 0.0f};
    }

    return scene::scene_rect{x0, y0, x1 - x0, y1 - y0};
}

bool rect_was_clipped(const scene::scene_rect& original, const scene::scene_rect& clipped)
{
    return clipped.x > original.x || clipped.y > original.y
        || clipped.right() < original.right() || clipped.bottom() < original.bottom();
}

scene::scene_rect active_clip_rect(
    const scene::scene_rect& viewport,
    const std::vector<scene::scene_rect>& clip_stack)
{
    scene::scene_rect clip = viewport;
    for (const scene::scene_rect& scope : clip_stack) {
        clip = intersect_rect(clip, scope);
    }
    return clip;
}

int surface_coordinate(float value, float origin, float extent, std::size_t surface_extent, bool upper)
{
    const float normalized = (value - origin) / extent;
    const float scaled = normalized * static_cast<float>(surface_extent);
    const float rounded = upper ? std::ceil(scaled) : std::floor(scaled);
    const int coordinate = static_cast<int>(rounded);
    return std::clamp(coordinate, 0, static_cast<int>(surface_extent));
}

unsigned char color_channel_byte(float value)
{
    const float clamped = std::clamp(value, 0.0f, 1.0f);
    return static_cast<unsigned char>(std::round(clamped * 255.0f));
}

scene::scene_rect raster_bounds_for_command(const ui::ui_draw_command& command)
{
    if (command.type == ui::ui_draw_command_type::text) {
        return command.content_bounds;
    }

    return command.bounds;
}

void fill_rect(
    std::vector<unsigned char>& rgba,
    const scene::scene_rect& viewport,
    std::size_t surface_width,
    std::size_t surface_height,
    const scene::scene_rect& rect,
    const ui::ui_color& color)
{
    if (rgba.empty() || !has_visible_area(viewport) || !has_visible_area(rect)) {
        return;
    }

    const int x0 = surface_coordinate(rect.x, viewport.x, viewport.width, surface_width, false);
    const int y0 = surface_coordinate(rect.y, viewport.y, viewport.height, surface_height, false);
    const int x1 = surface_coordinate(rect.right(), viewport.x, viewport.width, surface_width, true);
    const int y1 = surface_coordinate(rect.bottom(), viewport.y, viewport.height, surface_height, true);

    if (x1 <= x0 || y1 <= y0) {
        return;
    }

    const unsigned char red = color_channel_byte(color.red);
    const unsigned char green = color_channel_byte(color.green);
    const unsigned char blue = color_channel_byte(color.blue);
    const unsigned char alpha = color_channel_byte(color.alpha);
    if (alpha == 0) {
        return;
    }

    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const std::size_t index = (static_cast<std::size_t>(y) * surface_width + static_cast<std::size_t>(x)) * 4;
            rgba[index] = red;
            rgba[index + 1] = green;
            rgba[index + 2] = blue;
            rgba[index + 3] = alpha;
        }
    }
}

std::size_t shade_rect(
    std::vector<unsigned char>& coverage,
    const scene::scene_rect& viewport,
    std::size_t surface_width,
    std::size_t surface_height,
    const scene::scene_rect& rect)
{
    if (coverage.empty() || !has_visible_area(viewport) || !has_visible_area(rect)) {
        return 0;
    }

    const int x0 = surface_coordinate(rect.x, viewport.x, viewport.width, surface_width, false);
    const int y0 = surface_coordinate(rect.y, viewport.y, viewport.height, surface_height, false);
    const int x1 = surface_coordinate(rect.right(), viewport.x, viewport.width, surface_width, true);
    const int y1 = surface_coordinate(rect.bottom(), viewport.y, viewport.height, surface_height, true);

    if (x1 <= x0 || y1 <= y0) {
        return 0;
    }

    std::size_t newly_shaded = 0;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const std::size_t index = static_cast<std::size_t>(y) * surface_width + static_cast<std::size_t>(x);
            if (coverage[index] == 0) {
                coverage[index] = 1;
                ++newly_shaded;
            }
        }
    }
    return newly_shaded;
}

} // namespace

vulkan_renderer::vulkan_renderer(vulkan_renderer_options options)
    : options_(std::move(options))
{
}

void vulkan_renderer::submit(const scene::placed_scene& placed_scene)
{
    last_draw_list_ = ui::ui_renderer{}.build_draw_list(placed_scene);
    last_frame_stats_ = count_commands(last_draw_list_, &placed_scene);
    last_frame_summary_ = summarize_cpu_fallback(last_draw_list_, last_frame_stats_, options_);
    last_framebuffer_ = rasterize_cpu_fallback_framebuffer(last_draw_list_, options_);
}

void vulkan_renderer::submit(const ui::ui_draw_list& draw_list)
{
    last_draw_list_ = draw_list;
    last_frame_stats_ = count_commands(last_draw_list_);
    last_frame_summary_ = summarize_cpu_fallback(last_draw_list_, last_frame_stats_, options_);
    last_framebuffer_ = rasterize_cpu_fallback_framebuffer(last_draw_list_, options_);
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
    last_frame_summary_ = {};
    last_framebuffer_ = {};
}

const ui::ui_draw_list& vulkan_renderer::last_draw_list() const
{
    return last_draw_list_;
}

const vulkan_renderer_frame_stats& vulkan_renderer::last_frame_stats() const
{
    return last_frame_stats_;
}

const vulkan_renderer_frame_summary& vulkan_renderer::last_frame_summary() const
{
    return last_frame_summary_;
}

const vulkan_renderer_framebuffer& vulkan_renderer::last_framebuffer() const
{
    return last_framebuffer_;
}

const vulkan_renderer_options& vulkan_renderer::options() const
{
    return options_;
}

void vulkan_renderer::set_options(vulkan_renderer_options options)
{
    options_ = std::move(options);
}

vulkan_renderer_frame_stats vulkan_renderer::count_commands(
    const ui::ui_draw_list& draw_list,
    const scene::placed_scene* placed_scene)
{
    vulkan_renderer_frame_stats stats;
    stats.command_count = draw_list.commands.size();
    std::vector<scene::scene_node_id> input_enabled_draw_node_ids;
    std::vector<scene::scene_node_id> disabled_draw_node_ids;

    for (const ui::ui_draw_command& command : draw_list.commands) {
        if (is_draw_command(command.type)) {
            ++stats.draw_call_count;
            if (command.paint.color.visible() && has_visible_area(command.bounds)) {
                ++stats.visible_draw_call_count;
            }

            if (const scene::placed_scene_node* node = find_command_node(placed_scene, command)) {
                count_input_draw_node(
                    stats,
                    *node,
                    input_enabled_draw_node_ids,
                    disabled_draw_node_ids);
                count_semantic_draw_command(stats, *node);
            }
        }

        switch (command.type) {
        case ui::ui_draw_command_type::quad:
            ++stats.quad_count;
            break;
        case ui::ui_draw_command_type::text:
            ++stats.text_count;
            stats.text_run_count += command.text_runs.size();
            for (const scene::scene_text_run& run : command.text_runs) {
                stats.text_character_count += run.text.size();
            }
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

vulkan_renderer_frame_summary vulkan_renderer::summarize_cpu_fallback(
    const ui::ui_draw_list& draw_list,
    const vulkan_renderer_frame_stats& stats,
    const vulkan_renderer_options& options)
{
    vulkan_renderer_frame_summary summary;
    summary.backend = vulkan_renderer_backend::cpu_fallback;
    summary.viewport = options.viewport;
    summary.surface_width = options.fallback_surface_width;
    summary.surface_height = options.fallback_surface_height;

    if (stats.draw_call_count == 0 || !has_visible_area(options.viewport)
        || options.fallback_surface_width == 0 || options.fallback_surface_height == 0) {
        summary.discarded_draw_call_count = stats.draw_call_count;
        return summary;
    }

    std::vector<unsigned char> coverage(options.fallback_surface_width * options.fallback_surface_height, 0);
    std::vector<scene::scene_rect> clip_stack;

    for (const ui::ui_draw_command& command : draw_list.commands) {
        if (command.type == ui::ui_draw_command_type::push_clip) {
            clip_stack.push_back(intersect_rect(command.bounds, active_clip_rect(options.viewport, clip_stack)));
            continue;
        }

        if (command.type == ui::ui_draw_command_type::pop_clip) {
            if (!clip_stack.empty()) {
                clip_stack.pop_back();
            }
            continue;
        }

        if (!is_draw_command(command.type)) {
            continue;
        }

        if (!command.paint.color.visible() || !has_visible_area(command.bounds)) {
            ++summary.discarded_draw_call_count;
            continue;
        }

        const scene::scene_rect clip = active_clip_rect(options.viewport, clip_stack);
        const scene::scene_rect clipped_bounds = intersect_rect(command.bounds, clip);
        if (!has_visible_area(clipped_bounds)) {
            ++summary.discarded_draw_call_count;
            continue;
        }

        if (rect_was_clipped(command.bounds, clipped_bounds)) {
            ++summary.clipped_draw_call_count;
        }

        summary.shaded_pixel_count += shade_rect(
            coverage,
            options.viewport,
            options.fallback_surface_width,
            options.fallback_surface_height,
            clipped_bounds);
    }

    return summary;
}

vulkan_renderer_framebuffer vulkan_renderer::rasterize_cpu_fallback_framebuffer(
    const ui::ui_draw_list& draw_list,
    const vulkan_renderer_options& options)
{
    vulkan_renderer_framebuffer framebuffer;
    framebuffer.width = options.fallback_surface_width;
    framebuffer.height = options.fallback_surface_height;

    if (framebuffer.width == 0 || framebuffer.height == 0) {
        return framebuffer;
    }

    framebuffer.rgba.assign(framebuffer.width * framebuffer.height * 4, 0);
    if (!has_visible_area(options.viewport)) {
        return framebuffer;
    }

    std::vector<scene::scene_rect> clip_stack;

    for (const ui::ui_draw_command& command : draw_list.commands) {
        if (command.type == ui::ui_draw_command_type::push_clip) {
            clip_stack.push_back(intersect_rect(command.bounds, active_clip_rect(options.viewport, clip_stack)));
            continue;
        }

        if (command.type == ui::ui_draw_command_type::pop_clip) {
            if (!clip_stack.empty()) {
                clip_stack.pop_back();
            }
            continue;
        }

        if (!is_draw_command(command.type) || !command.paint.color.visible()) {
            continue;
        }

        const scene::scene_rect raster_bounds = raster_bounds_for_command(command);
        if (!has_visible_area(raster_bounds)) {
            continue;
        }

        const scene::scene_rect clip = active_clip_rect(options.viewport, clip_stack);
        const scene::scene_rect clipped_bounds = intersect_rect(raster_bounds, clip);
        if (!has_visible_area(clipped_bounds)) {
            continue;
        }

        fill_rect(
            framebuffer.rgba,
            options.viewport,
            framebuffer.width,
            framebuffer.height,
            clipped_bounds,
            command.paint.color);
    }

    return framebuffer;
}

} // namespace quiz_vulkan::render
