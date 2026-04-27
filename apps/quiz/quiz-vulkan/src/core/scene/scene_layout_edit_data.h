#pragma once

#include "core/scene/scene_layout_patch.h"

#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::scene {

class scene_layout_edit_data {
public:
    scene_layout_edit_data() = default;

    explicit scene_layout_edit_data(std::string source_name)
        : source_name_(std::move(source_name))
    {
    }

    const std::string& source_name() const
    {
        return source_name_;
    }

    const scene_layout_patch& patch() const
    {
        return patch_;
    }

    scene_layout_patch finish_patch()
    {
        return std::move(patch_);
    }

    scene_layout_edit_data& append_node(
        scene_node_id parent_id,
        scene_node_data node,
        std::size_t child_index = scene_layout_data::append_to_end)
    {
        patch_.append_node(std::move(parent_id), std::move(node), child_index);
        return *this;
    }

    scene_layout_edit_data& remove_node(scene_node_id node_id)
    {
        patch_.remove_node(std::move(node_id));
        return *this;
    }

    scene_layout_edit_data& set_text(scene_node_id node_id, std::vector<scene_text_run> text_runs)
    {
        patch_.set_text(std::move(node_id), std::move(text_runs));
        return *this;
    }

    scene_layout_edit_data& set_style(scene_node_id node_id, scene_style style)
    {
        patch_.set_style(std::move(node_id), std::move(style));
        return *this;
    }

    scene_layout_edit_data& set_bounds_rule(scene_node_id node_id, scene_layout_rule bounds_rule)
    {
        patch_.set_bounds_rule(std::move(node_id), bounds_rule);
        return *this;
    }

    scene_layout_edit_data& set_image(scene_node_id node_id, scene_image_ref image)
    {
        patch_.set_image(std::move(node_id), std::move(image));
        return *this;
    }

    scene_layout_edit_data& bind_action(scene_node_id node_id, scene_action_binding action)
    {
        patch_.bind_action(std::move(node_id), std::move(action));
        return *this;
    }

    scene_layout_edit_data& set_focus(scene_node_id node_id)
    {
        patch_.set_focus(std::move(node_id));
        return *this;
    }

    scene_layout_edit_data& clear_focus()
    {
        patch_.set_focus("");
        return *this;
    }

    scene_layout_edit_data& set_route(scene_route_state route_state)
    {
        patch_.set_route(std::move(route_state));
        return *this;
    }

    scene_layout_edit_data& start_transition(scene_animation_state animation_state)
    {
        patch_.start_transition(std::move(animation_state));
        return *this;
    }

private:
    std::string source_name_;
    scene_layout_patch patch_;
};

} // namespace quiz_vulkan::scene
