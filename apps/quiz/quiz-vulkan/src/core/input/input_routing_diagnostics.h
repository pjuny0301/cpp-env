#pragma once

#include "core/input/gesture_recognizer.h"
#include "core/input/input_event.h"
#include "core/input/text_input_model.h"
#include "platform/platform_input_event.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace quiz_vulkan::input {

enum class pointer_arbitration_decision {
    none,
    tracked,
    captured,
    ignored_by_capture,
    canceled,
    released,
    restarted,
};

enum class pointer_contact_kind {
    unknown,
    mouse_like,
    touch_like,
};

enum class keyboard_shortcut_intent {
    none,
    focus_traversal_next,
    focus_traversal_previous,
    submit,
    cancel,
    caret_previous,
    caret_next,
    caret_home,
    caret_end,
    selection_previous,
    selection_next,
    select_all,
    delete_backward,
    delete_forward,
};

enum class keyboard_repeat_policy {
    not_repeat,
    allowed,
    ignored,
};

struct keyboard_modifier_state {
    bool alt = false;
    bool ctrl = false;
    bool shift = false;
    bool meta = false;
};

struct keyboard_chord_diagnostic {
    std::string logical_key;
    std::int32_t key_code = 0;
    raw_platform_key_phase phase = raw_platform_key_phase::down;
    keyboard_modifier_state modifiers;
    bool repeat = false;
    keyboard_repeat_policy repeat_policy = keyboard_repeat_policy::not_repeat;
    keyboard_shortcut_intent intent = keyboard_shortcut_intent::none;
};

enum class action_route_policy_kind {
    pointer_capture_reset,
    pointer_capture_arbitration,
    wheel_summary,
    gesture_route_snapshot,
    text_commit_boundary,
    text_backspace_boundary,
    text_delete_forward_boundary,
    caret_moved,
    selection_changed,
    focus_traversal_next,
    focus_traversal_previous,
    text_submit_boundary,
    keyboard_cancel_intent,
    focus_loss,
    ime_preedit,
    ime_commit,
    ime_cancel,
    ime_composition_start,
};

struct action_route_policy_diagnostic {
    action_route_policy_kind kind = action_route_policy_kind::pointer_capture_reset;
    std::int64_t timestamp_ms = 0;
    bool emits_input_event = false;
    std::size_t event_index = 0;
    std::string target_id;
    std::size_t text_byte_count = 0;
    std::size_t text_byte_count_before = 0;
    std::size_t text_byte_count_after = 0;
    text_range caret_before;
    text_range caret_after;
    bool had_selection_before = false;
    bool has_selection_after = false;
    text_range selection_before;
    text_range selection_after;
    normalized_input_event_summary normalized_event;
    ime_composition_state composition;
    gesture_policy_snapshot gesture_policy;
    keyboard_chord_diagnostic keyboard;
    pointer_capture_snapshot pointer_capture_before;
    pointer_capture_snapshot pointer_capture_after;
    pointer_arbitration_decision pointer_decision = pointer_arbitration_decision::none;
    pointer_phase pointer_event_phase = pointer_phase::down;
    pointer_contact_kind pointer_contact = pointer_contact_kind::unknown;
    std::int32_t pointer_id = 0;
    std::size_t tracked_pointer_count_before = 0;
    std::size_t tracked_pointer_count_after = 0;
};

struct normalized_input_event_kind_counts {
    std::size_t tap = 0;
    std::size_t long_press = 0;
    std::size_t swipe_left = 0;
    std::size_t swipe_right = 0;
    std::size_t drag_start = 0;
    std::size_t drag_update = 0;
    std::size_t drag_end = 0;
    std::size_t drag_cancel = 0;
    std::size_t wheel = 0;
};

struct input_route_kind_counts {
    std::size_t pointer = 0;
    std::size_t text = 0;
    std::size_t ime = 0;
    std::size_t focus = 0;
    std::size_t wheel = 0;
    std::size_t total = 0;
};

struct input_diagnostic_summary {
    normalized_input_event_kind_counts normalized_events;
    input_route_kind_counts routes;
    std::size_t normalized_event_count = 0;
    bool pointer_capture_ended_cleanly = true;
    bool focus_ended_cleanly = true;
    bool preedit_ended_cleanly = true;
};

struct input_routing_diagnostics {
    std::vector<normalized_input_event_summary> normalized_events;
    std::vector<action_route_policy_diagnostic> action_routes;
    pointer_capture_snapshot pointer_capture;
    input_diagnostic_summary summary;
};

inline void count_input_diagnostic_normalized_event(
    input_diagnostic_summary& summary,
    input_event_summary_kind kind)
{
    ++summary.normalized_event_count;
    switch (kind) {
    case input_event_summary_kind::tap:
        ++summary.normalized_events.tap;
        return;
    case input_event_summary_kind::long_press:
        ++summary.normalized_events.long_press;
        return;
    case input_event_summary_kind::swipe_left:
        ++summary.normalized_events.swipe_left;
        return;
    case input_event_summary_kind::swipe_right:
        ++summary.normalized_events.swipe_right;
        return;
    case input_event_summary_kind::drag_start:
        ++summary.normalized_events.drag_start;
        return;
    case input_event_summary_kind::drag_update:
        ++summary.normalized_events.drag_update;
        return;
    case input_event_summary_kind::drag_end:
        ++summary.normalized_events.drag_end;
        return;
    case input_event_summary_kind::drag_cancel:
        ++summary.normalized_events.drag_cancel;
        return;
    case input_event_summary_kind::wheel:
        ++summary.normalized_events.wheel;
        return;
    }
}

inline void count_input_diagnostic_route(
    input_diagnostic_summary& summary,
    action_route_policy_kind kind)
{
    ++summary.routes.total;
    switch (kind) {
    case action_route_policy_kind::pointer_capture_reset:
    case action_route_policy_kind::pointer_capture_arbitration:
    case action_route_policy_kind::gesture_route_snapshot:
        ++summary.routes.pointer;
        return;
    case action_route_policy_kind::wheel_summary:
        ++summary.routes.wheel;
        return;
    case action_route_policy_kind::text_commit_boundary:
    case action_route_policy_kind::text_backspace_boundary:
    case action_route_policy_kind::text_delete_forward_boundary:
    case action_route_policy_kind::caret_moved:
    case action_route_policy_kind::selection_changed:
    case action_route_policy_kind::text_submit_boundary:
    case action_route_policy_kind::keyboard_cancel_intent:
        ++summary.routes.text;
        return;
    case action_route_policy_kind::focus_traversal_next:
    case action_route_policy_kind::focus_traversal_previous:
    case action_route_policy_kind::focus_loss:
        ++summary.routes.focus;
        return;
    case action_route_policy_kind::ime_preedit:
    case action_route_policy_kind::ime_commit:
    case action_route_policy_kind::ime_cancel:
    case action_route_policy_kind::ime_composition_start:
        ++summary.routes.ime;
        return;
    }
}

[[nodiscard]] inline bool input_diagnostic_pointer_capture_has_state(
    const pointer_capture_snapshot& snapshot)
{
    return snapshot.lifecycle != pointer_capture_lifecycle::idle
        || snapshot.active
        || snapshot.tracked_pointer_count > 0;
}

[[nodiscard]] inline input_diagnostic_summary summarize_input_routing_diagnostics(
    const input_routing_diagnostics& diagnostics,
    const text_input_model& text)
{
    input_diagnostic_summary summary{};
    for (const normalized_input_event_summary& event : diagnostics.normalized_events) {
        count_input_diagnostic_normalized_event(summary, event.kind);
    }
    for (const action_route_policy_diagnostic& route : diagnostics.action_routes) {
        count_input_diagnostic_route(summary, route.kind);
    }

    summary.pointer_capture_ended_cleanly =
        !input_diagnostic_pointer_capture_has_state(diagnostics.pointer_capture);
    summary.focus_ended_cleanly = text.has_focus() || text.focus_id().empty();
    summary.preedit_ended_cleanly = !text.ime_composition().active && text.preedit_text().empty();
    return summary;
}

inline void accumulate_input_diagnostic_summary(
    input_diagnostic_summary& target,
    const input_diagnostic_summary& source)
{
    target.normalized_events.tap += source.normalized_events.tap;
    target.normalized_events.long_press += source.normalized_events.long_press;
    target.normalized_events.swipe_left += source.normalized_events.swipe_left;
    target.normalized_events.swipe_right += source.normalized_events.swipe_right;
    target.normalized_events.drag_start += source.normalized_events.drag_start;
    target.normalized_events.drag_update += source.normalized_events.drag_update;
    target.normalized_events.drag_end += source.normalized_events.drag_end;
    target.normalized_events.drag_cancel += source.normalized_events.drag_cancel;
    target.normalized_events.wheel += source.normalized_events.wheel;
    target.routes.pointer += source.routes.pointer;
    target.routes.text += source.routes.text;
    target.routes.ime += source.routes.ime;
    target.routes.focus += source.routes.focus;
    target.routes.wheel += source.routes.wheel;
    target.routes.total += source.routes.total;
    target.normalized_event_count += source.normalized_event_count;
    target.pointer_capture_ended_cleanly = source.pointer_capture_ended_cleanly;
    target.focus_ended_cleanly = source.focus_ended_cleanly;
    target.preedit_ended_cleanly = source.preedit_ended_cleanly;
}

} // namespace quiz_vulkan::input
