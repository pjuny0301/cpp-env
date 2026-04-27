#include "core/domain/app_snapshot.hpp"
#include "core/ui/quiz_screens.h"

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

} // namespace

int main()
{
    using namespace quiz_vulkan;

    std::vector<domain::deck> decks;
    decks.push_back(make_test_deck());

    const domain::learning_state_map learning;
    const domain::app_snapshot home_snapshot = domain::make_app_snapshot(
        decks,
        std::nullopt,
        std::nullopt,
        nullptr,
        learning);

    scene::scene_layout_data data("test");
    const scene::scene_layout_patch patch = ui::make_quiz_screen_patch(home_snapshot);
    const scene::scene_layout_apply_result result = patch.apply_to(data);

    assert(result.applied());
    assert(data.contains_node("quiz_screens_root"));
    assert(data.contains_node("home_deck_deck1"));
    assert(data.has_focus());
    assert(data.focus_id() == "home_deck_deck1");
    assert(data.route_state().screen_id == "home");

    const scene::scene_node_data* deck_button = data.find_node("home_deck_deck1");
    assert(deck_button != nullptr);
    assert(deck_button->has_action_binding);
    assert(deck_button->action_binding.action_type == "select_deck");
    assert(deck_button->action_binding.payload == "deck1");

    domain::previous_answer_map previous_answers;
    domain::quiz_session session = domain::start_quiz_session(decks.front(), learning, previous_answers);
    const domain::app_snapshot quiz_snapshot = domain::make_app_snapshot(
        decks,
        std::optional<std::string>{"deck1"},
        std::optional<std::string>{"day1"},
        &session,
        learning);

    scene::scene_layout_data quiz_data("test_quiz");
    const scene::scene_layout_patch quiz_patch = ui::make_quiz_active_screen_patch(quiz_snapshot);
    assert(quiz_patch.apply_to(quiz_data).applied());
    assert(quiz_data.route_state().screen_id == "quiz_active");
    assert(quiz_data.route_state().metadata.at("session_phase") == "active");
    assert(quiz_data.contains_node("quiz_active_option_0"));

    const scene::scene_node_data* first_option = quiz_data.find_node("quiz_active_option_0");
    assert(first_option != nullptr);
    assert(first_option->input_enabled);
    assert(first_option->has_action_binding);
    assert(first_option->action_binding.action_type == "submit_option");

    return 0;
}
