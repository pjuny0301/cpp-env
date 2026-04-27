#include "core/layout/layout_placer.h"

#include <cassert>
#include <cstddef>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace {

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

bool near(float actual, float expected)
{
    return std::fabs(actual - expected) < 0.001f;
}

std::string rect_to_string(const quiz_vulkan::scene::scene_rect& rect)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1)
           << rect.x << "," << rect.y << "," << rect.width << "," << rect.height;
    return stream.str();
}

std::string node_snapshot_line(const quiz_vulkan::scene::placed_scene_node& node)
{
    std::ostringstream stream;
    stream << node.id
           << "|rect=" << rect_to_string(node.bounds)
           << "|role=" << quiz_vulkan::scene::to_string(node.semantics.role)
           << "|stage=" << quiz_vulkan::scene::to_string(node.semantics.quiz.stage)
           << "|feedback=" << quiz_vulkan::scene::to_string(node.semantics.quiz.feedback)
           << "|option=" << quiz_vulkan::scene::to_string(node.semantics.quiz.option_state)
           << "|input=" << (node.input_enabled ? "enabled" : "disabled");
    return stream.str();
}

std::string selected_layout_snapshot(
    const quiz_vulkan::scene::placed_scene& placed,
    const std::vector<std::string>& node_ids)
{
    std::ostringstream stream;
    stream << "usable=" << rect_to_string(placed.usable_bounds) << "\n";
    for (const std::string& node_id : node_ids) {
        const quiz_vulkan::scene::placed_scene_node* node = placed.find_node(node_id);
        assert(node != nullptr);
        stream << node_snapshot_line(*node) << "\n";
    }
    return stream.str();
}

} // namespace

int main()
{
    quiz_vulkan::scene::scene_layout_data scene("quiz");

    quiz_vulkan::scene::scene_layout_rule root_rule;
    root_rule.mode = quiz_vulkan::scene::scene_layout_mode::vertical;
    root_rule.padding = {10.0f, 12.0f, 10.0f, 12.0f};
    root_rule.gap = 4.0f;
    assert(scene.set_bounds_rule(scene.root_node_id(), root_rule));

    quiz_vulkan::scene::scene_node_data title;
    title.id = "title";
    title.kind = quiz_vulkan::scene::scene_node_kind::text;
    title.layout_rule.horizontal_alignment = quiz_vulkan::scene::scene_alignment::start;
    title.text_runs.push_back({"Hello", "heading"});

    quiz_vulkan::scene::scene_node_data button;
    button.id = "button";
    button.kind = quiz_vulkan::scene::scene_node_kind::input;
    button.layout_rule.has_height = true;
    button.layout_rule.height = 30.0f;
    button.action_binding = quiz_vulkan::scene::scene_action_binding{quiz_vulkan::scene::scene_action_trigger::press, "tap", "button"};
    button.has_action_binding = true;

    assert(scene.append_node("", title));
    assert(scene.append_node("", button));

    fixed_text_metrics metrics;
    const quiz_vulkan::scene::placed_scene placed = quiz_vulkan::scene::layout_placer().place(scene, {0.0f, 0.0f, 120.0f, 100.0f}, metrics);

    const quiz_vulkan::scene::placed_scene_node* root = placed.find_node("root");
    const quiz_vulkan::scene::placed_scene_node* placed_title = placed.find_node("title");
    const quiz_vulkan::scene::placed_scene_node* placed_button = placed.find_node("button");

    assert(root != nullptr);
    assert(placed_title != nullptr);
    assert(placed_button != nullptr);

    assert(near(root->bounds.width, 120.0f));
    assert(near(root->content_bounds.x, 10.0f));
    assert(near(root->content_bounds.y, 12.0f));

    assert(near(placed_title->bounds.x, 10.0f));
    assert(near(placed_title->bounds.y, 12.0f));
    assert(near(placed_title->bounds.width, 40.0f));
    assert(near(placed_title->bounds.height, 18.0f));

    assert(near(placed_button->bounds.x, 10.0f));
    assert(near(placed_button->bounds.y, 34.0f));
    assert(near(placed_button->bounds.width, 100.0f));
    assert(near(placed_button->bounds.height, 30.0f));
    assert(placed.input_regions.size() == 1);
    assert(placed.input_regions.front().node_id == "button");
    assert(near(placed.input_regions.front().bounds.height, 30.0f));

    quiz_vulkan::scene::scene_node_data badge;
    badge.id = "badge";
    badge.kind = quiz_vulkan::scene::scene_node_kind::container;
    badge.layout_rule.has_x = true;
    badge.layout_rule.x = 90.0f;
    badge.layout_rule.has_y = true;
    badge.layout_rule.y = 6.0f;
    badge.layout_rule.has_width = true;
    badge.layout_rule.width = 20.0f;
    badge.layout_rule.has_height = true;
    badge.layout_rule.height = 20.0f;
    assert(scene.set_bounds_rule(scene.root_node_id(), quiz_vulkan::scene::scene_layout_rule{}));
    assert(scene.append_node("", badge));

    const quiz_vulkan::scene::placed_scene overlay = quiz_vulkan::scene::layout_placer().place(scene, {0.0f, 0.0f, 120.0f, 100.0f}, metrics);
    const quiz_vulkan::scene::placed_scene_node* placed_badge = overlay.find_node("badge");
    assert(placed_badge != nullptr);
    assert(near(placed_badge->bounds.x, 90.0f));
    assert(near(placed_badge->bounds.y, 6.0f));
    assert(near(placed_badge->bounds.width, 20.0f));
    assert(near(placed_badge->bounds.height, 20.0f));

    quiz_vulkan::scene::scene_layout_data quiz_scene("quiz");
    quiz_vulkan::scene::scene_layout_rule quiz_root_rule;
    quiz_root_rule.mode = quiz_vulkan::scene::scene_layout_mode::vertical;
    quiz_root_rule.padding = {16.0f, 16.0f, 16.0f, 16.0f};
    quiz_root_rule.gap = 8.0f;
    quiz_root_rule.respect_safe_area = true;
    quiz_root_rule.avoid_keyboard = true;
    quiz_root_rule.clip_children = true;
    assert(quiz_scene.set_bounds_rule(quiz_scene.root_node_id(), quiz_root_rule));

    quiz_vulkan::scene::scene_node_semantics root_semantics;
    root_semantics.role = quiz_vulkan::scene::scene_node_role::app_shell;
    assert(quiz_scene.set_semantics(quiz_scene.root_node_id(), root_semantics));

    quiz_vulkan::scene::scene_node_data stage;
    stage.id = "question_stage";
    stage.kind = quiz_vulkan::scene::scene_node_kind::container;
    stage.layout_rule.mode = quiz_vulkan::scene::scene_layout_mode::vertical;
    stage.layout_rule.has_height = true;
    stage.layout_rule.height = 156.0f;
    stage.layout_rule.gap = 8.0f;
    stage.semantics.role = quiz_vulkan::scene::scene_node_role::quiz_question_stage;
    stage.semantics.quiz.stage = quiz_vulkan::scene::scene_quiz_stage::feedback;
    stage.semantics.quiz.question_length = quiz_vulkan::scene::scene_question_length_class::long_question;
    assert(quiz_scene.append_node("", stage));

    quiz_vulkan::scene::scene_node_data prompt;
    prompt.id = "question_prompt";
    prompt.kind = quiz_vulkan::scene::scene_node_kind::text;
    prompt.layout_rule.horizontal_alignment = quiz_vulkan::scene::scene_alignment::start;
    prompt.text_runs.push_back({"A retained prompt", "prompt"});
    prompt.semantics.role = quiz_vulkan::scene::scene_node_role::quiz_question_prompt;
    prompt.semantics.quiz.stage = quiz_vulkan::scene::scene_quiz_stage::feedback;
    prompt.semantics.quiz.question_length = quiz_vulkan::scene::scene_question_length_class::long_question;
    assert(quiz_scene.append_node("question_stage", prompt));

    quiz_vulkan::scene::scene_node_data feedback;
    feedback.id = "feedback_banner";
    feedback.kind = quiz_vulkan::scene::scene_node_kind::text;
    feedback.layout_rule.has_height = true;
    feedback.layout_rule.height = 40.0f;
    feedback.text_runs.push_back({"Incorrect", "feedback"});
    feedback.input_enabled = false;
    feedback.semantics.role = quiz_vulkan::scene::scene_node_role::quiz_feedback;
    feedback.semantics.quiz.stage = quiz_vulkan::scene::scene_quiz_stage::feedback;
    feedback.semantics.quiz.feedback = quiz_vulkan::scene::scene_quiz_feedback_state::incorrect;
    assert(quiz_scene.append_node("", feedback));

    quiz_vulkan::scene::scene_node_data option_group;
    option_group.id = "option_group";
    option_group.kind = quiz_vulkan::scene::scene_node_kind::container;
    option_group.layout_rule.mode = quiz_vulkan::scene::scene_layout_mode::vertical;
    option_group.layout_rule.has_height = true;
    option_group.layout_rule.height = 96.0f;
    option_group.layout_rule.gap = 8.0f;
    option_group.input_enabled = false;
    option_group.semantics.role = quiz_vulkan::scene::scene_node_role::quiz_option_group;
    option_group.semantics.quiz.stage = quiz_vulkan::scene::scene_quiz_stage::feedback;
    assert(quiz_scene.append_node("", option_group));

    quiz_vulkan::scene::scene_node_data option_a;
    option_a.id = "option_a";
    option_a.kind = quiz_vulkan::scene::scene_node_kind::input;
    option_a.layout_rule.has_height = true;
    option_a.layout_rule.height = 44.0f;
    option_a.text_runs.push_back({"Seoul", "option_correct"});
    option_a.input_enabled = false;
    option_a.semantics.role = quiz_vulkan::scene::scene_node_role::quiz_option;
    option_a.semantics.quiz.stage = quiz_vulkan::scene::scene_quiz_stage::feedback;
    option_a.semantics.quiz.option_state = quiz_vulkan::scene::scene_quiz_option_state::correct;
    option_a.semantics.quiz.feedback = quiz_vulkan::scene::scene_quiz_feedback_state::incorrect;
    option_a.semantics.quiz.option_index = 0;
    option_a.semantics.quiz.reveal_correctness = true;
    assert(quiz_scene.append_node("option_group", option_a));

    quiz_vulkan::scene::scene_node_data option_b;
    option_b.id = "option_b";
    option_b.kind = quiz_vulkan::scene::scene_node_kind::input;
    option_b.layout_rule.has_height = true;
    option_b.layout_rule.height = 44.0f;
    option_b.text_runs.push_back({"Busan", "option_incorrect"});
    option_b.input_enabled = false;
    option_b.semantics.role = quiz_vulkan::scene::scene_node_role::quiz_option;
    option_b.semantics.quiz.stage = quiz_vulkan::scene::scene_quiz_stage::feedback;
    option_b.semantics.quiz.option_state = quiz_vulkan::scene::scene_quiz_option_state::incorrect;
    option_b.semantics.quiz.feedback = quiz_vulkan::scene::scene_quiz_feedback_state::incorrect;
    option_b.semantics.quiz.option_index = 1;
    option_b.semantics.quiz.reveal_correctness = true;
    assert(quiz_scene.append_node("option_group", option_b));

    quiz_vulkan::scene::scene_node_data answer_dock;
    answer_dock.id = "answer_dock";
    answer_dock.kind = quiz_vulkan::scene::scene_node_kind::input;
    answer_dock.layout_rule.has_height = true;
    answer_dock.layout_rule.height = 48.0f;
    answer_dock.layout_rule.anchor_to_keyboard = true;
    answer_dock.text_runs.push_back({"Continue", "button"});
    answer_dock.action_binding = quiz_vulkan::scene::scene_action_binding{quiz_vulkan::scene::scene_action_trigger::press, "continue_after_feedback", ""};
    answer_dock.has_action_binding = true;
    answer_dock.semantics.role = quiz_vulkan::scene::scene_node_role::quiz_answer_dock;
    answer_dock.semantics.quiz.stage = quiz_vulkan::scene::scene_quiz_stage::feedback;
    answer_dock.semantics.quiz.feedback = quiz_vulkan::scene::scene_quiz_feedback_state::incorrect;
    assert(quiz_scene.append_node("", answer_dock));

    quiz_vulkan::scene::scene_layout_environment keyboard_environment;
    keyboard_environment.viewport = {0.0f, 0.0f, 360.0f, 640.0f};
    keyboard_environment.safe_area = {0.0f, 24.0f, 0.0f, 16.0f};
    keyboard_environment.keyboard.visible = true;
    keyboard_environment.keyboard.bottom_inset = 220.0f;
    keyboard_environment.keyboard.focused_node_id = "answer_dock";

    const quiz_vulkan::scene::placed_scene quiz_placed = quiz_vulkan::scene::layout_placer().place_with_environment(
        quiz_scene,
        keyboard_environment,
        metrics);
    assert(quiz_placed.find_node("root") != nullptr);
    assert(near(quiz_placed.find_node("root")->bounds.y, 24.0f));
    assert(near(quiz_placed.find_node("root")->bounds.height, 396.0f));
    assert(quiz_placed.input_regions.size() == 1);
    assert(quiz_placed.input_regions.front().node_id == "answer_dock");
    assert(quiz_placed.input_regions.front().semantics.role == quiz_vulkan::scene::scene_node_role::quiz_answer_dock);

    const std::string snapshot = selected_layout_snapshot(
        quiz_placed,
        {"root", "question_stage", "question_prompt", "feedback_banner", "option_group", "option_a", "option_b", "answer_dock"});
    const std::string expected_snapshot =
        "usable=0.0,24.0,360.0,396.0\n"
        "root|rect=0.0,24.0,360.0,396.0|role=app_shell|stage=none|feedback=none|option=idle|input=enabled\n"
        "question_stage|rect=16.0,40.0,328.0,156.0|role=quiz_question_stage|stage=feedback|feedback=none|option=idle|input=enabled\n"
        "question_prompt|rect=16.0,40.0,136.0,18.0|role=quiz_question_prompt|stage=feedback|feedback=none|option=idle|input=enabled\n"
        "feedback_banner|rect=16.0,204.0,328.0,40.0|role=quiz_feedback|stage=feedback|feedback=incorrect|option=idle|input=disabled\n"
        "option_group|rect=16.0,252.0,328.0,96.0|role=quiz_option_group|stage=feedback|feedback=none|option=idle|input=disabled\n"
        "option_a|rect=16.0,252.0,328.0,44.0|role=quiz_option|stage=feedback|feedback=incorrect|option=correct|input=disabled\n"
        "option_b|rect=16.0,304.0,328.0,44.0|role=quiz_option|stage=feedback|feedback=incorrect|option=incorrect|input=disabled\n"
        "answer_dock|rect=16.0,356.0,328.0,48.0|role=quiz_answer_dock|stage=feedback|feedback=incorrect|option=idle|input=enabled\n";
    assert(snapshot == expected_snapshot);

    return 0;
}
