#include "app/app_demo.h"
#include "app/app_state.h"
#include "core/domain/app_action.hpp"

#include <cassert>
#include <cstddef>
#include <string>
#include <vector>

namespace {

bool has_text_overlay_containing(
    const std::vector<quiz_vulkan::platform_text_overlay>& overlays,
    const std::string& expected)
{
    for (const quiz_vulkan::platform_text_overlay& overlay : overlays) {
        if (overlay.text.find(expected) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::size_t count_nonzero_framebuffer_pixels(
    const quiz_vulkan::render::vulkan_renderer_framebuffer& framebuffer)
{
    std::size_t count = 0;
    for (std::size_t offset = 0; offset + 3 < framebuffer.rgba.size(); offset += 4) {
        if (framebuffer.rgba[offset] != 0 || framebuffer.rgba[offset + 1] != 0
            || framebuffer.rgba[offset + 2] != 0 || framebuffer.rgba[offset + 3] != 0) {
            ++count;
        }
    }
    return count;
}

bool pixel_inside_text_overlay(
    std::size_t x,
    std::size_t y,
    const quiz_vulkan::platform_text_overlay& overlay)
{
    const float pixel_center_x = static_cast<float>(x) + 0.5f;
    const float pixel_center_y = static_cast<float>(y) + 0.5f;
    return pixel_center_x >= overlay.x
        && pixel_center_x < overlay.x + overlay.width
        && pixel_center_y >= overlay.y
        && pixel_center_y < overlay.y + overlay.height;
}

bool pixel_inside_any_text_overlay(
    std::size_t x,
    std::size_t y,
    const std::vector<quiz_vulkan::platform_text_overlay>& overlays)
{
    for (const quiz_vulkan::platform_text_overlay& overlay : overlays) {
        if (pixel_inside_text_overlay(x, y, overlay)) {
            return true;
        }
    }
    return false;
}

std::size_t count_nonzero_framebuffer_pixels_outside_text_overlays(
    const quiz_vulkan::render::vulkan_renderer_framebuffer& framebuffer,
    const std::vector<quiz_vulkan::platform_text_overlay>& overlays)
{
    std::size_t count = 0;
    for (std::size_t offset = 0; offset + 3 < framebuffer.rgba.size(); offset += 4) {
        if (framebuffer.rgba[offset] == 0 && framebuffer.rgba[offset + 1] == 0
            && framebuffer.rgba[offset + 2] == 0 && framebuffer.rgba[offset + 3] == 0) {
            continue;
        }

        const std::size_t pixel_index = offset / 4;
        const std::size_t x = pixel_index % framebuffer.width;
        const std::size_t y = pixel_index / framebuffer.width;
        if (!pixel_inside_any_text_overlay(x, y, overlays)) {
            ++count;
        }
    }
    return count;
}

} // namespace

int main()
{
    using namespace quiz_vulkan;

    app_state state({make_demo_deck()});

    app_render_frame deck_frame = render_app_frame(state.snapshot());
    const app_render_report& deck_report = deck_frame.report;
    assert(deck_report.screen_id == "deck_list");
    assert(deck_report.node_count > 0);
    assert(deck_report.frame_summary.nonblank());
    assert(!deck_frame.text_overlays.empty());
    assert(has_text_overlay_containing(deck_frame.text_overlays, "Native Quiz Demo"));
    assert(deck_report.frame_stats.text_count > 0);
    assert(deck_report.frame_stats.quad_count > 0);
    assert(count_nonzero_framebuffer_pixels(deck_frame.framebuffer) > 0);
    assert(count_nonzero_framebuffer_pixels_outside_text_overlays(
        deck_frame.framebuffer,
        deck_frame.text_overlays) > 0);

    state.dispatch(domain::make_select_deck_action("demo_deck"));
    state.dispatch(domain::make_select_day_action("day_1"));
    state.dispatch(domain::make_start_quiz_action(domain::quiz_mode::normal), 100);

    app_render_frame active_frame = render_app_frame(state.snapshot());
    const app_render_report& active_report = active_frame.report;
    assert(active_report.screen_id == "quiz_active");
    assert(active_report.input_region_count > 0);
    assert(active_report.frame_stats.draw_call_count > 0);
    assert(active_report.frame_summary.nonblank());
    assert(!active_frame.text_overlays.empty());
    assert(has_text_overlay_containing(active_frame.text_overlays, "Skip"));

    state.dispatch(domain::make_submit_option_action(0), 200);
    app_render_report feedback_report = render_app_snapshot(state.snapshot());
    assert(feedback_report.screen_id == "quiz_feedback");
    assert(feedback_report.frame_summary.nonblank());

    state.dispatch(domain::make_continue_after_feedback_action(), 300);
    app_render_report blank_input_report = render_app_snapshot(state.snapshot());
    assert(blank_input_report.screen_id == "quiz_active");
    assert(blank_input_report.input_region_count > 0);
    assert(blank_input_report.frame_summary.nonblank());
    assert(blank_input_report.frame_summary.shaded_pixel_count > 0);

    app_render_frame typed_blank_frame = render_app_frame(
        state.snapshot(),
        quiz_vulkan::scene::scene_rect{0.0f, 0.0f, 1280.0f, 720.0f},
        app_render_view_state{"typed-native-answer"});
    const scene::placed_scene_node* typed_answer = typed_blank_frame.placed_scene.find_node("quiz_active_text_answer");
    assert(typed_answer != nullptr);
    assert(!typed_answer->text_runs.empty());
    assert(typed_answer->text_runs.front().text == "typed-native-answer");
    assert(has_text_overlay_containing(typed_blank_frame.text_overlays, "typed-native-answer"));

    state.dispatch(domain::make_submit_text_answer_action("scene snapshot"), 400);
    app_render_report blank_feedback_report = render_app_snapshot(state.snapshot());
    assert(blank_feedback_report.screen_id == "quiz_feedback");
    assert(blank_feedback_report.frame_summary.nonblank());

    state.dispatch(domain::make_continue_after_feedback_action(), 500);
    app_render_report completed_report = render_app_snapshot(state.snapshot());
    assert(completed_report.screen_id == "quiz_results");
    assert(completed_report.frame_summary.nonblank());

    const std::string formatted = format_render_report("feedback", feedback_report);
    assert(formatted.find("feedback") != std::string::npos);
    assert(formatted.find("nonblank=true") != std::string::npos);

    return 0;
}
