#pragma once

#include "core/input/normalized_input_replay.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::input {

enum class input_action_candidate_kind {
    text_edit,
    focus_move,
    pointer_capture,
    gesture_candidate,
    wheel_scroll,
    ime_composition_start,
    ime_preedit,
    ime_commit,
    ime_cancel,
};

struct input_action_candidate_counts {
    std::size_t text_edit = 0;
    std::size_t focus_move = 0;
    std::size_t pointer_capture = 0;
    std::size_t gesture_candidate = 0;
    std::size_t wheel_scroll = 0;
    std::size_t ime_composition_start = 0;
    std::size_t ime_preedit = 0;
    std::size_t ime_commit = 0;
    std::size_t ime_cancel = 0;
    std::size_t total = 0;
};

struct input_action_candidate {
    input_action_candidate_kind kind = input_action_candidate_kind::text_edit;
    std::string batch_label;
    std::size_t batch_index = 0;
    std::size_t timeline_index = 0;
    std::int64_t timestamp_ms = 0;
    bool emits_input_event = false;
    std::size_t event_index = 0;
    std::string target_id;

    normalized_input_replay_focus_timeline_kind focus_kind =
        normalized_input_replay_focus_timeline_kind::caret_moved;
    std::string target_id_before;
    std::string target_id_after;
    bool had_focus_before = false;
    bool has_focus_after = false;
    bool target_changed = false;

    normalized_input_replay_pointer_timeline_kind pointer_kind =
        normalized_input_replay_pointer_timeline_kind::gesture_suppressed;
    std::int32_t pointer_id = 0;
    pointer_phase event_phase = pointer_phase::down;
    pointer_contact_kind pointer_contact = pointer_contact_kind::unknown;
    pointer_arbitration_decision pointer_decision = pointer_arbitration_decision::none;
    pointer_capture_snapshot capture_before;
    pointer_capture_snapshot capture_after;
    bool capture_changed = false;
    bool capture_ended_cleanly_after = true;
    std::size_t tracked_pointer_count_before = 0;
    std::size_t tracked_pointer_count_after = 0;

    normalized_input_replay_ime_timeline_phase ime_phase =
        normalized_input_replay_ime_timeline_phase::preedit_update;
    ime_composition_state composition;
    std::string utf8_text;
    std::string committed_text;
    bool preedit_text_valid = true;
    bool preedit_range_valid = true;
    bool stale_preedit_cleared_after = true;

    keyboard_chord_diagnostic keyboard;
    normalized_input_event_summary normalized_event;
    gesture_policy_snapshot gesture_policy;
    text_input_presentation_diff text_presentation_diff;
    normalized_input_replay_end_state end_state;

    std::size_t text_byte_count = 0;
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

struct input_action_candidate_batch_plan {
    std::string label;
    std::vector<input_action_candidate> candidates;
    input_action_candidate_counts counts;
};

struct input_action_candidate_plan {
    std::vector<input_action_candidate_batch_plan> batches;
    std::vector<input_action_candidate> candidates;
    input_action_candidate_counts counts;
    normalized_input_replay_end_state final_state;
};

inline void count_input_action_candidate_kind(
    input_action_candidate_counts& counts,
    input_action_candidate_kind kind)
{
    ++counts.total;
    switch (kind) {
    case input_action_candidate_kind::text_edit:
        ++counts.text_edit;
        return;
    case input_action_candidate_kind::focus_move:
        ++counts.focus_move;
        return;
    case input_action_candidate_kind::pointer_capture:
        ++counts.pointer_capture;
        return;
    case input_action_candidate_kind::gesture_candidate:
        ++counts.gesture_candidate;
        return;
    case input_action_candidate_kind::wheel_scroll:
        ++counts.wheel_scroll;
        return;
    case input_action_candidate_kind::ime_composition_start:
        ++counts.ime_composition_start;
        return;
    case input_action_candidate_kind::ime_preedit:
        ++counts.ime_preedit;
        return;
    case input_action_candidate_kind::ime_commit:
        ++counts.ime_commit;
        return;
    case input_action_candidate_kind::ime_cancel:
        ++counts.ime_cancel;
        return;
    }
}

[[nodiscard]] inline input_action_candidate_kind input_action_candidate_kind_for_ime_phase(
    normalized_input_replay_ime_timeline_phase phase)
{
    switch (phase) {
    case normalized_input_replay_ime_timeline_phase::composition_start:
        return input_action_candidate_kind::ime_composition_start;
    case normalized_input_replay_ime_timeline_phase::preedit_update:
        return input_action_candidate_kind::ime_preedit;
    case normalized_input_replay_ime_timeline_phase::commit:
        return input_action_candidate_kind::ime_commit;
    case normalized_input_replay_ime_timeline_phase::cancel:
        return input_action_candidate_kind::ime_cancel;
    }
    return input_action_candidate_kind::ime_preedit;
}

[[nodiscard]] inline bool input_action_candidate_pointer_kind_is_capture(
    normalized_input_replay_pointer_timeline_kind kind)
{
    return kind == normalized_input_replay_pointer_timeline_kind::pointer_capture_arbitration
        || kind == normalized_input_replay_pointer_timeline_kind::pointer_capture_reset;
}

[[nodiscard]] inline bool input_action_candidate_pointer_kind_is_wheel(
    normalized_input_replay_pointer_timeline_kind kind)
{
    return kind == normalized_input_replay_pointer_timeline_kind::wheel;
}

[[nodiscard]] inline bool input_action_candidate_pointer_kind_is_gesture(
    normalized_input_replay_pointer_timeline_kind kind)
{
    return !input_action_candidate_pointer_kind_is_capture(kind)
        && !input_action_candidate_pointer_kind_is_wheel(kind);
}

[[nodiscard]] inline std::int64_t input_action_candidate_timestamp_for_event(
    const input_event& event)
{
    return std::visit(
        [](const auto& value) {
            return value.timestamp_ms;
        },
        event);
}

[[nodiscard]] inline std::int64_t input_action_candidate_timestamp_for_batch(
    const normalized_input_replay_batch& batch)
{
    if (!batch.input_events.empty()) {
        return input_action_candidate_timestamp_for_event(batch.input_events.front());
    }
    if (!batch.focus.timeline.empty()) {
        return batch.focus.timeline.front().timestamp_ms;
    }
    if (!batch.ime.timeline.empty()) {
        return batch.ime.timeline.front().timestamp_ms;
    }
    if (!batch.pointer.timeline.empty()) {
        return batch.pointer.timeline.front().timestamp_ms;
    }
    return 0;
}

[[nodiscard]] inline bool input_action_candidate_batch_has_text_edit(
    const normalized_input_replay_batch& batch)
{
    return batch.summary.routes.text > 0 && batch.text_presentation_diff.changed;
}

inline void append_input_action_candidate(
    input_action_candidate_plan& plan,
    input_action_candidate_batch_plan& batch_plan,
    input_action_candidate candidate)
{
    count_input_action_candidate_kind(batch_plan.counts, candidate.kind);
    count_input_action_candidate_kind(plan.counts, candidate.kind);
    plan.candidates.push_back(candidate);
    batch_plan.candidates.push_back(std::move(candidate));
}

[[nodiscard]] inline input_action_candidate make_text_edit_input_action_candidate(
    const normalized_input_replay_batch& batch,
    std::size_t batch_index)
{
    return input_action_candidate{
        .kind = input_action_candidate_kind::text_edit,
        .batch_label = batch.label,
        .batch_index = batch_index,
        .timestamp_ms = input_action_candidate_timestamp_for_batch(batch),
        .target_id = batch.end_state.text_presentation.target_id,
        .text_presentation_diff = batch.text_presentation_diff,
        .end_state = batch.end_state,
        .text_byte_count = batch.end_state.text.size(),
        .text_byte_count_before =
            batch.text_presentation_diff.byte_counts.committed_text_bytes.before_count,
        .text_byte_count_after =
            batch.text_presentation_diff.byte_counts.committed_text_bytes.after_count,
        .caret_before = batch.text_presentation_diff.caret_range.before_range,
        .caret_after = batch.text_presentation_diff.caret_range.after_range,
        .caret_changed = batch.text_presentation_diff.caret_changed,
        .had_selection_before = batch.text_presentation_diff.has_selection.before_value,
        .has_selection_after = batch.text_presentation_diff.has_selection.after_value,
        .selection_before = batch.text_presentation_diff.selection_range.before_range,
        .selection_after = batch.text_presentation_diff.selection_range.after_range,
        .selection_changed = batch.text_presentation_diff.selection_changed,
    };
}

[[nodiscard]] inline input_action_candidate make_focus_move_input_action_candidate(
    const normalized_input_replay_batch& batch,
    const normalized_input_replay_focus_timeline_entry& entry,
    std::size_t batch_index,
    std::size_t timeline_index)
{
    return input_action_candidate{
        .kind = input_action_candidate_kind::focus_move,
        .batch_label = batch.label,
        .batch_index = batch_index,
        .timeline_index = timeline_index,
        .timestamp_ms = entry.timestamp_ms,
        .emits_input_event = entry.emits_input_event,
        .event_index = entry.event_index,
        .target_id = entry.target_id,
        .focus_kind = entry.kind,
        .target_id_before = entry.target_id_before,
        .target_id_after = entry.target_id_after,
        .had_focus_before = entry.had_focus_before,
        .has_focus_after = entry.has_focus_after,
        .target_changed = entry.target_changed,
        .keyboard = entry.keyboard,
        .text_byte_count_before = entry.text_byte_count_before,
        .text_byte_count_after = entry.text_byte_count_after,
        .caret_before = entry.caret_before,
        .caret_after = entry.caret_after,
        .caret_changed = entry.caret_changed,
        .had_selection_before = entry.had_selection_before,
        .has_selection_after = entry.has_selection_after,
        .selection_before = entry.selection_before,
        .selection_after = entry.selection_after,
        .selection_changed = entry.selection_changed,
    };
}

[[nodiscard]] inline input_action_candidate make_ime_input_action_candidate(
    const normalized_input_replay_batch& batch,
    const normalized_input_replay_ime_timeline_entry& entry,
    std::size_t batch_index,
    std::size_t timeline_index)
{
    return input_action_candidate{
        .kind = input_action_candidate_kind_for_ime_phase(entry.phase),
        .batch_label = batch.label,
        .batch_index = batch_index,
        .timeline_index = timeline_index,
        .timestamp_ms = entry.timestamp_ms,
        .emits_input_event = entry.emits_input_event,
        .event_index = entry.event_index,
        .target_id = entry.target_id,
        .ime_phase = entry.phase,
        .composition = entry.composition,
        .utf8_text = entry.utf8_text,
        .committed_text = entry.committed_text,
        .preedit_text_valid = entry.preedit_text_valid,
        .preedit_range_valid = entry.preedit_range_valid,
        .stale_preedit_cleared_after = entry.stale_preedit_cleared_after,
        .text_byte_count = entry.text_byte_count,
        .text_byte_count_before = entry.text_byte_count_before,
        .text_byte_count_after = entry.text_byte_count_after,
        .caret_before = entry.caret_before,
        .caret_after = entry.caret_after,
        .had_selection_before = entry.had_selection_before,
        .has_selection_after = entry.has_selection_after,
        .selection_before = entry.selection_before,
        .selection_after = entry.selection_after,
    };
}

[[nodiscard]] inline input_action_candidate make_pointer_input_action_candidate(
    const normalized_input_replay_batch& batch,
    const normalized_input_replay_pointer_timeline_entry& entry,
    input_action_candidate_kind kind,
    std::size_t batch_index,
    std::size_t timeline_index)
{
    return input_action_candidate{
        .kind = kind,
        .batch_label = batch.label,
        .batch_index = batch_index,
        .timeline_index = timeline_index,
        .timestamp_ms = entry.timestamp_ms,
        .emits_input_event = entry.emits_input_event,
        .event_index = entry.event_index,
        .target_id = batch.end_state.focus_id,
        .pointer_kind = entry.kind,
        .pointer_id = entry.pointer_id,
        .event_phase = entry.event_phase,
        .pointer_contact = entry.contact,
        .pointer_decision = entry.decision,
        .capture_before = entry.capture_before,
        .capture_after = entry.capture_after,
        .capture_changed = entry.capture_changed,
        .capture_ended_cleanly_after = entry.capture_ended_cleanly_after,
        .tracked_pointer_count_before = entry.tracked_pointer_count_before,
        .tracked_pointer_count_after = entry.tracked_pointer_count_after,
        .normalized_event = entry.normalized_event,
        .gesture_policy = entry.gesture_policy,
        .duration_ms = entry.duration_ms,
        .start_x = entry.start_x,
        .start_y = entry.start_y,
        .x = entry.x,
        .y = entry.y,
        .delta_x = entry.delta_x,
        .delta_y = entry.delta_y,
        .pixel_delta_x = entry.pixel_delta_x,
        .pixel_delta_y = entry.pixel_delta_y,
        .line_delta_x = entry.line_delta_x,
        .line_delta_y = entry.line_delta_y,
    };
}

inline void append_input_action_candidates_for_batch(
    input_action_candidate_plan& plan,
    input_action_candidate_batch_plan& batch_plan,
    const normalized_input_replay_batch& batch,
    std::size_t batch_index)
{
    if (input_action_candidate_batch_has_text_edit(batch)) {
        append_input_action_candidate(
            plan,
            batch_plan,
            make_text_edit_input_action_candidate(batch, batch_index));
    }

    for (std::size_t index = 0; index < batch.focus.timeline.size(); ++index) {
        append_input_action_candidate(
            plan,
            batch_plan,
            make_focus_move_input_action_candidate(
                batch,
                batch.focus.timeline[index],
                batch_index,
                index));
    }

    for (std::size_t index = 0; index < batch.ime.timeline.size(); ++index) {
        append_input_action_candidate(
            plan,
            batch_plan,
            make_ime_input_action_candidate(
                batch,
                batch.ime.timeline[index],
                batch_index,
                index));
    }

    for (std::size_t index = 0; index < batch.pointer.timeline.size(); ++index) {
        const normalized_input_replay_pointer_timeline_entry& entry = batch.pointer.timeline[index];
        if (entry.capture_changed || input_action_candidate_pointer_kind_is_capture(entry.kind)) {
            append_input_action_candidate(
                plan,
                batch_plan,
                make_pointer_input_action_candidate(
                    batch,
                    entry,
                    input_action_candidate_kind::pointer_capture,
                    batch_index,
                    index));
        }
        if (input_action_candidate_pointer_kind_is_wheel(entry.kind)) {
            append_input_action_candidate(
                plan,
                batch_plan,
                make_pointer_input_action_candidate(
                    batch,
                    entry,
                    input_action_candidate_kind::wheel_scroll,
                    batch_index,
                    index));
        } else if (input_action_candidate_pointer_kind_is_gesture(entry.kind)) {
            append_input_action_candidate(
                plan,
                batch_plan,
                make_pointer_input_action_candidate(
                    batch,
                    entry,
                    input_action_candidate_kind::gesture_candidate,
                    batch_index,
                    index));
        }
    }
}

[[nodiscard]] inline input_action_candidate_batch_plan plan_input_action_candidates_for_batch(
    const normalized_input_replay_batch& batch,
    std::size_t batch_index = 0)
{
    input_action_candidate_plan scratch;
    input_action_candidate_batch_plan batch_plan{
        .label = batch.label,
    };
    append_input_action_candidates_for_batch(scratch, batch_plan, batch, batch_index);
    return batch_plan;
}

[[nodiscard]] inline input_action_candidate_plan plan_input_action_candidates(
    std::span<const normalized_input_replay_batch> batches)
{
    input_action_candidate_plan plan;
    plan.batches.reserve(batches.size());
    for (std::size_t index = 0; index < batches.size(); ++index) {
        input_action_candidate_batch_plan batch_plan{
            .label = batches[index].label,
        };
        append_input_action_candidates_for_batch(plan, batch_plan, batches[index], index);
        plan.batches.push_back(std::move(batch_plan));
        plan.final_state = batches[index].end_state;
    }
    return plan;
}

[[nodiscard]] inline input_action_candidate_plan plan_input_action_candidates(
    const normalized_input_replay_recording& recording)
{
    input_action_candidate_plan plan = plan_input_action_candidates(
        std::span<const normalized_input_replay_batch>{recording.batches});
    plan.final_state = recording.final_state;
    return plan;
}

} // namespace quiz_vulkan::input
