#include "app/app_input_router.h"

#include "app/app_action_router.h"
#include "app/app_command_registry.h"
#include "app/app_quiz_scene_semantics.h"
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
        .pointer_id = event.pointer_id,
        .phase = phase,
        .button = event.pointer_button,
        .x = event.x,
        .y = event.y,
    };
}

raw_platform_key_event raw_key(
    const platform_input_event& event,
    std::int64_t timestamp_ms,
    raw_platform_key_phase phase)
{
    return raw_platform_key_event{
        .timestamp_ms = timestamp_ms,
        .phase = phase,
        .key_code = event.key_code,
        .logical_key = event.logical_key,
        .alt = event.alt,
        .ctrl = event.ctrl,
        .shift = event.shift,
        .meta = event.meta,
        .repeat = event.repeat,
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

app_input_route_result route_command_result(
    const scene::scene_command& command,
    std::optional<std::string_view> submitted_text,
    bool clear_text_after_action)
{
    const app_command_route_result routed_action = route_scene_command(command, submitted_text);
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

const scene::scene_event_handler* find_event_handler(
    const std::vector<scene::scene_event_handler>& handlers,
    scene::scene_action_trigger trigger)
{
    for (const scene::scene_event_handler& handler : handlers) {
        if (handler.trigger == trigger && !handler.commands.empty()) {
            return &handler;
        }
    }
    return nullptr;
}

bool submits_text_answer(const scene::scene_event_handler& handler)
{
    return !handler.commands.empty() && handler.commands.front().name == "submit_text_answer";
}

app_input_route_result route_event_handler_result(
    const scene::scene_event_handler& handler,
    std::optional<std::string_view> submitted_text,
    bool clear_text_after_action)
{
    if (handler.commands.empty()) {
        return {};
    }
    return route_command_result(handler.commands.front(), submitted_text, clear_text_after_action);
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

const scene::scene_event_handler* first_enabled_event_handler(
    const scene::placed_scene& placed_scene,
    scene::scene_action_trigger trigger)
{
    for (auto region = placed_scene.input_regions.rbegin(); region != placed_scene.input_regions.rend(); ++region) {
        if (!region->enabled) {
            continue;
        }
        if (const scene::scene_event_handler* handler = find_event_handler(region->event_handlers, trigger)) {
            return handler;
        }
    }
    return nullptr;
}

std::optional<scene::scene_action_trigger> trigger_for_non_tap_gesture(input::gesture_kind kind)
{
    if (kind == input::gesture_kind::swipe_left) {
        return scene::scene_action_trigger::swipe_left;
    }
    if (kind == input::gesture_kind::swipe_right) {
        return scene::scene_action_trigger::swipe_right;
    }
    if (kind == input::gesture_kind::long_press) {
        return scene::scene_action_trigger::long_press;
    }

    return std::nullopt;
}

app_input_route_result route_gesture(
    const input::gesture_event& event,
    const scene::placed_scene& placed_scene,
    std::string_view committed_text)
{
    if (event.kind == input::gesture_kind::tap) {
        const scene::scene_input_region* region = hit_test_input_region(
            placed_scene,
            event.x,
            event.y,
            scene::scene_action_trigger::press);
        if (region == nullptr) {
            return {};
        }

        const scene::scene_event_handler* press_handler =
            find_event_handler(region->event_handlers, scene::scene_action_trigger::press);
        const bool submits_text = press_handler != nullptr
            ? submits_text_answer(*press_handler)
            : region->action.action_type == "submit_text_answer";
        const std::optional<std::string_view> submitted_text =
            submits_text ? std::optional<std::string_view>{committed_text} : std::nullopt;
        if (press_handler != nullptr) {
            return route_event_handler_result(*press_handler, submitted_text, submits_text);
        }
        return route_action_result(region->action, submitted_text, submits_text);
    }

    const std::optional<scene::scene_action_trigger> trigger = trigger_for_non_tap_gesture(event.kind);
    if (!trigger.has_value()) {
        return {};
    }

    const scene::scene_event_handler* handler = first_enabled_event_handler(placed_scene, *trigger);
    if (handler == nullptr) {
        return {};
    }

    return route_event_handler_result(*handler, std::nullopt, false);
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
            raw_platform_pointer_event{
                .timestamp_ms = timestamp_ms,
                .pointer_id = legacy_pointer_id,
                .phase = raw_platform_pointer_phase::down,
                .button = raw_platform_pointer_button::primary,
                .x = event.x,
                .y = event.y,
            },
            raw_platform_pointer_event{
                .timestamp_ms = timestamp_ms,
                .pointer_id = legacy_pointer_id,
                .phase = raw_platform_pointer_phase::up,
                .button = raw_platform_pointer_button::primary,
                .x = event.x,
                .y = event.y,
            },
        };
    case platform_input_event_type::pointer_down:
        return {raw_pointer(event, timestamp_ms, raw_platform_pointer_phase::down)};
    case platform_input_event_type::pointer_move:
        return {raw_pointer(event, timestamp_ms, raw_platform_pointer_phase::move)};
    case platform_input_event_type::pointer_up:
        return {raw_pointer(event, timestamp_ms, raw_platform_pointer_phase::up)};
    case platform_input_event_type::pointer_cancel:
        return {raw_pointer(event, timestamp_ms, raw_platform_pointer_phase::cancel)};
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
    case platform_input_event_type::key_down:
        return {raw_key(event, timestamp_ms, raw_platform_key_phase::down)};
    case platform_input_event_type::key_up:
        return {raw_key(event, timestamp_ms, raw_platform_key_phase::up)};
    case platform_input_event_type::focus_gained:
        return {raw_platform_focus_event{
            .timestamp_ms = timestamp_ms,
            .phase = raw_platform_focus_phase::gained,
        }};
    case platform_input_event_type::focus_lost:
        return {raw_platform_focus_event{
            .timestamp_ms = timestamp_ms,
            .phase = raw_platform_focus_phase::lost,
        }};
    case platform_input_event_type::mouse_wheel:
        return {raw_platform_scroll_event{
            .timestamp_ms = timestamp_ms,
            .x = event.x,
            .y = event.y,
            .delta_x = event.delta_x,
            .delta_y = event.delta_y,
            .unit = event.scroll_unit,
            .alt = event.alt,
            .ctrl = event.ctrl,
            .shift = event.shift,
            .meta = event.meta,
        }};
    }

    return {};
}

std::optional<std::string> keyboard_input_target(const scene::placed_scene& placed_scene)
{
    for (const scene::placed_scene_node& node : placed_scene.nodes) {
        if (node.visible && node.input_enabled && presentation::accepts_keyboard_input(node.semantics)) {
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
