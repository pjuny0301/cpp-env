#pragma once

#include "core/input/input_engine.h"

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
    normalized_input_replay_end_state end_state;
};

struct normalized_input_replay_recording {
    std::vector<normalized_input_replay_batch> batches;
    input_diagnostic_summary summary;
    normalized_input_replay_keyboard_summary keyboard;
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
    return normalized_input_replay_batch{
        .label = std::move(label),
        .input_events = std::move(events),
        .normalized_events = diagnostics.normalized_events,
        .summary = diagnostics.summary,
        .keyboard = summarize_normalized_input_replay_keyboard_routes(diagnostics.action_routes),
        .end_state = capture_normalized_input_replay_end_state(engine),
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
        recording.final_state = batch.end_state;
        recording.batches.push_back(std::move(batch));
    }
    if (steps.empty()) {
        recording.final_state = capture_normalized_input_replay_end_state(engine);
    }
    return recording;
}

} // namespace quiz_vulkan::input
