#include "app/app_demo.h"
#include "app/app_state.h"
#include "core/domain/app_action.hpp"

#include <cassert>
#include <string>

int main()
{
    using namespace quiz_vulkan;

    app_state state({make_demo_deck()});

    app_render_report deck_report = render_app_snapshot(state.snapshot());
    assert(deck_report.screen_id == "deck_list");
    assert(deck_report.node_count > 0);
    assert(deck_report.frame_summary.nonblank());

    state.dispatch(domain::make_select_deck_action("demo_deck"));
    state.dispatch(domain::make_select_day_action("day_1"));
    state.dispatch(domain::make_start_quiz_action(domain::quiz_mode::normal), 100);

    app_render_report active_report = render_app_snapshot(state.snapshot());
    assert(active_report.screen_id == "quiz_active");
    assert(active_report.input_region_count > 0);
    assert(active_report.frame_stats.draw_call_count > 0);
    assert(active_report.frame_summary.nonblank());

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
