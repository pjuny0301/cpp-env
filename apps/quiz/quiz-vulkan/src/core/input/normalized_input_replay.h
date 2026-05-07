#pragma once

#include "core/input/input_engine.h"

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

struct normalized_input_replay_end_state {
    pointer_capture_snapshot pointer_capture;
    bool has_text_focus = false;
    std::string focus_id;
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
    normalized_input_replay_end_state end_state;
};

struct normalized_input_replay_recording {
    std::vector<normalized_input_replay_batch> batches;
    input_diagnostic_summary summary;
    normalized_input_replay_end_state final_state;
};

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
        .composition = text.ime_composition(),
    };
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
        recording.final_state = batch.end_state;
        recording.batches.push_back(std::move(batch));
    }
    if (steps.empty()) {
        recording.final_state = capture_normalized_input_replay_end_state(engine);
    }
    return recording;
}

} // namespace quiz_vulkan::input
