#include "app/app_action_router.h"

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace {

void require(bool condition, const char* message)
{
    if (condition) {
        return;
    }

    std::cerr << "app_action_router_tests failed: " << message << '\n';
    std::exit(1);
}

quiz_vulkan::scene::scene_action_binding action(std::string action_type, std::string payload = {})
{
    quiz_vulkan::scene::scene_action_binding binding;
    binding.action_type = std::move(action_type);
    binding.payload = std::move(payload);
    return binding;
}

bool contains(std::string_view value, std::string_view needle)
{
    return value.find(needle) != std::string_view::npos;
}

template <typename Payload>
const Payload* payload_if(const quiz_vulkan::app_action_route_result& result)
{
    if (!result.action.has_value()) {
        return nullptr;
    }

    return std::get_if<Payload>(&result.action->payload);
}

void test_identity_actions()
{
    using namespace quiz_vulkan;

    app_action_route_result source = route_scene_action(action("load_source", "file://deck.json"));
    require(source.ok(), "load_source routes");
    const auto* source_payload = payload_if<domain::load_source_action>(source);
    require(source_payload != nullptr, "load_source stores payload");
    require(source_payload->source_uri == "file://deck.json", "load_source preserves source URI");

    app_action_route_result deck = route_scene_action(action("select_deck", "math"));
    require(deck.ok(), "select_deck routes");
    const auto* deck_payload = payload_if<domain::select_deck_action>(deck);
    require(deck_payload != nullptr, "select_deck stores payload");
    require(deck_payload->deck_id == "math", "select_deck preserves deck id");

    app_action_route_result day = route_scene_action(action("select_day", "day1"));
    require(day.ok(), "select_day routes");
    const auto* day_payload = payload_if<domain::select_day_action>(day);
    require(day_payload != nullptr, "select_day stores payload");
    require(day_payload->day_id == "day1", "select_day preserves day id");

    app_action_route_result missing_deck = route_scene_action(action("select_deck"));
    require(!missing_deck.ok(), "empty select_deck fails");
    require(contains(missing_deck.error, "deck id"), "empty select_deck reports missing deck id");
}

void test_start_quiz_modes()
{
    using namespace quiz_vulkan;

    app_action_route_result wrong_note = route_scene_action(action("start_quiz", "wrong_note"));
    require(wrong_note.ok(), "wrong_note start_quiz routes");
    const auto* wrong_note_payload = payload_if<domain::start_quiz_action>(wrong_note);
    require(wrong_note_payload != nullptr, "wrong_note stores start payload");
    require(wrong_note_payload->mode == domain::quiz_mode::wrong_note, "wrong_note mode is preserved");

    app_action_route_result due = route_scene_action(action("start_quiz", "due"));
    require(due.ok(), "due start_quiz alias routes");
    const auto* due_payload = payload_if<domain::start_quiz_action>(due);
    require(due_payload != nullptr, "due stores start payload");
    require(due_payload->mode == domain::quiz_mode::normal, "due maps to normal quiz mode");

    app_action_route_result wrong = route_scene_action(action("start_quiz", "wrong"));
    require(wrong.ok(), "wrong start_quiz alias routes");
    const auto* wrong_payload = payload_if<domain::start_quiz_action>(wrong);
    require(wrong_payload != nullptr, "wrong stores start payload");
    require(wrong_payload->mode == domain::quiz_mode::wrong_only, "wrong maps to wrong_only quiz mode");

    app_action_route_result bad = route_scene_action(action("start_quiz", "missing"));
    require(!bad.ok(), "unknown start_quiz mode fails");
    require(contains(bad.error, "quiz mode"), "unknown start_quiz mode reports quiz mode error");
}

void test_submit_option()
{
    using namespace quiz_vulkan;

    app_action_route_result valid = route_scene_action(action("submit_option", "2"));
    require(valid.ok(), "submit_option routes");
    const auto* payload = payload_if<domain::submit_option_action>(valid);
    require(payload != nullptr, "submit_option stores payload");
    require(payload->option_index == 2, "submit_option parses index");

    app_action_route_result negative = route_scene_action(action("submit_option", "-1"));
    require(!negative.ok(), "negative submit_option fails");
    require(contains(negative.error, "non-negative integer"), "negative submit_option explains integer requirement");

    app_action_route_result text = route_scene_action(action("submit_option", "abc"));
    require(!text.ok(), "text submit_option fails");
    require(contains(text.error, "submit_option"), "text submit_option names action in error");
}

void test_submit_text_answer()
{
    using namespace quiz_vulkan;

    app_action_route_result missing_text = route_scene_action(action("submit_text_answer", "question_id"));
    require(!missing_text.ok(), "submit_text_answer without submitted text fails");
    require(contains(missing_text.error, "requires submitted text"), "missing text reports submitted text requirement");

    app_action_route_result submitted = route_scene_action(
        action("submit_text_answer", "question_id"),
        std::string_view{"scene snapshot"});
    require(submitted.ok(), "submit_text_answer with submitted text routes");
    const auto* payload = payload_if<domain::submit_text_answer_action>(submitted);
    require(payload != nullptr, "submit_text_answer stores payload");
    require(payload->answer_text == "scene snapshot", "submit_text_answer uses submitted text");
    require(payload->answer_text != "question_id", "submit_text_answer does not use question id payload as text");
}

void test_submit_multiselect()
{
    using namespace quiz_vulkan;

    app_action_route_result result = route_scene_action(action("submit_multiselect", "0, 2,5"));
    require(result.ok(), "submit_multiselect routes");
    const auto* payload = payload_if<domain::submit_multiselect_action>(result);
    require(payload != nullptr, "submit_multiselect stores payload");
    require(payload->option_indexes == std::vector<std::size_t>({0, 2, 5}), "submit_multiselect parses indexes");

    app_action_route_result bad = route_scene_action(action("submit_multiselect", "0,,2"));
    require(!bad.ok(), "bad submit_multiselect fails");
    require(contains(bad.error, "comma-separated"), "bad submit_multiselect reports comma-separated format");
}

void test_no_payload_actions()
{
    using namespace quiz_vulkan;

    require(route_scene_action(action("skip_question")).ok(), "skip_question routes without payload");
    require(route_scene_action(action("mark_question_known")).ok(), "mark_question_known routes without payload");
    require(route_scene_action(action("mark_question_unknown")).ok(), "mark_question_unknown routes without payload");
    require(route_scene_action(action("previous_question")).ok(), "previous_question routes without payload");
    require(route_scene_action(action("continue_after_feedback")).ok(), "continue_after_feedback routes without payload");
}

void test_update_setting()
{
    using namespace quiz_vulkan;

    app_action_route_result result = route_scene_action(action("update_setting", "wrong_note_enabled=yes"));
    require(result.ok(), "update_setting routes");
    const auto* payload = payload_if<domain::update_setting_action>(result);
    require(payload != nullptr, "update_setting stores payload");
    require(payload->name == "wrong_note_enabled", "update_setting parses setting name");
    require(payload->value == "yes", "update_setting parses setting value");

    app_action_route_result spaced = route_scene_action(action("update_setting", " ui_screen = deck_list "));
    require(spaced.ok(), "update_setting trims name and value");
    const auto* spaced_payload = payload_if<domain::update_setting_action>(spaced);
    require(spaced_payload != nullptr, "spaced update_setting stores payload");
    require(spaced_payload->name == "ui_screen", "spaced update_setting parses name");
    require(spaced_payload->value == "deck_list", "spaced update_setting parses value");

    app_action_route_result malformed = route_scene_action(action("update_setting", "wrong_note_enabled"));
    require(!malformed.ok(), "malformed update_setting fails");
    require(contains(malformed.error, "name=value"), "malformed update_setting reports name=value format");
}

void test_unknown_action()
{
    using namespace quiz_vulkan;

    app_action_route_result result = route_scene_action(action("tap", "button"));
    require(!result.ok(), "unknown action fails");
    require(contains(result.error, "Unsupported scene action type"), "unknown action reports unsupported type");
}

}  // namespace

int main()
{
    test_identity_actions();
    test_start_quiz_modes();
    test_submit_option();
    test_submit_text_answer();
    test_submit_multiselect();
    test_no_payload_actions();
    test_update_setting();
    test_unknown_action();

    return 0;
}
