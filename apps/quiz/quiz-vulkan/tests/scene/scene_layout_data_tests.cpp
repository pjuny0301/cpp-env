#include "core/scene/modifier_interface.h"

#include <cassert>
#include <memory>
#include <string>

namespace {

class append_title_modifier final : public quiz_vulkan::scene::scene_modifier {
public:
    void modify(const quiz_vulkan::scene::scene_modifier_context&, quiz_vulkan::scene::scene_layout_edit_data& edit_data) override
    {
        quiz_vulkan::scene::scene_node_data title;
        title.id = "title";
        title.kind = quiz_vulkan::scene::scene_node_kind::text;
        title.text_runs.push_back({"Question 1", "heading"});
        edit_data.append_node("", title);
        edit_data.set_focus("title");
    }
};

class rewrite_title_modifier final : public quiz_vulkan::scene::scene_modifier {
public:
    void modify(const quiz_vulkan::scene::scene_modifier_context& context, quiz_vulkan::scene::scene_layout_edit_data& edit_data) override
    {
        assert(context.current_scene != nullptr);
        assert(context.current_scene->contains_node("title"));
        edit_data.set_text("title", {{"Question 1 updated", "heading"}});
    }
};

} // namespace

int main()
{
    quiz_vulkan::scene::scene_layout_data scene("quiz");

    quiz_vulkan::scene::scene_node_data panel;
    panel.id = "panel";
    panel.kind = quiz_vulkan::scene::scene_node_kind::container;

    quiz_vulkan::scene::scene_node_data choice;
    choice.id = "choice_a";
    choice.kind = quiz_vulkan::scene::scene_node_kind::input;

    quiz_vulkan::scene::scene_node_semantics choice_semantics;
    choice_semantics.role = "choice";
    choice_semantics.label = "Choice A";
    choice_semantics
        .set_property("flow.stage", quiz_vulkan::scene::scene_value("question"))
        .set_property("choice.state", quiz_vulkan::scene::scene_value("selected"))
        .set_property("choice.index", quiz_vulkan::scene::scene_value(0));

    quiz_vulkan::scene::scene_layout_edit_data edit("test");
    edit.append_node("", panel)
        .append_node("panel", choice)
        .set_text("choice_a", {{"Choice A", "body"}})
        .bind_action("choice_a", {quiz_vulkan::scene::scene_action_trigger::press, "answer_selected", "A"})
        .set_semantics("choice_a", choice_semantics)
        .set_style("choice_a", quiz_vulkan::scene::scene_style{"choice", "#223344", "#ffffff", 1.0f, 4.0f})
        .set_route({"quiz/question", "quiz", {{"question_id", "1"}}})
        .start_transition({false, "fade_in", 0.0f, 0.2f});

    quiz_vulkan::scene::scene_layout_patch patch = edit.finish_patch();
    const quiz_vulkan::scene::scene_layout_apply_result apply_result = patch.apply_to(scene);
    assert(apply_result.applied());
    assert(scene.contains_node("panel"));
    assert(scene.contains_node("choice_a"));
    assert(scene.find_node("choice_a")->parent_id == "panel");
    assert(scene.find_node("choice_a")->text_runs.front().text == "Choice A");
    assert(scene.find_node("choice_a")->has_action_binding);
    assert(scene.find_node("choice_a")->action_binding.action_type == "answer_selected");
    assert(scene.find_node("choice_a")->has_event_handlers);
    assert(scene.find_node("choice_a")->event_handlers.size() == 1);
    assert(scene.find_node("choice_a")->event_handlers.front().commands.size() == 1);
    assert(scene.find_node("choice_a")->event_handlers.front().commands.front().name == "answer_selected");
    assert(scene.find_node("choice_a")->event_handlers.front().commands.front().find_arg("payload")->string_if() != nullptr);
    assert(*scene.find_node("choice_a")->event_handlers.front().commands.front().find_arg("payload")->string_if() == "A");
    assert(std::string(quiz_vulkan::scene::to_string(scene.find_node("choice_a")->event_handlers.front().trigger)) == "press");
    assert(scene.find_node("choice_a")->semantics.role == "choice");
    assert(*scene.find_node("choice_a")->semantics.string_property("choice.state") == "selected");
    assert(*scene.find_node("choice_a")->semantics.int_property("choice.index") == 0);
    assert(scene.style_tokens().at("choice").background_color == "#223344");
    assert(scene.route_state().route_id == "quiz/question");
    assert(scene.animation_state().active);
    assert(*scene.find_node("choice_a")->semantics.string_property("flow.stage") == "question");
    assert(scene.find_node("choice_a")->semantics.find_property("missing") == nullptr);

    quiz_vulkan::scene::scene_layout_patch bad_patch;
    bad_patch.set_focus("missing");
    const quiz_vulkan::scene::scene_layout_apply_result bad_result = bad_patch.apply_to(scene);
    assert(!bad_result.applied());
    assert(!bad_result.errors.empty());

    quiz_vulkan::scene::scene_layout_patch remove_patch;
    remove_patch.remove_node("panel");
    assert(remove_patch.apply_to(scene).applied());
    assert(!scene.contains_node("panel"));
    assert(!scene.contains_node("choice_a"));

    quiz_vulkan::scene::scene_layout_data_modifier modifier;
    modifier.add_modifier(std::make_shared<append_title_modifier>());
    modifier.add_modifier(std::make_shared<rewrite_title_modifier>());
    quiz_vulkan::scene::scene_modifier_context context;
    context.viewport = {0.0f, 0.0f, 800.0f, 600.0f};
    assert(modifier.apply(scene, context).applied());
    assert(scene.contains_node("title"));
    assert(scene.has_focus());
    assert(scene.focus_id() == "title");
    assert(scene.find_node("title")->text_runs.front().text == "Question 1 updated");

    quiz_vulkan::scene::scene_layout_edit_data command_edit("typed command");
    quiz_vulkan::scene::scene_node_data typed_button;
    typed_button.id = "typed_button";
    typed_button.kind = quiz_vulkan::scene::scene_node_kind::input;
    command_edit
        .append_node("", typed_button)
        .bind_event_handler(
            "typed_button",
            quiz_vulkan::scene::make_scene_event_handler(
                quiz_vulkan::scene::scene_action_trigger::press,
                {quiz_vulkan::scene::make_scene_command(
                    "submit_option",
                    {{"option_index", quiz_vulkan::scene::scene_value(2)}})},
                "question.active"));
    assert(command_edit.finish_patch().apply_to(scene).applied());
    const quiz_vulkan::scene::scene_node_data* typed_node = scene.find_node("typed_button");
    assert(typed_node != nullptr);
    assert(!typed_node->has_action_binding);
    assert(typed_node->has_event_handlers);
    assert(typed_node->event_handlers.front().commands.front().name == "submit_option");
    assert(*typed_node->event_handlers.front().commands.front().find_arg("option_index")->int_if() == 2);
    assert(typed_node->event_handlers.front().condition.has_value());
    assert(typed_node->event_handlers.front().condition.value() == "question.active");
    assert(std::string(quiz_vulkan::scene::to_string(quiz_vulkan::scene::scene_action_trigger::swipe_left)) == "swipe_left");
    assert(std::string(quiz_vulkan::scene::to_string(quiz_vulkan::scene::scene_action_trigger::long_press)) == "long_press");

    return 0;
}
