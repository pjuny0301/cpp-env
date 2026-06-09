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

} // namespace

int main()
{
    test_quiz_scene_event_replay_reaches_results();
    return 0;
}
