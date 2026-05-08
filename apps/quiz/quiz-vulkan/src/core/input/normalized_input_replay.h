#pragma once

#include "core/input/input_engine.h"
#include "core/input/platform_input_translator.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace quiz_vulkan::input {

struct normalized_input_replay_time_update {
    std::int64_t timestamp_ms = 0;
};

using normalized_input_replay_action = std::variant<
    raw_platform_input_event,
    raw_scroll_event,
    normalized_input_replay_time_update>;

struct normalized_input_replay_step {
    std::string label;
    normalized_input_replay_action action;
};

struct normalized_input_replay_options {
    std::string initial_focus_target_id;
};

struct normalized_input_replay_keyboard_intent_counts {
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

struct normalized_input_replay_keyboard_modifier_counts {
    std::size_t unmodified = 0;
    std::size_t alt = 0;
    std::size_t ctrl = 0;
    std::size_t shift = 0;
    std::size_t meta = 0;
};

struct normalized_input_replay_keyboard_repeat_policy_counts {
    std::size_t not_repeat = 0;
    std::size_t allowed = 0;
    std::size_t ignored = 0;
};

struct normalized_input_replay_keyboard_summary {
    std::vector<keyboard_chord_diagnostic> chords;
    normalized_input_replay_keyboard_intent_counts intents;
    normalized_input_replay_keyboard_modifier_counts modifiers;
    normalized_input_replay_keyboard_repeat_policy_counts repeat_policies;
    std::size_t total = 0;
    std::size_t emitted_input_event_routes = 0;
    std::size_t diagnostic_only_routes = 0;
};

enum class normalized_input_replay_ime_timeline_phase {
    composition_start,
    preedit_update,
    commit,
    cancel,
};

struct normalized_input_replay_ime_phase_counts {
    std::size_t composition_start = 0;
    std::size_t preedit_update = 0;
    std::size_t commit = 0;
    std::size_t cancel = 0;
};

struct normalized_input_replay_ime_timeline_entry {
    normalized_input_replay_ime_timeline_phase phase =
        normalized_input_replay_ime_timeline_phase::preedit_update;
    std::int64_t timestamp_ms = 0;
    bool emits_input_event = false;
    std::size_t event_index = 0;
    std::string target_id;
    std::string utf8_text;
    std::string committed_text;
    ime_composition_state composition;
    std::size_t text_byte_count = 0;
    std::size_t text_byte_count_before = 0;
    std::size_t text_byte_count_after = 0;
    text_range caret_before;
    text_range caret_after;
    bool had_selection_before = false;
    bool has_selection_after = false;
    text_range selection_before;
    text_range selection_after;
    bool preedit_text_valid = true;
    bool preedit_range_valid = true;
    bool stale_preedit_cleared_after = true;
    std::string committed_text_after;
    std::string display_text_after;
    std::string preedit_text_after;
};

struct normalized_input_replay_ime_summary {
    std::vector<normalized_input_replay_ime_timeline_entry> timeline;
    normalized_input_replay_ime_phase_counts phases;
    std::size_t total = 0;
    std::size_t emitted_input_event_routes = 0;
    std::size_t diagnostic_only_routes = 0;
    bool all_preedit_text_valid = true;
    bool all_preedit_ranges_valid = true;
    bool stale_preedit_cleared = true;
    std::string final_committed_text;
    std::string final_display_text;
    std::string final_preedit_text;
    text_range final_caret;
    bool final_has_selection = false;
    text_range final_selection;
    bool final_preedit_clean = true;
};

enum class normalized_input_replay_pointer_timeline_kind {
    pointer_capture_arbitration,
    pointer_capture_reset,
    tap,
    long_press,
    swipe_left,
    swipe_right,
    drag_start,
    drag_update,
    drag_end,
    drag_cancel,
    wheel,
    gesture_suppressed,
};

struct normalized_input_replay_pointer_timeline_counts {
    std::size_t pointer_capture_arbitration = 0;
    std::size_t pointer_capture_reset = 0;
    std::size_t tap = 0;
    std::size_t long_press = 0;
    std::size_t swipe_left = 0;
    std::size_t swipe_right = 0;
    std::size_t drag_start = 0;
    std::size_t drag_update = 0;
    std::size_t drag_end = 0;
    std::size_t drag_cancel = 0;
    std::size_t wheel = 0;
    std::size_t gesture_suppressed = 0;
};

struct normalized_input_replay_pointer_contact_counts {
    std::size_t unknown = 0;
    std::size_t mouse_like = 0;
    std::size_t touch_like = 0;
};

struct normalized_input_replay_pointer_capture_lifecycle_counts {
    std::size_t idle = 0;
    std::size_t tracking = 0;
    std::size_t captured = 0;
};

struct normalized_input_replay_pointer_decision_counts {
    std::size_t none = 0;
    std::size_t tracked = 0;
    std::size_t captured = 0;
    std::size_t ignored_by_capture = 0;
    std::size_t canceled = 0;
    std::size_t released = 0;
    std::size_t restarted = 0;
};

struct normalized_input_replay_pointer_timeline_entry {
    normalized_input_replay_pointer_timeline_kind kind =
        normalized_input_replay_pointer_timeline_kind::pointer_capture_arbitration;
    std::int64_t timestamp_ms = 0;
    bool emits_input_event = false;
    std::size_t event_index = 0;
    std::int32_t pointer_id = 0;
    pointer_phase event_phase = pointer_phase::down;
    pointer_contact_kind contact = pointer_contact_kind::unknown;
    pointer_arbitration_decision decision = pointer_arbitration_decision::none;
    pointer_capture_snapshot capture_before;
    pointer_capture_snapshot capture_after;
    std::size_t tracked_pointer_count_before = 0;
    std::size_t tracked_pointer_count_after = 0;
    normalized_input_event_summary normalized_event;
    gesture_policy_snapshot gesture_policy;
    bool capture_changed = false;
    bool capture_ended_cleanly_after = true;
    std::int64_t duration_ms = 0;
    float start_x = 0.0f;
    float start_y = 0.0f;
    float x = 0.0f;
    float y = 0.0f;
    float delta_x = 0.0f;
    float delta_y = 0.0f;
    float pixel_delta_x = 0.0f;
    float pixel_delta_y = 0.0f;
    float line_delta_x = 0.0f;
    float line_delta_y = 0.0f;
};

struct normalized_input_replay_pointer_summary {
    std::vector<normalized_input_replay_pointer_timeline_entry> timeline;
    std::vector<std::int32_t> pointer_ids;
    std::vector<std::int32_t> mouse_pointer_ids;
    std::vector<std::int32_t> touch_pointer_ids;
    normalized_input_replay_pointer_timeline_counts kinds;
    normalized_input_replay_pointer_contact_counts contacts;
    normalized_input_replay_pointer_capture_lifecycle_counts capture_before_lifecycles;
    normalized_input_replay_pointer_capture_lifecycle_counts capture_after_lifecycles;
    normalized_input_replay_pointer_decision_counts decisions;
    std::size_t total = 0;
    std::size_t emitted_input_event_routes = 0;
    std::size_t diagnostic_only_routes = 0;
    std::size_t capture_transition_count = 0;
    std::size_t wheel_routes = 0;
    bool saw_multipointer_touch = false;
    pointer_capture_snapshot final_capture;
    bool final_capture_clean = true;
};

enum class normalized_input_replay_focus_timeline_kind {
    focus_gain,
    focus_loss,
    focus_traversal_next,
    focus_traversal_previous,
    caret_moved,
    selection_changed,
};

struct normalized_input_replay_focus_timeline_counts {
    std::size_t focus_gain = 0;
    std::size_t focus_loss = 0;
    std::size_t focus_traversal_next = 0;
    std::size_t focus_traversal_previous = 0;
    std::size_t caret_moved = 0;
    std::size_t selection_changed = 0;
};

struct normalized_input_replay_focus_timeline_entry {
    normalized_input_replay_focus_timeline_kind kind =
        normalized_input_replay_focus_timeline_kind::caret_moved;
    std::int64_t timestamp_ms = 0;
    bool emits_input_event = false;
    std::size_t event_index = 0;
    std::string target_id;
    std::string target_id_before;
    std::string target_id_after;
    bool had_focus_before = false;
    bool has_focus_after = false;
    bool target_changed = false;
    std::size_t text_byte_count_before = 0;
    std::size_t text_byte_count_after = 0;
    text_range caret_before;
    text_range caret_after;
    bool caret_changed = false;
    bool had_selection_before = false;
    bool has_selection_after = false;
    text_range selection_before;
    text_range selection_after;
    bool selection_changed = false;
    keyboard_chord_diagnostic keyboard;
    bool focus_clean_after = true;
};

struct normalized_input_replay_focus_summary {
    std::vector<normalized_input_replay_focus_timeline_entry> timeline;
    normalized_input_replay_focus_timeline_counts kinds;
    std::size_t total = 0;
    std::size_t emitted_input_event_routes = 0;
    std::size_t diagnostic_only_routes = 0;
    std::size_t target_transition_count = 0;
    std::size_t caret_transition_count = 0;
    std::size_t selection_transition_count = 0;
    bool final_has_focus = false;
    std::string final_focus_id;
    std::size_t final_text_byte_count = 0;
    text_range final_caret;
    bool final_has_selection = false;
    text_range final_selection;
    bool final_focus_clean = true;
};

struct normalized_input_replay_end_state {
    pointer_capture_snapshot pointer_capture;
    bool has_text_focus = false;
    std::string focus_id;
    std::string text;
    std::string display_text;
    std::size_t caret_byte_offset = 0;
    bool has_selection = false;
    text_range selection;
    std::string preedit_text;
    ime_composition_state composition;
    bool pointer_capture_clean = true;
    bool focus_clean = true;
    bool preedit_clean = true;
};

struct normalized_input_replay_batch {
    std::string label;
    std::vector<input_event> input_events;
    std::vector<normalized_input_event_summary> normalized_events;
    input_diagnostic_summary summary;
    normalized_input_replay_keyboard_summary keyboard;
    normalized_input_replay_ime_summary ime;
    normalized_input_replay_pointer_summary pointer;
    normalized_input_replay_focus_summary focus;
    normalized_input_replay_end_state end_state;
};

struct normalized_input_replay_recording {
    std::vector<normalized_input_replay_batch> batches;
    input_diagnostic_summary summary;
    normalized_input_replay_keyboard_summary keyboard;
    normalized_input_replay_ime_summary ime;
    normalized_input_replay_pointer_summary pointer;
    normalized_input_replay_focus_summary focus;
    normalized_input_replay_end_state final_state;
};

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
    bool focus_changed = false;
    bool caret_changed = false;
    bool selection_changed = false;
    bool text_changed = false;
    bool display_text_changed = false;
    bool preedit_changed = false;
    bool pointer_capture_changed = false;
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
    normalized_input_replay_ime_diff ime;
    normalized_input_replay_focus_diff focus;
    normalized_input_replay_regression_summary regression;
};

[[nodiscard]] inline bool keyboard_chord_present(const keyboard_chord_diagnostic& chord)
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

inline void count_keyboard_intent(
    normalized_input_replay_keyboard_intent_counts& counts,
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

inline void count_keyboard_modifiers(
    normalized_input_replay_keyboard_modifier_counts& counts,
    keyboard_modifier_state modifiers)
{
    if (!modifiers.alt && !modifiers.ctrl && !modifiers.shift && !modifiers.meta) {
        ++counts.unmodified;
        return;
    }
    if (modifiers.alt) {
        ++counts.alt;
    }
    if (modifiers.ctrl) {
        ++counts.ctrl;
    }
    if (modifiers.shift) {
        ++counts.shift;
    }
    if (modifiers.meta) {
        ++counts.meta;
    }
}

inline void count_keyboard_repeat_policy(
    normalized_input_replay_keyboard_repeat_policy_counts& counts,
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

inline void accumulate_normalized_input_replay_keyboard_summary(
    normalized_input_replay_keyboard_summary& target,
    const normalized_input_replay_keyboard_summary& source)
{
    target.chords.insert(target.chords.end(), source.chords.begin(), source.chords.end());
    target.intents.none += source.intents.none;
    target.intents.focus_traversal_next += source.intents.focus_traversal_next;
    target.intents.focus_traversal_previous += source.intents.focus_traversal_previous;
    target.intents.submit += source.intents.submit;
    target.intents.cancel += source.intents.cancel;
    target.intents.caret_previous += source.intents.caret_previous;
    target.intents.caret_next += source.intents.caret_next;
    target.intents.caret_home += source.intents.caret_home;
    target.intents.caret_end += source.intents.caret_end;
    target.intents.selection_previous += source.intents.selection_previous;
    target.intents.selection_next += source.intents.selection_next;
    target.intents.select_all += source.intents.select_all;
    target.intents.delete_backward += source.intents.delete_backward;
    target.intents.delete_forward += source.intents.delete_forward;
    target.modifiers.unmodified += source.modifiers.unmodified;
    target.modifiers.alt += source.modifiers.alt;
    target.modifiers.ctrl += source.modifiers.ctrl;
    target.modifiers.shift += source.modifiers.shift;
    target.modifiers.meta += source.modifiers.meta;
    target.repeat_policies.not_repeat += source.repeat_policies.not_repeat;
    target.repeat_policies.allowed += source.repeat_policies.allowed;
    target.repeat_policies.ignored += source.repeat_policies.ignored;
    target.total += source.total;
    target.emitted_input_event_routes += source.emitted_input_event_routes;
    target.diagnostic_only_routes += source.diagnostic_only_routes;
}

[[nodiscard]] inline normalized_input_replay_keyboard_summary summarize_normalized_input_replay_keyboard_routes(
    std::span<const action_route_policy_diagnostic> routes)
{
    normalized_input_replay_keyboard_summary summary;
    for (const action_route_policy_diagnostic& route : routes) {
        if (!keyboard_chord_present(route.keyboard)) {
            continue;
        }

        summary.chords.push_back(route.keyboard);
        ++summary.total;
        if (route.emits_input_event) {
            ++summary.emitted_input_event_routes;
        } else {
            ++summary.diagnostic_only_routes;
        }
        count_keyboard_intent(summary.intents, route.keyboard.intent);
        count_keyboard_modifiers(summary.modifiers, route.keyboard.modifiers);
        count_keyboard_repeat_policy(summary.repeat_policies, route.keyboard.repeat_policy);
    }
    return summary;
}

[[nodiscard]] inline bool pointer_capture_snapshot_clean(const pointer_capture_snapshot& snapshot)
{
    return snapshot.lifecycle == pointer_capture_lifecycle::idle
        && !snapshot.active
        && snapshot.tracked_pointer_count == 0;
}

[[nodiscard]] inline bool normalized_input_replay_range_size_matches(
    text_range range,
    std::size_t byte_count)
{
    return range.end_byte >= range.start_byte
        && range.end_byte - range.start_byte == byte_count;
}

[[nodiscard]] inline bool normalized_input_replay_composition_range_valid(
    const ime_composition_state& composition)
{
    return normalized_input_replay_range_size_matches(
            composition.preedit_range,
            composition.preedit_text.size())
        && composition.caret_range.start_byte == composition.caret_range.end_byte
        && composition.caret_range.start_byte == composition.preedit_range.end_byte;
}

[[nodiscard]] inline bool normalized_input_replay_preedit_text_valid(
    const ime_composition_state& composition)
{
    return detail::valid_utf8(composition.preedit_text);
}

[[nodiscard]] inline bool normalized_input_replay_ime_route_kind(action_route_policy_kind kind)
{
    return kind == action_route_policy_kind::ime_composition_start
        || kind == action_route_policy_kind::ime_preedit
        || kind == action_route_policy_kind::ime_commit
        || kind == action_route_policy_kind::ime_cancel;
}

[[nodiscard]] inline normalized_input_replay_ime_timeline_phase normalized_input_replay_ime_phase_for(
    action_route_policy_kind kind)
{
    switch (kind) {
    case action_route_policy_kind::ime_composition_start:
        return normalized_input_replay_ime_timeline_phase::composition_start;
    case action_route_policy_kind::ime_commit:
        return normalized_input_replay_ime_timeline_phase::commit;
    case action_route_policy_kind::ime_cancel:
        return normalized_input_replay_ime_timeline_phase::cancel;
    case action_route_policy_kind::ime_preedit:
    case action_route_policy_kind::pointer_capture_reset:
    case action_route_policy_kind::pointer_capture_arbitration:
    case action_route_policy_kind::wheel_summary:
    case action_route_policy_kind::gesture_route_snapshot:
    case action_route_policy_kind::text_commit_boundary:
    case action_route_policy_kind::text_backspace_boundary:
    case action_route_policy_kind::text_delete_forward_boundary:
    case action_route_policy_kind::caret_moved:
    case action_route_policy_kind::selection_changed:
    case action_route_policy_kind::focus_traversal_next:
    case action_route_policy_kind::focus_traversal_previous:
    case action_route_policy_kind::text_submit_boundary:
    case action_route_policy_kind::keyboard_cancel_intent:
    case action_route_policy_kind::focus_loss:
        return normalized_input_replay_ime_timeline_phase::preedit_update;
    }

    return normalized_input_replay_ime_timeline_phase::preedit_update;
}

inline void count_normalized_input_replay_ime_phase(
    normalized_input_replay_ime_phase_counts& counts,
    normalized_input_replay_ime_timeline_phase phase)
{
    switch (phase) {
    case normalized_input_replay_ime_timeline_phase::composition_start:
        ++counts.composition_start;
        return;
    case normalized_input_replay_ime_timeline_phase::preedit_update:
        ++counts.preedit_update;
        return;
    case normalized_input_replay_ime_timeline_phase::commit:
        ++counts.commit;
        return;
    case normalized_input_replay_ime_timeline_phase::cancel:
        ++counts.cancel;
        return;
    }
}

[[nodiscard]] inline const ime_event* normalized_input_replay_ime_event_at(
    std::span<const input_event> events,
    std::size_t index)
{
    if (index >= events.size() || !std::holds_alternative<ime_event>(events[index])) {
        return nullptr;
    }

    return &std::get<ime_event>(events[index]);
}

inline void apply_normalized_input_replay_ime_final_state(
    normalized_input_replay_ime_summary& summary,
    const normalized_input_replay_end_state& end_state)
{
    summary.final_committed_text = end_state.text;
    summary.final_display_text = end_state.display_text;
    summary.final_preedit_text = end_state.preedit_text;
    summary.final_caret = text_range{
        .start_byte = end_state.caret_byte_offset,
        .end_byte = end_state.caret_byte_offset,
    };
    summary.final_has_selection = end_state.has_selection;
    summary.final_selection = end_state.selection;
    summary.final_preedit_clean = end_state.preedit_clean;
}

inline void accumulate_normalized_input_replay_ime_summary(
    normalized_input_replay_ime_summary& target,
    const normalized_input_replay_ime_summary& source)
{
    target.timeline.insert(target.timeline.end(), source.timeline.begin(), source.timeline.end());
    target.phases.composition_start += source.phases.composition_start;
    target.phases.preedit_update += source.phases.preedit_update;
    target.phases.commit += source.phases.commit;
    target.phases.cancel += source.phases.cancel;
    target.total += source.total;
    target.emitted_input_event_routes += source.emitted_input_event_routes;
    target.diagnostic_only_routes += source.diagnostic_only_routes;
    target.all_preedit_text_valid = target.all_preedit_text_valid && source.all_preedit_text_valid;
    target.all_preedit_ranges_valid = target.all_preedit_ranges_valid && source.all_preedit_ranges_valid;
    target.stale_preedit_cleared = target.stale_preedit_cleared && source.stale_preedit_cleared;
    target.final_committed_text = source.final_committed_text;
    target.final_display_text = source.final_display_text;
    target.final_preedit_text = source.final_preedit_text;
    target.final_caret = source.final_caret;
    target.final_has_selection = source.final_has_selection;
    target.final_selection = source.final_selection;
    target.final_preedit_clean = source.final_preedit_clean;
}

[[nodiscard]] inline normalized_input_replay_ime_summary summarize_normalized_input_replay_ime_routes(
    std::span<const action_route_policy_diagnostic> routes,
    std::span<const input_event> events,
    const normalized_input_replay_end_state& end_state)
{
    normalized_input_replay_ime_summary summary;
    apply_normalized_input_replay_ime_final_state(summary, end_state);

    for (const action_route_policy_diagnostic& route : routes) {
        if (!normalized_input_replay_ime_route_kind(route.kind)) {
            continue;
        }

        normalized_input_replay_ime_timeline_entry entry{
            .phase = normalized_input_replay_ime_phase_for(route.kind),
            .timestamp_ms = route.timestamp_ms,
            .emits_input_event = route.emits_input_event,
            .event_index = route.event_index,
            .target_id = route.target_id,
            .composition = route.composition,
            .text_byte_count = route.text_byte_count,
            .text_byte_count_before = route.text_byte_count_before,
            .text_byte_count_after = route.text_byte_count_after,
            .caret_before = route.caret_before,
            .caret_after = route.caret_after,
            .had_selection_before = route.had_selection_before,
            .has_selection_after = route.has_selection_after,
            .selection_before = route.selection_before,
            .selection_after = route.selection_after,
            .committed_text_after = end_state.text,
            .display_text_after = end_state.display_text,
            .preedit_text_after = end_state.preedit_text,
        };

        if (const ime_event* event = normalized_input_replay_ime_event_at(events, route.event_index)) {
            entry.utf8_text = event->utf8_text;
            if (event->kind == ime_event_kind::commit) {
                entry.committed_text = event->utf8_text;
            }
        }

        entry.preedit_text_valid = normalized_input_replay_preedit_text_valid(route.composition)
            && detail::valid_utf8(entry.utf8_text)
            && detail::valid_utf8(entry.committed_text);
        entry.preedit_range_valid = normalized_input_replay_composition_range_valid(route.composition);
        if (entry.phase == normalized_input_replay_ime_timeline_phase::commit
            || entry.phase == normalized_input_replay_ime_timeline_phase::cancel) {
            entry.stale_preedit_cleared_after = end_state.preedit_clean;
        }

        ++summary.total;
        if (route.emits_input_event) {
            ++summary.emitted_input_event_routes;
        } else {
            ++summary.diagnostic_only_routes;
        }
        count_normalized_input_replay_ime_phase(summary.phases, entry.phase);
        summary.all_preedit_text_valid = summary.all_preedit_text_valid && entry.preedit_text_valid;
        summary.all_preedit_ranges_valid = summary.all_preedit_ranges_valid && entry.preedit_range_valid;
        summary.stale_preedit_cleared = summary.stale_preedit_cleared && entry.stale_preedit_cleared_after;
        summary.timeline.push_back(std::move(entry));
    }

    return summary;
}

[[nodiscard]] inline bool normalized_input_replay_pointer_route_kind(action_route_policy_kind kind)
{
    return kind == action_route_policy_kind::pointer_capture_arbitration
        || kind == action_route_policy_kind::pointer_capture_reset
        || kind == action_route_policy_kind::gesture_route_snapshot
        || kind == action_route_policy_kind::wheel_summary;
}

[[nodiscard]] inline normalized_input_replay_pointer_timeline_kind
normalized_input_replay_pointer_kind_for_gesture(gesture_kind kind)
{
    switch (kind) {
    case gesture_kind::tap:
        return normalized_input_replay_pointer_timeline_kind::tap;
    case gesture_kind::long_press:
        return normalized_input_replay_pointer_timeline_kind::long_press;
    case gesture_kind::swipe_left:
        return normalized_input_replay_pointer_timeline_kind::swipe_left;
    case gesture_kind::swipe_right:
        return normalized_input_replay_pointer_timeline_kind::swipe_right;
    case gesture_kind::drag_start:
        return normalized_input_replay_pointer_timeline_kind::drag_start;
    case gesture_kind::drag_update:
        return normalized_input_replay_pointer_timeline_kind::drag_update;
    case gesture_kind::drag_end:
        return normalized_input_replay_pointer_timeline_kind::drag_end;
    case gesture_kind::drag_cancel:
        return normalized_input_replay_pointer_timeline_kind::drag_cancel;
    }

    return normalized_input_replay_pointer_timeline_kind::gesture_suppressed;
}

[[nodiscard]] inline normalized_input_replay_pointer_timeline_kind
normalized_input_replay_pointer_kind_for_summary(input_event_summary_kind kind)
{
    switch (kind) {
    case input_event_summary_kind::tap:
        return normalized_input_replay_pointer_timeline_kind::tap;
    case input_event_summary_kind::long_press:
        return normalized_input_replay_pointer_timeline_kind::long_press;
    case input_event_summary_kind::swipe_left:
        return normalized_input_replay_pointer_timeline_kind::swipe_left;
    case input_event_summary_kind::swipe_right:
        return normalized_input_replay_pointer_timeline_kind::swipe_right;
    case input_event_summary_kind::drag_start:
        return normalized_input_replay_pointer_timeline_kind::drag_start;
    case input_event_summary_kind::drag_update:
        return normalized_input_replay_pointer_timeline_kind::drag_update;
    case input_event_summary_kind::drag_end:
        return normalized_input_replay_pointer_timeline_kind::drag_end;
    case input_event_summary_kind::drag_cancel:
        return normalized_input_replay_pointer_timeline_kind::drag_cancel;
    case input_event_summary_kind::wheel:
        return normalized_input_replay_pointer_timeline_kind::wheel;
    }

    return normalized_input_replay_pointer_timeline_kind::gesture_suppressed;
}

[[nodiscard]] inline normalized_input_replay_pointer_timeline_kind
normalized_input_replay_pointer_kind_for_policy(const gesture_policy_snapshot& policy)
{
    if (policy.emitted_input_event) {
        return normalized_input_replay_pointer_kind_for_gesture(policy.emitted_kind);
    }

    switch (policy.decision) {
    case gesture_policy_decision::tap_accepted:
        return normalized_input_replay_pointer_timeline_kind::tap;
    case gesture_policy_decision::long_press_accepted:
        return normalized_input_replay_pointer_timeline_kind::long_press;
    case gesture_policy_decision::swipe_accepted:
        if (policy.direction == gesture_direction::left) {
            return normalized_input_replay_pointer_timeline_kind::swipe_left;
        }
        if (policy.direction == gesture_direction::right) {
            return normalized_input_replay_pointer_timeline_kind::swipe_right;
        }
        return normalized_input_replay_pointer_timeline_kind::gesture_suppressed;
    case gesture_policy_decision::drag_started:
        return normalized_input_replay_pointer_timeline_kind::drag_start;
    case gesture_policy_decision::drag_updated:
        return normalized_input_replay_pointer_timeline_kind::drag_update;
    case gesture_policy_decision::drag_released:
        return normalized_input_replay_pointer_timeline_kind::drag_end;
    case gesture_policy_decision::drag_canceled:
        return normalized_input_replay_pointer_timeline_kind::drag_cancel;
    case gesture_policy_decision::none:
    case gesture_policy_decision::tracking_started:
    case gesture_policy_decision::swipe_rejected_distance:
    case gesture_policy_decision::swipe_rejected_cross_axis:
    case gesture_policy_decision::swipe_rejected_duration:
    case gesture_policy_decision::release_suppressed:
    case gesture_policy_decision::ignored_by_capture:
        return normalized_input_replay_pointer_timeline_kind::gesture_suppressed;
    }

    return normalized_input_replay_pointer_timeline_kind::gesture_suppressed;
}

[[nodiscard]] inline normalized_input_replay_pointer_timeline_kind
normalized_input_replay_pointer_kind_for_route(const action_route_policy_diagnostic& route)
{
    switch (route.kind) {
    case action_route_policy_kind::pointer_capture_arbitration:
        return normalized_input_replay_pointer_timeline_kind::pointer_capture_arbitration;
    case action_route_policy_kind::pointer_capture_reset:
        return normalized_input_replay_pointer_timeline_kind::pointer_capture_reset;
    case action_route_policy_kind::wheel_summary:
        return normalized_input_replay_pointer_timeline_kind::wheel;
    case action_route_policy_kind::gesture_route_snapshot:
        if (route.emits_input_event) {
            return normalized_input_replay_pointer_kind_for_summary(route.normalized_event.kind);
        }
        return normalized_input_replay_pointer_kind_for_policy(route.gesture_policy);
    case action_route_policy_kind::text_commit_boundary:
    case action_route_policy_kind::text_backspace_boundary:
    case action_route_policy_kind::text_delete_forward_boundary:
    case action_route_policy_kind::caret_moved:
    case action_route_policy_kind::selection_changed:
    case action_route_policy_kind::focus_traversal_next:
    case action_route_policy_kind::focus_traversal_previous:
    case action_route_policy_kind::text_submit_boundary:
    case action_route_policy_kind::keyboard_cancel_intent:
    case action_route_policy_kind::focus_loss:
    case action_route_policy_kind::ime_preedit:
    case action_route_policy_kind::ime_commit:
    case action_route_policy_kind::ime_cancel:
    case action_route_policy_kind::ime_composition_start:
        return normalized_input_replay_pointer_timeline_kind::gesture_suppressed;
    }

    return normalized_input_replay_pointer_timeline_kind::gesture_suppressed;
}

[[nodiscard]] inline bool normalized_input_replay_pointer_capture_changed(
    const pointer_capture_snapshot& before,
    const pointer_capture_snapshot& after)
{
    return before.lifecycle != after.lifecycle
        || before.active != after.active
        || before.pointer_id != after.pointer_id
        || before.tracked_pointer_count != after.tracked_pointer_count;
}

[[nodiscard]] inline std::int32_t normalized_input_replay_pointer_id_for_route(
    const action_route_policy_diagnostic& route)
{
    if (route.pointer_id != 0) {
        return route.pointer_id;
    }
    if (route.normalized_event.pointer_id != 0) {
        return route.normalized_event.pointer_id;
    }
    return route.gesture_policy.pointer_id;
}

inline void add_unique_normalized_input_replay_pointer_id(
    std::vector<std::int32_t>& ids,
    std::int32_t pointer_id)
{
    if (pointer_id == 0) {
        return;
    }
    for (const std::int32_t existing_id : ids) {
        if (existing_id == pointer_id) {
            return;
        }
    }
    ids.push_back(pointer_id);
}

inline void count_normalized_input_replay_pointer_kind(
    normalized_input_replay_pointer_timeline_counts& counts,
    normalized_input_replay_pointer_timeline_kind kind)
{
    switch (kind) {
    case normalized_input_replay_pointer_timeline_kind::pointer_capture_arbitration:
        ++counts.pointer_capture_arbitration;
        return;
    case normalized_input_replay_pointer_timeline_kind::pointer_capture_reset:
        ++counts.pointer_capture_reset;
        return;
    case normalized_input_replay_pointer_timeline_kind::tap:
        ++counts.tap;
        return;
    case normalized_input_replay_pointer_timeline_kind::long_press:
        ++counts.long_press;
        return;
    case normalized_input_replay_pointer_timeline_kind::swipe_left:
        ++counts.swipe_left;
        return;
    case normalized_input_replay_pointer_timeline_kind::swipe_right:
        ++counts.swipe_right;
        return;
    case normalized_input_replay_pointer_timeline_kind::drag_start:
        ++counts.drag_start;
        return;
    case normalized_input_replay_pointer_timeline_kind::drag_update:
        ++counts.drag_update;
        return;
    case normalized_input_replay_pointer_timeline_kind::drag_end:
        ++counts.drag_end;
        return;
    case normalized_input_replay_pointer_timeline_kind::drag_cancel:
        ++counts.drag_cancel;
        return;
    case normalized_input_replay_pointer_timeline_kind::wheel:
        ++counts.wheel;
        return;
    case normalized_input_replay_pointer_timeline_kind::gesture_suppressed:
        ++counts.gesture_suppressed;
        return;
    }
}

inline void count_normalized_input_replay_pointer_contact(
    normalized_input_replay_pointer_contact_counts& counts,
    pointer_contact_kind contact)
{
    switch (contact) {
    case pointer_contact_kind::unknown:
        ++counts.unknown;
        return;
    case pointer_contact_kind::mouse_like:
        ++counts.mouse_like;
        return;
    case pointer_contact_kind::touch_like:
        ++counts.touch_like;
        return;
    }
}

inline void count_normalized_input_replay_pointer_lifecycle(
    normalized_input_replay_pointer_capture_lifecycle_counts& counts,
    pointer_capture_lifecycle lifecycle)
{
    switch (lifecycle) {
    case pointer_capture_lifecycle::idle:
        ++counts.idle;
        return;
    case pointer_capture_lifecycle::tracking:
        ++counts.tracking;
        return;
    case pointer_capture_lifecycle::captured:
        ++counts.captured;
        return;
    }
}

inline void count_normalized_input_replay_pointer_decision(
    normalized_input_replay_pointer_decision_counts& counts,
    pointer_arbitration_decision decision)
{
    switch (decision) {
    case pointer_arbitration_decision::none:
        ++counts.none;
        return;
    case pointer_arbitration_decision::tracked:
        ++counts.tracked;
        return;
    case pointer_arbitration_decision::captured:
        ++counts.captured;
        return;
    case pointer_arbitration_decision::ignored_by_capture:
        ++counts.ignored_by_capture;
        return;
    case pointer_arbitration_decision::canceled:
        ++counts.canceled;
        return;
    case pointer_arbitration_decision::released:
        ++counts.released;
        return;
    case pointer_arbitration_decision::restarted:
        ++counts.restarted;
        return;
    }
}

inline void apply_normalized_input_replay_pointer_final_state(
    normalized_input_replay_pointer_summary& summary,
    const normalized_input_replay_end_state& end_state)
{
    summary.final_capture = end_state.pointer_capture;
    summary.final_capture_clean = end_state.pointer_capture_clean;
}

inline void accumulate_normalized_input_replay_pointer_summary(
    normalized_input_replay_pointer_summary& target,
    const normalized_input_replay_pointer_summary& source)
{
    target.timeline.insert(target.timeline.end(), source.timeline.begin(), source.timeline.end());
    for (const std::int32_t pointer_id : source.pointer_ids) {
        add_unique_normalized_input_replay_pointer_id(target.pointer_ids, pointer_id);
    }
    for (const std::int32_t pointer_id : source.mouse_pointer_ids) {
        add_unique_normalized_input_replay_pointer_id(target.mouse_pointer_ids, pointer_id);
    }
    for (const std::int32_t pointer_id : source.touch_pointer_ids) {
        add_unique_normalized_input_replay_pointer_id(target.touch_pointer_ids, pointer_id);
    }

    target.kinds.pointer_capture_arbitration += source.kinds.pointer_capture_arbitration;
    target.kinds.pointer_capture_reset += source.kinds.pointer_capture_reset;
    target.kinds.tap += source.kinds.tap;
    target.kinds.long_press += source.kinds.long_press;
    target.kinds.swipe_left += source.kinds.swipe_left;
    target.kinds.swipe_right += source.kinds.swipe_right;
    target.kinds.drag_start += source.kinds.drag_start;
    target.kinds.drag_update += source.kinds.drag_update;
    target.kinds.drag_end += source.kinds.drag_end;
    target.kinds.drag_cancel += source.kinds.drag_cancel;
    target.kinds.wheel += source.kinds.wheel;
    target.kinds.gesture_suppressed += source.kinds.gesture_suppressed;

    target.contacts.unknown += source.contacts.unknown;
    target.contacts.mouse_like += source.contacts.mouse_like;
    target.contacts.touch_like += source.contacts.touch_like;
    target.capture_before_lifecycles.idle += source.capture_before_lifecycles.idle;
    target.capture_before_lifecycles.tracking += source.capture_before_lifecycles.tracking;
    target.capture_before_lifecycles.captured += source.capture_before_lifecycles.captured;
    target.capture_after_lifecycles.idle += source.capture_after_lifecycles.idle;
    target.capture_after_lifecycles.tracking += source.capture_after_lifecycles.tracking;
    target.capture_after_lifecycles.captured += source.capture_after_lifecycles.captured;
    target.decisions.none += source.decisions.none;
    target.decisions.tracked += source.decisions.tracked;
    target.decisions.captured += source.decisions.captured;
    target.decisions.ignored_by_capture += source.decisions.ignored_by_capture;
    target.decisions.canceled += source.decisions.canceled;
    target.decisions.released += source.decisions.released;
    target.decisions.restarted += source.decisions.restarted;
    target.total += source.total;
    target.emitted_input_event_routes += source.emitted_input_event_routes;
    target.diagnostic_only_routes += source.diagnostic_only_routes;
    target.capture_transition_count += source.capture_transition_count;
    target.wheel_routes += source.wheel_routes;
    target.saw_multipointer_touch = target.saw_multipointer_touch
        || source.saw_multipointer_touch
        || target.touch_pointer_ids.size() > 1;
    target.final_capture = source.final_capture;
    target.final_capture_clean = source.final_capture_clean;
}

[[nodiscard]] inline normalized_input_replay_pointer_summary summarize_normalized_input_replay_pointer_routes(
    std::span<const action_route_policy_diagnostic> routes,
    const normalized_input_replay_end_state& end_state)
{
    normalized_input_replay_pointer_summary summary;
    apply_normalized_input_replay_pointer_final_state(summary, end_state);

    for (const action_route_policy_diagnostic& route : routes) {
        if (!normalized_input_replay_pointer_route_kind(route.kind)) {
            continue;
        }

        normalized_input_replay_pointer_timeline_entry entry{
            .kind = normalized_input_replay_pointer_kind_for_route(route),
            .timestamp_ms = route.timestamp_ms,
            .emits_input_event = route.emits_input_event,
            .event_index = route.event_index,
            .pointer_id = normalized_input_replay_pointer_id_for_route(route),
            .event_phase = route.pointer_event_phase,
            .contact = route.pointer_contact,
            .decision = route.pointer_decision,
            .capture_before = route.pointer_capture_before,
            .capture_after = route.pointer_capture_after,
            .tracked_pointer_count_before = route.tracked_pointer_count_before,
            .tracked_pointer_count_after = route.tracked_pointer_count_after,
            .normalized_event = route.normalized_event,
            .gesture_policy = route.gesture_policy,
        };
        entry.capture_changed =
            normalized_input_replay_pointer_capture_changed(entry.capture_before, entry.capture_after);
        entry.capture_ended_cleanly_after = pointer_capture_snapshot_clean(entry.capture_after);

        const bool use_policy_geometry = route.kind != action_route_policy_kind::wheel_summary
            && (!route.emits_input_event
                || entry.kind == normalized_input_replay_pointer_timeline_kind::pointer_capture_arbitration
                || entry.kind == normalized_input_replay_pointer_timeline_kind::pointer_capture_reset);
        if (use_policy_geometry) {
            entry.duration_ms = route.gesture_policy.duration_ms;
            entry.start_x = route.gesture_policy.start_x;
            entry.start_y = route.gesture_policy.start_y;
            entry.x = route.gesture_policy.x;
            entry.y = route.gesture_policy.y;
            entry.delta_x = route.gesture_policy.delta_x;
            entry.delta_y = route.gesture_policy.delta_y;
        } else {
            entry.duration_ms = route.normalized_event.duration_ms;
            entry.start_x = route.normalized_event.start_x;
            entry.start_y = route.normalized_event.start_y;
            entry.x = route.normalized_event.x;
            entry.y = route.normalized_event.y;
            entry.delta_x = route.normalized_event.delta_x;
            entry.delta_y = route.normalized_event.delta_y;
            entry.pixel_delta_x = route.normalized_event.pixel_delta_x;
            entry.pixel_delta_y = route.normalized_event.pixel_delta_y;
            entry.line_delta_x = route.normalized_event.line_delta_x;
            entry.line_delta_y = route.normalized_event.line_delta_y;
        }

        ++summary.total;
        if (route.emits_input_event) {
            ++summary.emitted_input_event_routes;
        } else {
            ++summary.diagnostic_only_routes;
        }
        if (entry.kind == normalized_input_replay_pointer_timeline_kind::wheel) {
            ++summary.wheel_routes;
        }
        if (entry.capture_changed) {
            ++summary.capture_transition_count;
        }
        count_normalized_input_replay_pointer_kind(summary.kinds, entry.kind);
        count_normalized_input_replay_pointer_contact(summary.contacts, entry.contact);
        count_normalized_input_replay_pointer_lifecycle(
            summary.capture_before_lifecycles,
            entry.capture_before.lifecycle);
        count_normalized_input_replay_pointer_lifecycle(
            summary.capture_after_lifecycles,
            entry.capture_after.lifecycle);
        count_normalized_input_replay_pointer_decision(summary.decisions, entry.decision);

        add_unique_normalized_input_replay_pointer_id(summary.pointer_ids, entry.pointer_id);
        if (entry.contact == pointer_contact_kind::mouse_like) {
            add_unique_normalized_input_replay_pointer_id(summary.mouse_pointer_ids, entry.pointer_id);
        }
        if (entry.contact == pointer_contact_kind::touch_like) {
            add_unique_normalized_input_replay_pointer_id(summary.touch_pointer_ids, entry.pointer_id);
        }
        summary.saw_multipointer_touch = summary.saw_multipointer_touch
            || summary.touch_pointer_ids.size() > 1
            || entry.tracked_pointer_count_before > 1
            || entry.tracked_pointer_count_after > 1;
        summary.timeline.push_back(std::move(entry));
    }

    return summary;
}

[[nodiscard]] inline text_range normalized_input_replay_caret_range_for_offset(std::size_t offset)
{
    return text_range{
        .start_byte = offset,
        .end_byte = offset,
    };
}

[[nodiscard]] inline bool normalized_input_replay_same_text_range(text_range lhs, text_range rhs)
{
    return lhs.start_byte == rhs.start_byte && lhs.end_byte == rhs.end_byte;
}

[[nodiscard]] inline bool normalized_input_replay_focus_route_kind(action_route_policy_kind kind)
{
    return kind == action_route_policy_kind::focus_loss
        || kind == action_route_policy_kind::focus_traversal_next
        || kind == action_route_policy_kind::focus_traversal_previous
        || kind == action_route_policy_kind::caret_moved
        || kind == action_route_policy_kind::selection_changed;
}

[[nodiscard]] inline bool normalized_input_replay_focus_event_kind(text_event_kind kind)
{
    return kind == text_event_kind::focus_gained;
}

[[nodiscard]] inline normalized_input_replay_focus_timeline_kind normalized_input_replay_focus_kind_for_route(
    action_route_policy_kind kind)
{
    switch (kind) {
    case action_route_policy_kind::focus_loss:
        return normalized_input_replay_focus_timeline_kind::focus_loss;
    case action_route_policy_kind::focus_traversal_next:
        return normalized_input_replay_focus_timeline_kind::focus_traversal_next;
    case action_route_policy_kind::focus_traversal_previous:
        return normalized_input_replay_focus_timeline_kind::focus_traversal_previous;
    case action_route_policy_kind::caret_moved:
        return normalized_input_replay_focus_timeline_kind::caret_moved;
    case action_route_policy_kind::selection_changed:
        return normalized_input_replay_focus_timeline_kind::selection_changed;
    case action_route_policy_kind::pointer_capture_reset:
    case action_route_policy_kind::pointer_capture_arbitration:
    case action_route_policy_kind::wheel_summary:
    case action_route_policy_kind::gesture_route_snapshot:
    case action_route_policy_kind::text_commit_boundary:
    case action_route_policy_kind::text_backspace_boundary:
    case action_route_policy_kind::text_delete_forward_boundary:
    case action_route_policy_kind::text_submit_boundary:
    case action_route_policy_kind::keyboard_cancel_intent:
    case action_route_policy_kind::ime_preedit:
    case action_route_policy_kind::ime_commit:
    case action_route_policy_kind::ime_cancel:
    case action_route_policy_kind::ime_composition_start:
        return normalized_input_replay_focus_timeline_kind::caret_moved;
    }

    return normalized_input_replay_focus_timeline_kind::caret_moved;
}

[[nodiscard]] inline normalized_input_replay_focus_timeline_kind normalized_input_replay_focus_kind_for_event(
    text_event_kind kind)
{
    switch (kind) {
    case text_event_kind::focus_gained:
        return normalized_input_replay_focus_timeline_kind::focus_gain;
    case text_event_kind::focus_lost:
        return normalized_input_replay_focus_timeline_kind::focus_loss;
    case text_event_kind::caret_moved:
        return normalized_input_replay_focus_timeline_kind::caret_moved;
    case text_event_kind::selection_changed:
        return normalized_input_replay_focus_timeline_kind::selection_changed;
    case text_event_kind::commit:
    case text_event_kind::backspace:
    case text_event_kind::delete_forward:
    case text_event_kind::submit:
    case text_event_kind::cancel:
        return normalized_input_replay_focus_timeline_kind::caret_moved;
    }

    return normalized_input_replay_focus_timeline_kind::caret_moved;
}

inline void count_normalized_input_replay_focus_kind(
    normalized_input_replay_focus_timeline_counts& counts,
    normalized_input_replay_focus_timeline_kind kind)
{
    switch (kind) {
    case normalized_input_replay_focus_timeline_kind::focus_gain:
        ++counts.focus_gain;
        return;
    case normalized_input_replay_focus_timeline_kind::focus_loss:
        ++counts.focus_loss;
        return;
    case normalized_input_replay_focus_timeline_kind::focus_traversal_next:
        ++counts.focus_traversal_next;
        return;
    case normalized_input_replay_focus_timeline_kind::focus_traversal_previous:
        ++counts.focus_traversal_previous;
        return;
    case normalized_input_replay_focus_timeline_kind::caret_moved:
        ++counts.caret_moved;
        return;
    case normalized_input_replay_focus_timeline_kind::selection_changed:
        ++counts.selection_changed;
        return;
    }
}

inline void apply_normalized_input_replay_focus_entry_state(
    normalized_input_replay_focus_timeline_entry& entry,
    const normalized_input_replay_end_state& before_state,
    const normalized_input_replay_end_state& end_state)
{
    entry.target_id_before = before_state.focus_id;
    entry.target_id_after = end_state.focus_id;
    entry.had_focus_before = before_state.has_text_focus;
    entry.has_focus_after = end_state.has_text_focus;
    entry.target_changed = entry.target_id_before != entry.target_id_after
        || entry.had_focus_before != entry.has_focus_after;
    entry.focus_clean_after = end_state.focus_clean;
}

inline void finish_normalized_input_replay_focus_entry(
    normalized_input_replay_focus_timeline_entry& entry)
{
    entry.caret_changed = !normalized_input_replay_same_text_range(entry.caret_before, entry.caret_after);
    entry.selection_changed = entry.had_selection_before != entry.has_selection_after
        || !normalized_input_replay_same_text_range(entry.selection_before, entry.selection_after);
}

inline void apply_normalized_input_replay_focus_final_state(
    normalized_input_replay_focus_summary& summary,
    const normalized_input_replay_end_state& end_state)
{
    summary.final_has_focus = end_state.has_text_focus;
    summary.final_focus_id = end_state.focus_id;
    summary.final_text_byte_count = end_state.text.size();
    summary.final_caret = normalized_input_replay_caret_range_for_offset(end_state.caret_byte_offset);
    summary.final_has_selection = end_state.has_selection;
    summary.final_selection = end_state.selection;
    summary.final_focus_clean = end_state.focus_clean;
}

inline void add_normalized_input_replay_focus_entry(
    normalized_input_replay_focus_summary& summary,
    normalized_input_replay_focus_timeline_entry entry)
{
    ++summary.total;
    if (entry.emits_input_event) {
        ++summary.emitted_input_event_routes;
    } else {
        ++summary.diagnostic_only_routes;
    }
    if (entry.target_changed) {
        ++summary.target_transition_count;
    }
    if (entry.caret_changed) {
        ++summary.caret_transition_count;
    }
    if (entry.selection_changed) {
        ++summary.selection_transition_count;
    }
    count_normalized_input_replay_focus_kind(summary.kinds, entry.kind);
    summary.timeline.push_back(std::move(entry));
}

inline void accumulate_normalized_input_replay_focus_summary(
    normalized_input_replay_focus_summary& target,
    const normalized_input_replay_focus_summary& source)
{
    target.timeline.insert(target.timeline.end(), source.timeline.begin(), source.timeline.end());
    target.kinds.focus_gain += source.kinds.focus_gain;
    target.kinds.focus_loss += source.kinds.focus_loss;
    target.kinds.focus_traversal_next += source.kinds.focus_traversal_next;
    target.kinds.focus_traversal_previous += source.kinds.focus_traversal_previous;
    target.kinds.caret_moved += source.kinds.caret_moved;
    target.kinds.selection_changed += source.kinds.selection_changed;
    target.total += source.total;
    target.emitted_input_event_routes += source.emitted_input_event_routes;
    target.diagnostic_only_routes += source.diagnostic_only_routes;
    target.target_transition_count += source.target_transition_count;
    target.caret_transition_count += source.caret_transition_count;
    target.selection_transition_count += source.selection_transition_count;
    target.final_has_focus = source.final_has_focus;
    target.final_focus_id = source.final_focus_id;
    target.final_text_byte_count = source.final_text_byte_count;
    target.final_caret = source.final_caret;
    target.final_has_selection = source.final_has_selection;
    target.final_selection = source.final_selection;
    target.final_focus_clean = source.final_focus_clean;
}

[[nodiscard]] inline normalized_input_replay_focus_summary summarize_normalized_input_replay_focus_routes(
    std::span<const action_route_policy_diagnostic> routes,
    std::span<const input_event> events,
    const normalized_input_replay_end_state& before_state,
    const normalized_input_replay_end_state& end_state)
{
    normalized_input_replay_focus_summary summary;
    apply_normalized_input_replay_focus_final_state(summary, end_state);

    for (std::size_t index = 0; index < events.size(); ++index) {
        if (!std::holds_alternative<text_event>(events[index])) {
            continue;
        }

        const text_event& event = std::get<text_event>(events[index]);
        if (!normalized_input_replay_focus_event_kind(event.kind)) {
            continue;
        }

        normalized_input_replay_focus_timeline_entry entry{
            .kind = normalized_input_replay_focus_kind_for_event(event.kind),
            .timestamp_ms = event.timestamp_ms,
            .emits_input_event = true,
            .event_index = index,
            .target_id = event.target_id,
            .text_byte_count_before = before_state.text.size(),
            .text_byte_count_after = end_state.text.size(),
            .caret_before = normalized_input_replay_caret_range_for_offset(before_state.caret_byte_offset),
            .caret_after = normalized_input_replay_caret_range_for_offset(end_state.caret_byte_offset),
            .had_selection_before = before_state.has_selection,
            .has_selection_after = end_state.has_selection,
            .selection_before = before_state.selection,
            .selection_after = end_state.selection,
        };
        apply_normalized_input_replay_focus_entry_state(entry, before_state, end_state);
        finish_normalized_input_replay_focus_entry(entry);
        add_normalized_input_replay_focus_entry(summary, std::move(entry));
    }

    for (const action_route_policy_diagnostic& route : routes) {
        if (!normalized_input_replay_focus_route_kind(route.kind)) {
            continue;
        }

        normalized_input_replay_focus_timeline_entry entry{
            .kind = normalized_input_replay_focus_kind_for_route(route.kind),
            .timestamp_ms = route.timestamp_ms,
            .emits_input_event = route.emits_input_event,
            .event_index = route.event_index,
            .target_id = route.target_id,
            .text_byte_count_before = route.text_byte_count_before,
            .text_byte_count_after = route.text_byte_count_after,
            .caret_before = route.caret_before,
            .caret_after = route.caret_after,
            .had_selection_before = route.had_selection_before,
            .has_selection_after = route.has_selection_after,
            .selection_before = route.selection_before,
            .selection_after = route.selection_after,
            .keyboard = route.keyboard,
        };
        apply_normalized_input_replay_focus_entry_state(entry, before_state, end_state);
        finish_normalized_input_replay_focus_entry(entry);
        add_normalized_input_replay_focus_entry(summary, std::move(entry));
    }

    return summary;
}

[[nodiscard]] inline normalized_input_replay_end_state capture_normalized_input_replay_end_state(
    const input_engine& engine)
{
    const input_routing_diagnostics& diagnostics = engine.routing_diagnostics();
    const text_input_model& text = engine.text_model();
    normalized_input_replay_end_state state{
        .pointer_capture = diagnostics.pointer_capture,
        .has_text_focus = engine.has_text_focus(),
        .focus_id = engine.text_focus_id(),
        .text = text.text(),
        .display_text = text.display_text(),
        .caret_byte_offset = text.caret_byte_offset(),
        .preedit_text = text.preedit_text(),
        .composition = text.ime_composition(),
    };
    if (const auto selection = text.selection_range()) {
        state.has_selection = true;
        state.selection = *selection;
    }
    state.pointer_capture_clean = pointer_capture_snapshot_clean(state.pointer_capture);
    state.focus_clean = state.has_text_focus || state.focus_id.empty();
    state.preedit_clean = !state.composition.active && text.preedit_text().empty();
    return state;
}

[[nodiscard]] inline normalized_input_replay_batch record_normalized_input_batch(
    input_engine& engine,
    std::string label,
    const normalized_input_replay_action& action)
{
    const normalized_input_replay_end_state before_state =
        capture_normalized_input_replay_end_state(engine);
    std::vector<input_event> events = std::visit(
        [&engine](const auto& replay_action) -> std::vector<input_event> {
            using action_type = std::decay_t<decltype(replay_action)>;
            if constexpr (std::is_same_v<action_type, raw_platform_input_event>) {
                return engine.process_raw_event(replay_action);
            } else if constexpr (std::is_same_v<action_type, raw_scroll_event>) {
                return engine.process_scroll_event(replay_action);
            } else {
                return engine.update_time(replay_action.timestamp_ms);
            }
        },
        action);

    const input_routing_diagnostics& diagnostics = engine.routing_diagnostics();
    normalized_input_replay_end_state end_state = capture_normalized_input_replay_end_state(engine);
    normalized_input_replay_ime_summary ime =
        summarize_normalized_input_replay_ime_routes(diagnostics.action_routes, events, end_state);
    normalized_input_replay_pointer_summary pointer =
        summarize_normalized_input_replay_pointer_routes(diagnostics.action_routes, end_state);
    normalized_input_replay_focus_summary focus =
        summarize_normalized_input_replay_focus_routes(
            diagnostics.action_routes,
            events,
            before_state,
            end_state);
    return normalized_input_replay_batch{
        .label = std::move(label),
        .input_events = std::move(events),
        .normalized_events = diagnostics.normalized_events,
        .summary = diagnostics.summary,
        .keyboard = summarize_normalized_input_replay_keyboard_routes(diagnostics.action_routes),
        .ime = std::move(ime),
        .pointer = std::move(pointer),
        .focus = std::move(focus),
        .end_state = std::move(end_state),
    };
}

[[nodiscard]] inline normalized_input_replay_recording replay_normalized_input_fixture(
    input_engine& engine,
    std::span<const normalized_input_replay_step> steps,
    const normalized_input_replay_options& options = {})
{
    if (!options.initial_focus_target_id.empty()) {
        engine.focus_text_target(options.initial_focus_target_id);
    }

    normalized_input_replay_recording recording;
    recording.batches.reserve(steps.size());
    for (const normalized_input_replay_step& step : steps) {
        normalized_input_replay_batch batch =
        record_normalized_input_batch(engine, step.label, step.action);
        accumulate_input_diagnostic_summary(recording.summary, batch.summary);
        accumulate_normalized_input_replay_keyboard_summary(recording.keyboard, batch.keyboard);
        accumulate_normalized_input_replay_ime_summary(recording.ime, batch.ime);
        accumulate_normalized_input_replay_pointer_summary(recording.pointer, batch.pointer);
        accumulate_normalized_input_replay_focus_summary(recording.focus, batch.focus);
        recording.final_state = batch.end_state;
        recording.batches.push_back(std::move(batch));
    }
    if (steps.empty()) {
        recording.final_state = capture_normalized_input_replay_end_state(engine);
        recording.ime = summarize_normalized_input_replay_ime_routes(
            std::span<const action_route_policy_diagnostic>{},
            std::span<const input_event>{},
            recording.final_state);
        recording.pointer = summarize_normalized_input_replay_pointer_routes(
            std::span<const action_route_policy_diagnostic>{},
            recording.final_state);
        recording.focus = summarize_normalized_input_replay_focus_routes(
            std::span<const action_route_policy_diagnostic>{},
            std::span<const input_event>{},
            recording.final_state,
            recording.final_state);
    }
    return recording;
}

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
    };
    diff.focus_changed = diff.has_focus.changed || diff.focus_id.changed || diff.focus_clean.changed;
    diff.caret_changed = diff.caret.changed;
    diff.selection_changed = diff.has_selection.changed || diff.selection.changed;
    diff.text_changed = diff.text.changed;
    diff.display_text_changed = diff.display_text.changed;
    diff.preedit_changed = diff.preedit_text.changed || diff.preedit_clean.changed;
    diff.pointer_capture_changed = diff.pointer_capture.changed || diff.pointer_capture.clean_changed;
    diff.changed = diff.focus_changed
        || diff.caret_changed
        || diff.selection_changed
        || diff.text_changed
        || diff.display_text_changed
        || diff.preedit_changed
        || diff.pointer_capture_changed;
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
        .ime_timeline_changed = ime.timeline_changed,
        .keyboard_changed = keyboard.changed,
        .text_or_preedit_changed = final_state.text_changed
            || final_state.display_text_changed
            || final_state.preedit_changed
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
        .ime = diff_normalized_input_replay_ime(before.ime, after.ime),
        .focus = diff_normalized_input_replay_focus(before.focus, after.focus),
    };
    diff.regression = summarize_normalized_input_replay_diff(
        before,
        after,
        diff.final_state,
        diff.keyboard,
        diff.pointer,
        diff.ime,
        diff.focus);
    return diff;
}

} // namespace quiz_vulkan::input
