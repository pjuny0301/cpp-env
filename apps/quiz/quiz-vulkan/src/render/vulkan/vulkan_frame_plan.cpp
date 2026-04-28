#include "render/vulkan/vulkan_frame_plan.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace quiz_vulkan::render::vulkan_backend {
namespace {

bool has_visible_area(const render_rect& rect)
{
    return rect.width > 0.0f && rect.height > 0.0f;
}

bool is_draw_command(render_draw_command_type type)
{
    switch (type) {
    case render_draw_command_type::quad:
    case render_draw_command_type::text:
    case render_draw_command_type::image:
    case render_draw_command_type::debug_bounds:
        return true;
    case render_draw_command_type::push_clip:
    case render_draw_command_type::pop_clip:
        return false;
    }

    return false;
}

vulkan_batch_kind batch_kind_for(render_draw_command_type type)
{
    switch (type) {
    case render_draw_command_type::quad:
        return vulkan_batch_kind::quad;
    case render_draw_command_type::text:
        return vulkan_batch_kind::text;
    case render_draw_command_type::image:
        return vulkan_batch_kind::image;
    case render_draw_command_type::debug_bounds:
        return vulkan_batch_kind::debug_bounds;
    case render_draw_command_type::push_clip:
    case render_draw_command_type::pop_clip:
        break;
    }

    return vulkan_batch_kind::quad;
}

render_rect intersect_rect(const render_rect& left, const render_rect& right)
{
    const float x0 = std::max(left.x, right.x);
    const float y0 = std::max(left.y, right.y);
    const float x1 = std::min(left.right(), right.right());
    const float y1 = std::min(left.bottom(), right.bottom());

    if (x1 <= x0 || y1 <= y0) {
        return render_rect{x0, y0, 0.0f, 0.0f};
    }

    return render_rect{x0, y0, x1 - x0, y1 - y0};
}

bool rect_was_clipped(const render_rect& original, const render_rect& clipped)
{
    return clipped.x > original.x || clipped.y > original.y
        || clipped.right() < original.right() || clipped.bottom() < original.bottom();
}

render_rect active_clip_rect(
    const render_rect& viewport,
    const std::vector<render_rect>& clip_stack)
{
    render_rect clip = viewport;
    for (const render_rect& scope : clip_stack) {
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

vulkan_scissor_rect make_scissor(
    const render_rect& viewport,
    std::size_t surface_width,
    std::size_t surface_height,
    const render_rect& rect)
{
    if (!has_visible_area(viewport) || !has_visible_area(rect)
        || surface_width == 0 || surface_height == 0) {
        return {};
    }

    const int x0 = surface_coordinate(rect.x, viewport.x, viewport.width, surface_width, false);
    const int y0 = surface_coordinate(rect.y, viewport.y, viewport.height, surface_height, false);
    const int x1 = surface_coordinate(rect.right(), viewport.x, viewport.width, surface_width, true);
    const int y1 = surface_coordinate(rect.bottom(), viewport.y, viewport.height, surface_height, true);

    if (x1 <= x0 || y1 <= y0) {
        return {};
    }

    return vulkan_scissor_rect{
        .x = static_cast<std::int32_t>(x0),
        .y = static_cast<std::int32_t>(y0),
        .width = static_cast<std::uint32_t>(x1 - x0),
        .height = static_cast<std::uint32_t>(y1 - y0),
    };
}

std::array<vulkan_quad_vertex, 4> make_quad_vertices(
    const render_rect& rect,
    const render_color& color)
{
    const float x0 = rect.x;
    const float y0 = rect.y;
    const float x1 = rect.right();
    const float y1 = rect.bottom();

    return {
        vulkan_quad_vertex{.x = x0, .y = y0, .u = 0.0f, .v = 0.0f, .color = color},
        vulkan_quad_vertex{.x = x1, .y = y0, .u = 1.0f, .v = 0.0f, .color = color},
        vulkan_quad_vertex{.x = x1, .y = y1, .u = 1.0f, .v = 1.0f, .color = color},
        vulkan_quad_vertex{.x = x0, .y = y1, .u = 0.0f, .v = 1.0f, .color = color},
    };
}

} // namespace

vulkan_frame_plan build_vulkan_frame_plan(
    const render_draw_list& draw_list,
    const vulkan_frame_plan_options& options)
{
    vulkan_frame_plan plan;
    plan.viewport = options.viewport;
    plan.surface_width = options.surface_width;
    plan.surface_height = options.surface_height;

    std::vector<render_rect> clip_stack;

    for (std::size_t command_index = 0; command_index < draw_list.commands.size(); ++command_index) {
        const render_draw_command& command = draw_list.commands[command_index];
        if (command.type == render_draw_command_type::push_clip) {
            clip_stack.push_back(intersect_rect(command.bounds, active_clip_rect(options.viewport, clip_stack)));
            continue;
        }

        if (command.type == render_draw_command_type::pop_clip) {
            if (!clip_stack.empty()) {
                clip_stack.pop_back();
            }
            continue;
        }

        if (!is_draw_command(command.type)) {
            continue;
        }

        if (!command.paint.color.visible() || !has_visible_area(command.bounds)
            || !has_visible_area(options.viewport)
            || options.surface_width == 0 || options.surface_height == 0) {
            ++plan.discarded_draw_call_count;
            continue;
        }

        const render_rect clip = active_clip_rect(options.viewport, clip_stack);
        const render_rect clipped_bounds = intersect_rect(command.bounds, clip);
        if (!has_visible_area(clipped_bounds)) {
            ++plan.discarded_draw_call_count;
            continue;
        }

        vulkan_scissor_rect scissor = make_scissor(
            options.viewport,
            options.surface_width,
            options.surface_height,
            clipped_bounds);
        if (scissor.empty()) {
            ++plan.discarded_draw_call_count;
            continue;
        }

        if (rect_was_clipped(command.bounds, clipped_bounds)) {
            ++plan.clipped_draw_call_count;
        }

        plan.batches.push_back(vulkan_draw_batch{
            .kind = batch_kind_for(command.type),
            .command_index = command_index,
            .node_id = command.node_id,
            .bounds = command.bounds,
            .clipped_bounds = clipped_bounds,
            .paint = command.paint,
            .scissor = scissor,
            .vertices = make_quad_vertices(clipped_bounds, command.paint.color),
        });
    }

    return plan;
}

} // namespace quiz_vulkan::render::vulkan_backend
