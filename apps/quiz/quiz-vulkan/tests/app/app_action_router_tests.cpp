#include "app/app_action_router.h"
#include "app/app_command_registry.h"

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <optional>
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

quiz_vulkan::scene::scene_command command(
    std::string name,
    quiz_vulkan::scene::scene_command_args args = {})
{
    return quiz_vulkan::scene::make_scene_command(std::move(name), std::move(args));
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

template <typename Payload>
const Payload* command_payload_if(const quiz_vulkan::app_command_route_result& result)
{
    if (!result.action.has_value()) {
        return nullptr;
    }

    return std::get_if<Payload>(&result.action->payload);
}

void require_same_action_type(
    const quiz_vulkan::app_action_route_result& legacy,
    const quiz_vulkan::app_command_route_result& typed,
    const char* message)
{
    require(legacy.ok(), message);
    require(typed.ok(), message);
    require(legacy.action.has_value(), message);
    require(typed.action.has_value(), message);
    require(quiz_vulkan::domain::type_of(*legacy.action) == quiz_vulkan::domain::type_of(*typed.action), message);
}

void require_same_action_payload(
    const quiz_vulkan::domain::app_action& legacy,
    const quiz_vulkan::domain::app_action& typed,
    const char* message)
{
    using namespace quiz_vulkan;

    require(domain::type_of(legacy) == domain::type_of(typed), message);

    if (const auto* payload = std::get_if<domain::load_source_action>(&legacy.payload)) {
        const auto* typed_payload = std::get_if<domain::load_source_action>(&typed.payload);
        require(typed_payload != nullptr, message);
        require(payload->source_uri == typed_payload->source_uri, message);
        return;
    }
    if (const auto* payload = std::get_if<domain::select_deck_action>(&legacy.payload)) {
        const auto* typed_payload = std::get_if<domain::select_deck_action>(&typed.payload);
        require(typed_payload != nullptr, message);
        require(payload->deck_id == typed_payload->deck_id, message);
        return;
    }
    if (const auto* payload = std::get_if<domain::select_day_action>(&legacy.payload)) {
        const auto* typed_payload = std::get_if<domain::select_day_action>(&typed.payload);
        require(typed_payload != nullptr, message);
        require(payload->day_id == typed_payload->day_id, message);
        return;
    }
    if (const auto* payload = std::get_if<domain::start_quiz_action>(&legacy.payload)) {
        const auto* typed_payload = std::get_if<domain::start_quiz_action>(&typed.payload);
        require(typed_payload != nullptr, message);
        require(payload->mode == typed_payload->mode, message);
        require(payload->random_seed == typed_payload->random_seed, message);
        require(payload->shuffle == typed_payload->shuffle, message);
        return;
    }
    if (const auto* payload = std::get_if<domain::submit_option_action>(&legacy.payload)) {
        const auto* typed_payload = std::get_if<domain::submit_option_action>(&typed.payload);
        require(typed_payload != nullptr, message);
        require(payload->option_index == typed_payload->option_index, message);
        return;
    }
    if (const auto* payload = std::get_if<domain::submit_text_answer_action>(&legacy.payload)) {
        const auto* typed_payload = std::get_if<domain::submit_text_answer_action>(&typed.payload);
        require(typed_payload != nullptr, message);
        require(payload->answer_text == typed_payload->answer_text, message);
        return;
    }
    if (const auto* payload = std::get_if<domain::submit_multiselect_action>(&legacy.payload)) {
        const auto* typed_payload = std::get_if<domain::submit_multiselect_action>(&typed.payload);
        require(typed_payload != nullptr, message);
        require(payload->option_indexes == typed_payload->option_indexes, message);
        return;
    }
    if (const auto* payload = std::get_if<domain::update_setting_action>(&legacy.payload)) {
        const auto* typed_payload = std::get_if<domain::update_setting_action>(&typed.payload);
        require(typed_payload != nullptr, message);
        require(payload->name == typed_payload->name, message);
        require(payload->value == typed_payload->value, message);
        return;
    }
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

void test_typed_command_equivalence()
{
    using namespace quiz_vulkan;

    app_action_route_result legacy_start = route_scene_action(action("start_quiz", "wrong"));
    app_command_route_result typed_start = route_scene_command(
        command("start_quiz", {{"mode", scene::scene_value("wrong")}}));
    require_same_action_type(legacy_start, typed_start, "start_quiz typed route matches legacy type");
    const auto* legacy_start_payload = payload_if<domain::start_quiz_action>(legacy_start);
    const auto* typed_start_payload = command_payload_if<domain::start_quiz_action>(typed_start);
    require(legacy_start_payload != nullptr, "legacy start_quiz payload exists");
    require(typed_start_payload != nullptr, "typed start_quiz payload exists");
    require(legacy_start_payload->mode == typed_start_payload->mode, "typed start_quiz mode equals legacy route");

    app_action_route_result legacy_option = route_scene_action(action("submit_option", "2"));
    app_command_route_result typed_option = route_scene_command(
        command("submit_option", {{"option_index", scene::scene_value(2)}}));
    require_same_action_type(legacy_option, typed_option, "submit_option typed route matches legacy type");
    const auto* legacy_option_payload = payload_if<domain::submit_option_action>(legacy_option);
    const auto* typed_option_payload = command_payload_if<domain::submit_option_action>(typed_option);
    require(legacy_option_payload != nullptr, "legacy submit_option payload exists");
    require(typed_option_payload != nullptr, "typed submit_option payload exists");
    require(legacy_option_payload->option_index == typed_option_payload->option_index, "typed option index equals legacy route");

    app_action_route_result legacy_continue = route_scene_action(action("continue_after_feedback"));
    app_command_route_result typed_continue = route_scene_command(command("continue_after_feedback"));
    require_same_action_type(legacy_continue, typed_continue, "continue typed route matches legacy type");

    const scene::scene_event_handler wrapped_handler = scene::make_scene_event_handler(action("start_quiz", "normal"));
    require(wrapped_handler.commands.size() == 1, "legacy wrapper exposes one command");
    app_command_route_result wrapped_command = route_scene_command(wrapped_handler.commands.front());
    app_action_route_result wrapped_legacy = route_scene_action(action("start_quiz", "normal"));
    require_same_action_type(wrapped_legacy, wrapped_command, "legacy wrapper command routes like action binding");
    const auto* wrapped_legacy_payload = payload_if<domain::start_quiz_action>(wrapped_legacy);
    const auto* wrapped_command_payload = command_payload_if<domain::start_quiz_action>(wrapped_command);
    require(wrapped_legacy_payload != nullptr, "legacy wrapper action payload exists");
    require(wrapped_command_payload != nullptr, "legacy wrapper command payload exists");
    require(wrapped_legacy_payload->mode == wrapped_command_payload->mode, "legacy wrapper payload equals command payload");

    struct equivalence_case {
        const char* action_type;
        const char* payload;
        std::optional<std::string_view> submitted_text;
    };

    const std::vector<equivalence_case> cases = {
        {"load_source", "file://deck.json", std::nullopt},
        {"select_deck", "deck1", std::nullopt},
        {"select_day", "day1", std::nullopt},
        {"start_quiz", "due", std::nullopt},
        {"submit_option", "1", std::nullopt},
        {"submit_text_answer", "question_id_is_not_answer_text", std::string_view{"Seoul"}},
        {"submit_multiselect", "0, 2", std::nullopt},
        {"skip_question", "", std::nullopt},
        {"mark_question_known", "", std::nullopt},
        {"mark_question_unknown", "", std::nullopt},
        {"previous_question", "", std::nullopt},
        {"continue_after_feedback", "", std::nullopt},
        {"update_setting", "ui_screen=settings", std::nullopt},
    };

    for (const equivalence_case& test_case : cases) {
        const scene::scene_action_binding legacy_binding = action(test_case.action_type, test_case.payload);
        const app_action_route_result legacy = route_scene_action(legacy_binding, test_case.submitted_text);
        require(legacy.ok(), "legacy route in full equivalence matrix succeeds");

        const scene::scene_event_handler typed_handler = scene::make_scene_event_handler(legacy_binding);
        require(typed_handler.commands.size() == 1, "legacy binding wrapper emits one typed command");
        const app_command_route_result typed = route_scene_command(typed_handler.commands.front(), test_case.submitted_text);
        require(typed.ok(), "typed route in full equivalence matrix succeeds");
        require(legacy.action.has_value(), "legacy matrix action exists");
        require(typed.action.has_value(), "typed matrix action exists");
        require_same_action_payload(*legacy.action, *typed.action, "legacy and typed command payloads are identical");
    }

    const app_command_route_result typed_setting = route_scene_command(
        command("update_setting", {{"name", scene::scene_value("ui_screen")}, {"value", scene::scene_value("settings")}}));
    require(typed_setting.ok(), "typed update_setting explicit args route");
    const auto* typed_setting_payload = command_payload_if<domain::update_setting_action>(typed_setting);
    require(typed_setting_payload != nullptr, "typed update_setting payload exists");
    require(typed_setting_payload->name == "ui_screen", "typed update_setting name preserved");
    require(typed_setting_payload->value == "settings", "typed update_setting value preserved");

    const app_command_route_result typed_text = route_scene_command(
        command("submit_text_answer", {{"answer_text", scene::scene_value("typed answer")}}));
    require(typed_text.ok(), "typed submit_text_answer explicit answer routes");
    const auto* typed_text_payload = command_payload_if<domain::submit_text_answer_action>(typed_text);
    require(typed_text_payload != nullptr, "typed submit_text_answer payload exists");
    require(typed_text_payload->answer_text == "typed answer", "typed submit_text_answer answer preserved");

    const app_command_validation_result unknown_arg = validate_scene_command(
        command("start_quiz", {{"mode", scene::scene_value("normal")}, {"surprise", scene::scene_value("no")}}));
    require(!unknown_arg.ok(), "typed command rejects non-allowlisted args");
    require(contains(unknown_arg.error, "Unsupported argument"), "unknown arg validation reports allowlist failure");

    const app_command_validation_result unknown_command = validate_scene_command(command("delete_everything"));
    require(!unknown_command.ok(), "typed command rejects non-allowlisted command names");
    require(contains(unknown_command.error, "Unsupported scene command"), "unknown command validation reports unsupported command");
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
    test_typed_command_equivalence();
    test_submit_text_answer();
    test_submit_multiselect();
    test_no_payload_actions();
    test_update_setting();
    test_unknown_action();

    return 0;
}
