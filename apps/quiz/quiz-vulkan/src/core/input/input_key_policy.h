#pragma once

#include "core/input/input_engine.h"

namespace quiz_vulkan::input::detail {

[[nodiscard]] inline bool is_backspace_key(const raw_platform_key_event& event)
{
    return event.logical_key == "Backspace" || event.key_code == 8;
}

[[nodiscard]] inline bool is_submit_key(const raw_platform_key_event& event)
{
    return event.logical_key == "Enter"
        || event.logical_key == "NumpadEnter"
        || event.logical_key == "Return"
        || event.key_code == 13;
}

[[nodiscard]] inline bool is_arrow_left_key(const raw_platform_key_event& event)
{
    return event.logical_key == "ArrowLeft"
        || event.logical_key == "Left"
        || event.key_code == 37;
}

[[nodiscard]] inline bool is_arrow_right_key(const raw_platform_key_event& event)
{
    return event.logical_key == "ArrowRight"
        || event.logical_key == "Right"
        || event.key_code == 39;
}

[[nodiscard]] inline bool is_home_key(const raw_platform_key_event& event)
{
    return event.logical_key == "Home" || event.key_code == 36;
}

[[nodiscard]] inline bool is_end_key(const raw_platform_key_event& event)
{
    return event.logical_key == "End" || event.key_code == 35;
}

[[nodiscard]] inline bool is_tab_key(const raw_platform_key_event& event)
{
    return event.logical_key == "Tab" || event.key_code == 9;
}

[[nodiscard]] inline bool is_a_key(const raw_platform_key_event& event)
{
    return event.logical_key == "a"
        || event.logical_key == "A"
        || event.logical_key == "KeyA"
        || event.key_code == 65;
}

[[nodiscard]] inline bool is_select_all_key(const raw_platform_key_event& event)
{
    return !event.alt && (event.ctrl || event.meta) && is_a_key(event);
}

[[nodiscard]] inline bool is_keyboard_navigation_key(const raw_platform_key_event& event)
{
    return is_arrow_left_key(event)
        || is_arrow_right_key(event)
        || is_home_key(event)
        || is_end_key(event)
        || is_tab_key(event)
        || is_select_all_key(event);
}

[[nodiscard]] inline action_route_policy_kind navigation_policy_kind(const raw_platform_key_event& event)
{
    if (is_tab_key(event)) {
        return event.shift
            ? action_route_policy_kind::focus_traversal_previous
            : action_route_policy_kind::focus_traversal_next;
    }

    if (is_select_all_key(event)
        || ((is_arrow_left_key(event) || is_arrow_right_key(event)) && event.shift)) {
        return action_route_policy_kind::selection_changed;
    }

    return action_route_policy_kind::caret_moved;
}

} // namespace quiz_vulkan::input::detail
