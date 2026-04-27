#pragma once

#include "core/scene/scene_layout_data.h"

#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::scene {

enum class scene_layout_patch_operation_type {
    append_node,
    remove_node,
    set_text,
    set_style,
    set_bounds_rule,
    set_image,
    bind_action,
    set_semantics,
    set_focus,
    set_route,
    start_transition,
};

struct scene_layout_patch_operation {
    scene_layout_patch_operation_type type = scene_layout_patch_operation_type::append_node;
    scene_node_id parent_id;
    scene_node_id node_id;
    scene_node_data node;
    std::size_t child_index = scene_layout_data::append_to_end;
    std::vector<scene_text_run> text_runs;
    scene_style style;
    scene_layout_rule bounds_rule;
    scene_image_ref image;
    scene_action_binding action;
    scene_node_semantics semantics;
    scene_route_state route_state;
    scene_animation_state animation_state;
};

struct scene_layout_apply_result {
    std::vector<std::string> errors;

    bool applied() const
    {
        return errors.empty();
    }
};

class scene_layout_patch {
public:
    scene_layout_patch& append_node(
        scene_node_id parent_id,
        scene_node_data node,
        std::size_t child_index = scene_layout_data::append_to_end)
    {
        scene_layout_patch_operation operation;
        operation.type = scene_layout_patch_operation_type::append_node;
        operation.parent_id = std::move(parent_id);
        operation.node = std::move(node);
        operation.child_index = child_index;
        operations_.push_back(std::move(operation));
        return *this;
    }

    scene_layout_patch& remove_node(scene_node_id node_id)
    {
        scene_layout_patch_operation operation;
        operation.type = scene_layout_patch_operation_type::remove_node;
        operation.node_id = std::move(node_id);
        operations_.push_back(std::move(operation));
        return *this;
    }

    scene_layout_patch& set_text(scene_node_id node_id, std::vector<scene_text_run> text_runs)
    {
        scene_layout_patch_operation operation;
        operation.type = scene_layout_patch_operation_type::set_text;
        operation.node_id = std::move(node_id);
        operation.text_runs = std::move(text_runs);
        operations_.push_back(std::move(operation));
        return *this;
    }

    scene_layout_patch& set_style(scene_node_id node_id, scene_style style)
    {
        scene_layout_patch_operation operation;
        operation.type = scene_layout_patch_operation_type::set_style;
        operation.node_id = std::move(node_id);
        operation.style = std::move(style);
        operations_.push_back(std::move(operation));
        return *this;
    }

    scene_layout_patch& set_bounds_rule(scene_node_id node_id, scene_layout_rule bounds_rule)
    {
        scene_layout_patch_operation operation;
        operation.type = scene_layout_patch_operation_type::set_bounds_rule;
        operation.node_id = std::move(node_id);
        operation.bounds_rule = bounds_rule;
        operations_.push_back(std::move(operation));
        return *this;
    }

    scene_layout_patch& set_image(scene_node_id node_id, scene_image_ref image)
    {
        scene_layout_patch_operation operation;
        operation.type = scene_layout_patch_operation_type::set_image;
        operation.node_id = std::move(node_id);
        operation.image = std::move(image);
        operations_.push_back(std::move(operation));
        return *this;
    }

    scene_layout_patch& bind_action(scene_node_id node_id, scene_action_binding action)
    {
        scene_layout_patch_operation operation;
        operation.type = scene_layout_patch_operation_type::bind_action;
        operation.node_id = std::move(node_id);
        operation.action = std::move(action);
        operations_.push_back(std::move(operation));
        return *this;
    }

    scene_layout_patch& set_semantics(scene_node_id node_id, scene_node_semantics semantics)
    {
        scene_layout_patch_operation operation;
        operation.type = scene_layout_patch_operation_type::set_semantics;
        operation.node_id = std::move(node_id);
        operation.semantics = std::move(semantics);
        operations_.push_back(std::move(operation));
        return *this;
    }

    scene_layout_patch& set_focus(scene_node_id node_id)
    {
        scene_layout_patch_operation operation;
        operation.type = scene_layout_patch_operation_type::set_focus;
        operation.node_id = std::move(node_id);
        operations_.push_back(std::move(operation));
        return *this;
    }

    scene_layout_patch& set_route(scene_route_state route_state)
    {
        scene_layout_patch_operation operation;
        operation.type = scene_layout_patch_operation_type::set_route;
        operation.route_state = std::move(route_state);
        operations_.push_back(std::move(operation));
        return *this;
    }

    scene_layout_patch& start_transition(scene_animation_state animation_state)
    {
        scene_layout_patch_operation operation;
        operation.type = scene_layout_patch_operation_type::start_transition;
        operation.animation_state = std::move(animation_state);
        operations_.push_back(std::move(operation));
        return *this;
    }

    scene_layout_patch& append_patch(const scene_layout_patch& patch)
    {
        operations_.insert(operations_.end(), patch.operations_.begin(), patch.operations_.end());
        return *this;
    }

    bool empty() const
    {
        return operations_.empty();
    }

    const std::vector<scene_layout_patch_operation>& operations() const
    {
        return operations_;
    }

    scene_layout_apply_result apply_to(scene_layout_data& data) const
    {
        scene_layout_apply_result result;
        for (std::size_t index = 0; index < operations_.size(); ++index) {
            apply_operation(data, operations_[index], result);
        }
        return result;
    }

private:
    static void add_error(scene_layout_apply_result& result, const std::string& operation, const std::string& error)
    {
        result.errors.push_back(operation + ": " + error);
    }

    static void apply_operation(scene_layout_data& data, const scene_layout_patch_operation& operation, scene_layout_apply_result& result)
    {
        std::string error;
        switch (operation.type) {
        case scene_layout_patch_operation_type::append_node:
            if (!data.append_node(operation.parent_id, operation.node, &error, operation.child_index)) {
                add_error(result, "append_node", error);
            }
            break;
        case scene_layout_patch_operation_type::remove_node:
            if (!data.remove_node(operation.node_id, &error)) {
                add_error(result, "remove_node", error);
            }
            break;
        case scene_layout_patch_operation_type::set_text:
            if (!data.set_text(operation.node_id, operation.text_runs, &error)) {
                add_error(result, "set_text", error);
            }
            break;
        case scene_layout_patch_operation_type::set_style:
            if (!data.set_style(operation.node_id, operation.style, &error)) {
                add_error(result, "set_style", error);
            }
            break;
        case scene_layout_patch_operation_type::set_bounds_rule:
            if (!data.set_bounds_rule(operation.node_id, operation.bounds_rule, &error)) {
                add_error(result, "set_bounds_rule", error);
            }
            break;
        case scene_layout_patch_operation_type::set_image:
            if (!data.set_image(operation.node_id, operation.image, &error)) {
                add_error(result, "set_image", error);
            }
            break;
        case scene_layout_patch_operation_type::bind_action:
            if (!data.bind_action(operation.node_id, operation.action, &error)) {
                add_error(result, "bind_action", error);
            }
            break;
        case scene_layout_patch_operation_type::set_semantics:
            if (!data.set_semantics(operation.node_id, operation.semantics, &error)) {
                add_error(result, "set_semantics", error);
            }
            break;
        case scene_layout_patch_operation_type::set_focus:
            if (!data.set_focus(operation.node_id, &error)) {
                add_error(result, "set_focus", error);
            }
            break;
        case scene_layout_patch_operation_type::set_route:
            data.set_route(operation.route_state);
            break;
        case scene_layout_patch_operation_type::start_transition:
            data.start_transition(operation.animation_state);
            break;
        }
    }

    std::vector<scene_layout_patch_operation> operations_;
};

} // namespace quiz_vulkan::scene
