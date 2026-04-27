#pragma once

#include "core/scene/scene_layout_data.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::scene {

class text_metrics_interface {
public:
    virtual ~text_metrics_interface() = default;

    virtual scene_size measure_text(
        const std::vector<scene_text_run>& text_runs,
        const scene_style& style,
        float max_width) const = 0;
};

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

class layout_placer {
public:
    placed_scene place(
        const scene_layout_data& data,
        scene_rect viewport,
        const text_metrics_interface& text_metrics) const
    {
        scene_layout_environment environment;
        environment.viewport = viewport;
        return place_with_environment(data, environment, text_metrics);
    }

    placed_scene place_with_environment(
        const scene_layout_data& data,
        scene_layout_environment environment,
        const text_metrics_interface& text_metrics) const
    {
        placed_scene output;
        output.environment = std::move(environment);
        output.usable_bounds = output.environment.keyboard_safe_bounds();
        const scene_node_data* root = data.find_node(data.root_node_id());
        if (root == nullptr || !root->visible) {
            return output;
        }

        place_node(data, *root, output.environment.viewport, output.environment, text_metrics, output, 0);
        return output;
    }

private:
    static float positive(float value)
    {
        return std::max(0.0f, value);
    }

    static scene_size measure_content(
        const scene_layout_data& data,
        const scene_node_data& node,
        float max_width,
        const text_metrics_interface& text_metrics)
    {
        if (!node.text_runs.empty()) {
            return text_metrics.measure_text(node.text_runs, node.style, max_width);
        }

        if (node.has_image && node.image.aspect_ratio > 0.0f) {
            if (node.layout_rule.has_width && !node.layout_rule.has_height) {
                return scene_size{node.layout_rule.width, node.layout_rule.width / node.image.aspect_ratio};
            }
            if (node.layout_rule.has_height && !node.layout_rule.has_width) {
                return scene_size{node.layout_rule.height * node.image.aspect_ratio, node.layout_rule.height};
            }
        }

        if (!node.children.empty()) {
            if (node.layout_rule.mode == scene_layout_mode::vertical) {
                return measure_vertical_children(data, node, max_width, text_metrics);
            }
            if (node.layout_rule.mode == scene_layout_mode::horizontal) {
                return measure_horizontal_children(data, node, max_width, text_metrics);
            }
            return measure_overlay_children(data, node, max_width, text_metrics);
        }

        return scene_size{};
    }

    static float resolve_width(
        const scene_layout_data& data,
        const scene_node_data& node,
        const scene_rect& parent_content,
        scene_layout_mode parent_mode,
        const text_metrics_interface& text_metrics)
    {
        const float offset_x = node.layout_rule.has_x ? node.layout_rule.x : 0.0f;
        const float available = positive(parent_content.width - node.layout_rule.margin.horizontal() - offset_x);
        if (node.layout_rule.has_width) {
            return positive(node.layout_rule.width);
        }

        const scene_size measured = measure_content(data, node, available, text_metrics);
        if (node.layout_rule.horizontal_alignment == scene_alignment::stretch || parent_mode == scene_layout_mode::horizontal) {
            return available;
        }

        if (measured.width > 0.0f) {
            return positive(measured.width + node.layout_rule.padding.horizontal());
        }

        return available;
    }

    static float resolve_height(
        const scene_layout_data& data,
        const scene_node_data& node,
        const scene_rect& parent_content,
        scene_layout_mode parent_mode,
        float resolved_width,
        const text_metrics_interface& text_metrics)
    {
        const float offset_y = node.layout_rule.has_y ? node.layout_rule.y : 0.0f;
        const float available = positive(parent_content.height - node.layout_rule.margin.vertical() - offset_y);
        if (node.layout_rule.has_height) {
            return positive(node.layout_rule.height);
        }

        const scene_size measured = measure_content(data, node, resolved_width, text_metrics);
        if (measured.height > 0.0f) {
            return positive(measured.height + node.layout_rule.padding.vertical());
        }

        if (node.has_image && node.image.aspect_ratio > 0.0f && resolved_width > 0.0f) {
            return positive((resolved_width - node.layout_rule.padding.horizontal()) / node.image.aspect_ratio
                + node.layout_rule.padding.vertical());
        }

        if (parent_mode == scene_layout_mode::overlay) {
            return available;
        }

        return 0.0f;
    }

    static scene_rect measurement_content_rect(float max_width)
    {
        return scene_rect{0.0f, 0.0f, positive(max_width), 0.0f};
    }

    static scene_size measure_vertical_children(
        const scene_layout_data& data,
        const scene_node_data& node,
        float max_width,
        const text_metrics_interface& text_metrics)
    {
        const scene_rect content = measurement_content_rect(max_width - node.layout_rule.padding.horizontal());
        scene_size measured;
        bool first = true;

        for (const auto& child_id : node.children) {
            const scene_node_data* child = data.find_node(child_id);
            if (child == nullptr || !child->visible) {
                continue;
            }

            if (!first) {
                measured.height += node.layout_rule.gap;
            }

            const float child_width = resolve_width(data, *child, content, scene_layout_mode::vertical, text_metrics);
            const float child_height = resolve_height(data, *child, content, scene_layout_mode::vertical, child_width, text_metrics);
            measured.width = std::max(measured.width, child_width + child->layout_rule.margin.horizontal());
            measured.height += child->layout_rule.margin.top + child_height + child->layout_rule.margin.bottom;
            first = false;
        }

        return measured;
    }

    static scene_size measure_horizontal_children(
        const scene_layout_data& data,
        const scene_node_data& node,
        float max_width,
        const text_metrics_interface& text_metrics)
    {
        const scene_rect content = measurement_content_rect(max_width - node.layout_rule.padding.horizontal());
        scene_size measured;
        bool first = true;

        for (const auto& child_id : node.children) {
            const scene_node_data* child = data.find_node(child_id);
            if (child == nullptr || !child->visible) {
                continue;
            }

            if (!first) {
                measured.width += node.layout_rule.gap;
            }

            const float child_width = resolve_width(data, *child, content, scene_layout_mode::horizontal, text_metrics);
            const float child_height = resolve_height(data, *child, content, scene_layout_mode::horizontal, child_width, text_metrics);
            measured.width += child->layout_rule.margin.left + child_width + child->layout_rule.margin.right;
            measured.height = std::max(measured.height, child->layout_rule.margin.top + child_height + child->layout_rule.margin.bottom);
            first = false;
        }

        return measured;
    }

    static scene_size measure_overlay_children(
        const scene_layout_data& data,
        const scene_node_data& node,
        float max_width,
        const text_metrics_interface& text_metrics)
    {
        const scene_rect content = measurement_content_rect(max_width - node.layout_rule.padding.horizontal());
        scene_size measured;

        for (const auto& child_id : node.children) {
            const scene_node_data* child = data.find_node(child_id);
            if (child == nullptr || !child->visible) {
                continue;
            }

            const float child_width = resolve_width(data, *child, content, scene_layout_mode::overlay, text_metrics);
            const float child_height = resolve_height(data, *child, content, scene_layout_mode::overlay, child_width, text_metrics);
            const float child_x = (child->layout_rule.has_x ? child->layout_rule.x : 0.0f) + child->layout_rule.margin.left;
            const float child_y = (child->layout_rule.has_y ? child->layout_rule.y : 0.0f) + child->layout_rule.margin.top;
            measured.width = std::max(measured.width, child_x + child_width + child->layout_rule.margin.right);
            measured.height = std::max(measured.height, child_y + child_height + child->layout_rule.margin.bottom);
        }

        return measured;
    }

    static float align_x(const scene_node_data& node, const scene_rect& parent_content, float width)
    {
        const float offset_x = node.layout_rule.has_x ? node.layout_rule.x : 0.0f;
        const float base_x = parent_content.x + node.layout_rule.margin.left + offset_x;
        const float available = positive(parent_content.width - node.layout_rule.margin.horizontal() - offset_x);
        if (node.layout_rule.horizontal_alignment == scene_alignment::center && width < available) {
            return base_x + (available - width) * 0.5f;
        }
        if (node.layout_rule.horizontal_alignment == scene_alignment::end && width < available) {
            return base_x + (available - width);
        }
        return base_x;
    }

    static float align_y(const scene_node_data& node, const scene_rect& parent_content, float height)
    {
        const float offset_y = node.layout_rule.has_y ? node.layout_rule.y : 0.0f;
        const float base_y = parent_content.y + node.layout_rule.margin.top + offset_y;
        const float available = positive(parent_content.height - node.layout_rule.margin.vertical() - offset_y);
        if (node.layout_rule.vertical_alignment == scene_alignment::center && height < available) {
            return base_y + (available - height) * 0.5f;
        }
        if (node.layout_rule.vertical_alignment == scene_alignment::end && height < available) {
            return base_y + (available - height);
        }
        return base_y;
    }

    static scene_rect child_bounds_for_overlay(
        const scene_layout_data& data,
        const scene_node_data& child,
        const scene_rect& parent_content,
        const text_metrics_interface& text_metrics)
    {
        const float width = resolve_width(data, child, parent_content, scene_layout_mode::overlay, text_metrics);
        const float height = resolve_height(data, child, parent_content, scene_layout_mode::overlay, width, text_metrics);
        return scene_rect{align_x(child, parent_content, width), align_y(child, parent_content, height), width, height};
    }

    static scene_rect child_bounds_for_vertical(
        const scene_layout_data& data,
        const scene_node_data& child,
        const scene_rect& parent_content,
        float y,
        const text_metrics_interface& text_metrics)
    {
        const float width = resolve_width(data, child, parent_content, scene_layout_mode::vertical, text_metrics);
        const float height = resolve_height(data, child, parent_content, scene_layout_mode::vertical, width, text_metrics);
        const float x = align_x(child, parent_content, width);
        return scene_rect{x, y + child.layout_rule.margin.top, width, height};
    }

    static scene_rect child_bounds_for_horizontal(
        const scene_layout_data& data,
        const scene_node_data& child,
        const scene_rect& parent_content,
        float x,
        const text_metrics_interface& text_metrics)
    {
        const float width = resolve_width(data, child, parent_content, scene_layout_mode::horizontal, text_metrics);
        const float height = resolve_height(data, child, parent_content, scene_layout_mode::horizontal, width, text_metrics);
        const float y = align_y(child, parent_content, height);
        return scene_rect{x + child.layout_rule.margin.left, y, width, height};
    }

    static void place_children(
        const scene_layout_data& data,
        const scene_node_data& node,
        const scene_rect& content_bounds,
        const scene_layout_environment& environment,
        const text_metrics_interface& text_metrics,
        placed_scene& output,
        std::size_t depth)
    {
        if (node.layout_rule.mode == scene_layout_mode::vertical) {
            float y = content_bounds.y;
            bool first = true;
            for (const auto& child_id : node.children) {
                const scene_node_data* child = data.find_node(child_id);
                if (child == nullptr || !child->visible) {
                    continue;
                }
                if (!first) {
                    y += node.layout_rule.gap;
                }
                scene_rect child_bounds = child_bounds_for_vertical(data, *child, content_bounds, y, text_metrics);
                place_node(data, *child, child_bounds, environment, text_metrics, output, depth + 1);
                y = child_bounds.bottom() + child->layout_rule.margin.bottom;
                first = false;
            }
            return;
        }

        if (node.layout_rule.mode == scene_layout_mode::horizontal) {
            float x = content_bounds.x;
            bool first = true;
            for (const auto& child_id : node.children) {
                const scene_node_data* child = data.find_node(child_id);
                if (child == nullptr || !child->visible) {
                    continue;
                }
                if (!first) {
                    x += node.layout_rule.gap;
                }
                scene_rect child_bounds = child_bounds_for_horizontal(data, *child, content_bounds, x, text_metrics);
                place_node(data, *child, child_bounds, environment, text_metrics, output, depth + 1);
                x = child_bounds.right() + child->layout_rule.margin.right;
                first = false;
            }
            return;
        }

        for (const auto& child_id : node.children) {
            const scene_node_data* child = data.find_node(child_id);
            if (child == nullptr || !child->visible) {
                continue;
            }
            scene_rect child_bounds = child_bounds_for_overlay(data, *child, content_bounds, text_metrics);
            place_node(data, *child, child_bounds, environment, text_metrics, output, depth + 1);
        }
    }

    static scene_rect apply_environment_constraints(
        const scene_node_data& node,
        scene_rect bounds,
        const scene_layout_environment& environment)
    {
        if (node.layout_rule.respect_safe_area) {
            bounds = bounds.clipped_to(environment.safe_bounds());
        }

        if (node.layout_rule.avoid_keyboard) {
            bounds = bounds.clipped_to(environment.keyboard_safe_bounds());
        }

        if (node.layout_rule.anchor_to_keyboard && environment.keyboard.visible && environment.keyboard.bottom_inset > 0.0f) {
            const scene_rect usable_bounds = environment.keyboard_safe_bounds();
            const float overflow = bounds.bottom() - usable_bounds.bottom();
            if (overflow > 0.0f) {
                bounds.y = std::max(usable_bounds.y, bounds.y - overflow);
            }
        }

        return bounds;
    }

    static void place_node(
        const scene_layout_data& data,
        const scene_node_data& node,
        scene_rect bounds,
        const scene_layout_environment& environment,
        const text_metrics_interface& text_metrics,
        placed_scene& output,
        std::size_t depth)
    {
        bounds = apply_environment_constraints(node, bounds, environment);
        const scene_rect content_bounds = bounds.inset(node.layout_rule.padding);

        placed_scene_node placed;
        placed.id = node.id;
        placed.parent_id = node.parent_id;
        placed.kind = node.kind;
        placed.bounds = bounds;
        placed.content_bounds = content_bounds;
        placed.layout_rule = node.layout_rule;
        placed.style = node.style;
        placed.text_runs = node.text_runs;
        placed.image = node.image;
        placed.has_image = node.has_image;
        placed.action_binding = node.action_binding;
        placed.has_action_binding = node.has_action_binding;
        placed.semantics = node.semantics;
        placed.depth = depth;
        placed.visible = node.visible;
        placed.input_enabled = node.input_enabled;
        output.nodes.push_back(std::move(placed));

        if (node.has_action_binding && !node.action_binding.empty()) {
            output.input_regions.push_back(scene_input_region{node.id, bounds, node.action_binding, node.input_enabled, node.semantics});
        }

        place_children(data, node, content_bounds, environment, text_metrics, output, depth);
    }
};

} // namespace quiz_vulkan::scene
