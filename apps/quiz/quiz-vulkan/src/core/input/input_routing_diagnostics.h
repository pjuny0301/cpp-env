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

struct input_routing_count_delta {
    std::size_t before_count = 0;
    std::size_t after_count = 0;
    std::int64_t delta = 0;
    bool changed = false;
};

struct input_routing_bool_delta {
    bool before_value = false;
    bool after_value = false;
    bool changed = false;
};

struct input_routing_pointer_capture_delta {
    pointer_capture_snapshot before_capture;
    pointer_capture_snapshot after_capture;
    bool before_has_state = false;
    bool after_has_state = false;
    input_routing_bool_delta active;
    input_routing_count_delta tracked_pointer_count;
    bool lifecycle_changed = false;
    bool pointer_id_changed = false;
    bool changed = false;
};

struct normalized_input_event_kind_count_deltas {
    input_routing_count_delta tap;
    input_routing_count_delta long_press;
    input_routing_count_delta swipe_left;
    input_routing_count_delta swipe_right;
    input_routing_count_delta drag_start;
    input_routing_count_delta drag_update;
    input_routing_count_delta drag_end;
    input_routing_count_delta drag_cancel;
    input_routing_count_delta wheel;
};

struct input_route_kind_count_deltas {
    input_routing_count_delta pointer;
    input_routing_count_delta text;
    input_routing_count_delta ime;
    input_routing_count_delta focus;
    input_routing_count_delta wheel;
    input_routing_count_delta total;
};

struct action_route_policy_kind_counts {
    std::size_t pointer_capture_reset = 0;
    std::size_t pointer_capture_arbitration = 0;
    std::size_t wheel_summary = 0;
    std::size_t gesture_route_snapshot = 0;
    std::size_t text_commit_boundary = 0;
    std::size_t text_backspace_boundary = 0;
    std::size_t text_delete_forward_boundary = 0;
    std::size_t caret_moved = 0;
    std::size_t selection_changed = 0;
    std::size_t focus_traversal_next = 0;
    std::size_t focus_traversal_previous = 0;
    std::size_t text_submit_boundary = 0;
    std::size_t keyboard_cancel_intent = 0;
    std::size_t focus_loss = 0;
    std::size_t ime_preedit = 0;
    std::size_t ime_commit = 0;
    std::size_t ime_cancel = 0;
    std::size_t ime_composition_start = 0;
};

struct action_route_policy_kind_count_deltas {
    input_routing_count_delta pointer_capture_reset;
    input_routing_count_delta pointer_capture_arbitration;
    input_routing_count_delta wheel_summary;
    input_routing_count_delta gesture_route_snapshot;
    input_routing_count_delta text_commit_boundary;
    input_routing_count_delta text_backspace_boundary;
    input_routing_count_delta text_delete_forward_boundary;
    input_routing_count_delta caret_moved;
    input_routing_count_delta selection_changed;
    input_routing_count_delta focus_traversal_next;
    input_routing_count_delta focus_traversal_previous;
    input_routing_count_delta text_submit_boundary;
    input_routing_count_delta keyboard_cancel_intent;
    input_routing_count_delta focus_loss;
    input_routing_count_delta ime_preedit;
    input_routing_count_delta ime_commit;
    input_routing_count_delta ime_cancel;
    input_routing_count_delta ime_composition_start;
};

struct input_routing_keyboard_intent_counts {
    std::size_t none = 0;
    std::size_t focus_traversal_next = 0;
    std::size_t focus_traversal_previous = 0;
    std::size_t submit = 0;
    std::size_t cancel = 0;
    std::size_t caret_previous = 0;
    std::size_t caret_next = 0;
    std::size_t caret_home = 0;
    std::size_t caret_end = 0;
    std::size_t selection_previous = 0;
    std::size_t selection_next = 0;
    std::size_t select_all = 0;
    std::size_t delete_backward = 0;
    std::size_t delete_forward = 0;
};

struct input_routing_keyboard_repeat_policy_counts {
    std::size_t not_repeat = 0;
    std::size_t allowed = 0;
    std::size_t ignored = 0;
};

struct input_routing_keyboard_route_counts {
    input_routing_keyboard_intent_counts intents;
    input_routing_keyboard_repeat_policy_counts repeat_policies;
    std::size_t total = 0;
    std::size_t emitted_input_event_routes = 0;
    std::size_t diagnostic_only_routes = 0;
};

struct input_routing_keyboard_intent_count_deltas {
    input_routing_count_delta none;
    input_routing_count_delta focus_traversal_next;
    input_routing_count_delta focus_traversal_previous;
    input_routing_count_delta submit;
    input_routing_count_delta cancel;
    input_routing_count_delta caret_previous;
    input_routing_count_delta caret_next;
    input_routing_count_delta caret_home;
    input_routing_count_delta caret_end;
    input_routing_count_delta selection_previous;
    input_routing_count_delta selection_next;
    input_routing_count_delta select_all;
    input_routing_count_delta delete_backward;
    input_routing_count_delta delete_forward;
};

struct input_routing_keyboard_repeat_policy_count_deltas {
    input_routing_count_delta not_repeat;
    input_routing_count_delta allowed;
    input_routing_count_delta ignored;
};

struct input_routing_keyboard_route_count_deltas {
    input_routing_keyboard_intent_count_deltas intents;
    input_routing_keyboard_repeat_policy_count_deltas repeat_policies;
    input_routing_count_delta total;
    input_routing_count_delta emitted_input_event_routes;
    input_routing_count_delta diagnostic_only_routes;
    bool changed = false;
};

struct input_routing_diagnostics_diff {
    input_routing_count_delta normalized_event_count;
    input_routing_count_delta normalized_event_summary_count;
    input_routing_count_delta action_route_count;
    normalized_input_event_kind_count_deltas normalized_events;
    input_route_kind_count_deltas routes;
    action_route_policy_kind_count_deltas action_routes;
    input_routing_keyboard_route_count_deltas keyboard_routes;
    input_routing_pointer_capture_delta pointer_capture;
    input_routing_bool_delta pointer_capture_ended_cleanly;
    input_routing_bool_delta focus_ended_cleanly;
    input_routing_bool_delta preedit_ended_cleanly;
    bool normalized_events_changed = false;
    bool action_routes_changed = false;
    bool keyboard_routes_changed = false;
    bool pointer_capture_changed = false;
    bool clean_state_changed = false;
    bool changed = false;
};

[[nodiscard]] inline std::int64_t input_routing_size_delta(
    std::size_t before_count,
    std::size_t after_count)
{
    if (after_count >= before_count) {
        return static_cast<std::int64_t>(after_count - before_count);
    }
    return -static_cast<std::int64_t>(before_count - after_count);
}

[[nodiscard]] inline input_routing_count_delta diff_input_routing_count(
    std::size_t before_count,
    std::size_t after_count)
{
    return input_routing_count_delta{
        .before_count = before_count,
        .after_count = after_count,
        .delta = input_routing_size_delta(before_count, after_count),
        .changed = before_count != after_count,
    };
}

[[nodiscard]] inline input_routing_bool_delta diff_input_routing_bool(
    bool before_value,
    bool after_value)
{
    return input_routing_bool_delta{
        .before_value = before_value,
        .after_value = after_value,
        .changed = before_value != after_value,
    };
}

[[nodiscard]] inline bool input_routing_keyboard_chord_present(
    const keyboard_chord_diagnostic& chord)
{
    return !chord.logical_key.empty()
        || chord.key_code != 0
        || chord.modifiers.alt
        || chord.modifiers.ctrl
        || chord.modifiers.shift
        || chord.modifiers.meta
        || chord.repeat
        || chord.intent != keyboard_shortcut_intent::none;
}

inline void count_input_routing_action_route_policy_kind(
    action_route_policy_kind_counts& counts,
    action_route_policy_kind kind)
{
    switch (kind) {
    case action_route_policy_kind::pointer_capture_reset:
        ++counts.pointer_capture_reset;
        return;
    case action_route_policy_kind::pointer_capture_arbitration:
        ++counts.pointer_capture_arbitration;
        return;
    case action_route_policy_kind::wheel_summary:
        ++counts.wheel_summary;
        return;
    case action_route_policy_kind::gesture_route_snapshot:
        ++counts.gesture_route_snapshot;
        return;
    case action_route_policy_kind::text_commit_boundary:
        ++counts.text_commit_boundary;
        return;
    case action_route_policy_kind::text_backspace_boundary:
        ++counts.text_backspace_boundary;
        return;
    case action_route_policy_kind::text_delete_forward_boundary:
        ++counts.text_delete_forward_boundary;
        return;
    case action_route_policy_kind::caret_moved:
        ++counts.caret_moved;
        return;
    case action_route_policy_kind::selection_changed:
        ++counts.selection_changed;
        return;
    case action_route_policy_kind::focus_traversal_next:
        ++counts.focus_traversal_next;
        return;
    case action_route_policy_kind::focus_traversal_previous:
        ++counts.focus_traversal_previous;
        return;
    case action_route_policy_kind::text_submit_boundary:
        ++counts.text_submit_boundary;
        return;
    case action_route_policy_kind::keyboard_cancel_intent:
        ++counts.keyboard_cancel_intent;
        return;
    case action_route_policy_kind::focus_loss:
        ++counts.focus_loss;
        return;
    case action_route_policy_kind::ime_preedit:
        ++counts.ime_preedit;
        return;
    case action_route_policy_kind::ime_commit:
        ++counts.ime_commit;
        return;
    case action_route_policy_kind::ime_cancel:
        ++counts.ime_cancel;
        return;
    case action_route_policy_kind::ime_composition_start:
        ++counts.ime_composition_start;
        return;
    }
}

[[nodiscard]] inline action_route_policy_kind_counts summarize_input_routing_action_route_policy_kinds(
    const input_routing_diagnostics& diagnostics)
{
    action_route_policy_kind_counts counts{};
    for (const action_route_policy_diagnostic& route : diagnostics.action_routes) {
        count_input_routing_action_route_policy_kind(counts, route.kind);
    }
    return counts;
}

inline void count_input_routing_keyboard_intent(
    input_routing_keyboard_intent_counts& counts,
    keyboard_shortcut_intent intent)
{
    switch (intent) {
    case keyboard_shortcut_intent::none:
        ++counts.none;
        return;
    case keyboard_shortcut_intent::focus_traversal_next:
        ++counts.focus_traversal_next;
        return;
    case keyboard_shortcut_intent::focus_traversal_previous:
        ++counts.focus_traversal_previous;
        return;
    case keyboard_shortcut_intent::submit:
        ++counts.submit;
        return;
    case keyboard_shortcut_intent::cancel:
        ++counts.cancel;
        return;
    case keyboard_shortcut_intent::caret_previous:
        ++counts.caret_previous;
        return;
    case keyboard_shortcut_intent::caret_next:
        ++counts.caret_next;
        return;
    case keyboard_shortcut_intent::caret_home:
        ++counts.caret_home;
        return;
    case keyboard_shortcut_intent::caret_end:
        ++counts.caret_end;
        return;
    case keyboard_shortcut_intent::selection_previous:
        ++counts.selection_previous;
        return;
    case keyboard_shortcut_intent::selection_next:
        ++counts.selection_next;
        return;
    case keyboard_shortcut_intent::select_all:
        ++counts.select_all;
        return;
    case keyboard_shortcut_intent::delete_backward:
        ++counts.delete_backward;
        return;
    case keyboard_shortcut_intent::delete_forward:
        ++counts.delete_forward;
        return;
    }
}

inline void count_input_routing_keyboard_repeat_policy(
    input_routing_keyboard_repeat_policy_counts& counts,
    keyboard_repeat_policy policy)
{
    switch (policy) {
    case keyboard_repeat_policy::not_repeat:
        ++counts.not_repeat;
        return;
    case keyboard_repeat_policy::allowed:
        ++counts.allowed;
        return;
    case keyboard_repeat_policy::ignored:
        ++counts.ignored;
        return;
    }
}

[[nodiscard]] inline input_routing_keyboard_route_counts summarize_input_routing_keyboard_routes(
    const input_routing_diagnostics& diagnostics)
{
    input_routing_keyboard_route_counts counts{};
    for (const action_route_policy_diagnostic& route : diagnostics.action_routes) {
        if (!input_routing_keyboard_chord_present(route.keyboard)) {
            continue;
        }

        ++counts.total;
        if (route.emits_input_event) {
            ++counts.emitted_input_event_routes;
        } else {
            ++counts.diagnostic_only_routes;
        }
        count_input_routing_keyboard_intent(counts.intents, route.keyboard.intent);
        count_input_routing_keyboard_repeat_policy(counts.repeat_policies, route.keyboard.repeat_policy);
    }
    return counts;
}

[[nodiscard]] inline bool input_routing_normalized_event_deltas_changed(
    const normalized_input_event_kind_count_deltas& deltas)
{
    return deltas.tap.changed
        || deltas.long_press.changed
        || deltas.swipe_left.changed
        || deltas.swipe_right.changed
        || deltas.drag_start.changed
        || deltas.drag_update.changed
        || deltas.drag_end.changed
        || deltas.drag_cancel.changed
        || deltas.wheel.changed;
}

[[nodiscard]] inline bool input_routing_route_kind_deltas_changed(
    const input_route_kind_count_deltas& deltas)
{
    return deltas.pointer.changed
        || deltas.text.changed
        || deltas.ime.changed
        || deltas.focus.changed
        || deltas.wheel.changed
        || deltas.total.changed;
}

[[nodiscard]] inline bool input_routing_action_route_deltas_changed(
    const action_route_policy_kind_count_deltas& deltas)
{
    return deltas.pointer_capture_reset.changed
        || deltas.pointer_capture_arbitration.changed
        || deltas.wheel_summary.changed
        || deltas.gesture_route_snapshot.changed
        || deltas.text_commit_boundary.changed
        || deltas.text_backspace_boundary.changed
        || deltas.text_delete_forward_boundary.changed
        || deltas.caret_moved.changed
        || deltas.selection_changed.changed
        || deltas.focus_traversal_next.changed
        || deltas.focus_traversal_previous.changed
        || deltas.text_submit_boundary.changed
        || deltas.keyboard_cancel_intent.changed
        || deltas.focus_loss.changed
        || deltas.ime_preedit.changed
        || deltas.ime_commit.changed
        || deltas.ime_cancel.changed
        || deltas.ime_composition_start.changed;
}

[[nodiscard]] inline bool input_routing_keyboard_intent_deltas_changed(
    const input_routing_keyboard_intent_count_deltas& deltas)
{
    return deltas.none.changed
        || deltas.focus_traversal_next.changed
        || deltas.focus_traversal_previous.changed
        || deltas.submit.changed
        || deltas.cancel.changed
        || deltas.caret_previous.changed
        || deltas.caret_next.changed
        || deltas.caret_home.changed
        || deltas.caret_end.changed
        || deltas.selection_previous.changed
        || deltas.selection_next.changed
        || deltas.select_all.changed
        || deltas.delete_backward.changed
        || deltas.delete_forward.changed;
}

[[nodiscard]] inline bool input_routing_keyboard_repeat_policy_deltas_changed(
    const input_routing_keyboard_repeat_policy_count_deltas& deltas)
{
    return deltas.not_repeat.changed
        || deltas.allowed.changed
        || deltas.ignored.changed;
}

[[nodiscard]] inline input_routing_pointer_capture_delta diff_input_routing_pointer_capture(
    pointer_capture_snapshot before,
    pointer_capture_snapshot after)
{
    input_routing_pointer_capture_delta diff{
        .before_capture = before,
        .after_capture = after,
        .before_has_state = input_diagnostic_pointer_capture_has_state(before),
        .after_has_state = input_diagnostic_pointer_capture_has_state(after),
        .active = diff_input_routing_bool(before.active, after.active),
        .tracked_pointer_count =
            diff_input_routing_count(before.tracked_pointer_count, after.tracked_pointer_count),
        .lifecycle_changed = before.lifecycle != after.lifecycle,
        .pointer_id_changed = before.pointer_id != after.pointer_id,
    };
    diff.changed = diff.before_has_state != diff.after_has_state
        || diff.active.changed
        || diff.tracked_pointer_count.changed
        || diff.lifecycle_changed
        || diff.pointer_id_changed;
    return diff;
}

[[nodiscard]] inline normalized_input_event_kind_count_deltas diff_normalized_input_event_kind_counts(
    const normalized_input_event_kind_counts& before,
    const normalized_input_event_kind_counts& after)
{
    return normalized_input_event_kind_count_deltas{
        .tap = diff_input_routing_count(before.tap, after.tap),
        .long_press = diff_input_routing_count(before.long_press, after.long_press),
        .swipe_left = diff_input_routing_count(before.swipe_left, after.swipe_left),
        .swipe_right = diff_input_routing_count(before.swipe_right, after.swipe_right),
        .drag_start = diff_input_routing_count(before.drag_start, after.drag_start),
        .drag_update = diff_input_routing_count(before.drag_update, after.drag_update),
        .drag_end = diff_input_routing_count(before.drag_end, after.drag_end),
        .drag_cancel = diff_input_routing_count(before.drag_cancel, after.drag_cancel),
        .wheel = diff_input_routing_count(before.wheel, after.wheel),
    };
}

[[nodiscard]] inline input_route_kind_count_deltas diff_input_route_kind_counts(
    const input_route_kind_counts& before,
    const input_route_kind_counts& after)
{
    return input_route_kind_count_deltas{
        .pointer = diff_input_routing_count(before.pointer, after.pointer),
        .text = diff_input_routing_count(before.text, after.text),
        .ime = diff_input_routing_count(before.ime, after.ime),
        .focus = diff_input_routing_count(before.focus, after.focus),
        .wheel = diff_input_routing_count(before.wheel, after.wheel),
        .total = diff_input_routing_count(before.total, after.total),
    };
}

[[nodiscard]] inline action_route_policy_kind_count_deltas diff_action_route_policy_kind_counts(
    const action_route_policy_kind_counts& before,
    const action_route_policy_kind_counts& after)
{
    return action_route_policy_kind_count_deltas{
        .pointer_capture_reset =
            diff_input_routing_count(before.pointer_capture_reset, after.pointer_capture_reset),
        .pointer_capture_arbitration =
            diff_input_routing_count(before.pointer_capture_arbitration, after.pointer_capture_arbitration),
        .wheel_summary = diff_input_routing_count(before.wheel_summary, after.wheel_summary),
        .gesture_route_snapshot =
            diff_input_routing_count(before.gesture_route_snapshot, after.gesture_route_snapshot),
        .text_commit_boundary =
            diff_input_routing_count(before.text_commit_boundary, after.text_commit_boundary),
        .text_backspace_boundary =
            diff_input_routing_count(before.text_backspace_boundary, after.text_backspace_boundary),
        .text_delete_forward_boundary =
            diff_input_routing_count(before.text_delete_forward_boundary, after.text_delete_forward_boundary),
        .caret_moved = diff_input_routing_count(before.caret_moved, after.caret_moved),
        .selection_changed = diff_input_routing_count(before.selection_changed, after.selection_changed),
        .focus_traversal_next =
            diff_input_routing_count(before.focus_traversal_next, after.focus_traversal_next),
        .focus_traversal_previous =
            diff_input_routing_count(before.focus_traversal_previous, after.focus_traversal_previous),
        .text_submit_boundary =
            diff_input_routing_count(before.text_submit_boundary, after.text_submit_boundary),
        .keyboard_cancel_intent =
            diff_input_routing_count(before.keyboard_cancel_intent, after.keyboard_cancel_intent),
        .focus_loss = diff_input_routing_count(before.focus_loss, after.focus_loss),
        .ime_preedit = diff_input_routing_count(before.ime_preedit, after.ime_preedit),
        .ime_commit = diff_input_routing_count(before.ime_commit, after.ime_commit),
        .ime_cancel = diff_input_routing_count(before.ime_cancel, after.ime_cancel),
        .ime_composition_start =
            diff_input_routing_count(before.ime_composition_start, after.ime_composition_start),
    };
}

[[nodiscard]] inline input_routing_keyboard_route_count_deltas diff_input_routing_keyboard_route_counts(
    const input_routing_keyboard_route_counts& before,
    const input_routing_keyboard_route_counts& after)
{
    input_routing_keyboard_route_count_deltas diff{
        .intents = input_routing_keyboard_intent_count_deltas{
            .none = diff_input_routing_count(before.intents.none, after.intents.none),
            .focus_traversal_next =
                diff_input_routing_count(
                    before.intents.focus_traversal_next,
                    after.intents.focus_traversal_next),
            .focus_traversal_previous =
                diff_input_routing_count(
                    before.intents.focus_traversal_previous,
                    after.intents.focus_traversal_previous),
            .submit = diff_input_routing_count(before.intents.submit, after.intents.submit),
            .cancel = diff_input_routing_count(before.intents.cancel, after.intents.cancel),
            .caret_previous = diff_input_routing_count(before.intents.caret_previous, after.intents.caret_previous),
            .caret_next = diff_input_routing_count(before.intents.caret_next, after.intents.caret_next),
            .caret_home = diff_input_routing_count(before.intents.caret_home, after.intents.caret_home),
            .caret_end = diff_input_routing_count(before.intents.caret_end, after.intents.caret_end),
            .selection_previous =
                diff_input_routing_count(before.intents.selection_previous, after.intents.selection_previous),
            .selection_next = diff_input_routing_count(before.intents.selection_next, after.intents.selection_next),
            .select_all = diff_input_routing_count(before.intents.select_all, after.intents.select_all),
            .delete_backward =
                diff_input_routing_count(before.intents.delete_backward, after.intents.delete_backward),
            .delete_forward = diff_input_routing_count(before.intents.delete_forward, after.intents.delete_forward),
        },
        .repeat_policies = input_routing_keyboard_repeat_policy_count_deltas{
            .not_repeat =
                diff_input_routing_count(before.repeat_policies.not_repeat, after.repeat_policies.not_repeat),
            .allowed = diff_input_routing_count(before.repeat_policies.allowed, after.repeat_policies.allowed),
            .ignored = diff_input_routing_count(before.repeat_policies.ignored, after.repeat_policies.ignored),
        },
        .total = diff_input_routing_count(before.total, after.total),
        .emitted_input_event_routes =
            diff_input_routing_count(before.emitted_input_event_routes, after.emitted_input_event_routes),
        .diagnostic_only_routes =
            diff_input_routing_count(before.diagnostic_only_routes, after.diagnostic_only_routes),
    };
    diff.changed = diff.total.changed
        || diff.emitted_input_event_routes.changed
        || diff.diagnostic_only_routes.changed
        || input_routing_keyboard_intent_deltas_changed(diff.intents)
        || input_routing_keyboard_repeat_policy_deltas_changed(diff.repeat_policies);
    return diff;
}

[[nodiscard]] inline input_routing_diagnostics_diff diff_input_routing_diagnostics(
    const input_routing_diagnostics& before,
    const input_routing_diagnostics& after)
{
    const action_route_policy_kind_counts before_action_routes =
        summarize_input_routing_action_route_policy_kinds(before);
    const action_route_policy_kind_counts after_action_routes =
        summarize_input_routing_action_route_policy_kinds(after);
    const input_routing_keyboard_route_counts before_keyboard_routes =
        summarize_input_routing_keyboard_routes(before);
    const input_routing_keyboard_route_counts after_keyboard_routes =
        summarize_input_routing_keyboard_routes(after);

    input_routing_diagnostics_diff diff{
        .normalized_event_count =
            diff_input_routing_count(
                before.summary.normalized_event_count,
                after.summary.normalized_event_count),
        .normalized_event_summary_count =
            diff_input_routing_count(before.normalized_events.size(), after.normalized_events.size()),
        .action_route_count = diff_input_routing_count(before.action_routes.size(), after.action_routes.size()),
        .normalized_events =
            diff_normalized_input_event_kind_counts(
                before.summary.normalized_events,
                after.summary.normalized_events),
        .routes = diff_input_route_kind_counts(before.summary.routes, after.summary.routes),
        .action_routes = diff_action_route_policy_kind_counts(before_action_routes, after_action_routes),
        .keyboard_routes = diff_input_routing_keyboard_route_counts(before_keyboard_routes, after_keyboard_routes),
        .pointer_capture = diff_input_routing_pointer_capture(before.pointer_capture, after.pointer_capture),
        .pointer_capture_ended_cleanly =
            diff_input_routing_bool(
                before.summary.pointer_capture_ended_cleanly,
                after.summary.pointer_capture_ended_cleanly),
        .focus_ended_cleanly =
            diff_input_routing_bool(before.summary.focus_ended_cleanly, after.summary.focus_ended_cleanly),
        .preedit_ended_cleanly =
            diff_input_routing_bool(before.summary.preedit_ended_cleanly, after.summary.preedit_ended_cleanly),
    };
    diff.normalized_events_changed =
        diff.normalized_event_count.changed
        || diff.normalized_event_summary_count.changed
        || input_routing_normalized_event_deltas_changed(diff.normalized_events);
    diff.action_routes_changed =
        diff.action_route_count.changed
        || input_routing_route_kind_deltas_changed(diff.routes)
        || input_routing_action_route_deltas_changed(diff.action_routes);
    diff.keyboard_routes_changed = diff.keyboard_routes.changed;
    diff.pointer_capture_changed = diff.pointer_capture.changed;
    diff.clean_state_changed =
        diff.pointer_capture_ended_cleanly.changed
        || diff.focus_ended_cleanly.changed
        || diff.preedit_ended_cleanly.changed;
    diff.changed =
        diff.normalized_events_changed
        || diff.action_routes_changed
        || diff.keyboard_routes_changed
        || diff.pointer_capture_changed
        || diff.clean_state_changed;
    return diff;
}

} // namespace quiz_vulkan::input
