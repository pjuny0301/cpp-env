#pragma once

#include "app/app_input_router.h"
#include "app/app_quiz_screens.h"
#include "app/app_state.h"
#include "core/domain/app_action.hpp"
#include "core/input/input_event.h"
#include "core/layout/layout_placer.h"
#include "core/scene/placed_scene.h"
#include "core/scene/scene_layout_data.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace quiz_vulkan {

enum class app_scene_scenario_input_kind {
    tap_node,
    text_submit,
    swipe_left,
    swipe_right,
    long_press,
};

struct app_scene_scenario_step {
    std::string name;
    app_scene_scenario_input_kind input = app_scene_scenario_input_kind::tap_node;
    std::string target_node_id;
    std::string committed_text;
    std::int64_t now_ms = 0;
};

struct app_scene_scenario_frame {
    domain::app_snapshot snapshot;
    scene::scene_layout_data layout;
    scene::placed_scene placed;
    std::string error;

    explicit app_scene_scenario_frame(std::string scene_name = "app_scene_scenario")
        : layout(std::move(scene_name))
    {
    }

    bool ok() const
    {
        return error.empty();
    }
};

struct app_scene_scenario_trace_entry {
    std::string step_name;
    std::string event_kind;
    std::string target_node_id;
    std::string before_screen_id;
    std::string after_screen_id;
    std::string before_focus_id;
    std::string after_focus_id;
    std::string action_type;
    bool handled = false;
    bool needs_render = false;
    bool clear_text_after_action = false;
    std::size_t before_node_count = 0;
    std::size_t after_node_count = 0;
    std::size_t before_input_region_count = 0;
    std::size_t after_input_region_count = 0;
    std::string error;
};

struct app_scene_scenario_result {
    std::vector<app_scene_scenario_trace_entry> trace;
    app_scene_scenario_frame final_frame;
    std::string error;

    app_scene_scenario_result()
        : final_frame("app_scene_scenario_final")
    {
    }

    bool ok() const
    {
        return error.empty();
    }
};

inline std::string to_string(app_scene_scenario_input_kind input)
{
    switch (input) {
        case app_scene_scenario_input_kind::tap_node:
            return "tap_node";
        case app_scene_scenario_input_kind::text_submit:
            return "text_submit";
        case app_scene_scenario_input_kind::swipe_left:
            return "swipe_left";
        case app_scene_scenario_input_kind::swipe_right:
            return "swipe_right";
        case app_scene_scenario_input_kind::long_press:
            return "long_press";
    }

    return "tap_node";
}

inline app_scene_scenario_frame make_app_scene_scenario_frame(
    const app_state& state,
    scene::scene_rect viewport,
    const scene::text_metrics_interface& text_metrics,
    std::string scene_name = "app_scene_scenario")
{
    app_scene_scenario_frame frame(std::move(scene_name));
    frame.snapshot = state.snapshot();

    const scene::scene_layout_patch patch = presentation::make_quiz_screen_patch(frame.snapshot);
    const scene::scene_layout_apply_result applied = patch.apply_to(frame.layout);
    if (!applied.applied()) {
        frame.error = applied.errors.empty() ? "scenario frame patch apply failed" : applied.errors.front();
        return frame;
    }

    frame.placed = scene::layout_placer().place(frame.layout, viewport, text_metrics);
    return frame;
}

inline const scene::scene_input_region* find_scenario_input_region(
    const scene::placed_scene& placed,
    std::string_view node_id)
{
    for (auto region = placed.input_regions.rbegin(); region != placed.input_regions.rend(); ++region) {
        if (region->enabled && region->node_id == node_id) {
            return &(*region);
        }
    }
    return nullptr;
}

inline input::input_event make_scenario_input_event(
    const app_scene_scenario_step& step,
    const scene::placed_scene& placed,
    std::string& error)
{
    if (step.input == app_scene_scenario_input_kind::text_submit) {
        return input::text_event{
            .kind = input::text_event_kind::submit,
            .timestamp_ms = step.now_ms,
            .target_id = step.target_node_id,
            .utf8_text = step.committed_text,
        };
    }

    scene::scene_rect bounds = placed.usable_bounds;
    if (step.input == app_scene_scenario_input_kind::tap_node) {
        const scene::scene_input_region* region = find_scenario_input_region(placed, step.target_node_id);
        if (region == nullptr) {
            error = "scenario target input region not found: " + step.target_node_id;
            return input::gesture_event{};
        }
        bounds = region->bounds;
    }

    input::gesture_event event;
    event.timestamp_ms = step.now_ms;
    event.start_x = bounds.x + bounds.width * 0.5f;
    event.start_y = bounds.y + bounds.height * 0.5f;
    event.x = event.start_x;
    event.y = event.start_y;

    switch (step.input) {
        case app_scene_scenario_input_kind::tap_node:
            event.kind = input::gesture_kind::tap;
            break;
        case app_scene_scenario_input_kind::text_submit:
            break;
        case app_scene_scenario_input_kind::swipe_left:
            event.kind = input::gesture_kind::swipe_left;
            event.delta_x = -120.0f;
            break;
        case app_scene_scenario_input_kind::swipe_right:
            event.kind = input::gesture_kind::swipe_right;
            event.delta_x = 120.0f;
            break;
        case app_scene_scenario_input_kind::long_press:
            event.kind = input::gesture_kind::long_press;
            event.duration_ms = 650;
            break;
    }

    return event;
}

inline app_scene_scenario_result run_app_scene_scenario(
    app_state state,
    const std::vector<app_scene_scenario_step>& steps,
    scene::scene_rect viewport,
    const scene::text_metrics_interface& text_metrics)
{
    app_scene_scenario_result result;

    for (const app_scene_scenario_step& step : steps) {
        app_scene_scenario_frame before = make_app_scene_scenario_frame(
            state,
            viewport,
            text_metrics,
            "scenario_before_" + step.name);
        if (!before.ok()) {
            result.error = before.error;
            return result;
        }

        app_scene_scenario_trace_entry trace;
        trace.step_name = step.name;
        trace.event_kind = to_string(step.input);
        trace.target_node_id = step.target_node_id;
        trace.before_screen_id = before.layout.route_state().screen_id;
        trace.before_focus_id = before.layout.has_focus() ? before.layout.focus_id() : std::string{};
        trace.before_node_count = before.layout.nodes().size();
        trace.before_input_region_count = before.placed.input_regions.size();

        std::string event_error;
        const input::input_event event = make_scenario_input_event(step, before.placed, event_error);
        if (!event_error.empty()) {
            result.error = event_error;
            trace.error = event_error;
            result.trace.push_back(std::move(trace));
            return result;
        }

        app_input_route_result routed = route_normalized_input_event(event, before.placed, step.committed_text);
        if (!routed.ok()) {
            result.error = routed.error;
            trace.error = routed.error;
            result.trace.push_back(std::move(trace));
            return result;
        }

        trace.handled = routed.handled;
        trace.needs_render = routed.needs_render;
        trace.clear_text_after_action = routed.clear_text_after_action;
        if (routed.action.has_value()) {
            trace.action_type = std::string(domain::to_string(domain::type_of(*routed.action)));
            state.dispatch(*routed.action, step.now_ms);
        }

        app_scene_scenario_frame after = make_app_scene_scenario_frame(
            state,
            viewport,
            text_metrics,
            "scenario_after_" + step.name);
        if (!after.ok()) {
            result.error = after.error;
            trace.error = after.error;
            result.trace.push_back(std::move(trace));
            return result;
        }

        trace.after_screen_id = after.layout.route_state().screen_id;
        trace.after_focus_id = after.layout.has_focus() ? after.layout.focus_id() : std::string{};
        trace.after_node_count = after.layout.nodes().size();
        trace.after_input_region_count = after.placed.input_regions.size();
        result.trace.push_back(std::move(trace));
    }

    result.final_frame = make_app_scene_scenario_frame(
        state,
        viewport,
        text_metrics,
        "scenario_final");
    if (!result.final_frame.ok()) {
        result.error = result.final_frame.error;
    }
    return result;
}

} // namespace quiz_vulkan
