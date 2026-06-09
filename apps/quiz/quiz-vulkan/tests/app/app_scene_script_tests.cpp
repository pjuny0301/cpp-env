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
    quiz_question.image_uri = "asset://korea-map.png";
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
    quiz_deck.source_uri = "fixture://geography.quizdeck";
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

quiz_vulkan::domain::app_snapshot make_feedback_snapshot()
{
    using namespace quiz_vulkan::domain;

    std::vector<deck> decks;
    decks.push_back(make_test_deck());

    const learning_state_map learning;
    const previous_answer_map previous_answers;
    quiz_session session = start_quiz_session(decks.front(), learning, previous_answers);
    (void)submit_option_answer(session, decks.front(), 0, 100);

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

    presentation::app_scene_script_node question_learning;
    question_learning.id = "question_learning";
    question_learning.parent_id = "script_root";
    question_learning.kind = scene::scene_node_kind::text;
    question_learning.debug_name = "question learning";
    question_learning.style.token = "muted";
    question_learning.bindings.push_back({"text", "{{ question.learning }}"});
    script.nodes.push_back(std::move(question_learning));

    presentation::app_scene_script_node question_learning_flags;
    question_learning_flags.id = "question_learning_flags";
    question_learning_flags.parent_id = "script_root";
    question_learning_flags.kind = scene::scene_node_kind::text;
    question_learning_flags.debug_name = "question learning flags";
    question_learning_flags.style.token = "muted";
    question_learning_flags.bindings.push_back({"text", "{{ question.is_learning }} / {{ question.is_known }} / {{ question.is_unknown }} / {{ question.is_wrong_note }}"});
    script.nodes.push_back(std::move(question_learning_flags));

    presentation::app_scene_script_node question_has_image;
    question_has_image.id = "question_has_image";
    question_has_image.parent_id = "script_root";
    question_has_image.kind = scene::scene_node_kind::text;
    question_has_image.debug_name = "question has image";
    question_has_image.style.token = "muted";
    question_has_image.bindings.push_back({"text", "{{ question.has_image }}"});
    script.nodes.push_back(std::move(question_has_image));

    presentation::app_scene_script_node question_image_uri;
    question_image_uri.id = "question_image_uri";
    question_image_uri.parent_id = "script_root";
    question_image_uri.kind = scene::scene_node_kind::text;
    question_image_uri.debug_name = "question image uri";
    question_image_uri.style.token = "muted";
    question_image_uri.bindings.push_back({"text", "{{ choose(question.has_image, question.image_uri, \"no-image\") }}"});
    script.nodes.push_back(std::move(question_image_uri));

    presentation::app_scene_script_node question_image;
    question_image.id = "question_image";
    question_image.parent_id = "script_root";
    question_image.kind = scene::scene_node_kind::image;
    question_image.debug_name = "question image";
    question_image.image.aspect_ratio = 1.0f;
    question_image.bindings.push_back({"image.uri", "{{ choose(question.has_image, question.image_uri, \"\") }}"});
    question_image.bindings.push_back({"image.alt_text", "{{ concat(\"Question image for \", question.id) }}"});
    question_image.bindings.push_back({"image.aspect_ratio", "1.6"});
    question_image.bindings.push_back({"style.opacity", "0.75"});
    question_image.bindings.push_back({"style.border_radius", "8"});
    script.nodes.push_back(std::move(question_image));

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

    presentation::app_scene_script_node feedback_exists;
    feedback_exists.id = "feedback_exists";
    feedback_exists.parent_id = "script_root";
    feedback_exists.kind = scene::scene_node_kind::text;
    feedback_exists.debug_name = "feedback exists";
    feedback_exists.style.token = "muted";
    feedback_exists.bindings.push_back({"text", "{{ feedback.exists }}"});
    script.nodes.push_back(std::move(feedback_exists));

    presentation::app_scene_script_node feedback_label;
    feedback_label.id = "feedback_label";
    feedback_label.parent_id = "script_root";
    feedback_label.kind = scene::scene_node_kind::text;
    feedback_label.debug_name = "feedback label";
    feedback_label.style.token = "muted";
    feedback_label.bindings.push_back({"text", "{{ choose(feedback.exists, feedback.outcome, \"none\") }}"});
    script.nodes.push_back(std::move(feedback_label));

    presentation::app_scene_script_node feedback_selected_count;
    feedback_selected_count.id = "feedback_selected_option_count";
    feedback_selected_count.parent_id = "script_root";
    feedback_selected_count.kind = scene::scene_node_kind::text;
    feedback_selected_count.debug_name = "feedback selected option count";
    feedback_selected_count.style.token = "muted";
    feedback_selected_count.bindings.push_back({"text", "{{ choose(feedback.exists, feedback.selected_option_count, 0) }}"});
    script.nodes.push_back(std::move(feedback_selected_count));

    presentation::app_scene_script_node feedback_answered_at;
    feedback_answered_at.id = "feedback_answered_at";
    feedback_answered_at.parent_id = "script_root";
    feedback_answered_at.kind = scene::scene_node_kind::text;
    feedback_answered_at.debug_name = "feedback answered at";
    feedback_answered_at.style.token = "muted";
    feedback_answered_at.bindings.push_back({"text", "{{ choose(feedback.exists, feedback.answered_at_ms, 0) }}"});
    script.nodes.push_back(std::move(feedback_answered_at));

    presentation::app_scene_script_node settings_count;
    settings_count.id = "settings_count";
    settings_count.parent_id = "script_root";
    settings_count.kind = scene::scene_node_kind::text;
    settings_count.debug_name = "settings count";
    settings_count.style.token = "muted";
    settings_count.bindings.push_back({"text", "{{ settings.count }}"});
    script.nodes.push_back(std::move(settings_count));

    presentation::app_scene_script_node settings_count_label;
    settings_count_label.id = "settings_count_label";
    settings_count_label.parent_id = "script_root";
    settings_count_label.kind = scene::scene_node_kind::text;
    settings_count_label.debug_name = "settings count label";
    settings_count_label.style.token = "muted";
    settings_count_label.bindings.push_back({"text", "{{ format_count(settings.count, \"setting\") }}"});
    script.nodes.push_back(std::move(settings_count_label));

    presentation::app_scene_script_node setting_route;
    setting_route.id = "setting_route";
    setting_route.parent_id = "script_root";
    setting_route.kind = scene::scene_node_kind::text;
    setting_route.debug_name = "setting route";
    setting_route.style.token = "muted";
    setting_route.bindings.push_back({"text", "{{ setting(\"ui_screen\", \"unset\") }}"});
    script.nodes.push_back(std::move(setting_route));

    presentation::app_scene_script_node error_exists;
    error_exists.id = "error_exists";
    error_exists.parent_id = "script_root";
    error_exists.kind = scene::scene_node_kind::text;
    error_exists.debug_name = "error exists";
    error_exists.style.token = "muted";
    error_exists.bindings.push_back({"text", "{{ error.exists }}"});
    script.nodes.push_back(std::move(error_exists));

    presentation::app_scene_script_node error_message;
    error_message.id = "error_message";
    error_message.parent_id = "script_root";
    error_message.kind = scene::scene_node_kind::text;
    error_message.debug_name = "error message";
    error_message.style.token = "muted";
    error_message.bindings.push_back({"text", "{{ error.message }}"});
    script.nodes.push_back(std::move(error_message));

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

    presentation::app_scene_script_node deck_source;
    deck_source.id = "selected_deck_source";
    deck_source.parent_id = "script_root";
    deck_source.kind = scene::scene_node_kind::text;
    deck_source.debug_name = "selected deck source";
    deck_source.style.token = "muted";
    deck_source.bindings.push_back({"text", "{{ choose(selected_deck.has_source, selected_deck.source_uri, \"no-source\") }}"});
    script.nodes.push_back(std::move(deck_source));

    presentation::app_scene_script_node day_summary;
    day_summary.id = "selected_day_summary";
    day_summary.parent_id = "script_root";
    day_summary.kind = scene::scene_node_kind::text;
    day_summary.debug_name = "selected day summary";
    day_summary.style.token = "muted";
    day_summary.bindings.push_back({"text", "{{ selected_day.title }} / {{ selected_day.question_count }} questions"});
    script.nodes.push_back(std::move(day_summary));

    presentation::app_scene_script_node day_question_count_label;
    day_question_count_label.id = "selected_day_question_count_label";
    day_question_count_label.parent_id = "script_root";
    day_question_count_label.kind = scene::scene_node_kind::text;
    day_question_count_label.debug_name = "selected day question count label";
    day_question_count_label.style.token = "muted";
    day_question_count_label.bindings.push_back({"text", "{{ format_count(selected_day.question_count, \"question\") }}"});
    script.nodes.push_back(std::move(day_question_count_label));

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

    presentation::app_scene_script_node function_title_pipe_literal;
    function_title_pipe_literal.id = "deck_day_function_pipe_literal";
    function_title_pipe_literal.parent_id = "script_root";
    function_title_pipe_literal.kind = scene::scene_node_kind::text;
    function_title_pipe_literal.debug_name = "deck day function pipe literal";
    function_title_pipe_literal.style.token = "muted";
    function_title_pipe_literal.bindings.push_back({"text", "{{ concat(selected_deck.title, \" | \", selected_day.title) }}"});
    script.nodes.push_back(std::move(function_title_pipe_literal));

    presentation::app_scene_script_node safe_deck_id;
    safe_deck_id.id = "safe_deck_id";
    safe_deck_id.parent_id = "script_root";
    safe_deck_id.kind = scene::scene_node_kind::text;
    safe_deck_id.debug_name = "safe deck id";
    safe_deck_id.style.token = "muted";
    safe_deck_id.bindings.push_back({"text", "{{ safe_id(concat(selected_deck.title, \" deck\")) }}"});
    script.nodes.push_back(std::move(safe_deck_id));

    presentation::app_scene_script_node session_active_flag;
    session_active_flag.id = "session_active_flag";
    session_active_flag.parent_id = "script_root";
    session_active_flag.kind = scene::scene_node_kind::text;
    session_active_flag.debug_name = "session active flag";
    session_active_flag.style.token = "muted";
    session_active_flag.bindings.push_back({"text", "{{ equals(session.phase, \"active\") }}"});
    script.nodes.push_back(std::move(session_active_flag));

    presentation::app_scene_script_node empty_error_flag;
    empty_error_flag.id = "empty_error_flag";
    empty_error_flag.parent_id = "script_root";
    empty_error_flag.kind = scene::scene_node_kind::text;
    empty_error_flag.debug_name = "empty error flag";
    empty_error_flag.style.token = "muted";
    empty_error_flag.bindings.push_back({"text", "{{ empty(error.message) }}"});
    script.nodes.push_back(std::move(empty_error_flag));

    presentation::app_scene_script_node string_predicates;
    string_predicates.id = "string_predicate_flags";
    string_predicates.parent_id = "script_root";
    string_predicates.kind = scene::scene_node_kind::text;
    string_predicates.debug_name = "string predicate flags";
    string_predicates.style.token = "muted";
    string_predicates.bindings.push_back({"text", "{{ contains(question.prompt, \"Korea\") }} / {{ starts_with(selected_deck.source_uri, \"fixture://\") }} / {{ ends_with(selected_deck.source_uri, \".quizdeck\") }}"});
    script.nodes.push_back(std::move(string_predicates));

    presentation::app_scene_script_node prompt_length;
    prompt_length.id = "question_prompt_length";
    prompt_length.parent_id = "script_root";
    prompt_length.kind = scene::scene_node_kind::text;
    prompt_length.debug_name = "question prompt length";
    prompt_length.style.token = "muted";
    prompt_length.bindings.push_back({"text", "{{ length(question.prompt) }}"});
    script.nodes.push_back(std::move(prompt_length));

    presentation::app_scene_script_node choice_label;
    choice_label.id = "choice_error_label";
    choice_label.parent_id = "script_root";
    choice_label.kind = scene::scene_node_kind::text;
    choice_label.debug_name = "choice error label";
    choice_label.style.token = "muted";
    choice_label.bindings.push_back({"text", "{{ choose(error.exists, error.message, \"No error\") }}"});
    script.nodes.push_back(std::move(choice_label));

    presentation::app_scene_script_node lazy_choice;
    lazy_choice.id = "lazy_choice_missing_long_text";
    lazy_choice.parent_id = "script_root";
    lazy_choice.kind = scene::scene_node_kind::text;
    lazy_choice.debug_name = "lazy choice missing long text";
    lazy_choice.style.token = "muted";
    lazy_choice.bindings.push_back({"text", "{{ choose(question.has_long_text, question.long_text, \"No long text\") }}"});
    script.nodes.push_back(std::move(lazy_choice));

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

    presentation::app_scene_script_node contains_condition;
    contains_condition.id = "function_condition_contains_korea";
    contains_condition.parent_id = "script_root";
    contains_condition.kind = scene::scene_node_kind::text;
    contains_condition.debug_name = "function condition contains Korea";
    contains_condition.condition = "contains(question.prompt, \"Korea\")";
    contains_condition.style.token = "muted";
    contains_condition.text_runs.push_back({"Function condition contains Korea", "muted"});
    script.nodes.push_back(std::move(contains_condition));

    presentation::app_scene_script_node hidden_contains_condition;
    hidden_contains_condition.id = "function_condition_contains_busan";
    hidden_contains_condition.parent_id = "script_root";
    hidden_contains_condition.kind = scene::scene_node_kind::text;
    hidden_contains_condition.debug_name = "function condition contains Busan";
    hidden_contains_condition.condition = "contains(question.prompt, \"Busan\")";
    hidden_contains_condition.style.token = "muted";
    hidden_contains_condition.text_runs.push_back({"Function condition contains Busan", "muted"});
    script.nodes.push_back(std::move(hidden_contains_condition));

    presentation::app_scene_script_node seeded_start;
    seeded_start.id = "seeded_start_button";
    seeded_start.parent_id = "script_root";
    seeded_start.kind = scene::scene_node_kind::input;
    seeded_start.debug_name = "seeded start";
    seeded_start.style.token = "button";
    seeded_start.text_runs.push_back({"Seeded random start", "button"});
    presentation::app_scene_script_event_handler_template seeded_start_press;
    seeded_start_press.trigger = scene::scene_action_trigger::press;
    seeded_start_press.commands.push_back({"start_quiz", {
        {"mode", "random"},
        {"random_seed", "{{ 123 }}"},
        {"shuffle", "{{ true }}"},
    }});
    seeded_start.events.push_back(std::move(seeded_start_press));
    script.nodes.push_back(std::move(seeded_start));

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
    options.bindings.push_back({"layout.gap", "6"});
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
    option.bindings.push_back({"layout.height", "48"});
    option.bindings.push_back({"text", "{{ option.text }}"});
    presentation::app_scene_script_event_handler_template press;
    press.trigger = scene::scene_action_trigger::press;
    press.commands.push_back({"submit_option", {{"option_index", "{{ option.index }}"}}});
    option.events.push_back(std::move(press));
    script.nodes.push_back(std::move(option));

    presentation::app_scene_script_node stable_option_id;
    stable_option_id.id = "option_safe_{{ safe_id(option.text, option.index) }}";
    stable_option_id.parent_id = "question_options";
    stable_option_id.kind = scene::scene_node_kind::text;
    stable_option_id.debug_name = "stable option id";
    stable_option_id.style.token = "muted";
    stable_option_id.repeater = presentation::app_scene_script_repeater{"option", "question.options"};
    stable_option_id.bindings.push_back({"text", "{{ option.text }}"});
    script.nodes.push_back(std::move(stable_option_id));

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
    const scene::scene_node_data* question_learning = data.find_node("question_learning");
    const scene::scene_node_data* question_learning_flags = data.find_node("question_learning_flags");
    require(question_learning != nullptr, "question learning node exists");
    require(question_learning_flags != nullptr, "question learning flags node exists");
    require(question_learning->text_runs.front().text == "learning", "question learning binding renders");
    require(question_learning_flags->text_runs.front().text == "true / false / false / false", "question learning flags render");

    domain::app_snapshot known_snapshot = snapshot;
    require(
        known_snapshot.active_session.has_value() && known_snapshot.active_session->current_question.has_value(),
        "known snapshot fixture has current question");
    known_snapshot.active_session->current_question->learning = domain::learning_state::known;
    const presentation::app_scene_script_compile_result known_compiled =
        presentation::compile_app_scene_script(script, known_snapshot);
    require(known_compiled.ok(), "script compiles with known question learning state");
    scene::scene_layout_data known_data("script_known_question_learning_test");
    apply_patch_to_scene(*known_compiled.patch, known_data);
    require(known_data.find_node("question_learning")->text_runs.front().text == "known", "question learning renders known");
    require(
        known_data.find_node("question_learning_flags")->text_runs.front().text == "false / true / false / false",
        "question learning flags render known state");
    const scene::scene_node_data* question_has_image = data.find_node("question_has_image");
    const scene::scene_node_data* question_image_uri = data.find_node("question_image_uri");
    const scene::scene_node_data* question_image = data.find_node("question_image");
    require(question_has_image != nullptr, "question has image node exists");
    require(question_image_uri != nullptr, "question image uri node exists");
    require(question_image != nullptr, "question image node exists");
    require(question_has_image->text_runs.front().text == "true", "question has image binding renders");
    require(question_image_uri->text_runs.front().text == "asset://korea-map.png", "question image uri binding renders");
    require(question_image->has_image, "question image binding enables image");
    require(question_image->image.uri == "asset://korea-map.png", "question image binding sets image uri");
    require(question_image->image.alt_text == "Question image for q1", "question image binding sets alt text");
    require(question_image->image.aspect_ratio == 1.6f, "question image binding sets aspect ratio");
    require(question_image->style.opacity == 0.75f, "style opacity binding renders");
    require(question_image->style.border_radius == 8.0f, "style border radius binding renders");
    const scene::scene_node_data* progress = data.find_node("session_progress");
    const scene::scene_node_data* session_mode = data.find_node("session_mode_phase");
    const scene::scene_node_data* session_count = data.find_node("session_question_count");
    const scene::scene_node_data* feedback_exists = data.find_node("feedback_exists");
    const scene::scene_node_data* feedback_label = data.find_node("feedback_label");
    const scene::scene_node_data* feedback_selected_count = data.find_node("feedback_selected_option_count");
    const scene::scene_node_data* feedback_answered_at = data.find_node("feedback_answered_at");
    require(progress != nullptr, "session progress node exists");
    require(session_mode != nullptr, "session mode node exists");
    require(session_count != nullptr, "session count node exists");
    require(feedback_exists != nullptr, "feedback exists node exists");
    require(feedback_label != nullptr, "feedback label node exists");
    require(feedback_selected_count != nullptr, "feedback selected count node exists");
    require(feedback_answered_at != nullptr, "feedback answered at node exists");
    require(progress->text_runs.front().text == "Question 1 of 1", "session progress binding renders");
    require(session_mode->text_runs.front().text == "normal / active", "session mode and phase render");
    require(session_count->text_runs.front().text == "1", "session question count renders");
    require(feedback_exists->text_runs.front().text == "false", "feedback exists binding renders false");
    require(feedback_label->text_runs.front().text == "none", "feedback fallback renders without pending feedback");
    require(feedback_selected_count->text_runs.front().text == "0", "feedback selected count fallback renders");
    require(feedback_answered_at->text_runs.front().text == "0", "feedback answered-at fallback renders");
    const scene::scene_node_data* settings_count = data.find_node("settings_count");
    const scene::scene_node_data* settings_count_label = data.find_node("settings_count_label");
    const scene::scene_node_data* setting_route = data.find_node("setting_route");
    const scene::scene_node_data* error_exists = data.find_node("error_exists");
    const scene::scene_node_data* error_message = data.find_node("error_message");
    require(settings_count != nullptr, "settings count node exists");
    require(settings_count_label != nullptr, "settings count label node exists");
    require(setting_route != nullptr, "setting route node exists");
    require(error_exists != nullptr, "error exists node exists");
    require(error_message != nullptr, "error message node exists");
    require(settings_count->text_runs.front().text == "0", "settings count binding renders");
    require(settings_count_label->text_runs.front().text == "0 settings", "format_count renders plural fallback");
    require(setting_route->text_runs.front().text == "unset", "setting function renders fallback");
    require(error_exists->text_runs.front().text == "false", "error exists binding renders");
    require(error_message->text_runs.front().text.empty(), "empty error message binding renders");

    domain::app_snapshot status_snapshot = snapshot;
    status_snapshot.settings["ui_screen"] = "settings";
    status_snapshot.error_message = "Load failed";
    const presentation::app_scene_script_compile_result status_compiled =
        presentation::compile_app_scene_script(script, status_snapshot);
    require(status_compiled.ok(), "script compiles with app status");
    scene::scene_layout_data status_data("script_status_test");
    apply_patch_to_scene(*status_compiled.patch, status_data);
    require(status_data.find_node("settings_count")->text_runs.front().text == "1", "settings count renders entries");
    require(status_data.find_node("settings_count_label")->text_runs.front().text == "1 setting", "format_count renders singular");
    require(status_data.find_node("setting_route")->text_runs.front().text == "settings", "setting function renders configured value");
    require(status_data.find_node("error_exists")->text_runs.front().text == "true", "error exists binding renders present error");
    require(status_data.find_node("error_message")->text_runs.front().text == "Load failed", "error message binding renders present error");
    require(status_data.find_node("empty_error_flag")->text_runs.front().text == "false", "empty function renders present error");
    require(status_data.find_node("choice_error_label")->text_runs.front().text == "Load failed", "choose function renders selected branch");

    const domain::app_snapshot feedback_snapshot = make_feedback_snapshot();
    const presentation::app_scene_script_compile_result feedback_compiled =
        presentation::compile_app_scene_script(script, feedback_snapshot);
    require(feedback_compiled.ok(), "script compiles with pending feedback");
    scene::scene_layout_data feedback_data("script_feedback_test");
    apply_patch_to_scene(*feedback_compiled.patch, feedback_data);
    require(feedback_data.find_node("feedback_exists")->text_runs.front().text == "true", "feedback exists binding renders true");
    require(feedback_data.find_node("feedback_label")->text_runs.front().text == "correct", "feedback outcome binding renders");
    require(feedback_data.find_node("feedback_selected_option_count")->text_runs.front().text == "1", "feedback selected count renders");
    require(feedback_data.find_node("feedback_answered_at")->text_runs.front().text == "100", "feedback answered-at renders");

    const scene::scene_node_data* learning_summary = data.find_node("learning_summary");
    const scene::scene_node_data* known_count = data.find_node("learning_known_count");
    require(learning_summary != nullptr, "learning summary node exists");
    require(known_count != nullptr, "known count node exists");
    require(learning_summary->text_runs.front().text == "Learning 1 / Known 0 / Unknown 0 / Wrong note 0", "learning summary binding renders");
    require(known_count->text_runs.front().text == "0", "known count binding renders");
    const scene::scene_node_data* deck_title = data.find_node("selected_deck_title");
    const scene::scene_node_data* deck_source = data.find_node("selected_deck_source");
    const scene::scene_node_data* day_summary = data.find_node("selected_day_summary");
    const scene::scene_node_data* day_question_count_label = data.find_node("selected_day_question_count_label");
    const scene::scene_node_data* deck_count = data.find_node("deck_count");
    require(deck_title != nullptr, "selected deck title node exists");
    require(deck_source != nullptr, "selected deck source node exists");
    require(day_summary != nullptr, "selected day summary node exists");
    require(day_question_count_label != nullptr, "selected day question count label node exists");
    require(deck_count != nullptr, "deck count node exists");
    require(deck_title->text_runs.front().text == "Geography", "selected deck title binding renders");
    require(deck_source->text_runs.front().text == "fixture://geography.quizdeck", "selected deck source binding renders");
    require(day_summary->text_runs.front().text == "Day 1 / 1 questions", "selected day summary binding renders");
    require(day_question_count_label->text_runs.front().text == "1 question", "format_count renders selected day count");
    require(deck_count->text_runs.front().text == "1", "deck count binding renders");
    const scene::scene_node_data* formatted_prompt = data.find_node("question_prompt_upper");
    const scene::scene_node_data* formatted_type = data.find_node("question_type_lower");
    require(formatted_prompt != nullptr, "formatted prompt node exists");
    require(formatted_type != nullptr, "formatted type node exists");
    require(formatted_prompt->text_runs.front().text == "CAPITAL OF KOREA?", "upper formatter renders prompt");
    require(formatted_type->text_runs.front().text == "answer", "formatter chain renders question type");
    const scene::scene_node_data* function_title = data.find_node("deck_day_function_title");
    const scene::scene_node_data* function_title_upper = data.find_node("deck_day_function_title_upper");
    const scene::scene_node_data* function_title_pipe_literal = data.find_node("deck_day_function_pipe_literal");
    const scene::scene_node_data* safe_deck_id = data.find_node("safe_deck_id");
    const scene::scene_node_data* session_active_flag = data.find_node("session_active_flag");
    const scene::scene_node_data* empty_error_flag = data.find_node("empty_error_flag");
    const scene::scene_node_data* string_predicates = data.find_node("string_predicate_flags");
    const scene::scene_node_data* prompt_length = data.find_node("question_prompt_length");
    const scene::scene_node_data* choice_label = data.find_node("choice_error_label");
    const scene::scene_node_data* lazy_choice = data.find_node("lazy_choice_missing_long_text");
    require(function_title != nullptr, "function title node exists");
    require(function_title_upper != nullptr, "function title upper node exists");
    require(function_title_pipe_literal != nullptr, "function title pipe literal node exists");
    require(safe_deck_id != nullptr, "safe id function node exists");
    require(session_active_flag != nullptr, "function active flag node exists");
    require(empty_error_flag != nullptr, "empty function node exists");
    require(string_predicates != nullptr, "string predicate function node exists");
    require(prompt_length != nullptr, "length function node exists");
    require(choice_label != nullptr, "choose function node exists");
    require(lazy_choice != nullptr, "lazy choose function node exists");
    require(function_title->text_runs.front().text == "Geography / Day 1", "concat function renders text");
    require(function_title_upper->text_runs.front().text == "GEOGRAPHY / DAY 1", "function result supports formatter chain");
    require(function_title_pipe_literal->text_runs.front().text == "Geography | Day 1", "quoted pipe literal is not treated as formatter");
    require(safe_deck_id->text_runs.front().text == "geography_deck", "safe_id function renders stable id text");
    require(session_active_flag->text_runs.front().text == "true", "equals function renders boolean");
    require(empty_error_flag->text_runs.front().text == "true", "empty function renders boolean");
    require(string_predicates->text_runs.front().text == "true / true / true", "string predicate functions render booleans");
    require(prompt_length->text_runs.front().text == "17", "length function renders string length");
    require(choice_label->text_runs.front().text == "No error", "choose function renders fallback branch");
    require(lazy_choice->text_runs.front().text == "No long text", "choose function only evaluates selected branch");
    require(data.contains_node("function_condition_active"), "function expression can drive conditions");
    require(data.contains_node("function_condition_not_completed"), "not function can drive conditions");
    require(data.contains_node("function_condition_contains_korea"), "contains function can drive true conditions");
    require(!data.contains_node("function_condition_contains_busan"), "contains function can drive false conditions");
    require(!data.contains_node("question_long_text"), "false condition suppresses long text node");

    const scene::scene_node_data* seeded_start = data.find_node("seeded_start_button");
    require(seeded_start != nullptr, "seeded start node exists");
    require(seeded_start->has_event_handlers, "seeded start emits event handler");
    require(seeded_start->event_handlers.size() == 1, "seeded start emits one event handler");
    require(seeded_start->event_handlers.front().commands.size() == 1, "seeded start emits one command");
    const scene::scene_command& seeded_start_command = seeded_start->event_handlers.front().commands.front();
    const app_command_route_result seeded_start_routed = route_scene_command(seeded_start_command);
    require(seeded_start_routed.ok(), "seeded start command routes through registry");
    const auto* seeded_start_action = std::get_if<domain::start_quiz_action>(&seeded_start_routed.action->payload);
    require(seeded_start_action != nullptr, "seeded start command routes to start_quiz action");
    require(seeded_start_action->mode == domain::quiz_mode::random, "seeded start preserves mode");
    require(seeded_start_action->random_seed.has_value(), "seeded start preserves random seed");
    require(*seeded_start_action->random_seed == 123U, "seeded start random seed is deterministic");
    require(seeded_start_action->shuffle, "seeded start preserves shuffle");

    const scene::scene_node_data* option_0 = data.find_node("option_0");
    const scene::scene_node_data* option_1 = data.find_node("option_1");
    const scene::scene_node_data* options = data.find_node("question_options");
    require(options != nullptr, "options container exists");
    require(option_0 != nullptr, "first repeated option exists");
    require(option_1 != nullptr, "second repeated option exists");
    require(options->layout_rule.gap == 6.0f, "layout gap binding renders");
    require(option_0->layout_rule.has_height, "layout height binding marks height explicit");
    require(option_0->layout_rule.height == 48.0f, "layout height binding renders");
    require(option_0->text_runs.front().text == "Seoul", "first option text binding renders");
    require(option_1->text_runs.front().text == "Busan", "second option text binding renders");
    require(data.contains_node("option_safe_seoul"), "safe_id can render stable repeated option id");
    require(data.contains_node("option_safe_busan"), "safe_id renders each repeated option id");
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
    const auto command_node = std::find_if(
        bad_command.nodes.begin(),
        bad_command.nodes.end(),
        [](const presentation::app_scene_script_node& node) {
            return node.id == "option_{{ option.index }}";
        });
    require(command_node != bad_command.nodes.end(), "bad command fixture finds option command node");
    command_node->events.front().commands.front().name = "delete_everything";
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

    presentation::app_scene_script_document empty_extra_arg = make_active_question_script();
    append_invalid_function_node(empty_extra_arg, "{{ empty(error.message, \"fallback\") }}");
    require_compile_error_contains(
        empty_extra_arg,
        snapshot,
        "empty expects 1 argument",
        "empty function arg count errors are reported");

    presentation::app_scene_script_document choose_missing_arg = make_active_question_script();
    append_invalid_function_node(choose_missing_arg, "{{ choose(error.exists, error.message) }}");
    require_compile_error_contains(
        choose_missing_arg,
        snapshot,
        "choose expects 3 argument",
        "choose function arg count errors are reported");

    presentation::app_scene_script_document contains_missing_arg = make_active_question_script();
    append_invalid_function_node(contains_missing_arg, "{{ contains(question.prompt) }}");
    require_compile_error_contains(
        contains_missing_arg,
        snapshot,
        "contains expects 2 argument",
        "contains function arg count errors are reported");

    presentation::app_scene_script_document length_extra_arg = make_active_question_script();
    append_invalid_function_node(length_extra_arg, "{{ length(question.prompt, \"extra\") }}");
    require_compile_error_contains(
        length_extra_arg,
        snapshot,
        "length expects 1 argument",
        "length function arg count errors are reported");

    presentation::app_scene_script_document format_count_missing_arg = make_active_question_script();
    append_invalid_function_node(format_count_missing_arg, "{{ format_count(settings.count) }}");
    require_compile_error_contains(
        format_count_missing_arg,
        snapshot,
        "format_count expects 2 or 3 argument",
        "format_count arg count errors are reported");

    presentation::app_scene_script_document format_count_bad_count = make_active_question_script();
    append_invalid_function_node(format_count_bad_count, "{{ format_count(\"many\", \"question\") }}");
    require_compile_error_contains(
        format_count_bad_count,
        snapshot,
        "format_count argument 1 must be an integer",
        "format_count integer errors are reported");

    presentation::app_scene_script_document invalid_numeric_binding = make_active_question_script();
    const auto image_node = std::find_if(
        invalid_numeric_binding.nodes.begin(),
        invalid_numeric_binding.nodes.end(),
        [](const presentation::app_scene_script_node& node) {
            return node.id == "question_image";
        });
    require(image_node != invalid_numeric_binding.nodes.end(), "invalid numeric binding fixture finds image node");
    image_node->bindings.push_back({"image.aspect_ratio", "wide"});
    require_compile_error_contains(
        invalid_numeric_binding,
        snapshot,
        "image.aspect_ratio requires a numeric expression",
        "numeric binding errors are reported");

    presentation::app_scene_script_document safe_id_missing_arg = make_active_question_script();
    append_invalid_function_node(safe_id_missing_arg, "{{ safe_id() }}");
    require_compile_error_contains(
        safe_id_missing_arg,
        snapshot,
        "safe_id expects 1 or 2 argument",
        "safe_id function arg count errors are reported");

    presentation::app_scene_script_document setting_missing_arg = make_active_question_script();
    append_invalid_function_node(setting_missing_arg, "{{ setting() }}");
    require_compile_error_contains(
        setting_missing_arg,
        snapshot,
        "setting expects 1 or 2 argument",
        "setting function arg count errors are reported");
}

void test_dynamic_node_id_collisions_are_reported()
{
    using namespace quiz_vulkan;

    const domain::app_snapshot snapshot = make_active_snapshot();
    presentation::app_scene_script_document duplicate_dynamic_id = make_active_question_script();

    presentation::app_scene_script_node duplicate;
    duplicate.id = "duplicate_{{ true }}";
    duplicate.parent_id = "script_root";
    duplicate.kind = scene::scene_node_kind::text;
    duplicate.debug_name = "duplicate dynamic id";
    duplicate.style.token = "muted";
    duplicate.repeater = presentation::app_scene_script_repeater{"option", "question.options"};
    duplicate.bindings.push_back({"text", "{{ option.text }}"});
    duplicate_dynamic_id.nodes.push_back(std::move(duplicate));

    const presentation::app_scene_script_compile_result compiled =
        presentation::compile_app_scene_script(duplicate_dynamic_id, snapshot);
    require(!compiled.ok(), "duplicate dynamic node ids fail compile");
    require(contains(compiled.error, "duplicate id"), "duplicate dynamic node id error is reported");
    require(contains(compiled.error, "duplicate_true"), "duplicate dynamic node id value is reported");
}

}  // namespace

int main()
{
    test_phase3_dsl_compiles_bindings_repeaters_conditions_events();
    test_phase3_dsl_validation_and_determinism();
    test_compiled_patch_unwrap_reports_failure_context();
    test_expression_function_errors_are_reported();
    test_dynamic_node_id_collisions_are_reported();
    return 0;
}
