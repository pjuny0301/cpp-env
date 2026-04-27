#include "core/layout/input_hit_test.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>

namespace {

void require(bool condition, const char* message)
{
    if (condition) {
        return;
    }

    std::cerr << "input_hit_test_tests failed: " << message << '\n';
    std::exit(1);
}

quiz_vulkan::scene::scene_action_binding action(
    quiz_vulkan::scene::scene_action_trigger trigger = quiz_vulkan::scene::scene_action_trigger::press)
{
    quiz_vulkan::scene::scene_action_binding binding;
    binding.trigger = trigger;
    binding.action_type = "activate";
    return binding;
}

quiz_vulkan::scene::scene_input_region region(
    std::string node_id,
    quiz_vulkan::scene::scene_rect bounds,
    quiz_vulkan::scene::scene_action_trigger trigger = quiz_vulkan::scene::scene_action_trigger::press,
    bool enabled = true)
{
    return quiz_vulkan::scene::scene_input_region{std::move(node_id), bounds, action(trigger), enabled, {}};
}

void test_basic_hit()
{
    quiz_vulkan::scene::placed_scene placed;
    placed.input_regions.push_back(region("button", {10.0f, 20.0f, 80.0f, 40.0f}));

    const quiz_vulkan::scene::scene_input_region* hit = quiz_vulkan::hit_test_input_region(placed, 24.0f, 30.0f);

    require(hit != nullptr, "basic hit returns a region");
    require(hit->node_id == "button", "basic hit returns the matching node");
}

void test_outside_miss()
{
    quiz_vulkan::scene::placed_scene placed;
    placed.input_regions.push_back(region("button", {10.0f, 20.0f, 80.0f, 40.0f}));

    require(quiz_vulkan::hit_test_input_region(placed, 9.0f, 30.0f) == nullptr, "left outside misses");
    require(quiz_vulkan::hit_test_input_region(placed, 24.0f, 61.0f) == nullptr, "bottom outside misses");
}

void test_disabled_region_skip()
{
    quiz_vulkan::scene::placed_scene placed;
    placed.input_regions.push_back(region("enabled", {0.0f, 0.0f, 100.0f, 100.0f}));
    placed.input_regions.push_back(region("disabled", {10.0f, 10.0f, 80.0f, 80.0f}, quiz_vulkan::scene::scene_action_trigger::press, false));

    const quiz_vulkan::scene::scene_input_region* hit = quiz_vulkan::hit_test_input_region(placed, 20.0f, 20.0f);

    require(hit != nullptr, "disabled top region is skipped");
    require(hit->node_id == "enabled", "hit falls through to enabled region");
}

void test_trigger_filter()
{
    quiz_vulkan::scene::placed_scene placed;
    placed.input_regions.push_back(region("text_input", {0.0f, 0.0f, 100.0f, 40.0f}, quiz_vulkan::scene::scene_action_trigger::change));

    require(quiz_vulkan::hit_test_input_region(placed, 20.0f, 20.0f) == nullptr, "default press trigger does not match change");

    const quiz_vulkan::scene::scene_input_region* hit = quiz_vulkan::hit_test_input_region(
        placed,
        20.0f,
        20.0f,
        quiz_vulkan::scene::scene_action_trigger::change);

    require(hit != nullptr, "matching change trigger hits");
    require(hit->node_id == "text_input", "matching trigger returns region");
}

void test_overlap_topmost_selection()
{
    quiz_vulkan::scene::placed_scene placed;
    placed.input_regions.push_back(region("parent", {0.0f, 0.0f, 100.0f, 100.0f}));
    placed.input_regions.push_back(region("child", {20.0f, 20.0f, 40.0f, 40.0f}));
    placed.input_regions.push_back(region("later_sibling", {30.0f, 30.0f, 20.0f, 20.0f}));

    const quiz_vulkan::scene::scene_input_region* hit = quiz_vulkan::hit_test_input_region(placed, 35.0f, 35.0f);

    require(hit != nullptr, "overlap hit returns a region");
    require(hit->node_id == "later_sibling", "last overlapping region wins");
}

void test_edge_inclusion_exclusion()
{
    quiz_vulkan::scene::placed_scene placed;
    placed.input_regions.push_back(region("box", {10.0f, 20.0f, 30.0f, 40.0f}));

    const quiz_vulkan::scene::scene_input_region* left_top = quiz_vulkan::hit_test_input_region(placed, 10.0f, 20.0f);

    require(left_top != nullptr, "left and top edges are included");
    require(left_top->node_id == "box", "left and top edges hit the region");
    require(quiz_vulkan::hit_test_input_region(placed, 40.0f, 30.0f) == nullptr, "right edge is excluded");
    require(quiz_vulkan::hit_test_input_region(placed, 20.0f, 60.0f) == nullptr, "bottom edge is excluded");
}

} // namespace

int main()
{
    test_basic_hit();
    test_outside_miss();
    test_disabled_region_skip();
    test_trigger_filter();
    test_overlap_topmost_selection();
    test_edge_inclusion_exclusion();
    return 0;
}
