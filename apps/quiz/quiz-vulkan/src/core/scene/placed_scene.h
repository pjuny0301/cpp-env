#pragma once

#include "core/scene/scene_layout_data.h"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace quiz_vulkan::scene {

struct placed_scene_node {
    scene_node_id id;
    scene_node_id parent_id;
    scene_node_kind kind = scene_node_kind::container;
    scene_rect bounds;
    scene_rect content_bounds;
    scene_layout_rule layout_rule;
    scene_style style;
    std::vector<scene_text_run> text_runs;
    scene_image_ref image;
    bool has_image = false;
    scene_action_binding action_binding;
    bool has_action_binding = false;
    scene_node_semantics semantics;
    std::size_t depth = 0;
    bool visible = true;
    bool input_enabled = true;
};

struct placed_scene {
    scene_layout_environment environment;
    scene_rect usable_bounds;
    std::vector<placed_scene_node> nodes;
    std::vector<scene_input_region> input_regions;

    const placed_scene_node* find_node(const scene_node_id& id) const
    {
        const auto found = std::find_if(
            nodes.begin(),
            nodes.end(),
            [&](const placed_scene_node& node) {
                return node.id == id;
            });
        return found == nodes.end() ? nullptr : &(*found);
    }
};

} // namespace quiz_vulkan::scene
