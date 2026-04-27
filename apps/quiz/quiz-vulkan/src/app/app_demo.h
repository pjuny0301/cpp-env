#pragma once

#include "core/domain/app_snapshot.hpp"
#include "core/domain/quiz_model.hpp"
#include "core/layout/layout_placer.h"
#include "core/scene/scene_layout_data.h"
#include "render/vulkan/vulkan_renderer.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace quiz_vulkan {

struct app_render_report {
    std::string screen_id;
    std::size_t node_count = 0;
    std::size_t input_region_count = 0;
    render::vulkan_renderer_frame_stats frame_stats;
    render::vulkan_renderer_frame_summary frame_summary;
};

struct app_render_frame {
    app_render_report report;
    scene::placed_scene placed_scene;
};

domain::deck make_demo_deck();
app_render_frame render_app_frame(
    const domain::app_snapshot& snapshot,
    scene::scene_rect viewport = {0.0f, 0.0f, 1280.0f, 720.0f});
app_render_report render_app_snapshot(
    const domain::app_snapshot& snapshot,
    scene::scene_rect viewport = {0.0f, 0.0f, 1280.0f, 720.0f});
std::string format_render_report(std::string_view label, const app_render_report& report);

} // namespace quiz_vulkan
