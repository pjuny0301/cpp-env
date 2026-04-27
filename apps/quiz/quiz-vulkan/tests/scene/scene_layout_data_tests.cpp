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

    quiz_vulkan::scene::scene_layout_edit_data edit("test");
    edit.append_node("", panel)
        .append_node("panel", choice)
        .set_text("choice_a", {{"Choice A", "body"}})
        .bind_action("choice_a", {quiz_vulkan::scene::scene_action_trigger::press, "answer_selected", "A"})
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
    assert(scene.style_tokens().at("choice").background_color == "#223344");
    assert(scene.route_state().route_id == "quiz/question");
    assert(scene.animation_state().active);

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
    quiz_vulkan::scene::scene_modifier_context context;
    context.viewport = {0.0f, 0.0f, 800.0f, 600.0f};
    assert(modifier.apply(scene, context).applied());
    assert(scene.contains_node("title"));
    assert(scene.has_focus());
    assert(scene.focus_id() == "title");

    return 0;
}
