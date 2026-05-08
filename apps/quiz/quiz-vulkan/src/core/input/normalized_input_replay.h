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

} // namespace quiz_vulkan::input

#include "core/input/normalized_input_replay_diff.h"
