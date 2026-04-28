#include "app/app_input_router.h"

#include "app/app_action_router.h"
#include "core/layout/input_hit_test.h"

#include <type_traits>
#include <utility>
#include <variant>

namespace quiz_vulkan {
namespace {

constexpr std::int32_t legacy_pointer_id = 0;

raw_platform_pointer_event raw_pointer(
    const platform_input_event& event,
    std::int64_t timestamp_ms,
    raw_platform_pointer_phase phase)
{
    return raw_platform_pointer_event{
        .timestamp_ms = timestamp_ms,
        .pointer_id = legacy_pointer_id,
        .phase = phase,
        .button = raw_platform_pointer_button::primary,
        .x = event.x,
        .y = event.y,
    };
}

app_input_route_result render_only_result()
{
    return app_input_route_result{
        .handled = true,
        .needs_render = true,
        .clear_text_after_action = false,
        .action = std::nullopt,
        .error = {},
    };
}

app_input_route_result route_error(std::string error)
{
    return app_input_route_result{
        .handled = true,
        .needs_render = false,
        .clear_text_after_action = false,
        .action = std::nullopt,
        .error = std::move(error),
    };
}

app_input_route_result route_action_result(
    const scene::scene_action_binding& binding,
    std::optional<std::string_view> submitted_text,
    bool clear_text_after_action)
{
    const app_action_route_result routed_action = route_scene_action(binding, submitted_text);
    if (!routed_action.ok() || !routed_action.action.has_value()) {
        return route_error(routed_action.error);
    }

    return app_input_route_result{
        .handled = true,
        .needs_render = true,
        .clear_text_after_action = clear_text_after_action,
        .action = routed_action.action,
        .error = {},
    };
}

std::optional<scene::scene_action_binding> text_submit_action(const scene::placed_scene& placed_scene)
{
    for (auto region = placed_scene.input_regions.rbegin(); region != placed_scene.input_regions.rend(); ++region) {
        if (!region->enabled || region->action.action_type != "submit_text_answer") {
            continue;
        }
        return region->action;
    }
    return std::nullopt;
}

app_input_route_result route_gesture(
    const input::gesture_event& event,
    const scene::placed_scene& placed_scene,
    std::string_view committed_text)
{
    if (event.kind != input::gesture_kind::tap) {
        return {};
    }

    const scene::scene_input_region* region = hit_test_input_region(
        placed_scene,
        event.x,
        event.y,
        scene::scene_action_trigger::press);
    if (region == nullptr) {
        return {};
    }

    const bool submits_text = region->action.action_type == "submit_text_answer";
    const std::optional<std::string_view> submitted_text =
        submits_text ? std::optional<std::string_view>{committed_text} : std::nullopt;
    return route_action_result(region->action, submitted_text, submits_text);
}

app_input_route_result route_text_event(
    const input::text_event& event,
    const scene::placed_scene& placed_scene)
{
    if (event.kind != input::text_event_kind::submit) {
        return render_only_result();
    }

    const std::optional<scene::scene_action_binding> binding = text_submit_action(placed_scene);
    if (!binding.has_value()) {
        return {};
    }

    return route_action_result(*binding, std::string_view{event.utf8_text}, true);
}

} // namespace

bool app_input_route_result::ok() const
{
    return error.empty();
}

std::vector<raw_platform_input_event> normalize_platform_input_event(
    const platform_input_event& event,
    std::int64_t timestamp_ms)
{
    switch (event.type) {
    case platform_input_event_type::pointer_press:
        return {
            raw_pointer(event, timestamp_ms, raw_platform_pointer_phase::down),
            raw_pointer(event, timestamp_ms, raw_platform_pointer_phase::up),
        };
    case platform_input_event_type::text_input:
        return {raw_platform_text_event{
            .timestamp_ms = timestamp_ms,
            .utf8_text = event.text,
        }};
    case platform_input_event_type::text_backspace:
        return {raw_platform_key_event{
            .timestamp_ms = timestamp_ms,
            .phase = raw_platform_key_phase::down,
            .key_code = 8,
            .logical_key = "Backspace",
        }};
    case platform_input_event_type::text_submit:
        return {raw_platform_key_event{
            .timestamp_ms = timestamp_ms,
            .phase = raw_platform_key_phase::down,
            .key_code = 13,
            .logical_key = "Enter",
        }};
    }

    return {};
}

std::optional<std::string> keyboard_input_target(const scene::placed_scene& placed_scene)
{
    for (const scene::placed_scene_node& node : placed_scene.nodes) {
        if (node.visible && node.input_enabled && node.semantics.quiz.accepts_keyboard_input) {
            return node.id;
        }
    }
    return std::nullopt;
}

app_input_route_result route_normalized_input_event(
    const input::input_event& event,
    const scene::placed_scene& placed_scene,
    std::string_view committed_text)
{
    return std::visit(
        [&](const auto& normalized_event) -> app_input_route_result {
            using event_type = std::decay_t<decltype(normalized_event)>;
            if constexpr (std::is_same_v<event_type, input::gesture_event>) {
                return route_gesture(normalized_event, placed_scene, committed_text);
            } else if constexpr (std::is_same_v<event_type, input::text_event>) {
                return route_text_event(normalized_event, placed_scene);
            } else {
                return render_only_result();
            }
        },
        event);
}

} // namespace quiz_vulkan
