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
    normalized_input_replay_end_state end_state;
};

struct normalized_input_replay_recording {
    std::vector<normalized_input_replay_batch> batches;
    input_diagnostic_summary summary;
    normalized_input_replay_keyboard_summary keyboard;
    normalized_input_replay_ime_summary ime;
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
    return normalized_input_replay_batch{
        .label = std::move(label),
        .input_events = std::move(events),
        .normalized_events = diagnostics.normalized_events,
        .summary = diagnostics.summary,
        .keyboard = summarize_normalized_input_replay_keyboard_routes(diagnostics.action_routes),
        .ime = std::move(ime),
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
        recording.final_state = batch.end_state;
        recording.batches.push_back(std::move(batch));
    }
    if (steps.empty()) {
        recording.final_state = capture_normalized_input_replay_end_state(engine);
        recording.ime = summarize_normalized_input_replay_ime_routes(
            std::span<const action_route_policy_diagnostic>{},
            std::span<const input_event>{},
            recording.final_state);
    }
    return recording;
}

} // namespace quiz_vulkan::input
