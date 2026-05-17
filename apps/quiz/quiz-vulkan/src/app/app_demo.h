#pragma once

#include "core/domain/app_snapshot.hpp"
#include "core/domain/quiz_model.hpp"
#include "core/scene/placed_scene.h"
#include "core/scene/scene_layout_data.h"
#include "render/text/text_engine.h"
#include "render/vulkan/vulkan_renderer.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace quiz_vulkan::render {
class image_resolver_interface;
class image_texture_pipeline_interface;
} // namespace quiz_vulkan::render

namespace quiz_vulkan {

struct app_render_report {
    std::string screen_id;
    std::size_t node_count = 0;
    std::size_t input_region_count = 0;
    std::size_t modifier_error_count = 0;
    std::string first_modifier_error;
    render::vulkan_renderer_frame_stats frame_stats;
    render::vulkan_renderer_frame_summary frame_summary;
    std::size_t image_texture_command_count = 0;
    std::size_t image_texture_request_count = 0;
    std::size_t image_texture_ready_count = 0;
    std::size_t image_texture_failure_count = 0;
    std::size_t image_texture_mapped_count = 0;
    bool image_texture_pipeline_ran = false;
    bool image_texture_handoff_ready = true;
    bool image_texture_renderer_handoff_ready = true;
    std::string image_texture_diagnostic;
};

struct app_render_frame {
    app_render_report report;
    scene::placed_scene placed_scene;
    render::vulkan_renderer_framebuffer framebuffer;
};

struct app_render_view_state {
    std::string_view typed_text_answer;
};

domain::deck make_demo_deck();
app_render_frame render_app_frame(
    const domain::app_snapshot& snapshot,
    scene::scene_rect viewport = {0.0f, 0.0f, 1280.0f, 720.0f},
    app_render_view_state view_state = {});
app_render_frame render_app_frame_with_engines(
    const domain::app_snapshot& snapshot,
    scene::scene_rect viewport,
    app_render_view_state view_state,
    render::text_engine_interface& text_engine,
    render::vulkan_renderer& renderer,
    render::image_texture_pipeline_interface* image_texture_pipeline = nullptr,
    const render::image_resolver_interface* image_resolver = nullptr);
app_render_report render_app_snapshot(
    const domain::app_snapshot& snapshot,
    scene::scene_rect viewport = {0.0f, 0.0f, 1280.0f, 720.0f});
std::string format_render_report(std::string_view label, const app_render_report& report);

} // namespace quiz_vulkan
