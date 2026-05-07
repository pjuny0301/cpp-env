#pragma once

#include "core/input/input_engine.h"

namespace quiz_vulkan::input::detail {

[[nodiscard]] inline bool is_backspace_key(const raw_platform_key_event& event)
{
    return event.logical_key == "Backspace" || event.key_code == 8;
}

[[nodiscard]] inline bool is_delete_key(const raw_platform_key_event& event)
{
    return event.logical_key == "Delete" || event.logical_key == "Del" || event.key_code == 46;
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

[[nodiscard]] inline bool is_escape_key(const raw_platform_key_event& event)
{
    return event.logical_key == "Escape" || event.logical_key == "Esc" || event.key_code == 27;
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

[[nodiscard]] inline keyboard_shortcut_intent shortcut_intent(const raw_platform_key_event& event)
{
    if (is_tab_key(event)) {
        return event.shift
            ? keyboard_shortcut_intent::focus_traversal_previous
            : keyboard_shortcut_intent::focus_traversal_next;
    }
    if (is_submit_key(event)) {
        return keyboard_shortcut_intent::submit;
    }
    if (is_escape_key(event)) {
        return keyboard_shortcut_intent::cancel;
    }
    if (is_select_all_key(event)) {
        return keyboard_shortcut_intent::select_all;
    }
    if (is_backspace_key(event)) {
        return keyboard_shortcut_intent::delete_backward;
    }
    if (is_delete_key(event)) {
        return keyboard_shortcut_intent::delete_forward;
    }
    if (is_arrow_left_key(event)) {
        return event.shift
            ? keyboard_shortcut_intent::selection_previous
            : keyboard_shortcut_intent::caret_previous;
    }
    if (is_arrow_right_key(event)) {
        return event.shift
            ? keyboard_shortcut_intent::selection_next
            : keyboard_shortcut_intent::caret_next;
    }
    if (is_home_key(event)) {
        return keyboard_shortcut_intent::caret_home;
    }
    if (is_end_key(event)) {
        return keyboard_shortcut_intent::caret_end;
    }

    return keyboard_shortcut_intent::none;
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

[[nodiscard]] inline action_route_policy_kind route_policy_kind_for_shortcut(
    const raw_platform_key_event& event,
    keyboard_shortcut_intent intent)
{
    switch (intent) {
    case keyboard_shortcut_intent::focus_traversal_next:
        return action_route_policy_kind::focus_traversal_next;
    case keyboard_shortcut_intent::focus_traversal_previous:
        return action_route_policy_kind::focus_traversal_previous;
    case keyboard_shortcut_intent::submit:
        return action_route_policy_kind::text_submit_boundary;
    case keyboard_shortcut_intent::cancel:
        return action_route_policy_kind::keyboard_cancel_intent;
    case keyboard_shortcut_intent::delete_backward:
        return action_route_policy_kind::text_backspace_boundary;
    case keyboard_shortcut_intent::delete_forward:
        return action_route_policy_kind::text_delete_forward_boundary;
    case keyboard_shortcut_intent::select_all:
    case keyboard_shortcut_intent::selection_previous:
    case keyboard_shortcut_intent::selection_next:
        return action_route_policy_kind::selection_changed;
    case keyboard_shortcut_intent::caret_previous:
    case keyboard_shortcut_intent::caret_next:
    case keyboard_shortcut_intent::caret_home:
    case keyboard_shortcut_intent::caret_end:
    case keyboard_shortcut_intent::none:
        return navigation_policy_kind(event);
    }

    return action_route_policy_kind::caret_moved;
}

[[nodiscard]] inline keyboard_repeat_policy repeat_policy_for(
    const raw_platform_key_event& event,
    keyboard_shortcut_intent intent)
{
    if (!event.repeat) {
        return keyboard_repeat_policy::not_repeat;
    }

    switch (intent) {
    case keyboard_shortcut_intent::focus_traversal_next:
    case keyboard_shortcut_intent::focus_traversal_previous:
    case keyboard_shortcut_intent::submit:
    case keyboard_shortcut_intent::cancel:
        return keyboard_repeat_policy::ignored;
    case keyboard_shortcut_intent::caret_previous:
    case keyboard_shortcut_intent::caret_next:
    case keyboard_shortcut_intent::caret_home:
    case keyboard_shortcut_intent::caret_end:
    case keyboard_shortcut_intent::selection_previous:
    case keyboard_shortcut_intent::selection_next:
    case keyboard_shortcut_intent::select_all:
    case keyboard_shortcut_intent::delete_backward:
    case keyboard_shortcut_intent::delete_forward:
    case keyboard_shortcut_intent::none:
        return keyboard_repeat_policy::allowed;
    }

    return keyboard_repeat_policy::allowed;
}

[[nodiscard]] inline bool is_ignored_repeat(keyboard_repeat_policy policy)
{
    return policy == keyboard_repeat_policy::ignored;
}

inline void apply_keyboard_chord(
    action_route_policy_diagnostic& diagnostic,
    const raw_platform_key_event& event,
    keyboard_shortcut_intent intent,
    keyboard_repeat_policy repeat_policy)
{
    diagnostic.keyboard = keyboard_chord_diagnostic{
        .logical_key = event.logical_key,
        .key_code = event.key_code,
        .phase = event.phase,
        .modifiers = keyboard_modifier_state{
            .alt = event.alt,
            .ctrl = event.ctrl,
            .shift = event.shift,
            .meta = event.meta,
        },
        .repeat = event.repeat,
        .repeat_policy = repeat_policy,
        .intent = intent,
    };
}

} // namespace quiz_vulkan::input::detail
