#include "core/layout/layout_placer.h"
#include "core/domain/app_snapshot.hpp"
#include "app/app_quiz_screens.h"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

void require(bool condition, const char* message)
{
    assert((condition) && message);
}

bool near(float actual, float expected)
{
    return std::fabs(actual - expected) < 0.001f;
}

class fixed_text_metrics final : public quiz_vulkan::scene::text_metrics_interface {
public:
    quiz_vulkan::scene::scene_size measure_text(
        const std::vector<quiz_vulkan::scene::scene_text_run>& text_runs,
        const quiz_vulkan::scene::scene_style&,
        float) const override
    {
        std::size_t character_count = 0;
        for (const auto& run : text_runs) {
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

quiz_vulkan::domain::deck make_blank_input_deck()
{
    using namespace quiz_vulkan::domain;

    question quiz_question;
    quiz_question.id = "q_blank";
    quiz_question.prompt = "Fill the blank";
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

quiz_vulkan::domain::deck make_learning_groups_deck()
{
    using namespace quiz_vulkan::domain;

    day quiz_day;
    quiz_day.id = "day1";
    quiz_day.title = "Day 1";

    const std::vector<std::string> question_ids = {"q_learning", "q_known", "q_wrong"};
    for (const std::string& question_id : question_ids) {
        question quiz_question;
        quiz_question.id = question_id;
        quiz_question.prompt = "Question " + question_id;
        quiz_question.type = question_type::answer;
        quiz_question.options.push_back(option{"Yes", true});
        quiz_question.options.push_back(option{"No", false});
        quiz_day.questions.push_back(std::move(quiz_question));
    }

    deck quiz_deck;
    quiz_deck.id = "deck1";
    quiz_deck.title = "Learning groups";
    quiz_deck.days.push_back(std::move(quiz_day));
    return quiz_deck;
}

quiz_vulkan::domain::learning_state_map make_learning_groups_state()
{
    using namespace quiz_vulkan::domain;

    learning_state_map learning;

    question_learning known;
    known.state = learning_state::known;
    learning["q_known"] = known;

    question_learning wrong_note;
    wrong_note.state = learning_state::wrong_note;
    learning["q_wrong"] = wrong_note;

    return learning;
}

quiz_vulkan::domain::app_snapshot make_snapshot(
    const std::vector<quiz_vulkan::domain::deck>& decks,
    const quiz_vulkan::domain::quiz_session* session = nullptr,
    std::unordered_map<std::string, std::string> settings = {},
    std::optional<std::string> error_message = std::nullopt,
    quiz_vulkan::domain::learning_state_map learning = {})
{
    return quiz_vulkan::domain::make_app_snapshot(
        decks,
        std::optional<std::string>{"deck1"},
        std::optional<std::string>{"day1"},
        session,
        learning,
        std::move(settings),
        std::move(error_message));
}

quiz_vulkan::domain::quiz_session make_active_session(const quiz_vulkan::domain::deck& deck)
{
    const quiz_vulkan::domain::learning_state_map learning;
    const quiz_vulkan::domain::previous_answer_map previous_answers;
    return quiz_vulkan::domain::start_quiz_session(deck, learning, previous_answers);
}

quiz_vulkan::domain::quiz_session make_completed_session(const quiz_vulkan::domain::deck& deck)
{
    quiz_vulkan::domain::quiz_session session = make_active_session(deck);

    while (!session.completed) {
        if (session.pending_feedback.has_value()) {
            quiz_vulkan::domain::continue_after_feedback(session);
        } else {
            (void)quiz_vulkan::domain::skip_current_question(session, 100);
        }
    }

    return session;
}

void apply_patch_to_scene(
    const quiz_vulkan::scene::scene_layout_patch& patch,
    quiz_vulkan::scene::scene_layout_data& data)
{
    const quiz_vulkan::scene::scene_layout_apply_result result = patch.apply_to(data);
    require(result.applied(), "screen patch applies");
}

void require_within_visible_bottom(
    const quiz_vulkan::scene::placed_scene& placed,
    const char* node_id,
    const char* message)
{
    const quiz_vulkan::scene::placed_scene_node* node = placed.find_node(node_id);
    require(node != nullptr, message);
    require(node->bounds.bottom() <= placed.usable_bounds.bottom() + 0.001f, message);
}

void require_start_quiz_action(
    const quiz_vulkan::scene::scene_layout_data& data,
    const char* node_id,
    const char* payload,
    const char* message)
{
    const quiz_vulkan::scene::scene_node_data* node = data.find_node(node_id);
    require(node != nullptr, message);
    require(node->has_action_binding, message);
    require(node->action_binding.action_type == "start_quiz", message);
    require(node->action_binding.payload == payload, message);
}

void require_node_role(
    const quiz_vulkan::scene::scene_layout_data& data,
    const char* node_id,
    quiz_vulkan::scene::scene_node_role role,
    const char* message)
{
    const quiz_vulkan::scene::scene_node_data* node = data.find_node(node_id);
    require(node != nullptr, message);
    require(node->semantics.role == role, message);
}

} // namespace

int main()
{
    using namespace quiz_vulkan;

    std::vector<domain::deck> decks;
    decks.push_back(make_test_deck());

    const domain::learning_state_map learning;
    const domain::app_snapshot deck_list_snapshot = domain::make_app_snapshot(
        decks,
        std::nullopt,
        std::nullopt,
        nullptr,
        learning);

    const presentation::quiz_screen_descriptor deck_list_descriptor = presentation::describe_quiz_screen(deck_list_snapshot);
    require(deck_list_descriptor.kind == presentation::quiz_screen_kind::deck_list, "deck list descriptor selected");
    require(deck_list_descriptor.route.metadata.at("descriptor_version") == "quiz_screen_descriptor_v1", "descriptor version emitted");
    require(deck_list_descriptor.route.metadata.at("layout_contract") == "deck_grid", "deck list layout contract emitted");
    require(deck_list_descriptor.route.metadata.at("known_count") == "0", "deck list known count metadata emitted");
    require(deck_list_descriptor.route.metadata.at("wrong_note_count") == "0", "deck list wrong-note count metadata emitted");

    scene::scene_layout_data deck_list_data("test_deck_list");
    apply_patch_to_scene(presentation::make_quiz_screen_patch(deck_list_snapshot), deck_list_data);
    require(deck_list_data.contains_node("quiz_screens_root"), "deck list root exists");
    require(deck_list_data.contains_node("deck_list_deck_deck1"), "deck button exists");
    require(deck_list_data.has_focus(), "deck list has focus");
    require(deck_list_data.focus_id() == "deck_list_deck_deck1", "deck list focuses first deck");
    require(deck_list_data.route_state().screen_id == "deck_list", "deck list route set");

    const scene::scene_node_data* deck_button = deck_list_data.find_node("deck_list_deck_deck1");
    require(deck_button != nullptr, "deck button can be found");
    require(deck_button->has_action_binding, "deck button action exists");
    require(deck_button->action_binding.action_type == "select_deck", "deck button selects deck");
    require(deck_button->action_binding.payload == "deck1", "deck button payload contains deck id");

    const domain::app_snapshot day_intro_snapshot = make_snapshot(decks);
    scene::scene_layout_data day_intro_data("test_day_intro");
    apply_patch_to_scene(presentation::make_quiz_screen_patch(day_intro_snapshot), day_intro_data);
    require(day_intro_data.route_state().screen_id == "day_intro", "day intro route selected");
    require(day_intro_data.route_state().metadata.at("selected_day_id") == "day1", "day intro selected day metadata emitted");
    require(day_intro_data.route_state().metadata.at("selected_day_question_count") == "1", "day intro question count metadata emitted");
    require(day_intro_data.contains_node("day_intro_start_normal"), "day intro normal start action exists");
    require(!day_intro_data.contains_node("day_intro_start_known"), "day intro known action is hidden without known questions");
    require(!day_intro_data.contains_node("day_intro_start_wrong_note"), "day intro wrong-note action is hidden without wrong notes");
    require_start_quiz_action(day_intro_data, "day_intro_start_normal", "normal", "day intro normal action starts normal quiz");

    std::vector<domain::deck> learning_decks;
    learning_decks.push_back(make_learning_groups_deck());
    const domain::learning_state_map learning_groups = make_learning_groups_state();
    const domain::app_snapshot learning_day_intro_snapshot = make_snapshot(
        learning_decks,
        nullptr,
        {},
        std::nullopt,
        learning_groups);
    scene::scene_layout_data learning_day_intro_data("test_learning_day_intro");
    apply_patch_to_scene(presentation::make_quiz_screen_patch(learning_day_intro_snapshot), learning_day_intro_data);
    require(learning_day_intro_data.route_state().screen_id == "day_intro", "learning day intro route selected");
    require(learning_day_intro_data.route_state().metadata.at("known_count") == "1", "known count metadata emitted");
    require(learning_day_intro_data.route_state().metadata.at("wrong_note_count") == "1", "wrong-note count metadata emitted");
    require(learning_day_intro_data.contains_node("day_intro_start_known"), "known mode action exists when known questions exist");
    require(learning_day_intro_data.contains_node("day_intro_start_wrong_note"), "wrong-note mode action exists when wrong notes exist");
    require_node_role(learning_day_intro_data, "day_intro_modes", scene::scene_node_role::quiz_controls, "day intro mode actions are tagged as controls");
    require_start_quiz_action(learning_day_intro_data, "day_intro_start_normal", "normal", "learning day intro normal action starts normal quiz");
    require_start_quiz_action(learning_day_intro_data, "day_intro_start_known", "known", "known action starts known quiz");
    require_start_quiz_action(learning_day_intro_data, "day_intro_start_wrong_note", "wrong_note", "wrong-note action starts wrong-note quiz");

    domain::quiz_session active_session = make_active_session(decks.front());
    const domain::app_snapshot active_snapshot = make_snapshot(decks, &active_session);
    scene::scene_layout_data active_data("test_active");
    apply_patch_to_scene(presentation::make_quiz_screen_patch(active_snapshot), active_data);
    require(active_data.route_state().screen_id == "quiz_active", "active quiz route selected");
    require(active_data.route_state().metadata.at("session_phase") == "active", "active session phase metadata emitted");
    require(active_data.route_state().metadata.at("question_id") == "q1", "active question id metadata emitted");
    require(active_data.route_state().metadata.at("question_option_count") == "2", "active option count metadata emitted");
    require(active_data.route_state().metadata.at("input_locked") == "false", "active input is unlocked");
    require(active_data.contains_node("quiz_active_option_0"), "active first option exists");

    const scene::scene_node_data* first_option = active_data.find_node("quiz_active_option_0");
    require(first_option != nullptr, "active first option can be found");
    require(first_option->input_enabled, "active first option is enabled");
    require(first_option->has_action_binding, "active first option action exists");
    require(first_option->action_binding.action_type == "submit_option", "active first option submits option");

    std::vector<domain::deck> blank_decks;
    blank_decks.push_back(make_blank_input_deck());
    domain::quiz_session blank_session = make_active_session(blank_decks.front());
    const domain::app_snapshot blank_snapshot = make_snapshot(blank_decks, &blank_session);
    scene::scene_layout_data blank_data("test_blank_input");
    apply_patch_to_scene(presentation::make_quiz_screen_patch(blank_snapshot), blank_data);
    require(blank_data.route_state().screen_id == "quiz_active", "blank input route selected");
    require(blank_data.route_state().metadata.at("keyboard_safe_layout") == "true", "blank input advertises keyboard-safe layout");
    require(blank_data.has_focus(), "blank input screen has focus");
    require(blank_data.focus_id() == "quiz_active_text_answer", "blank input focuses text answer");

    const scene::scene_node_data* blank_root = blank_data.find_node("quiz_screens_root");
    require(blank_root != nullptr, "blank input root exists");
    require(blank_root->layout_rule.respect_safe_area, "blank input root respects safe area");
    require(blank_root->layout_rule.avoid_keyboard, "blank input root avoids keyboard");

    const scene::scene_node_data* answer_dock = blank_data.find_node("quiz_active_answer_dock");
    require(answer_dock != nullptr, "blank input answer dock exists");
    require(answer_dock->layout_rule.anchor_to_keyboard, "blank input answer dock anchors to keyboard");
    require(answer_dock->semantics.role == scene::scene_node_role::quiz_answer_dock, "blank input answer dock is tagged");

    const scene::scene_node_data* text_answer = blank_data.find_node("quiz_active_text_answer");
    require(text_answer != nullptr, "blank input text answer exists");
    require(text_answer->semantics.role == scene::scene_node_role::quiz_answer_input, "blank input answer is tagged");
    require(text_answer->semantics.quiz.accepts_keyboard_input, "blank input answer accepts keyboard input");

    fixed_text_metrics metrics;
    scene::scene_layout_environment keyboard_environment;
    keyboard_environment.viewport = {0.0f, 0.0f, 360.0f, 640.0f};
    keyboard_environment.safe_area = {0.0f, 24.0f, 0.0f, 16.0f};
    keyboard_environment.keyboard.visible = true;
    keyboard_environment.keyboard.bottom_inset = 220.0f;
    keyboard_environment.keyboard.focused_node_id = blank_data.focus_id();

    const scene::placed_scene blank_placed = scene::layout_placer().place_with_environment(
        blank_data,
        keyboard_environment,
        metrics);
    require(near(blank_placed.usable_bounds.bottom(), 420.0f), "keyboard inset sets visible bottom");
    require_within_visible_bottom(blank_placed, "quiz_active_prompt", "blank input prompt remains visible");
    require_within_visible_bottom(blank_placed, "quiz_active_answer_dock", "blank input dock remains visible");
    require_within_visible_bottom(blank_placed, "quiz_active_text_answer", "blank input text answer remains visible");

    const scene::placed_scene_node* placed_answer_dock = blank_placed.find_node("quiz_active_answer_dock");
    const scene::placed_scene_node* placed_text_answer = blank_placed.find_node("quiz_active_text_answer");
    require(placed_answer_dock != nullptr, "placed answer dock exists");
    require(placed_text_answer != nullptr, "placed text answer exists");
    require(near(placed_answer_dock->bounds.y, 180.0f), "answer dock has stable keyboard-safe placement");
    require(near(placed_text_answer->bounds.y, 180.0f), "text answer starts at the dock top");

    domain::quiz_session feedback_session = make_active_session(decks.front());
    const std::optional<domain::answer_record> feedback_record = domain::submit_option_answer(
        feedback_session,
        decks.front(),
        1,
        100);
    require(feedback_record.has_value(), "submitting option creates feedback");
    const domain::app_snapshot feedback_snapshot = make_snapshot(decks, &feedback_session);
    scene::scene_layout_data feedback_data("test_feedback");
    apply_patch_to_scene(presentation::make_quiz_screen_patch(feedback_snapshot), feedback_data);
    require(feedback_data.route_state().screen_id == "quiz_feedback", "feedback route selected");
    require(feedback_data.route_state().metadata.at("feedback_outcome") == "incorrect", "feedback outcome metadata emitted");
    require(feedback_data.route_state().metadata.at("input_locked") == "true", "feedback input is locked");
    require(feedback_data.contains_node("quiz_feedback_continue"), "feedback continue button exists");

    const scene::scene_node_data* feedback_option = feedback_data.find_node("quiz_feedback_option_1");
    require(feedback_option != nullptr, "feedback submitted option exists");
    require(!feedback_option->input_enabled, "feedback option is disabled");
    require(!feedback_option->has_action_binding, "feedback option cannot submit again");

    domain::continue_after_feedback(feedback_session);
    const domain::app_snapshot results_snapshot = make_snapshot(decks, &feedback_session);
    scene::scene_layout_data results_data("test_results");
    apply_patch_to_scene(presentation::make_quiz_screen_patch(results_snapshot), results_data);
    require(results_data.route_state().screen_id == "quiz_results", "results route selected");
    require(results_data.route_state().metadata.at("completed") == "true", "results completed metadata emitted");
    require(results_data.contains_node("quiz_results_start_normal"), "results normal action exists");
    require(!results_data.contains_node("quiz_results_start_known"), "results known action is hidden without known questions");
    require(!results_data.contains_node("quiz_results_start_wrong_note"), "results wrong-note action is hidden without wrong notes");

    domain::quiz_session learning_completed_session = make_completed_session(learning_decks.front());
    const domain::app_snapshot learning_results_snapshot = make_snapshot(
        learning_decks,
        &learning_completed_session,
        {},
        std::nullopt,
        learning_groups);
    scene::scene_layout_data learning_results_data("test_learning_results");
    apply_patch_to_scene(presentation::make_quiz_screen_patch(learning_results_snapshot), learning_results_data);
    require(learning_results_data.route_state().screen_id == "quiz_results", "learning results route selected");
    require(learning_results_data.route_state().metadata.at("known_count") == "1", "results known count metadata emitted");
    require(learning_results_data.route_state().metadata.at("wrong_note_count") == "1", "results wrong-note count metadata emitted");
    require_node_role(learning_results_data, "quiz_results_actions", scene::scene_node_role::quiz_controls, "results mode actions are tagged as controls");
    require_start_quiz_action(learning_results_data, "quiz_results_start_normal", "normal", "results normal action starts normal quiz");
    require_start_quiz_action(learning_results_data, "quiz_results_start_known", "known", "results known action starts known quiz");
    require_start_quiz_action(learning_results_data, "quiz_results_start_wrong_note", "wrong_note", "results wrong-note action starts wrong-note quiz");

    const domain::app_snapshot settings_snapshot = make_snapshot(
        decks,
        nullptr,
        {{"ui_screen", "settings"}, {"source_uri", "fixture://deck"}});
    scene::scene_layout_data settings_data("test_settings");
    apply_patch_to_scene(presentation::make_quiz_screen_patch(settings_snapshot), settings_data);
    require(settings_data.route_state().screen_id == "settings", "settings route selected");
    require(settings_data.route_state().metadata.at("layout_contract") == "settings_list", "settings layout contract emitted");
    require(settings_data.route_state().metadata.at("settings_count") == "2", "settings count metadata emitted");
    require(settings_data.contains_node("settings_entry_source_uri"), "settings source entry exists");
    require(settings_data.contains_node("settings_close"), "settings close action exists");

    const domain::app_snapshot error_snapshot = make_snapshot(
        decks,
        nullptr,
        {},
        std::optional<std::string>{"Deck not found: missing"});
    scene::scene_layout_data error_data("test_error");
    apply_patch_to_scene(presentation::make_quiz_screen_patch(error_snapshot), error_data);
    require(error_data.route_state().screen_id == "error", "error route selected");
    require(error_data.route_state().metadata.at("has_error") == "true", "error metadata emitted");
    require(error_data.route_state().metadata.at("layout_contract") == "error_recovery", "error layout contract emitted");
    require(error_data.contains_node("error_error_banner"), "error banner exists");
    require(error_data.contains_node("error_deck_deck1"), "error recovery deck action exists");

    return 0;
}
