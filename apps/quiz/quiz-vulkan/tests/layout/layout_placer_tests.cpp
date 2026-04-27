#include "core/layout/layout_placer.h"

#include <cassert>
#include <cstddef>
#include <cmath>
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

    return 0;
}
