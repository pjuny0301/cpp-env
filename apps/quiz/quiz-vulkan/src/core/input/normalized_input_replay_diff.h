#pragma once

#include "core/input/normalized_input_replay.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

namespace quiz_vulkan::input {

struct normalized_input_replay_count_delta {
    std::size_t before_count = 0;
    std::size_t after_count = 0;
    std::int64_t delta = 0;
    bool changed = false;
};

struct normalized_input_replay_bool_delta {
    bool before_value = false;
    bool after_value = false;
    bool changed = false;
};

struct normalized_input_replay_range_delta {
    text_range before_range;
    text_range after_range;
    bool changed = false;
};

struct normalized_input_replay_string_delta {
    std::string before_value;
    std::string after_value;
    std::int64_t byte_delta = 0;
    bool changed = false;
};

struct normalized_input_replay_pointer_capture_delta {
    pointer_capture_snapshot before_capture;
    pointer_capture_snapshot after_capture;
    bool before_clean = true;
    bool after_clean = true;
    bool changed = false;
    bool clean_changed = false;
};

struct normalized_input_replay_final_state_diff {
    normalized_input_replay_bool_delta has_focus;
    normalized_input_replay_string_delta focus_id;
    normalized_input_replay_string_delta text;
    normalized_input_replay_string_delta display_text;
    normalized_input_replay_string_delta preedit_text;
    normalized_input_replay_range_delta caret;
    normalized_input_replay_bool_delta has_selection;
    normalized_input_replay_range_delta selection;
    normalized_input_replay_bool_delta focus_clean;
    normalized_input_replay_bool_delta preedit_clean;
    normalized_input_replay_pointer_capture_delta pointer_capture;
    text_input_presentation_diff text_presentation;
    bool focus_changed = false;
    bool caret_changed = false;
    bool selection_changed = false;
    bool text_changed = false;
    bool display_text_changed = false;
    bool preedit_changed = false;
    bool pointer_capture_changed = false;
    bool text_presentation_changed = false;
    bool changed = false;
};

struct normalized_input_replay_keyboard_intent_count_deltas {
    normalized_input_replay_count_delta none;
    normalized_input_replay_count_delta focus_traversal_next;
    normalized_input_replay_count_delta focus_traversal_previous;
    normalized_input_replay_count_delta submit;
    normalized_input_replay_count_delta cancel;
    normalized_input_replay_count_delta caret_previous;
    normalized_input_replay_count_delta caret_next;
    normalized_input_replay_count_delta caret_home;
    normalized_input_replay_count_delta caret_end;
    normalized_input_replay_count_delta selection_previous;
    normalized_input_replay_count_delta selection_next;
    normalized_input_replay_count_delta select_all;
    normalized_input_replay_count_delta delete_backward;
    normalized_input_replay_count_delta delete_forward;
};

struct normalized_input_replay_keyboard_modifier_count_deltas {
    normalized_input_replay_count_delta unmodified;
    normalized_input_replay_count_delta alt;
    normalized_input_replay_count_delta ctrl;
    normalized_input_replay_count_delta shift;
    normalized_input_replay_count_delta meta;
};

struct normalized_input_replay_keyboard_repeat_policy_count_deltas {
    normalized_input_replay_count_delta not_repeat;
    normalized_input_replay_count_delta allowed;
    normalized_input_replay_count_delta ignored;
};

struct normalized_input_replay_keyboard_diff {
    normalized_input_replay_count_delta chord_count;
    normalized_input_replay_count_delta total;
    normalized_input_replay_count_delta emitted_input_event_routes;
    normalized_input_replay_count_delta diagnostic_only_routes;
    normalized_input_replay_keyboard_intent_count_deltas intents;
    normalized_input_replay_keyboard_modifier_count_deltas modifiers;
    normalized_input_replay_keyboard_repeat_policy_count_deltas repeat_policies;
    bool changed = false;
};

struct normalized_input_replay_pointer_timeline_count_deltas {
    normalized_input_replay_count_delta pointer_capture_arbitration;
    normalized_input_replay_count_delta pointer_capture_reset;
    normalized_input_replay_count_delta tap;
    normalized_input_replay_count_delta long_press;
    normalized_input_replay_count_delta swipe_left;
    normalized_input_replay_count_delta swipe_right;
    normalized_input_replay_count_delta drag_start;
    normalized_input_replay_count_delta drag_update;
    normalized_input_replay_count_delta drag_end;
    normalized_input_replay_count_delta drag_cancel;
    normalized_input_replay_count_delta wheel;
    normalized_input_replay_count_delta gesture_suppressed;
};

struct normalized_input_replay_pointer_diff {
    normalized_input_replay_count_delta timeline_entries;
    normalized_input_replay_count_delta total;
    normalized_input_replay_count_delta emitted_input_event_routes;
    normalized_input_replay_count_delta diagnostic_only_routes;
    normalized_input_replay_count_delta capture_transition_count;
    normalized_input_replay_count_delta wheel_routes;
    normalized_input_replay_count_delta pointer_id_count;
    normalized_input_replay_count_delta mouse_pointer_id_count;
    normalized_input_replay_count_delta touch_pointer_id_count;
    normalized_input_replay_pointer_timeline_count_deltas kinds;
    normalized_input_replay_bool_delta saw_multipointer_touch;
    normalized_input_replay_pointer_capture_delta final_capture;
    bool timeline_changed = false;
    bool capture_changed = false;
    bool changed = false;
};

struct normalized_input_replay_ime_phase_count_deltas {
    normalized_input_replay_count_delta composition_start;
    normalized_input_replay_count_delta preedit_update;
    normalized_input_replay_count_delta commit;
    normalized_input_replay_count_delta cancel;
};

struct normalized_input_replay_ime_diff {
    normalized_input_replay_count_delta timeline_entries;
    normalized_input_replay_count_delta total;
    normalized_input_replay_count_delta emitted_input_event_routes;
    normalized_input_replay_count_delta diagnostic_only_routes;
    normalized_input_replay_ime_phase_count_deltas phases;
    normalized_input_replay_bool_delta all_preedit_text_valid;
    normalized_input_replay_bool_delta all_preedit_ranges_valid;
    normalized_input_replay_bool_delta stale_preedit_cleared;
    normalized_input_replay_string_delta final_committed_text;
    normalized_input_replay_string_delta final_display_text;
    normalized_input_replay_string_delta final_preedit_text;
    normalized_input_replay_range_delta final_caret;
    normalized_input_replay_bool_delta final_has_selection;
    normalized_input_replay_range_delta final_selection;
    normalized_input_replay_bool_delta final_preedit_clean;
    bool timeline_changed = false;
    bool final_text_changed = false;
    bool final_preedit_changed = false;
    bool changed = false;
};

struct normalized_input_replay_focus_timeline_count_deltas {
    normalized_input_replay_count_delta focus_gain;
    normalized_input_replay_count_delta focus_loss;
    normalized_input_replay_count_delta focus_traversal_next;
    normalized_input_replay_count_delta focus_traversal_previous;
    normalized_input_replay_count_delta caret_moved;
    normalized_input_replay_count_delta selection_changed;
};

struct normalized_input_replay_focus_diff {
    normalized_input_replay_count_delta timeline_entries;
    normalized_input_replay_count_delta total;
    normalized_input_replay_count_delta emitted_input_event_routes;
    normalized_input_replay_count_delta diagnostic_only_routes;
    normalized_input_replay_count_delta target_transition_count;
    normalized_input_replay_count_delta caret_transition_count;
    normalized_input_replay_count_delta selection_transition_count;
    normalized_input_replay_focus_timeline_count_deltas kinds;
    normalized_input_replay_bool_delta final_has_focus;
    normalized_input_replay_string_delta final_focus_id;
    normalized_input_replay_count_delta final_text_byte_count;
    normalized_input_replay_range_delta final_caret;
    normalized_input_replay_bool_delta final_has_selection;
    normalized_input_replay_range_delta final_selection;
    normalized_input_replay_bool_delta final_focus_clean;
    bool timeline_changed = false;
    bool final_focus_changed = false;
    bool final_caret_changed = false;
    bool final_selection_changed = false;
    bool changed = false;
};

struct normalized_input_replay_regression_summary {
    normalized_input_replay_count_delta batch_count;
    normalized_input_replay_count_delta normalized_event_count;
    normalized_input_replay_count_delta route_count;
    bool final_state_changed = false;
    bool focus_caret_selection_changed = false;
    bool pointer_capture_changed = false;
    bool pointer_timeline_changed = false;
    bool gesture_policy_changed = false;
    bool ime_timeline_changed = false;
    bool keyboard_changed = false;
    bool text_or_preedit_changed = false;
    bool focus_timeline_changed = false;
    std::size_t changed_category_count = 0;
    bool changed = false;
};

struct normalized_input_replay_diff {
    normalized_input_replay_final_state_diff final_state;
    normalized_input_replay_keyboard_diff keyboard;
    normalized_input_replay_pointer_diff pointer;
    input_routing_gesture_policy_diff gesture_policies;
    normalized_input_replay_ime_diff ime;
    normalized_input_replay_focus_diff focus;
    normalized_input_replay_regression_summary regression;
};

[[nodiscard]] inline std::int64_t normalized_input_replay_size_delta(
    std::size_t before_count,
    std::size_t after_count)
{
    if (after_count >= before_count) {
        return static_cast<std::int64_t>(after_count - before_count);
    }
    return -static_cast<std::int64_t>(before_count - after_count);
}

[[nodiscard]] inline normalized_input_replay_count_delta normalized_input_replay_diff_count(
    std::size_t before_count,
    std::size_t after_count)
{
    return normalized_input_replay_count_delta{
        .before_count = before_count,
        .after_count = after_count,
        .delta = normalized_input_replay_size_delta(before_count, after_count),
        .changed = before_count != after_count,
    };
}

[[nodiscard]] inline normalized_input_replay_bool_delta normalized_input_replay_diff_bool(
    bool before_value,
    bool after_value)
{
    return normalized_input_replay_bool_delta{
        .before_value = before_value,
        .after_value = after_value,
        .changed = before_value != after_value,
    };
}

[[nodiscard]] inline normalized_input_replay_range_delta normalized_input_replay_diff_range(
    text_range before_range,
    text_range after_range)
{
    return normalized_input_replay_range_delta{
        .before_range = before_range,
        .after_range = after_range,
        .changed = !normalized_input_replay_same_text_range(before_range, after_range),
    };
}

[[nodiscard]] inline normalized_input_replay_string_delta normalized_input_replay_diff_string(
    std::string before_value,
    std::string after_value)
{
    const std::size_t before_size = before_value.size();
    const std::size_t after_size = after_value.size();
    const bool changed = before_value != after_value;
    return normalized_input_replay_string_delta{
        .before_value = std::move(before_value),
        .after_value = std::move(after_value),
        .byte_delta = normalized_input_replay_size_delta(before_size, after_size),
        .changed = changed,
    };
}

[[nodiscard]] inline normalized_input_replay_pointer_capture_delta normalized_input_replay_diff_pointer_capture(
    pointer_capture_snapshot before_capture,
    pointer_capture_snapshot after_capture)
{
    const bool before_clean = pointer_capture_snapshot_clean(before_capture);
    const bool after_clean = pointer_capture_snapshot_clean(after_capture);
    return normalized_input_replay_pointer_capture_delta{
        .before_capture = before_capture,
        .after_capture = after_capture,
        .before_clean = before_clean,
        .after_clean = after_clean,
        .changed = normalized_input_replay_pointer_capture_changed(before_capture, after_capture),
        .clean_changed = before_clean != after_clean,
    };
}

[[nodiscard]] inline bool normalized_input_replay_keyboard_intent_deltas_changed(
    const normalized_input_replay_keyboard_intent_count_deltas& deltas)
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

[[nodiscard]] inline bool normalized_input_replay_keyboard_modifier_deltas_changed(
    const normalized_input_replay_keyboard_modifier_count_deltas& deltas)
{
    return deltas.unmodified.changed
        || deltas.alt.changed
        || deltas.ctrl.changed
        || deltas.shift.changed
        || deltas.meta.changed;
}

[[nodiscard]] inline bool normalized_input_replay_keyboard_repeat_policy_deltas_changed(
    const normalized_input_replay_keyboard_repeat_policy_count_deltas& deltas)
{
    return deltas.not_repeat.changed
        || deltas.allowed.changed
        || deltas.ignored.changed;
}

[[nodiscard]] inline normalized_input_replay_keyboard_diff diff_normalized_input_replay_keyboard(
    const normalized_input_replay_keyboard_summary& before,
    const normalized_input_replay_keyboard_summary& after)
{
    normalized_input_replay_keyboard_diff diff{
        .chord_count = normalized_input_replay_diff_count(before.chords.size(), after.chords.size()),
        .total = normalized_input_replay_diff_count(before.total, after.total),
        .emitted_input_event_routes =
            normalized_input_replay_diff_count(before.emitted_input_event_routes, after.emitted_input_event_routes),
        .diagnostic_only_routes =
            normalized_input_replay_diff_count(before.diagnostic_only_routes, after.diagnostic_only_routes),
        .intents = normalized_input_replay_keyboard_intent_count_deltas{
            .none = normalized_input_replay_diff_count(before.intents.none, after.intents.none),
            .focus_traversal_next =
                normalized_input_replay_diff_count(
                    before.intents.focus_traversal_next,
                    after.intents.focus_traversal_next),
            .focus_traversal_previous =
                normalized_input_replay_diff_count(
                    before.intents.focus_traversal_previous,
                    after.intents.focus_traversal_previous),
            .submit = normalized_input_replay_diff_count(before.intents.submit, after.intents.submit),
            .cancel = normalized_input_replay_diff_count(before.intents.cancel, after.intents.cancel),
            .caret_previous =
                normalized_input_replay_diff_count(before.intents.caret_previous, after.intents.caret_previous),
            .caret_next = normalized_input_replay_diff_count(before.intents.caret_next, after.intents.caret_next),
            .caret_home = normalized_input_replay_diff_count(before.intents.caret_home, after.intents.caret_home),
            .caret_end = normalized_input_replay_diff_count(before.intents.caret_end, after.intents.caret_end),
            .selection_previous =
                normalized_input_replay_diff_count(
                    before.intents.selection_previous,
                    after.intents.selection_previous),
            .selection_next =
                normalized_input_replay_diff_count(before.intents.selection_next, after.intents.selection_next),
            .select_all = normalized_input_replay_diff_count(before.intents.select_all, after.intents.select_all),
            .delete_backward =
                normalized_input_replay_diff_count(before.intents.delete_backward, after.intents.delete_backward),
            .delete_forward =
                normalized_input_replay_diff_count(before.intents.delete_forward, after.intents.delete_forward),
        },
        .modifiers = normalized_input_replay_keyboard_modifier_count_deltas{
            .unmodified =
                normalized_input_replay_diff_count(before.modifiers.unmodified, after.modifiers.unmodified),
            .alt = normalized_input_replay_diff_count(before.modifiers.alt, after.modifiers.alt),
            .ctrl = normalized_input_replay_diff_count(before.modifiers.ctrl, after.modifiers.ctrl),
            .shift = normalized_input_replay_diff_count(before.modifiers.shift, after.modifiers.shift),
            .meta = normalized_input_replay_diff_count(before.modifiers.meta, after.modifiers.meta),
        },
        .repeat_policies = normalized_input_replay_keyboard_repeat_policy_count_deltas{
            .not_repeat =
                normalized_input_replay_diff_count(
                    before.repeat_policies.not_repeat,
                    after.repeat_policies.not_repeat),
            .allowed =
                normalized_input_replay_diff_count(before.repeat_policies.allowed, after.repeat_policies.allowed),
            .ignored =
                normalized_input_replay_diff_count(before.repeat_policies.ignored, after.repeat_policies.ignored),
        },
    };
    diff.changed = diff.chord_count.changed
        || diff.total.changed
        || diff.emitted_input_event_routes.changed
        || diff.diagnostic_only_routes.changed
        || normalized_input_replay_keyboard_intent_deltas_changed(diff.intents)
        || normalized_input_replay_keyboard_modifier_deltas_changed(diff.modifiers)
        || normalized_input_replay_keyboard_repeat_policy_deltas_changed(diff.repeat_policies);
    return diff;
}

[[nodiscard]] inline bool normalized_input_replay_pointer_timeline_deltas_changed(
    const normalized_input_replay_pointer_timeline_count_deltas& deltas)
{
    return deltas.pointer_capture_arbitration.changed
        || deltas.pointer_capture_reset.changed
        || deltas.tap.changed
        || deltas.long_press.changed
        || deltas.swipe_left.changed
        || deltas.swipe_right.changed
        || deltas.drag_start.changed
        || deltas.drag_update.changed
        || deltas.drag_end.changed
        || deltas.drag_cancel.changed
        || deltas.wheel.changed
        || deltas.gesture_suppressed.changed;
}

[[nodiscard]] inline normalized_input_replay_pointer_diff diff_normalized_input_replay_pointer(
    const normalized_input_replay_pointer_summary& before,
    const normalized_input_replay_pointer_summary& after)
{
    normalized_input_replay_pointer_diff diff{
        .timeline_entries = normalized_input_replay_diff_count(before.timeline.size(), after.timeline.size()),
        .total = normalized_input_replay_diff_count(before.total, after.total),
        .emitted_input_event_routes =
            normalized_input_replay_diff_count(before.emitted_input_event_routes, after.emitted_input_event_routes),
        .diagnostic_only_routes =
            normalized_input_replay_diff_count(before.diagnostic_only_routes, after.diagnostic_only_routes),
        .capture_transition_count =
            normalized_input_replay_diff_count(before.capture_transition_count, after.capture_transition_count),
        .wheel_routes = normalized_input_replay_diff_count(before.wheel_routes, after.wheel_routes),
        .pointer_id_count = normalized_input_replay_diff_count(before.pointer_ids.size(), after.pointer_ids.size()),
        .mouse_pointer_id_count =
            normalized_input_replay_diff_count(before.mouse_pointer_ids.size(), after.mouse_pointer_ids.size()),
        .touch_pointer_id_count =
            normalized_input_replay_diff_count(before.touch_pointer_ids.size(), after.touch_pointer_ids.size()),
        .kinds = normalized_input_replay_pointer_timeline_count_deltas{
            .pointer_capture_arbitration =
                normalized_input_replay_diff_count(
                    before.kinds.pointer_capture_arbitration,
                    after.kinds.pointer_capture_arbitration),
            .pointer_capture_reset =
                normalized_input_replay_diff_count(
                    before.kinds.pointer_capture_reset,
                    after.kinds.pointer_capture_reset),
            .tap = normalized_input_replay_diff_count(before.kinds.tap, after.kinds.tap),
            .long_press = normalized_input_replay_diff_count(before.kinds.long_press, after.kinds.long_press),
            .swipe_left = normalized_input_replay_diff_count(before.kinds.swipe_left, after.kinds.swipe_left),
            .swipe_right = normalized_input_replay_diff_count(before.kinds.swipe_right, after.kinds.swipe_right),
            .drag_start = normalized_input_replay_diff_count(before.kinds.drag_start, after.kinds.drag_start),
            .drag_update = normalized_input_replay_diff_count(before.kinds.drag_update, after.kinds.drag_update),
            .drag_end = normalized_input_replay_diff_count(before.kinds.drag_end, after.kinds.drag_end),
            .drag_cancel = normalized_input_replay_diff_count(before.kinds.drag_cancel, after.kinds.drag_cancel),
            .wheel = normalized_input_replay_diff_count(before.kinds.wheel, after.kinds.wheel),
            .gesture_suppressed =
                normalized_input_replay_diff_count(before.kinds.gesture_suppressed, after.kinds.gesture_suppressed),
        },
        .saw_multipointer_touch =
            normalized_input_replay_diff_bool(before.saw_multipointer_touch, after.saw_multipointer_touch),
        .final_capture = normalized_input_replay_diff_pointer_capture(before.final_capture, after.final_capture),
    };
    diff.timeline_changed = diff.timeline_entries.changed
        || diff.total.changed
        || diff.emitted_input_event_routes.changed
        || diff.diagnostic_only_routes.changed
        || diff.capture_transition_count.changed
        || diff.wheel_routes.changed
        || diff.pointer_id_count.changed
        || diff.mouse_pointer_id_count.changed
        || diff.touch_pointer_id_count.changed
        || diff.saw_multipointer_touch.changed
        || normalized_input_replay_pointer_timeline_deltas_changed(diff.kinds);
    diff.capture_changed = diff.final_capture.changed || diff.final_capture.clean_changed;
    diff.changed = diff.timeline_changed || diff.capture_changed;
    return diff;
}

[[nodiscard]] inline bool normalized_input_replay_ime_phase_deltas_changed(
    const normalized_input_replay_ime_phase_count_deltas& deltas)
{
    return deltas.composition_start.changed
        || deltas.preedit_update.changed
        || deltas.commit.changed
        || deltas.cancel.changed;
}

[[nodiscard]] inline normalized_input_replay_ime_diff diff_normalized_input_replay_ime(
    const normalized_input_replay_ime_summary& before,
    const normalized_input_replay_ime_summary& after)
{
    normalized_input_replay_ime_diff diff{
        .timeline_entries = normalized_input_replay_diff_count(before.timeline.size(), after.timeline.size()),
        .total = normalized_input_replay_diff_count(before.total, after.total),
        .emitted_input_event_routes =
            normalized_input_replay_diff_count(before.emitted_input_event_routes, after.emitted_input_event_routes),
        .diagnostic_only_routes =
            normalized_input_replay_diff_count(before.diagnostic_only_routes, after.diagnostic_only_routes),
        .phases = normalized_input_replay_ime_phase_count_deltas{
            .composition_start =
                normalized_input_replay_diff_count(before.phases.composition_start, after.phases.composition_start),
            .preedit_update =
                normalized_input_replay_diff_count(before.phases.preedit_update, after.phases.preedit_update),
            .commit = normalized_input_replay_diff_count(before.phases.commit, after.phases.commit),
            .cancel = normalized_input_replay_diff_count(before.phases.cancel, after.phases.cancel),
        },
        .all_preedit_text_valid =
            normalized_input_replay_diff_bool(before.all_preedit_text_valid, after.all_preedit_text_valid),
        .all_preedit_ranges_valid =
            normalized_input_replay_diff_bool(before.all_preedit_ranges_valid, after.all_preedit_ranges_valid),
        .stale_preedit_cleared =
            normalized_input_replay_diff_bool(before.stale_preedit_cleared, after.stale_preedit_cleared),
        .final_committed_text =
            normalized_input_replay_diff_string(before.final_committed_text, after.final_committed_text),
        .final_display_text =
            normalized_input_replay_diff_string(before.final_display_text, after.final_display_text),
        .final_preedit_text =
            normalized_input_replay_diff_string(before.final_preedit_text, after.final_preedit_text),
        .final_caret = normalized_input_replay_diff_range(before.final_caret, after.final_caret),
        .final_has_selection =
            normalized_input_replay_diff_bool(before.final_has_selection, after.final_has_selection),
        .final_selection = normalized_input_replay_diff_range(before.final_selection, after.final_selection),
        .final_preedit_clean =
            normalized_input_replay_diff_bool(before.final_preedit_clean, after.final_preedit_clean),
    };
    diff.timeline_changed = diff.timeline_entries.changed
        || diff.total.changed
        || diff.emitted_input_event_routes.changed
        || diff.diagnostic_only_routes.changed
        || normalized_input_replay_ime_phase_deltas_changed(diff.phases);
    diff.final_text_changed = diff.final_committed_text.changed || diff.final_display_text.changed;
    diff.final_preedit_changed = diff.final_preedit_text.changed || diff.final_preedit_clean.changed;
    diff.changed = diff.timeline_changed
        || diff.all_preedit_text_valid.changed
        || diff.all_preedit_ranges_valid.changed
        || diff.stale_preedit_cleared.changed
        || diff.final_text_changed
        || diff.final_preedit_changed
        || diff.final_caret.changed
        || diff.final_has_selection.changed
        || diff.final_selection.changed;
    return diff;
}

[[nodiscard]] inline bool normalized_input_replay_focus_timeline_deltas_changed(
    const normalized_input_replay_focus_timeline_count_deltas& deltas)
{
    return deltas.focus_gain.changed
        || deltas.focus_loss.changed
        || deltas.focus_traversal_next.changed
        || deltas.focus_traversal_previous.changed
        || deltas.caret_moved.changed
        || deltas.selection_changed.changed;
}

[[nodiscard]] inline normalized_input_replay_focus_diff diff_normalized_input_replay_focus(
    const normalized_input_replay_focus_summary& before,
    const normalized_input_replay_focus_summary& after)
{
    normalized_input_replay_focus_diff diff{
        .timeline_entries = normalized_input_replay_diff_count(before.timeline.size(), after.timeline.size()),
        .total = normalized_input_replay_diff_count(before.total, after.total),
        .emitted_input_event_routes =
            normalized_input_replay_diff_count(before.emitted_input_event_routes, after.emitted_input_event_routes),
        .diagnostic_only_routes =
            normalized_input_replay_diff_count(before.diagnostic_only_routes, after.diagnostic_only_routes),
        .target_transition_count =
            normalized_input_replay_diff_count(before.target_transition_count, after.target_transition_count),
        .caret_transition_count =
            normalized_input_replay_diff_count(before.caret_transition_count, after.caret_transition_count),
        .selection_transition_count =
            normalized_input_replay_diff_count(before.selection_transition_count, after.selection_transition_count),
        .kinds = normalized_input_replay_focus_timeline_count_deltas{
            .focus_gain = normalized_input_replay_diff_count(before.kinds.focus_gain, after.kinds.focus_gain),
            .focus_loss = normalized_input_replay_diff_count(before.kinds.focus_loss, after.kinds.focus_loss),
            .focus_traversal_next =
                normalized_input_replay_diff_count(
                    before.kinds.focus_traversal_next,
                    after.kinds.focus_traversal_next),
            .focus_traversal_previous =
                normalized_input_replay_diff_count(
                    before.kinds.focus_traversal_previous,
                    after.kinds.focus_traversal_previous),
            .caret_moved = normalized_input_replay_diff_count(before.kinds.caret_moved, after.kinds.caret_moved),
            .selection_changed =
                normalized_input_replay_diff_count(before.kinds.selection_changed, after.kinds.selection_changed),
        },
        .final_has_focus = normalized_input_replay_diff_bool(before.final_has_focus, after.final_has_focus),
        .final_focus_id = normalized_input_replay_diff_string(before.final_focus_id, after.final_focus_id),
        .final_text_byte_count =
            normalized_input_replay_diff_count(before.final_text_byte_count, after.final_text_byte_count),
        .final_caret = normalized_input_replay_diff_range(before.final_caret, after.final_caret),
        .final_has_selection =
            normalized_input_replay_diff_bool(before.final_has_selection, after.final_has_selection),
        .final_selection = normalized_input_replay_diff_range(before.final_selection, after.final_selection),
        .final_focus_clean = normalized_input_replay_diff_bool(before.final_focus_clean, after.final_focus_clean),
    };
    diff.timeline_changed = diff.timeline_entries.changed
        || diff.total.changed
        || diff.emitted_input_event_routes.changed
        || diff.diagnostic_only_routes.changed
        || diff.target_transition_count.changed
        || diff.caret_transition_count.changed
        || diff.selection_transition_count.changed
        || normalized_input_replay_focus_timeline_deltas_changed(diff.kinds);
    diff.final_focus_changed =
        diff.final_has_focus.changed || diff.final_focus_id.changed || diff.final_focus_clean.changed;
    diff.final_caret_changed = diff.final_caret.changed;
    diff.final_selection_changed = diff.final_has_selection.changed || diff.final_selection.changed;
    diff.changed = diff.timeline_changed
        || diff.final_focus_changed
        || diff.final_text_byte_count.changed
        || diff.final_caret_changed
        || diff.final_selection_changed;
    return diff;
}

[[nodiscard]] inline normalized_input_replay_final_state_diff diff_normalized_input_replay_final_state(
    const normalized_input_replay_end_state& before,
    const normalized_input_replay_end_state& after)
{
    normalized_input_replay_final_state_diff diff{
        .has_focus = normalized_input_replay_diff_bool(before.has_text_focus, after.has_text_focus),
        .focus_id = normalized_input_replay_diff_string(before.focus_id, after.focus_id),
        .text = normalized_input_replay_diff_string(before.text, after.text),
        .display_text = normalized_input_replay_diff_string(before.display_text, after.display_text),
        .preedit_text = normalized_input_replay_diff_string(before.preedit_text, after.preedit_text),
        .caret = normalized_input_replay_diff_range(
            normalized_input_replay_caret_range_for_offset(before.caret_byte_offset),
            normalized_input_replay_caret_range_for_offset(after.caret_byte_offset)),
        .has_selection = normalized_input_replay_diff_bool(before.has_selection, after.has_selection),
        .selection = normalized_input_replay_diff_range(before.selection, after.selection),
        .focus_clean = normalized_input_replay_diff_bool(before.focus_clean, after.focus_clean),
        .preedit_clean = normalized_input_replay_diff_bool(before.preedit_clean, after.preedit_clean),
        .pointer_capture = normalized_input_replay_diff_pointer_capture(before.pointer_capture, after.pointer_capture),
        .text_presentation =
            diff_text_input_presentation_snapshots(before.text_presentation, after.text_presentation),
    };
    diff.focus_changed =
        diff.has_focus.changed
        || diff.focus_id.changed
        || diff.focus_clean.changed
        || diff.text_presentation.focus_changed;
    diff.caret_changed = diff.caret.changed || diff.text_presentation.caret_changed;
    diff.selection_changed =
        diff.has_selection.changed
        || diff.selection.changed
        || diff.text_presentation.selection_changed;
    diff.text_changed = diff.text.changed || diff.text_presentation.committed_text_changed;
    diff.display_text_changed =
        diff.display_text.changed || diff.text_presentation.display_text_changed;
    diff.preedit_changed =
        diff.preedit_text.changed
        || diff.preedit_clean.changed
        || diff.text_presentation.preedit_changed;
    diff.pointer_capture_changed = diff.pointer_capture.changed || diff.pointer_capture.clean_changed;
    diff.text_presentation_changed = diff.text_presentation.changed;
    diff.changed = diff.focus_changed
        || diff.caret_changed
        || diff.selection_changed
        || diff.text_changed
        || diff.display_text_changed
        || diff.preedit_changed
        || diff.pointer_capture_changed
        || diff.text_presentation_changed;
    return diff;
}

inline void normalized_input_replay_count_regression_category(
    bool changed,
    std::size_t& changed_category_count)
{
    if (changed) {
        ++changed_category_count;
    }
}

[[nodiscard]] inline normalized_input_replay_regression_summary summarize_normalized_input_replay_diff(
    const normalized_input_replay_recording& before,
    const normalized_input_replay_recording& after,
    const normalized_input_replay_final_state_diff& final_state,
    const normalized_input_replay_keyboard_diff& keyboard,
    const normalized_input_replay_pointer_diff& pointer,
    const input_routing_gesture_policy_diff& gesture_policies,
    const normalized_input_replay_ime_diff& ime,
    const normalized_input_replay_focus_diff& focus)
{
    normalized_input_replay_regression_summary summary{
        .batch_count = normalized_input_replay_diff_count(before.batches.size(), after.batches.size()),
        .normalized_event_count =
            normalized_input_replay_diff_count(
                before.summary.normalized_event_count,
                after.summary.normalized_event_count),
        .route_count = normalized_input_replay_diff_count(before.summary.routes.total, after.summary.routes.total),
        .final_state_changed = final_state.changed,
        .focus_caret_selection_changed = final_state.focus_changed
            || final_state.caret_changed
            || final_state.selection_changed
            || focus.final_focus_changed
            || focus.final_caret_changed
            || focus.final_selection_changed,
        .pointer_capture_changed = final_state.pointer_capture_changed || pointer.capture_changed,
        .pointer_timeline_changed = pointer.timeline_changed,
        .gesture_policy_changed = gesture_policies.changed,
        .ime_timeline_changed = ime.timeline_changed,
        .keyboard_changed = keyboard.changed,
        .text_or_preedit_changed = final_state.text_changed
            || final_state.display_text_changed
            || final_state.preedit_changed
            || final_state.text_presentation_changed
            || ime.final_text_changed
            || ime.final_preedit_changed,
        .focus_timeline_changed = focus.timeline_changed,
    };
    normalized_input_replay_count_regression_category(summary.final_state_changed, summary.changed_category_count);
    normalized_input_replay_count_regression_category(
        summary.focus_caret_selection_changed,
        summary.changed_category_count);
    normalized_input_replay_count_regression_category(
        summary.pointer_capture_changed,
        summary.changed_category_count);
    normalized_input_replay_count_regression_category(
        summary.pointer_timeline_changed,
        summary.changed_category_count);
    normalized_input_replay_count_regression_category(
        summary.gesture_policy_changed,
        summary.changed_category_count);
    normalized_input_replay_count_regression_category(summary.ime_timeline_changed, summary.changed_category_count);
    normalized_input_replay_count_regression_category(summary.keyboard_changed, summary.changed_category_count);
    normalized_input_replay_count_regression_category(
        summary.text_or_preedit_changed,
        summary.changed_category_count);
    normalized_input_replay_count_regression_category(summary.focus_timeline_changed, summary.changed_category_count);
    summary.changed = summary.changed_category_count > 0
        || summary.batch_count.changed
        || summary.normalized_event_count.changed
        || summary.route_count.changed;
    return summary;
}

[[nodiscard]] inline normalized_input_replay_diff diff_normalized_input_replay_recordings(
    const normalized_input_replay_recording& before,
    const normalized_input_replay_recording& after)
{
    normalized_input_replay_diff diff{
        .final_state = diff_normalized_input_replay_final_state(before.final_state, after.final_state),
        .keyboard = diff_normalized_input_replay_keyboard(before.keyboard, after.keyboard),
        .pointer = diff_normalized_input_replay_pointer(before.pointer, after.pointer),
        .gesture_policies = diff_input_routing_gesture_policies(
            normalized_input_replay_gesture_policy_diagnostics(before.gesture_policies),
            normalized_input_replay_gesture_policy_diagnostics(after.gesture_policies)),
        .ime = diff_normalized_input_replay_ime(before.ime, after.ime),
        .focus = diff_normalized_input_replay_focus(before.focus, after.focus),
    };
    diff.regression = summarize_normalized_input_replay_diff(
        before,
        after,
        diff.final_state,
        diff.keyboard,
        diff.pointer,
        diff.gesture_policies,
        diff.ime,
        diff.focus);
    return diff;
}

} // namespace quiz_vulkan::input
