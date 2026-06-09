#include "app/app_scene_scenario.h"

#include <cassert>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace {

void require(bool condition, const char* message)
{
    assert((condition) && message);
}

class fixed_text_metrics final : public quiz_vulkan::scene::text_metrics_interface {
public:
    quiz_vulkan::scene::scene_size measure_text(
        const std::vector<quiz_vulkan::scene::scene_text_run>& text_runs,
        const quiz_vulkan::scene::scene_style&,
        float) const override
    {
        std::size_t character_count = 0;
        for (const quiz_vulkan::scene::scene_text_run& run : text_runs) {
            character_count += run.text.size();
        }
        return quiz_vulkan::scene::scene_size{static_cast<float>(character_count * 8), 18.0f};
    }
};

quiz_vulkan::domain::deck make_test_deck()
{
    using namespace quiz_vulkan::domain;

    question quiz_question;
    quiz_question.id = "q1";
    quiz_question.prompt = "Capital of Korea?";
    quiz_question.type = question_type::answer;
    quiz_question.options.push_back(option{"Seoul", true});
    quiz_question.options.push_back(option{"Busan", false});

    day quiz_day;
    quiz_day.id = "day1";
    quiz_day.title = "Day 1";
    quiz_day.questions.push_back(std::move(quiz_question));

    deck quiz_deck;
    quiz_deck.id = "deck1";
    quiz_deck.title = "Geography";
    quiz_deck.days.push_back(std::move(quiz_day));
    return quiz_deck;
}

quiz_vulkan::domain::deck make_two_question_deck()
{
    using namespace quiz_vulkan::domain;

    question first_question;
    first_question.id = "q1";
    first_question.prompt = "Capital of Korea?";
    first_question.type = question_type::answer;
    first_question.options.push_back(option{"Seoul", true});
    first_question.options.push_back(option{"Busan", false});

    question second_question;
    second_question.id = "q2";
    second_question.prompt = "Capital of Japan?";
    second_question.type = question_type::answer;
    second_question.options.push_back(option{"Tokyo", true});
    second_question.options.push_back(option{"Osaka", false});

    day quiz_day;
    quiz_day.id = "day1";
    quiz_day.title = "Day 1";
    quiz_day.questions.push_back(std::move(first_question));
    quiz_day.questions.push_back(std::move(second_question));

    deck quiz_deck;
    quiz_deck.id = "deck1";
    quiz_deck.title = "Geography";
    quiz_deck.days.push_back(std::move(quiz_day));
    return quiz_deck;
}

quiz_vulkan::domain::deck make_blank_text_deck()
{
    using namespace quiz_vulkan::domain;

    question quiz_question;
    quiz_question.id = "q_blank";
    quiz_question.prompt = "Capital of Korea is ____.";
    quiz_question.type = question_type::blank;
    quiz_question.accepted_answers.push_back("Seoul");

    day quiz_day;
    quiz_day.id = "day1";
    quiz_day.title = "Day 1";
    quiz_day.questions.push_back(std::move(quiz_question));

    deck quiz_deck;
    quiz_deck.id = "deck1";
    quiz_deck.title = "Geography";
    quiz_deck.days.push_back(std::move(quiz_day));
    return quiz_deck;
}

quiz_vulkan::domain::deck make_multiselect_deck()
{
    using namespace quiz_vulkan::domain;

    question quiz_question;
    quiz_question.id = "q_multi";
    quiz_question.prompt = "Select the capital of Korea.";
    quiz_question.type = question_type::multiselect;
    quiz_question.options.push_back(option{"Seoul", true});
    quiz_question.options.push_back(option{"Busan", false});

    day quiz_day;
    quiz_day.id = "day1";
    quiz_day.title = "Day 1";
    quiz_day.questions.push_back(std::move(quiz_question));

    deck quiz_deck;
    quiz_deck.id = "deck1";
    quiz_deck.title = "Geography";
    quiz_deck.days.push_back(std::move(quiz_day));
    return quiz_deck;
}

void require_trace_entry(
    const quiz_vulkan::app_scene_scenario_trace_entry& entry,
    const char* before_screen,
    const char* action_type,
    const char* after_screen,
    const char* message)
{
    require(entry.before_screen_id == before_screen, message);
    require(entry.action_type == action_type, message);
    require(entry.after_screen_id == after_screen, message);
    require(entry.handled, message);
    require(entry.needs_render, message);
    require(entry.before_node_count > 1, message);
    require(entry.after_node_count > 1, message);
    require(entry.before_input_region_count > 0, message);
    require(entry.after_input_region_count > 0, message);
}

void test_quiz_scene_event_replay_reaches_results()
{
    using namespace quiz_vulkan;

    app_state state({make_test_deck()});
    state.dispatch(domain::make_select_day_action("day1"), 10);

    const fixed_text_metrics metrics;
    const app_scene_scenario_result result = run_app_scene_scenario(
        state,
        {
            app_scene_scenario_step{
                .name = "start_normal",
                .input = app_scene_scenario_input_kind::tap_node,
                .target_node_id = "day_intro_start_normal",
                .now_ms = 100,
            },
            app_scene_scenario_step{
                .name = "answer_first_option",
                .input = app_scene_scenario_input_kind::tap_node,
                .target_node_id = "quiz_active_option_0",
                .now_ms = 200,
            },
            app_scene_scenario_step{
                .name = "continue_feedback",
                .input = app_scene_scenario_input_kind::swipe_right,
                .now_ms = 300,
            },
        },
        {0.0f, 0.0f, 360.0f, 640.0f},
        metrics);

    require(result.ok(), "scenario replay succeeds");
    require(result.trace.size() == 3, "scenario emits one trace entry per step");
    require_trace_entry(result.trace[0], "day_intro", "start_quiz", "quiz_active", "start step trace is stable");
    require(result.trace[0].before_focus_id == "day_intro_start_normal", "start step captures day intro focus");
    require(result.trace[0].after_focus_id == "quiz_active_option_0", "start step captures active focus");

    require_trace_entry(result.trace[1], "quiz_active", "submit_option", "quiz_feedback", "answer step trace is stable");
    require(result.trace[1].target_node_id == "quiz_active_option_0", "answer step records target node");
    require(result.trace[1].after_focus_id == "quiz_feedback_continue", "answer step captures feedback focus");

    require_trace_entry(result.trace[2], "quiz_feedback", "continue_after_feedback", "quiz_results", "continue step trace is stable");
    require(result.trace[2].event_kind == "swipe_right", "continue step records gesture kind");
    require(result.final_frame.layout.route_state().screen_id == "quiz_results", "final frame is results");
    require(result.final_frame.layout.contains_node("quiz_results_actions"), "final frame emits results actions");
    require(result.final_frame.snapshot.screen == domain::app_screen::completed, "final snapshot is completed");
}

void test_quiz_scene_deck_navigation_replay_reaches_day_intro()
{
    using namespace quiz_vulkan;

    app_state state({make_test_deck()});

    const fixed_text_metrics metrics;
    const app_scene_scenario_result result = run_app_scene_scenario(
        state,
        {
            app_scene_scenario_step{
                .name = "select_deck",
                .input = app_scene_scenario_input_kind::tap_node,
                .target_node_id = "deck_list_deck_deck1",
                .now_ms = 100,
            },
            app_scene_scenario_step{
                .name = "select_day",
                .input = app_scene_scenario_input_kind::tap_node,
                .target_node_id = "deck_view_day_day1",
                .now_ms = 200,
            },
        },
        {0.0f, 0.0f, 360.0f, 640.0f},
        metrics);

    require(result.ok(), "deck navigation scenario replay succeeds");
    require(result.trace.size() == 2, "deck navigation scenario emits one trace entry per step");
    require_trace_entry(result.trace[0], "deck_list", "select_deck", "deck_view", "select deck trace is stable");
    require(result.trace[0].target_node_id == "deck_list_deck_deck1", "select deck records target node");
    require(result.trace[0].before_focus_id == "deck_list_deck_deck1", "select deck starts from deck focus");
    require(result.trace[0].after_focus_id == "deck_view_day_day1", "select deck captures first day focus");

    require_trace_entry(result.trace[1], "deck_view", "select_day", "day_intro", "select day trace is stable");
    require(result.trace[1].target_node_id == "deck_view_day_day1", "select day records target node");
    require(result.trace[1].after_focus_id == "day_intro_start_normal", "select day captures day intro focus");

    require(result.final_frame.layout.route_state().screen_id == "day_intro", "deck navigation final frame is day intro");
    require(result.final_frame.snapshot.selected_deck_id.has_value(), "deck navigation final snapshot has selected deck");
    require(result.final_frame.snapshot.selected_day_id.has_value(), "deck navigation final snapshot has selected day");
    require(*result.final_frame.snapshot.selected_deck_id == "deck1", "deck navigation selects deck1");
    require(*result.final_frame.snapshot.selected_day_id == "day1", "deck navigation selects day1");
}

void test_quiz_scene_error_recovery_replay_selects_deck()
{
    using namespace quiz_vulkan;

    app_state state({make_test_deck()});
    state.dispatch(domain::make_select_deck_action("missing"), 10);

    const fixed_text_metrics metrics;
    const app_scene_scenario_result result = run_app_scene_scenario(
        state,
        {
            app_scene_scenario_step{
                .name = "recover_with_deck",
                .input = app_scene_scenario_input_kind::tap_node,
                .target_node_id = "error_deck_deck1",
                .now_ms = 100,
            },
        },
        {0.0f, 0.0f, 360.0f, 640.0f},
        metrics);

    require(result.ok(), "error recovery scenario replay succeeds");
    require(result.trace.size() == 1, "error recovery scenario emits one trace entry");
    require_trace_entry(result.trace[0], "error", "select_deck", "deck_view", "error recovery trace is stable");
    require(result.trace[0].target_node_id == "error_deck_deck1", "error recovery records target node");
    require(result.trace[0].before_focus_id == "error_deck_deck1", "error recovery starts from recovery deck focus");
    require(result.trace[0].after_focus_id == "deck_view_day_day1", "error recovery captures deck view focus");

    require(!result.final_frame.snapshot.error_message.has_value(), "error recovery clears error message");
    require(result.final_frame.snapshot.selected_deck_id.has_value(), "error recovery final snapshot has selected deck");
    require(*result.final_frame.snapshot.selected_deck_id == "deck1", "error recovery selects deck1");
    require(result.final_frame.layout.route_state().screen_id == "deck_view", "error recovery final frame is deck view");
}

void test_quiz_scene_text_submit_replay_records_feedback()
{
    using namespace quiz_vulkan;

    app_state state({make_blank_text_deck()});
    state.dispatch(domain::make_select_day_action("day1"), 10);

    const fixed_text_metrics metrics;
    const app_scene_scenario_result result = run_app_scene_scenario(
        state,
        {
            app_scene_scenario_step{
                .name = "start_normal",
                .input = app_scene_scenario_input_kind::tap_node,
                .target_node_id = "day_intro_start_normal",
                .now_ms = 100,
            },
            app_scene_scenario_step{
                .name = "submit_text_answer",
                .input = app_scene_scenario_input_kind::text_submit,
                .target_node_id = "quiz_active_text_answer",
                .committed_text = "Seoul",
                .now_ms = 200,
            },
        },
        {0.0f, 0.0f, 360.0f, 640.0f},
        metrics);

    require(result.ok(), "text answer scenario replay succeeds");
    require(result.trace.size() == 2, "text answer scenario emits one trace entry per step");
    require_trace_entry(result.trace[0], "day_intro", "start_quiz", "quiz_active", "text scenario start trace is stable");
    require(result.trace[0].after_focus_id == "quiz_active_text_answer", "blank question focuses text input");

    require_trace_entry(result.trace[1], "quiz_active", "submit_text_answer", "quiz_feedback", "text submit trace is stable");
    require(result.trace[1].event_kind == "text_submit", "text submit records text event kind");
    require(result.trace[1].target_node_id == "quiz_active_text_answer", "text submit records target node");
    require(result.trace[1].clear_text_after_action, "text submit requests committed text clear");
    require(result.trace[1].after_focus_id == "quiz_feedback_continue", "text submit captures feedback focus");

    require(result.final_frame.snapshot.screen == domain::app_screen::feedback, "text submit final snapshot is feedback");
    require(result.final_frame.snapshot.active_session.has_value(), "text submit final snapshot has session");
    require(result.final_frame.snapshot.active_session->feedback.has_value(), "text submit final snapshot has feedback");
    const domain::answer_record& feedback = *result.final_frame.snapshot.active_session->feedback;
    require(feedback.outcome == domain::answer_outcome::correct, "text submit feedback is correct");
    require(feedback.submitted_text_answers.size() == 1, "text submit records one answer");
    require(feedback.submitted_text_answers.front() == "seoul", "text submit records normalized answer");
}

void test_quiz_scene_incorrect_text_submit_replay_records_feedback()
{
    using namespace quiz_vulkan;

    app_state state({make_blank_text_deck()});
    state.dispatch(domain::make_select_day_action("day1"), 10);

    const fixed_text_metrics metrics;
    const app_scene_scenario_result result = run_app_scene_scenario(
        state,
        {
            app_scene_scenario_step{
                .name = "start_normal",
                .input = app_scene_scenario_input_kind::tap_node,
                .target_node_id = "day_intro_start_normal",
                .now_ms = 100,
            },
            app_scene_scenario_step{
                .name = "submit_wrong_text_answer",
                .input = app_scene_scenario_input_kind::text_submit,
                .target_node_id = "quiz_active_text_answer",
                .committed_text = "Busan",
                .now_ms = 200,
            },
        },
        {0.0f, 0.0f, 360.0f, 640.0f},
        metrics);

    require(result.ok(), "incorrect text answer scenario replay succeeds");
    require(result.trace.size() == 2, "incorrect text answer scenario emits one trace entry per step");
    require_trace_entry(result.trace[1], "quiz_active", "submit_text_answer", "quiz_feedback", "incorrect text submit trace is stable");
    require(result.trace[1].event_kind == "text_submit", "incorrect text submit records text event kind");
    require(result.trace[1].clear_text_after_action, "incorrect text submit requests committed text clear");

    require(result.final_frame.snapshot.screen == domain::app_screen::feedback, "incorrect text submit final snapshot is feedback");
    require(result.final_frame.snapshot.active_session.has_value(), "incorrect text submit final snapshot has session");
    require(result.final_frame.snapshot.active_session->feedback.has_value(), "incorrect text submit final snapshot has feedback");
    const domain::answer_record& feedback = *result.final_frame.snapshot.active_session->feedback;
    require(feedback.outcome == domain::answer_outcome::incorrect, "incorrect text submit feedback is incorrect");
    require(feedback.submitted_text_answers.size() == 1, "incorrect text submit records one answer");
    require(feedback.submitted_text_answers.front() == "busan", "incorrect text submit records normalized answer");
}

void test_quiz_scene_multiselect_replay_records_feedback()
{
    using namespace quiz_vulkan;

    app_state state({make_multiselect_deck()});
    state.dispatch(domain::make_select_day_action("day1"), 10);

    const fixed_text_metrics metrics;
    const app_scene_scenario_result result = run_app_scene_scenario(
        state,
        {
            app_scene_scenario_step{
                .name = "start_normal",
                .input = app_scene_scenario_input_kind::tap_node,
                .target_node_id = "day_intro_start_normal",
                .now_ms = 100,
            },
            app_scene_scenario_step{
                .name = "submit_multiselect_option",
                .input = app_scene_scenario_input_kind::tap_node,
                .target_node_id = "quiz_active_option_0",
                .now_ms = 200,
            },
        },
        {0.0f, 0.0f, 360.0f, 640.0f},
        metrics);

    require(result.ok(), "multiselect scenario replay succeeds");
    require(result.trace.size() == 2, "multiselect scenario emits one trace entry per step");
    require_trace_entry(result.trace[1], "quiz_active", "submit_multiselect", "quiz_feedback", "multiselect trace is stable");
    require(result.trace[1].target_node_id == "quiz_active_option_0", "multiselect records target node");
    require(result.trace[1].after_focus_id == "quiz_feedback_continue", "multiselect captures feedback focus");

    require(result.final_frame.snapshot.screen == domain::app_screen::feedback, "multiselect final snapshot is feedback");
    require(result.final_frame.snapshot.active_session.has_value(), "multiselect final snapshot has session");
    require(result.final_frame.snapshot.active_session->feedback.has_value(), "multiselect final snapshot has feedback");
    const domain::answer_record& feedback = *result.final_frame.snapshot.active_session->feedback;
    require(feedback.outcome == domain::answer_outcome::correct, "multiselect feedback is correct");
    require(feedback.selected_option_indexes.size() == 1, "multiselect records one selected option");
    require(feedback.selected_option_indexes.front() == 0, "multiselect records selected option index");
}

void test_quiz_scene_results_known_mode_replay_starts_known_session()
{
    using namespace quiz_vulkan;

    app_state state({make_test_deck()});
    state.dispatch(domain::make_select_day_action("day1"), 10);
    state.dispatch(domain::make_start_quiz_action(domain::quiz_mode::normal), 20);
    state.dispatch(domain::make_mark_question_known_action(), 30);

    const fixed_text_metrics metrics;
    const app_scene_scenario_result result = run_app_scene_scenario(
        state,
        {
            app_scene_scenario_step{
                .name = "start_known_from_results",
                .input = app_scene_scenario_input_kind::tap_node,
                .target_node_id = "quiz_results_start_known",
                .now_ms = 100,
            },
        },
        {0.0f, 0.0f, 360.0f, 640.0f},
        metrics);

    require(result.ok(), "known mode results scenario replay succeeds");
    require(result.trace.size() == 1, "known mode results scenario emits one trace entry");
    require_trace_entry(result.trace[0], "quiz_results", "start_quiz", "quiz_active", "known mode start trace is stable");
    require(result.trace[0].target_node_id == "quiz_results_start_known", "known mode start records target node");
    require(result.trace[0].after_focus_id == "quiz_active_option_0", "known mode start captures active option focus");

    require(result.final_frame.snapshot.screen == domain::app_screen::quiz, "known mode final snapshot is quiz");
    require(result.final_frame.snapshot.active_session.has_value(), "known mode final snapshot has session");
    const domain::session_snapshot& session = *result.final_frame.snapshot.active_session;
    require(session.mode == domain::quiz_mode::known, "known mode final session is known");
    require(session.current_question.has_value(), "known mode final session has current question");
    require(session.current_question->question_id == "q1", "known mode final session starts known question");
    require(result.final_frame.snapshot.learning.known_count == 1, "known mode final snapshot preserves known count");
}

void test_quiz_scene_swipe_skip_replay_reaches_results()
{
    using namespace quiz_vulkan;

    app_state state({make_test_deck()});
    state.dispatch(domain::make_select_day_action("day1"), 10);

    const fixed_text_metrics metrics;
    const app_scene_scenario_result result = run_app_scene_scenario(
        state,
        {
            app_scene_scenario_step{
                .name = "start_normal",
                .input = app_scene_scenario_input_kind::tap_node,
                .target_node_id = "day_intro_start_normal",
                .now_ms = 100,
            },
            app_scene_scenario_step{
                .name = "skip_with_swipe",
                .input = app_scene_scenario_input_kind::swipe_right,
                .now_ms = 200,
            },
        },
        {0.0f, 0.0f, 360.0f, 640.0f},
        metrics);

    require(result.ok(), "swipe skip scenario replay succeeds");
    require(result.trace.size() == 2, "swipe skip scenario emits one trace entry per step");
    require_trace_entry(result.trace[1], "quiz_active", "skip_question", "quiz_results", "swipe skip trace is stable");
    require(result.trace[1].event_kind == "swipe_right", "swipe skip records gesture kind");
    require(result.trace[1].before_focus_id == "quiz_active_option_0", "swipe skip starts from active option focus");
    require(result.trace[1].after_focus_id == "quiz_results_start_normal", "swipe skip captures results focus");
    require(result.final_frame.snapshot.screen == domain::app_screen::completed, "swipe skip final snapshot is completed");
    require(result.final_frame.layout.contains_node("quiz_results_actions"), "swipe skip final frame emits results actions");
}

void test_quiz_scene_long_press_mark_unknown_updates_learning()
{
    using namespace quiz_vulkan;

    app_state state({make_test_deck()});
    state.dispatch(domain::make_select_day_action("day1"), 10);

    const fixed_text_metrics metrics;
    const app_scene_scenario_result result = run_app_scene_scenario(
        state,
        {
            app_scene_scenario_step{
                .name = "start_normal",
                .input = app_scene_scenario_input_kind::tap_node,
                .target_node_id = "day_intro_start_normal",
                .now_ms = 100,
            },
            app_scene_scenario_step{
                .name = "mark_unknown_with_long_press",
                .input = app_scene_scenario_input_kind::long_press,
                .now_ms = 200,
            },
        },
        {0.0f, 0.0f, 360.0f, 640.0f},
        metrics);

    require(result.ok(), "long press mark unknown scenario replay succeeds");
    require(result.trace.size() == 2, "long press scenario emits one trace entry per step");
    require_trace_entry(result.trace[1], "quiz_active", "mark_question_unknown", "quiz_results", "long press trace is stable");
    require(result.trace[1].event_kind == "long_press", "long press records gesture kind");
    require(result.trace[1].after_focus_id == "quiz_results_start_normal", "long press captures results focus");
    require(result.final_frame.snapshot.screen == domain::app_screen::completed, "long press final snapshot is completed");
    require(result.final_frame.snapshot.learning.question_count == 1, "long press final snapshot summarizes learning");
    require(result.final_frame.snapshot.learning.unknown_count == 1, "long press marks question unknown");
}

void test_quiz_scene_swipe_previous_replay_returns_to_prior_question()
{
    using namespace quiz_vulkan;

    app_state state({make_two_question_deck()});
    state.dispatch(domain::make_select_day_action("day1"), 10);

    const fixed_text_metrics metrics;
    const app_scene_scenario_result result = run_app_scene_scenario(
        state,
        {
            app_scene_scenario_step{
                .name = "start_normal",
                .input = app_scene_scenario_input_kind::tap_node,
                .target_node_id = "day_intro_start_normal",
                .now_ms = 100,
            },
            app_scene_scenario_step{
                .name = "answer_first_option",
                .input = app_scene_scenario_input_kind::tap_node,
                .target_node_id = "quiz_active_option_0",
                .now_ms = 200,
            },
            app_scene_scenario_step{
                .name = "continue_feedback",
                .input = app_scene_scenario_input_kind::swipe_right,
                .now_ms = 300,
            },
            app_scene_scenario_step{
                .name = "previous_with_swipe",
                .input = app_scene_scenario_input_kind::swipe_left,
                .now_ms = 400,
            },
        },
        {0.0f, 0.0f, 360.0f, 640.0f},
        metrics);

    require(result.ok(), "swipe previous scenario replay succeeds");
    require(result.trace.size() == 4, "swipe previous scenario emits one trace entry per step");
    require_trace_entry(result.trace[2], "quiz_feedback", "continue_after_feedback", "quiz_active", "continue to second question trace is stable");
    require(result.trace[2].event_kind == "swipe_right", "continue to second question records gesture kind");
    require(result.trace[2].after_focus_id == "quiz_active_option_0", "second question active focus is captured");

    require_trace_entry(result.trace[3], "quiz_active", "previous_question", "quiz_active", "previous question trace is stable");
    require(result.trace[3].event_kind == "swipe_left", "previous question records gesture kind");
    require(result.trace[3].before_focus_id == "quiz_active_option_0", "previous question starts from second question focus");
    require(result.trace[3].after_focus_id == "quiz_active_option_0", "previous question returns to active option focus");

    require(result.final_frame.snapshot.screen == domain::app_screen::quiz, "previous final snapshot remains in quiz");
    require(result.final_frame.snapshot.active_session.has_value(), "previous final snapshot has session");
    const domain::session_snapshot& session = *result.final_frame.snapshot.active_session;
    require(session.current_index == 0, "previous final snapshot returns to first question index");
    require(session.current_question.has_value(), "previous final snapshot has current question");
    require(session.current_question->question_id == "q1", "previous final snapshot returns to q1");
    require(!session.feedback.has_value(), "previous final snapshot has no pending feedback");
}

void test_quiz_scene_settings_close_replay_returns_to_deck_list()
{
    using namespace quiz_vulkan;

    app_state state({make_test_deck()});
    state.dispatch(domain::make_update_setting_action("ui_screen", "settings"), 10);

    const fixed_text_metrics metrics;
    const app_scene_scenario_result result = run_app_scene_scenario(
        state,
        {
            app_scene_scenario_step{
                .name = "close_settings",
                .input = app_scene_scenario_input_kind::tap_node,
                .target_node_id = "settings_close",
                .now_ms = 100,
            },
        },
        {0.0f, 0.0f, 360.0f, 640.0f},
        metrics);

    require(result.ok(), "settings close scenario replay succeeds");
    require(result.trace.size() == 1, "settings close scenario emits one trace entry");
    require_trace_entry(result.trace[0], "settings", "update_setting", "deck_list", "settings close trace is stable");
    require(result.trace[0].target_node_id == "settings_close", "settings close records target node");
    require(result.trace[0].before_focus_id == "settings_close", "settings close starts from close focus");
    require(result.trace[0].after_focus_id == "deck_list_deck_deck1", "settings close captures deck list focus");

    require(result.final_frame.layout.route_state().screen_id == "deck_list", "settings close final frame is deck list");
    require(result.final_frame.layout.contains_node("deck_list_decks"), "settings close final frame emits deck list");
    const auto setting = result.final_frame.snapshot.settings.find("ui_screen");
    require(setting != result.final_frame.snapshot.settings.end(), "settings close final snapshot keeps ui_screen setting");
    require(setting->second == "deck_list", "settings close final snapshot updates route setting");
}

void test_quiz_scene_missing_target_records_failure_trace()
{
    using namespace quiz_vulkan;

    app_state state({make_test_deck()});

    const fixed_text_metrics metrics;
    const app_scene_scenario_result result = run_app_scene_scenario(
        state,
        {
            app_scene_scenario_step{
                .name = "tap_missing_target",
                .input = app_scene_scenario_input_kind::tap_node,
                .target_node_id = "deck_list_missing_deck",
                .now_ms = 100,
            },
        },
        {0.0f, 0.0f, 360.0f, 640.0f},
        metrics);

    require(!result.ok(), "missing target scenario reports failure");
    require(result.error.find("scenario target input region not found: deck_list_missing_deck") != std::string::npos,
        "missing target scenario reports target id");
    require(result.trace.size() == 1, "missing target scenario records failed step trace");

    const app_scene_scenario_trace_entry& trace = result.trace.front();
    require(trace.step_name == "tap_missing_target", "missing target trace records step name");
    require(trace.event_kind == "tap_node", "missing target trace records event kind");
    require(trace.target_node_id == "deck_list_missing_deck", "missing target trace records target node");
    require(trace.before_screen_id == "deck_list", "missing target trace records before screen");
    require(trace.before_focus_id == "deck_list_deck_deck1", "missing target trace records before focus");
    require(trace.before_node_count > 1, "missing target trace records before node count");
    require(trace.before_input_region_count > 0, "missing target trace records before input regions");
    require(!trace.handled, "missing target trace is not handled");
    require(!trace.needs_render, "missing target trace does not request render");
    require(trace.action_type.empty(), "missing target trace records no action");
    require(trace.error == result.error, "missing target trace records failure error");
}

} // namespace

int main()
{
    test_quiz_scene_event_replay_reaches_results();
    test_quiz_scene_deck_navigation_replay_reaches_day_intro();
    test_quiz_scene_error_recovery_replay_selects_deck();
    test_quiz_scene_text_submit_replay_records_feedback();
    test_quiz_scene_incorrect_text_submit_replay_records_feedback();
    test_quiz_scene_multiselect_replay_records_feedback();
    test_quiz_scene_results_known_mode_replay_starts_known_session();
    test_quiz_scene_swipe_skip_replay_reaches_results();
    test_quiz_scene_long_press_mark_unknown_updates_learning();
    test_quiz_scene_swipe_previous_replay_returns_to_prior_question();
    test_quiz_scene_settings_close_replay_returns_to_deck_list();
    test_quiz_scene_missing_target_records_failure_trace();
    return 0;
}
