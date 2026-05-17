#pragma once

#include "core/scene/placed_scene.h"

namespace quiz_vulkan {

inline const scene::scene_input_region* hit_test_input_region(
    const scene::placed_scene& placed,
    float x,
    float y,
    scene::scene_action_trigger trigger = scene::scene_action_trigger::press)
{
    // Input regions follow layout traversal/paint order, so reverse order prefers topmost descendants.
    for (auto region = placed.input_regions.rbegin(); region != placed.input_regions.rend(); ++region) {
        if (!region->enabled || region->action.trigger != trigger) {
            continue;
        }

        const scene::scene_rect& bounds = region->bounds;
        if (x >= bounds.x && y >= bounds.y && x < bounds.right() && y < bounds.bottom()) {
            return &(*region);
        }
    }

    return nullptr;
}

} // namespace quiz_vulkan
