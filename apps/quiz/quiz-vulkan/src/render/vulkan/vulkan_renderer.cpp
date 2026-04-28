#include "render/vulkan/vulkan_renderer.h"
#include "render/vulkan/vulkan_backend_adapter.h"
#include "render/vulkan/vulkan_frame_plan.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {
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

void fill_rect(
    std::vector<unsigned char>& rgba,
    const render_rect& viewport,
    std::size_t surface_width,
    std::size_t surface_height,
    const render_rect& rect,
    const render_color& color)
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
    const render_rect& viewport,
    std::size_t surface_width,
    std::size_t surface_height,
    const render_rect& rect)
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

vulkan_backend::vulkan_backend_frame_result submit_optional_vulkan_backend_frame(
    const render_draw_list& draw_list,
    const vulkan_renderer_options& options)
{
    if (!options.prefer_vulkan) {
        return {};
    }

    vulkan_backend::null_vulkan_backend_device device;
    return vulkan_backend::submit_vulkan_backend_frame(device, draw_list, options.viewport);
}

} // namespace

vulkan_renderer::vulkan_renderer(vulkan_renderer_options options)
    : options_(std::move(options))
{
}

void vulkan_renderer::submit(const render_draw_list& draw_list)
{
    last_draw_list_ = draw_list;
    last_frame_stats_ = count_commands(last_draw_list_);
    last_backend_frame_result_ = submit_optional_vulkan_backend_frame(last_draw_list_, options_);
    last_frame_summary_ = summarize_cpu_fallback(
        last_draw_list_,
        last_frame_stats_,
        last_backend_frame_result_,
        options_);
    last_framebuffer_ = rasterize_cpu_fallback_framebuffer(last_draw_list_, options_);
}

void vulkan_renderer::submit(const std::vector<render_draw_command>& commands)
{
    render_draw_list draw_list;
    draw_list.commands = commands;
    submit(draw_list);
}

void vulkan_renderer::clear()
{
    last_draw_list_.clear();
    last_frame_stats_ = {};
    last_frame_summary_ = {};
    last_backend_frame_result_ = {};
    last_framebuffer_ = {};
}

const render_draw_list& vulkan_renderer::last_draw_list() const
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

const vulkan_backend::vulkan_backend_frame_result& vulkan_renderer::last_backend_frame_result() const
{
    return last_backend_frame_result_;
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
    const render_draw_list& draw_list)
{
    vulkan_renderer_frame_stats stats;
    stats.command_count = draw_list.commands.size();

    for (const render_draw_command& command : draw_list.commands) {
        if (is_draw_command(command.type)) {
            ++stats.draw_call_count;
            if (command.paint.color.visible() && has_visible_area(command.bounds)) {
                ++stats.visible_draw_call_count;
            }
        }

        switch (command.type) {
        case render_draw_command_type::quad:
            ++stats.quad_count;
            break;
        case render_draw_command_type::text:
            ++stats.text_count;
            stats.text_run_count += command.text_runs.size();
            for (const render_text_run& run : command.text_runs) {
                stats.text_character_count += run.text.size();
            }
            break;
        case render_draw_command_type::image:
            ++stats.image_count;
            break;
        case render_draw_command_type::push_clip:
            ++stats.push_clip_count;
            break;
        case render_draw_command_type::pop_clip:
            ++stats.pop_clip_count;
            break;
        case render_draw_command_type::debug_bounds:
            ++stats.debug_bounds_count;
            break;
        }
    }

    return stats;
}

vulkan_renderer_frame_summary vulkan_renderer::summarize_cpu_fallback(
    const render_draw_list& draw_list,
    const vulkan_renderer_frame_stats& stats,
    const vulkan_backend::vulkan_backend_frame_result& backend_result,
    const vulkan_renderer_options& options)
{
    vulkan_renderer_frame_summary summary;
    summary.backend = vulkan_renderer_backend::cpu_fallback;
    summary.viewport = options.viewport;
    summary.surface_width = options.fallback_surface_width;
    summary.surface_height = options.fallback_surface_height;
    summary.backend_surface_width = backend_result.surface.width;
    summary.backend_surface_height = backend_result.surface.height;
    summary.backend_planned_batch_count = backend_result.planned_batch_count;
    summary.backend_surface_ready = backend_result.surface_ready;
    summary.backend_frame_begun = backend_result.frame_begun;
    summary.backend_commands_recorded = backend_result.commands_recorded;
    summary.backend_frame_submitted = backend_result.frame_submitted;
    summary.backend_frame_presented = backend_result.frame_presented;
    summary.backend_attempted = backend_result.attempted;
    summary.backend_fallback_required = backend_result.fallback_required;
    summary.backend_fallback_reason = backend_result.fallback_reason;

    if (stats.draw_call_count == 0 || !has_visible_area(options.viewport)
        || options.fallback_surface_width == 0 || options.fallback_surface_height == 0) {
        summary.discarded_draw_call_count = stats.draw_call_count;
        return summary;
    }

    std::vector<unsigned char> coverage(options.fallback_surface_width * options.fallback_surface_height, 0);
    const vulkan_backend::vulkan_frame_plan plan = vulkan_backend::build_vulkan_frame_plan(
        draw_list,
        vulkan_backend::vulkan_frame_plan_options{
            .viewport = options.viewport,
            .surface_width = options.fallback_surface_width,
            .surface_height = options.fallback_surface_height,
        });

    summary.clipped_draw_call_count = plan.clipped_draw_call_count;
    summary.discarded_draw_call_count = plan.discarded_draw_call_count;

    for (const vulkan_backend::vulkan_draw_batch& batch : plan.batches) {
        summary.shaded_pixel_count += shade_rect(
            coverage,
            options.viewport,
            options.fallback_surface_width,
            options.fallback_surface_height,
            batch.clipped_bounds);
    }

    return summary;
}

vulkan_renderer_framebuffer vulkan_renderer::rasterize_cpu_fallback_framebuffer(
    const render_draw_list& draw_list,
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

    const vulkan_backend::vulkan_frame_plan plan = vulkan_backend::build_vulkan_frame_plan(
        draw_list,
        vulkan_backend::vulkan_frame_plan_options{
            .viewport = options.viewport,
            .surface_width = framebuffer.width,
            .surface_height = framebuffer.height,
        });

    for (const vulkan_backend::vulkan_draw_batch& batch : plan.batches) {
        if (batch.kind == vulkan_backend::vulkan_batch_kind::text) {
            continue;
        }

        fill_rect(
            framebuffer.rgba,
            options.viewport,
            framebuffer.width,
            framebuffer.height,
            batch.clipped_bounds,
            batch.paint.color);
    }

    return framebuffer;
}

} // namespace quiz_vulkan::render
