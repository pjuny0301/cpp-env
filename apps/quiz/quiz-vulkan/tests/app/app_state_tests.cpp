#include "app/app_state.h"

#include <cassert>
#include <optional>
#include <utility>
#include <vector>

namespace {

quiz_vulkan::domain::deck make_test_deck()
{
    using namespace quiz_vulkan::domain;

    question quiz_question;
    quiz_question.id = "q1";
    quiz_question.prompt = "2 + 2";
    quiz_question.type = question_type::answer;
    quiz_question.options.push_back(option{"4", true});
    quiz_question.options.push_back(option{"5", false});

    day quiz_day;
    quiz_day.id = "day1";
    quiz_day.title = "Day 1";
    quiz_day.questions.push_back(std::move(quiz_question));

    deck quiz_deck;
    quiz_deck.id = "math";
    quiz_deck.title = "Math";
    quiz_deck.days.push_back(std::move(quiz_day));
    return quiz_deck;
}

} // namespace

int main()
{
    using namespace quiz_vulkan;

    app_state state({make_test_deck()});
    assert(state.snapshot().screen == domain::app_screen::deck_select);

    state.dispatch(domain::make_select_deck_action("math"));
    assert(state.selected_deck_id().has_value());
    assert(*state.selected_deck_id() == "math");
    assert(state.snapshot().screen == domain::app_screen::day_select);

    state.dispatch(domain::make_select_day_action("day1"));
    assert(state.selected_day_id().has_value());
    assert(*state.selected_day_id() == "day1");

    state.dispatch(domain::make_start_quiz_action(domain::quiz_mode::normal), 10);
    assert(state.active_session().has_value());
    assert(domain::phase_of(*state.active_session()) == domain::quiz_session_phase::active);

    state.dispatch(domain::make_submit_option_action(0), 20);
    assert(domain::phase_of(*state.active_session()) == domain::quiz_session_phase::feedback);
    assert(state.learning().at("q1").correct_streak == 1);

    state.dispatch(domain::make_continue_after_feedback_action(), 30);
    assert(domain::phase_of(*state.active_session()) == domain::quiz_session_phase::completed);
    assert(state.snapshot().screen == domain::app_screen::completed);

    state.dispatch(domain::make_previous_question_action(), 35);
    assert(domain::phase_of(*state.active_session()) == domain::quiz_session_phase::active);
    assert(state.active_session()->current_index == 0);

    state.dispatch(domain::make_mark_question_known_action(), 36);
    assert(state.learning().at("q1").state == domain::learning_state::known);
    assert(state.snapshot().learning.known_count == 1);

    state.dispatch(domain::make_start_quiz_action(domain::quiz_mode::known), 40);
    state.dispatch(domain::make_submit_option_action(1), 50);
    assert(state.learning().at("q1").wrong_count == 1);
    assert(state.learning().at("q1").state == domain::learning_state::learning);

    state.dispatch(domain::make_select_day_action("missing"));
    assert(state.snapshot().screen == domain::app_screen::error);

    return 0;
}
