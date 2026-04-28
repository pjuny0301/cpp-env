#pragma once

#include "render/render_draw_list.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace quiz_vulkan::render::vulkan_backend {

struct vulkan_scissor_rect {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    bool empty() const
    {
        return width == 0 || height == 0;
    }
};

struct vulkan_quad_vertex {
    float x = 0.0f;
    float y = 0.0f;
    float u = 0.0f;
    float v = 0.0f;
    render_color color;
};

enum class vulkan_batch_kind {
    quad,
    text,
    image,
    debug_bounds,
};

struct vulkan_draw_batch {
    vulkan_batch_kind kind = vulkan_batch_kind::quad;
    std::size_t command_index = 0;
    render_node_id node_id;
    render_rect bounds;
    render_rect clipped_bounds;
    render_paint paint;
    vulkan_scissor_rect scissor;
    std::array<vulkan_quad_vertex, 4> vertices{};
};

struct vulkan_frame_plan_options {
    render_rect viewport;
    std::size_t surface_width = 0;
    std::size_t surface_height = 0;
};

struct vulkan_frame_plan {
    render_rect viewport;
    std::size_t surface_width = 0;
    std::size_t surface_height = 0;
    std::size_t clipped_draw_call_count = 0;
    std::size_t discarded_draw_call_count = 0;
    std::vector<vulkan_draw_batch> batches;

    bool empty() const
    {
        return batches.empty();
    }
};

vulkan_frame_plan build_vulkan_frame_plan(
    const render_draw_list& draw_list,
    const vulkan_frame_plan_options& options);

} // namespace quiz_vulkan::render::vulkan_backend
