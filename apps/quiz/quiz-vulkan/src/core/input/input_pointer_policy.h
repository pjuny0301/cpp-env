#pragma once

#include "core/input/input_routing_diagnostics.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace quiz_vulkan::input::detail {

[[nodiscard]] inline bool is_primary_pointer(const raw_platform_pointer_event& event)
{
    return event.button == raw_platform_pointer_button::none
        || event.button == raw_platform_pointer_button::primary;
}

[[nodiscard]] inline pointer_contact_kind pointer_contact_for(const raw_platform_pointer_event& event)
{
    return event.button == raw_platform_pointer_button::none
        ? pointer_contact_kind::touch_like
        : pointer_contact_kind::mouse_like;
}

[[nodiscard]] inline bool has_pointer_capture_state(const pointer_capture_snapshot& snapshot)
{
    return snapshot.lifecycle != pointer_capture_lifecycle::idle
        || snapshot.active
        || snapshot.tracked_pointer_count > 0;
}

[[nodiscard]] inline bool pointer_capture_changed(
    const pointer_capture_snapshot& before,
    const pointer_capture_snapshot& after)
{
    return before.lifecycle != after.lifecycle
        || before.active != after.active
        || before.pointer_id != after.pointer_id
        || before.tracked_pointer_count != after.tracked_pointer_count;
}

inline void apply_pointer_arbitration(
    action_route_policy_diagnostic& diagnostic,
    std::int32_t pointer_id,
    pointer_phase phase,
    pointer_arbitration_decision decision)
{
    diagnostic.pointer_id = pointer_id;
    diagnostic.pointer_event_phase = phase;
    diagnostic.pointer_decision = decision;
}

inline void apply_pointer_route_context(
    action_route_policy_diagnostic& diagnostic,
    const raw_platform_pointer_event& event,
    pointer_phase phase,
    pointer_arbitration_decision decision,
    const pointer_capture_snapshot& pointer_capture_before,
    const pointer_capture_snapshot& pointer_capture_after)
{
    apply_pointer_arbitration(diagnostic, event.pointer_id, phase, decision);
    diagnostic.pointer_contact = pointer_contact_for(event);
    diagnostic.tracked_pointer_count_before = pointer_capture_before.tracked_pointer_count;
    diagnostic.tracked_pointer_count_after = pointer_capture_after.tracked_pointer_count;
}

[[nodiscard]] inline bool has_gesture_kind(
    const std::vector<gesture_event>& gestures,
    std::int32_t pointer_id,
    gesture_kind kind)
{
    return std::ranges::any_of(gestures, [pointer_id, kind](const gesture_event& gesture) {
        return gesture.pointer_id == pointer_id && gesture.kind == kind;
    });
}

[[nodiscard]] inline bool is_emitless_gesture_policy(const gesture_policy_snapshot& snapshot)
{
    switch (snapshot.decision) {
    case gesture_policy_decision::swipe_rejected_distance:
    case gesture_policy_decision::swipe_rejected_cross_axis:
    case gesture_policy_decision::swipe_rejected_duration:
    case gesture_policy_decision::release_suppressed:
        return true;
    case gesture_policy_decision::none:
    case gesture_policy_decision::tracking_started:
    case gesture_policy_decision::tap_accepted:
    case gesture_policy_decision::long_press_accepted:
    case gesture_policy_decision::swipe_accepted:
    case gesture_policy_decision::drag_started:
    case gesture_policy_decision::drag_updated:
    case gesture_policy_decision::drag_released:
    case gesture_policy_decision::drag_canceled:
    case gesture_policy_decision::ignored_by_capture:
        return false;
    }

    return false;
}

[[nodiscard]] inline pointer_arbitration_decision pointer_decision_for(
    const raw_platform_pointer_event& event,
    pointer_phase phase,
    const pointer_capture_snapshot& before,
    const pointer_capture_snapshot& after,
    const std::vector<gesture_event>& gestures)
{
    if (before.active && before.pointer_id != event.pointer_id) {
        return pointer_arbitration_decision::ignored_by_capture;
    }

    if (phase == pointer_phase::down
        && has_gesture_kind(gestures, event.pointer_id, gesture_kind::drag_cancel)) {
        return pointer_arbitration_decision::restarted;
    }

    if (phase == pointer_phase::cancel
        && has_pointer_capture_state(before)
        && pointer_capture_changed(before, after)) {
        return pointer_arbitration_decision::canceled;
    }

    if (before.active
        && before.pointer_id == event.pointer_id
        && after.lifecycle == pointer_capture_lifecycle::idle
        && phase == pointer_phase::up) {
        return pointer_arbitration_decision::released;
    }

    if (!before.active && after.active && after.pointer_id == event.pointer_id) {
        return pointer_arbitration_decision::captured;
    }

    if (phase == pointer_phase::down
        && after.lifecycle == pointer_capture_lifecycle::tracking
        && pointer_capture_changed(before, after)) {
        return pointer_arbitration_decision::tracked;
    }

    return pointer_arbitration_decision::none;
}

} // namespace quiz_vulkan::input::detail
