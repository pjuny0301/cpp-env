#include "app/app_command_registry.h"
#include "app/app_scene_script.h"
#include "core/domain/app_snapshot.hpp"
#include "core/layout/layout_placer.h"

#include <cassert>
#include <algorithm>
#include <optional>
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

void test_scene_script_package_manifest()
{
    using namespace quiz_vulkan;

    const presentation::app_scene_script_package_manifest manifest =
        presentation::app_scene_script_package_manifest_for_runtime();
    require(manifest.package_id == "quiz.scene-script", "scene script package id is stable");
    require(manifest.package_version == "0.1.0", "scene script package version is stable");
    require(manifest.min_supported_schema_version == presentation::app_scene_script_template_schema_version,
        "scene script min schema version is template selector");
    require(manifest.max_supported_schema_version == presentation::app_scene_script_node_dsl_schema_version,
        "scene script max schema version is node DSL");
    require(presentation::app_scene_script_schema_version_supported(manifest.template_schema_version),
        "template schema version is supported");
    require(presentation::app_scene_script_schema_version_supported(manifest.node_dsl_schema_version),
        "node DSL schema version is supported");
    require(!presentation::app_scene_script_schema_version_supported(0), "schema version zero is rejected");
    require(!presentation::app_scene_script_schema_version_supported(manifest.max_supported_schema_version + 1),
        "future schema version is rejected until runtime support lands");

    const auto has_artifact = [&](std::string_view artifact_id) {
        return std::find(manifest.compatible_artifact_ids.begin(), manifest.compatible_artifact_ids.end(), artifact_id)
            != manifest.compatible_artifact_ids.end();
    };
    require(has_artifact("quiz-vulkan"), "native artifact id is listed");
    require(has_artifact("android-quiz-app"), "android artifact id is listed");
    require(has_artifact("quiz-editor"), "editor artifact id is listed");
}

}  // namespace

int main()
{
    test_phase3_dsl_compiles_bindings_repeaters_conditions_events();
    test_phase3_dsl_validation_and_determinism();
    test_scene_script_package_manifest();
    return 0;
}
