#pragma once

#include "core/scene/scene_layout_data.h"
#include "core/scene/scene_layout_edit_data.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::scene {

struct scene_modifier_context {
    const scene_layout_data* current_scene = nullptr;
    scene_rect viewport;
    float delta_seconds = 0.0f;
    scene_route_state route_state;
};

class scene_modifier {
public:
    virtual ~scene_modifier() = default;

    virtual void modify(const scene_modifier_context& context, scene_layout_edit_data& edit_data) = 0;
};

class scene_layout_data_modifier {
public:
    scene_layout_data_modifier() = default;

    explicit scene_layout_data_modifier(std::vector<std::shared_ptr<scene_modifier>> modifiers)
        : modifiers_(std::move(modifiers))
    {
    }

    void add_modifier(std::shared_ptr<scene_modifier> modifier)
    {
        modifiers_.push_back(std::move(modifier));
    }

    const std::vector<std::shared_ptr<scene_modifier>>& modifiers() const
    {
        return modifiers_;
    }

    scene_layout_patch build_patch(const scene_modifier_context& context) const
    {
        scene_layout_edit_data edit_data("scene_layout_data_modifier");
        for (const auto& modifier : modifiers_) {
            if (modifier) {
                modifier->modify(context, edit_data);
            }
        }
        return edit_data.finish_patch();
    }

    scene_layout_apply_result apply(scene_layout_data& data, scene_modifier_context context) const
    {
        context.current_scene = &data;
        scene_layout_patch patch = build_patch(context);
        return patch.apply_to(data);
    }

private:
    std::vector<std::shared_ptr<scene_modifier>> modifiers_;
};

} // namespace quiz_vulkan::scene
