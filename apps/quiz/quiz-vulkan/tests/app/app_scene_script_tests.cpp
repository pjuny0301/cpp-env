#include "app/app_command_registry.h"
#include "app/app_scene_script.h"
#include "core/domain/app_snapshot.hpp"
#include "core/layout/layout_placer.h"

#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace {

void require(bool condition, const char* message)
{
    assert((condition) && message);
}

bool contains(std::string_view value, std::string_view needle)
{
    return value.find(needle) != std::string_view::npos;
}

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

quiz_vulkan::domain::app_snapshot make_active_snapshot()
{
    using namespace quiz_vulkan::domain;

    std::vector<deck> decks;
    decks.push_back(make_test_deck());

    const learning_state_map learning;
    const previous_answer_map previous_answers;
    quiz_session session = start_quiz_session(decks.front(), learning, previous_answers);

    return make_app_snapshot(
        decks,
        std::optional<std::string>{"deck1"},
        std::optional<std::string>{"day1"},
        &session,
        learning);
}

quiz_vulkan::presentation::app_scene_script_document make_active_question_script()
{
    using namespace quiz_vulkan;

    presentation::app_scene_script_document script;
    script.schema_version = presentation::app_scene_script_node_dsl_schema_version;
    script.template_id = "test:quiz_active.prompt_options.v1";
    script.screen = "script_quiz_active";

    scene::scene_layout_rule root_rule;
    root_rule.mode = scene::scene_layout_mode::vertical;
    root_rule.gap = 8.0f;
    root_rule.padding = {12.0f, 12.0f, 12.0f, 12.0f};

    presentation::app_scene_script_node root;
    root.id = "script_root";
    root.kind = scene::scene_node_kind::container;
    root.debug_name = "script root";
    root.layout_rule = root_rule;
    root.style.token = "script_screen";
    root.style.background_color = "#101820";
    root.style.foreground_color = "#f6f7f9";
    script.nodes.push_back(std::move(root));

    presentation::app_scene_script_node prompt;
    prompt.id = "question_prompt";
    prompt.parent_id = "script_root";
    prompt.kind = scene::scene_node_kind::text;
    prompt.debug_name = "question prompt";
    prompt.style.token = "heading";
    prompt.style.foreground_color = "#ffffff";
    prompt.bindings.push_back({"text", "{{ question.prompt }}"});
    script.nodes.push_back(std::move(prompt));

    presentation::app_scene_script_node progress;
    progress.id = "session_progress";
    progress.parent_id = "script_root";
    progress.kind = scene::scene_node_kind::text;
    progress.debug_name = "session progress";
    progress.style.token = "muted";
    progress.bindings.push_back({"text", "{{ session.progress }}"});
    script.nodes.push_back(std::move(progress));

    presentation::app_scene_script_node session_mode;
    session_mode.id = "session_mode_phase";
    session_mode.parent_id = "script_root";
    session_mode.kind = scene::scene_node_kind::text;
    session_mode.debug_name = "session mode phase";
    session_mode.style.token = "muted";
    session_mode.bindings.push_back({"text", "{{ session.mode }} / {{ session.phase }}"});
    script.nodes.push_back(std::move(session_mode));

    presentation::app_scene_script_node session_count;
    session_count.id = "session_question_count";
    session_count.parent_id = "script_root";
    session_count.kind = scene::scene_node_kind::text;
    session_count.debug_name = "session question count";
    session_count.style.token = "muted";
    session_count.bindings.push_back({"text", "{{ session.question_count }}"});
    script.nodes.push_back(std::move(session_count));

    presentation::app_scene_script_node learning_summary;
    learning_summary.id = "learning_summary";
    learning_summary.parent_id = "script_root";
    learning_summary.kind = scene::scene_node_kind::text;
    learning_summary.debug_name = "learning summary";
    learning_summary.style.token = "muted";
    learning_summary.bindings.push_back({"text", "{{ learning.summary }}"});
    script.nodes.push_back(std::move(learning_summary));

    presentation::app_scene_script_node known_count;
    known_count.id = "learning_known_count";
    known_count.parent_id = "script_root";
    known_count.kind = scene::scene_node_kind::text;
    known_count.debug_name = "learning known count";
    known_count.style.token = "muted";
    known_count.bindings.push_back({"text", "{{ learning.known_count }}"});
    script.nodes.push_back(std::move(known_count));

    presentation::app_scene_script_node deck_title;
    deck_title.id = "selected_deck_title";
    deck_title.parent_id = "script_root";
    deck_title.kind = scene::scene_node_kind::text;
    deck_title.debug_name = "selected deck title";
    deck_title.style.token = "muted";
    deck_title.bindings.push_back({"text", "{{ selected_deck.title }}"});
    script.nodes.push_back(std::move(deck_title));

    presentation::app_scene_script_node day_summary;
    day_summary.id = "selected_day_summary";
    day_summary.parent_id = "script_root";
    day_summary.kind = scene::scene_node_kind::text;
    day_summary.debug_name = "selected day summary";
    day_summary.style.token = "muted";
    day_summary.bindings.push_back({"text", "{{ selected_day.title }} / {{ selected_day.question_count }} questions"});
    script.nodes.push_back(std::move(day_summary));

    presentation::app_scene_script_node deck_count;
    deck_count.id = "deck_count";
    deck_count.parent_id = "script_root";
    deck_count.kind = scene::scene_node_kind::text;
    deck_count.debug_name = "deck count";
    deck_count.style.token = "muted";
    deck_count.bindings.push_back({"text", "{{ deck.count }}"});
    script.nodes.push_back(std::move(deck_count));

    presentation::app_scene_script_node formatted_prompt;
    formatted_prompt.id = "question_prompt_upper";
    formatted_prompt.parent_id = "script_root";
    formatted_prompt.kind = scene::scene_node_kind::text;
    formatted_prompt.debug_name = "formatted question prompt";
    formatted_prompt.style.token = "muted";
    formatted_prompt.bindings.push_back({"text", "{{ question.prompt | upper }}"});
    script.nodes.push_back(std::move(formatted_prompt));

    presentation::app_scene_script_node formatted_type;
    formatted_type.id = "question_type_lower";
    formatted_type.parent_id = "script_root";
    formatted_type.kind = scene::scene_node_kind::text;
    formatted_type.debug_name = "formatted question type";
    formatted_type.style.token = "muted";
    formatted_type.bindings.push_back({"text", "{{ question.type | upper | lower }}"});
    script.nodes.push_back(std::move(formatted_type));

    presentation::app_scene_script_node function_title;
    function_title.id = "deck_day_function_title";
    function_title.parent_id = "script_root";
    function_title.kind = scene::scene_node_kind::text;
    function_title.debug_name = "deck day function title";
    function_title.style.token = "muted";
    function_title.bindings.push_back({"text", "{{ concat(selected_deck.title, \" / \", selected_day.title) }}"});
    script.nodes.push_back(std::move(function_title));

    presentation::app_scene_script_node function_title_upper;
    function_title_upper.id = "deck_day_function_title_upper";
    function_title_upper.parent_id = "script_root";
    function_title_upper.kind = scene::scene_node_kind::text;
    function_title_upper.debug_name = "deck day function title upper";
    function_title_upper.style.token = "muted";
    function_title_upper.bindings.push_back({"text", "{{ concat(selected_deck.title, \" / \", selected_day.title) | upper }}"});
    script.nodes.push_back(std::move(function_title_upper));

    presentation::app_scene_script_node session_active_flag;
    session_active_flag.id = "session_active_flag";
    session_active_flag.parent_id = "script_root";
    session_active_flag.kind = scene::scene_node_kind::text;
    session_active_flag.debug_name = "session active flag";
    session_active_flag.style.token = "muted";
    session_active_flag.bindings.push_back({"text", "{{ equals(session.phase, \"active\") }}"});
    script.nodes.push_back(std::move(session_active_flag));

    presentation::app_scene_script_node function_condition;
    function_condition.id = "function_condition_active";
    function_condition.parent_id = "script_root";
    function_condition.kind = scene::scene_node_kind::text;
    function_condition.debug_name = "function condition active";
    function_condition.condition = "equals(session.phase, \"active\")";
    function_condition.style.token = "muted";
    function_condition.text_runs.push_back({"Function condition active", "muted"});
    script.nodes.push_back(std::move(function_condition));

    presentation::app_scene_script_node not_condition;
    not_condition.id = "function_condition_not_completed";
    not_condition.parent_id = "script_root";
    not_condition.kind = scene::scene_node_kind::text;
    not_condition.debug_name = "function condition not completed";
    not_condition.condition = "not(session.completed)";
    not_condition.style.token = "muted";
    not_condition.text_runs.push_back({"Function condition not completed", "muted"});
    script.nodes.push_back(std::move(not_condition));

    presentation::app_scene_script_node long_text;
    long_text.id = "question_long_text";
    long_text.parent_id = "script_root";
    long_text.kind = scene::scene_node_kind::text;
    long_text.debug_name = "question long text";
    long_text.condition = "question.has_long_text";
    long_text.style.token = "body";
    long_text.bindings.push_back({"text", "{{ question.long_text }}"});
    script.nodes.push_back(std::move(long_text));

    scene::scene_layout_rule options_rule;
    options_rule.mode = scene::scene_layout_mode::vertical;
    options_rule.gap = 4.0f;

    presentation::app_scene_script_node options;
    options.id = "question_options";
    options.parent_id = "script_root";
    options.kind = scene::scene_node_kind::container;
    options.debug_name = "question options";
    options.condition = "question.has_options";
    options.layout_rule = options_rule;
    options.style.token = "option_group";
    script.nodes.push_back(std::move(options));

    scene::scene_layout_rule option_rule;
    option_rule.has_height = true;
    option_rule.height = 44.0f;
    option_rule.padding = {12.0f, 10.0f, 12.0f, 10.0f};

    presentation::app_scene_script_node option;
    option.id = "option_{{ option.index }}";
    option.parent_id = "question_options";
    option.kind = scene::scene_node_kind::input;
    option.debug_name = "answer option";
    option.layout_rule = option_rule;
    option.style.token = "button";
    option.style.background_color = "#29445a";
    option.style.foreground_color = "#ffffff";
    option.style.border_radius = 6.0f;
    option.repeater = presentation::app_scene_script_repeater{"option", "question.options"};
    option.bindings.push_back({"text", "{{ option.text }}"});
    presentation::app_scene_script_event_handler_template press;
    press.trigger = scene::scene_action_trigger::press;
    press.commands.push_back({"submit_option", {{"option_index", "{{ option.index }}"}}});
    option.events.push_back(std::move(press));
    script.nodes.push_back(std::move(option));

    script.transitions.push_back({"script_enter", 0.2f, "question.exists"});
    return script;
}

void apply_patch_to_scene(
    const quiz_vulkan::scene::scene_layout_patch& patch,
    quiz_vulkan::scene::scene_layout_data& data)
{
    const quiz_vulkan::scene::scene_layout_apply_result result = patch.apply_to(data);
    require(result.applied(), "script patch applies");
}

void test_phase3_dsl_compiles_bindings_repeaters_conditions_events()
{
    using namespace quiz_vulkan;

    const domain::app_snapshot snapshot = make_active_snapshot();
    const presentation::app_scene_script_document script = make_active_question_script();

    const presentation::app_scene_script_validation_result validation =
        presentation::validate_app_scene_script_document(script);
    require(validation.ok(), "phase3 script schema validates");

    const presentation::app_scene_script_compile_result compiled =
        presentation::compile_app_scene_script(script, snapshot);
    require(compiled.ok(), "phase3 script compiles");

    scene::scene_layout_data data("script_test");
    apply_patch_to_scene(*compiled.patch, data);
    require(data.route_state().screen_id == "script_quiz_active", "script route is emitted");
    require(data.animation_state().active, "script transition starts");
    require(data.animation_state().name == "script_enter", "script transition name is emitted");

    const scene::scene_node_data* prompt = data.find_node("question_prompt");
    require(prompt != nullptr, "prompt node exists");
    require(prompt->text_runs.size() == 1, "prompt text run exists");
    require(prompt->text_runs.front().text == "Capital of Korea?", "prompt binding renders question.prompt");
    const scene::scene_node_data* progress = data.find_node("session_progress");
    const scene::scene_node_data* session_mode = data.find_node("session_mode_phase");
    const scene::scene_node_data* session_count = data.find_node("session_question_count");
    require(progress != nullptr, "session progress node exists");
    require(session_mode != nullptr, "session mode node exists");
    require(session_count != nullptr, "session count node exists");
    require(progress->text_runs.front().text == "Question 1 of 1", "session progress binding renders");
    require(session_mode->text_runs.front().text == "normal / active", "session mode and phase render");
    require(session_count->text_runs.front().text == "1", "session question count renders");
    const scene::scene_node_data* learning_summary = data.find_node("learning_summary");
    const scene::scene_node_data* known_count = data.find_node("learning_known_count");
    require(learning_summary != nullptr, "learning summary node exists");
    require(known_count != nullptr, "known count node exists");
    require(learning_summary->text_runs.front().text == "Learning 1 / Known 0 / Unknown 0 / Wrong note 0", "learning summary binding renders");
    require(known_count->text_runs.front().text == "0", "known count binding renders");
    const scene::scene_node_data* deck_title = data.find_node("selected_deck_title");
    const scene::scene_node_data* day_summary = data.find_node("selected_day_summary");
    const scene::scene_node_data* deck_count = data.find_node("deck_count");
    require(deck_title != nullptr, "selected deck title node exists");
    require(day_summary != nullptr, "selected day summary node exists");
    require(deck_count != nullptr, "deck count node exists");
    require(deck_title->text_runs.front().text == "Geography", "selected deck title binding renders");
    require(day_summary->text_runs.front().text == "Day 1 / 1 questions", "selected day summary binding renders");
    require(deck_count->text_runs.front().text == "1", "deck count binding renders");
    const scene::scene_node_data* formatted_prompt = data.find_node("question_prompt_upper");
    const scene::scene_node_data* formatted_type = data.find_node("question_type_lower");
    require(formatted_prompt != nullptr, "formatted prompt node exists");
    require(formatted_type != nullptr, "formatted type node exists");
    require(formatted_prompt->text_runs.front().text == "CAPITAL OF KOREA?", "upper formatter renders prompt");
    require(formatted_type->text_runs.front().text == "answer", "formatter chain renders question type");
    const scene::scene_node_data* function_title = data.find_node("deck_day_function_title");
    const scene::scene_node_data* function_title_upper = data.find_node("deck_day_function_title_upper");
    const scene::scene_node_data* session_active_flag = data.find_node("session_active_flag");
    require(function_title != nullptr, "function title node exists");
    require(function_title_upper != nullptr, "function title upper node exists");
    require(session_active_flag != nullptr, "function active flag node exists");
    require(function_title->text_runs.front().text == "Geography / Day 1", "concat function renders text");
    require(function_title_upper->text_runs.front().text == "GEOGRAPHY / DAY 1", "function result supports formatter chain");
    require(session_active_flag->text_runs.front().text == "true", "equals function renders boolean");
    require(data.contains_node("function_condition_active"), "function expression can drive conditions");
    require(data.contains_node("function_condition_not_completed"), "not function can drive conditions");
    require(!data.contains_node("question_long_text"), "false condition suppresses long text node");

    const scene::scene_node_data* option_0 = data.find_node("option_0");
    const scene::scene_node_data* option_1 = data.find_node("option_1");
    require(option_0 != nullptr, "first repeated option exists");
    require(option_1 != nullptr, "second repeated option exists");
    require(option_0->text_runs.front().text == "Seoul", "first option text binding renders");
    require(option_1->text_runs.front().text == "Busan", "second option text binding renders");
    require(!option_0->has_action_binding, "script command does not require legacy action binding");
    require(option_0->has_event_handlers, "script event handler is emitted");
    require(option_0->event_handlers.size() == 1, "script emits one event handler");
    require(option_0->event_handlers.front().commands.size() == 1, "script event emits one command");

    const scene::scene_command& command = option_0->event_handlers.front().commands.front();
    require(command.name == "submit_option", "script command name is emitted");
    const scene::scene_value* option_index = command.find_arg("option_index");
    require(option_index != nullptr, "script command arg is emitted");
    require(option_index->int_if() != nullptr, "option index arg preserves integer type");
    require(*option_index->int_if() == 0, "option index arg is deterministic");

    const app_command_route_result routed = route_scene_command(command);
    require(routed.ok(), "script command validates through app command registry");
    const auto* action = std::get_if<domain::submit_option_action>(&routed.action->payload);
    require(action != nullptr, "script command routes to submit_option action");
    require(action->option_index == 0, "script command routes with bound option index");
}

void test_phase3_dsl_validation_and_determinism()
{
    using namespace quiz_vulkan;

    const domain::app_snapshot snapshot = make_active_snapshot();
    presentation::app_scene_script_document script = make_active_question_script();

    const presentation::app_scene_script_compile_result first =
        presentation::compile_app_scene_script(script, snapshot);
    const presentation::app_scene_script_compile_result second =
        presentation::compile_app_scene_script(script, snapshot);
    require(first.ok(), "first deterministic compile succeeds");
    require(second.ok(), "second deterministic compile succeeds");

    scene::scene_layout_data first_data("script_first");
    scene::scene_layout_data second_data("script_second");
    apply_patch_to_scene(*first.patch, first_data);
    apply_patch_to_scene(*second.patch, second_data);
    require(first_data.nodes().size() == second_data.nodes().size(), "deterministic compile emits same node count");
    require(first_data.find_node("option_1")->text_runs.front().text == second_data.find_node("option_1")->text_runs.front().text,
        "deterministic compile emits same repeater text");

    presentation::app_scene_script_node duplicate;
    duplicate.id = "script_root";
    duplicate.kind = scene::scene_node_kind::container;
    script.nodes.push_back(std::move(duplicate));
    const presentation::app_scene_script_validation_result duplicate_validation =
        presentation::validate_app_scene_script_document(script);
    require(!duplicate_validation.ok(), "duplicate literal node id fails validation");

    presentation::app_scene_script_document bad_command = make_active_question_script();
    bad_command.nodes.back().events.front().commands.front().name = "delete_everything";
    const presentation::app_scene_script_validation_result command_validation =
        presentation::validate_app_scene_script_document(bad_command);
    require(!command_validation.ok(), "non-allowlisted command fails validation");
}

void test_compiled_patch_unwrap_reports_failure_context()
{
    using namespace quiz_vulkan;

    presentation::app_scene_script_compile_result failed;
    failed.error = "script contains duplicate literal node id: script_root";

    bool threw = false;
    try {
        (void)presentation::require_compiled_app_scene_script_patch(std::move(failed), "test:bad_script");
    } catch (const std::logic_error& error) {
        threw = true;
        const std::string message = error.what();
        require(message.find("test:bad_script") != std::string::npos, "compile failure context is reported");
        require(message.find("duplicate literal node id") != std::string::npos, "compile failure reason is reported");
    }

    require(threw, "failed compile result throws before patch unwrap");
}

void append_invalid_function_node(
    quiz_vulkan::presentation::app_scene_script_document& script,
    std::string expression)
{
    quiz_vulkan::presentation::app_scene_script_node invalid;
    invalid.id = "invalid_function_node";
    invalid.parent_id = "script_root";
    invalid.kind = quiz_vulkan::scene::scene_node_kind::text;
    invalid.debug_name = "invalid function node";
    invalid.style.token = "muted";
    invalid.bindings.push_back({"text", std::move(expression)});
    script.nodes.push_back(std::move(invalid));
}

void require_compile_error_contains(
    const quiz_vulkan::presentation::app_scene_script_document& script,
    const quiz_vulkan::domain::app_snapshot& snapshot,
    std::string_view expected_error,
    const char* message)
{
    const quiz_vulkan::presentation::app_scene_script_compile_result compiled =
        quiz_vulkan::presentation::compile_app_scene_script(script, snapshot);
    require(!compiled.ok(), message);
    require(contains(compiled.error, expected_error), message);
}

void test_expression_function_errors_are_reported()
{
    using namespace quiz_vulkan;

    const domain::app_snapshot snapshot = make_active_snapshot();

    presentation::app_scene_script_document missing_arg = make_active_question_script();
    append_invalid_function_node(missing_arg, "{{ equals(session.phase) }}");
    require_compile_error_contains(
        missing_arg,
        snapshot,
        "expects 2 argument",
        "function arg count errors are reported");

    presentation::app_scene_script_document unsupported = make_active_question_script();
    append_invalid_function_node(unsupported, "{{ mystery(session.phase) }}");
    require_compile_error_contains(
        unsupported,
        snapshot,
        "unsupported script function: mystery",
        "unsupported function errors are reported");

    presentation::app_scene_script_document unterminated = make_active_question_script();
    append_invalid_function_node(unterminated, "{{ concat(\"unterminated) }}");
    require_compile_error_contains(
        unterminated,
        snapshot,
        "unterminated string literal",
        "unterminated function string errors are reported");
}

}  // namespace

int main()
{
    test_phase3_dsl_compiles_bindings_repeaters_conditions_events();
    test_phase3_dsl_validation_and_determinism();
    test_compiled_patch_unwrap_reports_failure_context();
    test_expression_function_errors_are_reported();
    return 0;
}
