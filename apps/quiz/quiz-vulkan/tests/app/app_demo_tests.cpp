#include "app/app_demo.h"
#include "app/app_render_pipeline.h"
#include "app/app_state.h"
#include "core/domain/app_action.hpp"

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <utility>

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

std::filesystem::path write_ppm_image_fixture()
{
    const std::filesystem::path path = std::filesystem::current_path() / "app_demo_image_fixture.ppm";
    std::ofstream file(path, std::ios::binary);
    file << "P6\n1 1\n255\n";
    const unsigned char pixel[] = {0xff, 0x00, 0x00};
    file.write(reinterpret_cast<const char*>(pixel), sizeof(pixel));
    return path;
}

quiz_vulkan::domain::deck make_image_demo_deck(std::string image_uri)
{
    quiz_vulkan::domain::deck deck = quiz_vulkan::make_demo_deck();
    deck.days.front().questions.front().image_uri = std::move(image_uri);
    return deck;
}

int main()
{
    using namespace quiz_vulkan;

    app_state state({make_demo_deck()});

    app_render_frame deck_frame = render_app_frame(state.snapshot());
    const app_render_report& deck_report = deck_frame.report;
    assert(deck_report.screen_id == "deck_list");
    assert(deck_report.modifier_error_count == 0);
    assert(deck_report.first_modifier_error.empty());
    assert(deck_report.node_count > 0);
    assert(deck_report.frame_summary.nonblank());
    assert(deck_report.frame_stats.text_count > 0);
    assert(deck_report.frame_stats.quad_count > 0);
    assert(count_nonzero_framebuffer_pixels(deck_frame.framebuffer) > 0);

    state.dispatch(domain::make_select_deck_action("demo_deck"));
    state.dispatch(domain::make_select_day_action("day_1"));
    state.dispatch(domain::make_start_quiz_action(domain::quiz_mode::normal), 100);

    app_render_frame active_frame = render_app_frame(state.snapshot());
    const app_render_report& active_report = active_frame.report;
    assert(active_report.screen_id == "quiz_active");
    assert(active_report.modifier_error_count == 0);
    assert(active_report.input_region_count > 0);
    assert(active_report.frame_stats.draw_call_count > 0);
    assert(active_report.frame_summary.nonblank());

    domain::app_snapshot active_snapshot = state.snapshot();
    default_app_render_pipeline pipeline;
    app_render_frame pipeline_frame = pipeline.render(app_render_request{
        .snapshot = &active_snapshot,
    });
    assert(pipeline_frame.report.screen_id == "quiz_active");
    assert(pipeline.renderer().last_frame_stats().command_count == pipeline_frame.report.frame_stats.command_count);
    assert(pipeline.renderer().last_draw_list().size() == pipeline_frame.report.frame_stats.command_count);

    const std::filesystem::path image_fixture = write_ppm_image_fixture();
    app_state image_state({make_image_demo_deck(image_fixture.filename().generic_string())});
    image_state.dispatch(domain::make_select_deck_action("demo_deck"));
    image_state.dispatch(domain::make_select_day_action("day_1"));
    image_state.dispatch(domain::make_start_quiz_action(domain::quiz_mode::normal), 100);
    domain::app_snapshot image_snapshot = image_state.snapshot();
    default_app_render_pipeline image_pipeline(default_app_render_pipeline_config{
        .image_base_directory = image_fixture.parent_path(),
    });
    app_render_frame image_frame = image_pipeline.render(app_render_request{
        .snapshot = &image_snapshot,
    });
    assert(image_frame.report.screen_id == "quiz_active");
    assert(image_frame.report.frame_stats.image_count > 0);
    assert(image_frame.report.image_texture_command_count == 1);
    assert(image_frame.report.image_texture_request_count == 1);
    assert(image_frame.report.image_texture_pipeline_ran);
    assert(image_frame.report.image_texture_handoff_ready);
    if (!image_frame.report.image_texture_renderer_handoff_ready) {
        std::fprintf(
            stderr,
            "image texture report: requests=%zu ready=%zu failures=%zu mapped=%zu diagnostic=%s\n",
            image_frame.report.image_texture_request_count,
            image_frame.report.image_texture_ready_count,
            image_frame.report.image_texture_failure_count,
            image_frame.report.image_texture_mapped_count,
            image_frame.report.image_texture_diagnostic.c_str());
    }
    assert(image_frame.report.image_texture_renderer_handoff_ready);
    assert(image_frame.report.image_texture_ready_count == 1);
    assert(image_frame.report.image_texture_mapped_count == 1);
    assert(image_frame.report.image_texture_frame_entry_count == 1);
    assert(image_frame.report.image_texture_upload_result_available);
    assert(image_frame.report.image_texture_resource_packets_ready);
    assert(image_frame.report.image_texture_resource_packet_count == 1);
    assert(image_frame.report.image_texture_resource_ready_count == 1);
    assert(image_frame.report.image_texture_quad_packets_ready);
    assert(image_frame.report.image_texture_quad_packet_count == 1);
    assert(image_frame.report.image_texture_quad_ready_count == 1);
    assert(image_frame.report.image_texture_draw_payloads_ready);
    assert(image_frame.report.image_texture_payload_count == 1);
    assert(image_frame.report.image_texture_payload_ready_count == 1);
    assert(image_frame.report.image_texture_payload_placeholder_count == 0);
    assert(image_frame.report.image_texture_payload_blocked_count == 0);
    assert(image_pipeline.image_texture_pipeline().diagnostic_snapshot().ready_count == 1);
    assert(image_pipeline.image_source_bytes_loader().base_directory() == image_fixture.parent_path());

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
        quiz_vulkan::scene::scene_rect{.x = 0.0f, .y = 0.0f, .width = 1280.0f, .height = 720.0f},
        app_render_view_state{.typed_text_answer = "typed-native-answer"});
    const scene::placed_scene_node* typed_answer = typed_blank_frame.placed_scene.find_node("quiz_active_text_answer");
    assert(typed_answer != nullptr);
    assert(!typed_answer->text_runs.empty());
    assert(typed_answer->text_runs.front().text == "typed-native-answer");
    assert(typed_blank_frame.report.frame_stats.text_character_count >= std::string{"typed-native-answer"}.size());

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
