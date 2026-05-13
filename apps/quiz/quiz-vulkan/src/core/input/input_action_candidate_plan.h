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

inline constexpr std::size_t input_action_candidate_npos = static_cast<std::size_t>(-1);

enum class input_action_candidate_result_status {
    selected,
    supporting_evidence,
    diagnostic_only,
    rejected,
};

enum class input_action_candidate_result_reason {
    text_edit_diff,
    focus_transition,
    pointer_capture_transition,
    pointer_capture_supports_selected,
    wheel_delta,
    gesture_emitted,
    gesture_policy_rejected,
    gesture_policy_suppressed,
    ime_composition_started,
    ime_preedit_valid,
    ime_preedit_invalid,
    ime_committed,
    ime_commit_invalid,
    ime_canceled,
    ime_cancel_invalid,
    coalesced_by_selected_candidate,
    no_observable_delta,
};

struct input_action_candidate_result_counts {
    input_action_candidate_counts selected;
    input_action_candidate_counts supporting_evidence;
    input_action_candidate_counts diagnostic_only;
    input_action_candidate_counts rejected;
    std::size_t total = 0;
};

struct input_action_candidate_result {
    input_action_candidate candidate;
    input_action_candidate_result_status status =
        input_action_candidate_result_status::diagnostic_only;
    input_action_candidate_result_reason reason =
        input_action_candidate_result_reason::no_observable_delta;
    std::size_t candidate_index = 0;
    std::size_t selected_candidate_index = input_action_candidate_npos;
    int priority = 0;
    bool selected = false;
    bool supports_selected_candidate = false;
    bool emits_input_event = false;
    bool has_text_delta = false;
    bool has_focus_transition = false;
    bool has_pointer_capture_transition = false;
    bool has_gesture_event = false;
    bool has_wheel_delta = false;
    bool has_ime_payload = false;
    bool evidence_clean = true;
};

struct input_action_candidate_batch_resolution {
    std::string label;
    std::vector<input_action_candidate_result> results;
    input_action_candidate_result_counts counts;
    std::size_t selected_candidate_index = input_action_candidate_npos;
    bool has_selected_candidate = false;
};

struct input_action_candidate_resolution {
    std::vector<input_action_candidate_batch_resolution> batches;
    std::vector<input_action_candidate_result> results;
    input_action_candidate_result_counts counts;
    normalized_input_replay_end_state final_state;
};

struct input_action_resolution_result_summary {
    input_action_candidate_kind kind = input_action_candidate_kind::text_edit;
    input_action_candidate_result_status status =
        input_action_candidate_result_status::diagnostic_only;
    input_action_candidate_result_reason reason =
        input_action_candidate_result_reason::no_observable_delta;
    std::string batch_label;
    std::size_t batch_index = 0;
    std::size_t candidate_index = 0;
    std::size_t selected_candidate_index = input_action_candidate_npos;
    std::size_t event_index = 0;
    int priority = 0;
    std::int64_t timestamp_ms = 0;
    bool selected = false;
    bool supports_selected_candidate = false;
    bool emits_input_event = false;
    bool evidence_clean = true;
    bool has_text_delta = false;
    bool has_focus_transition = false;
    bool has_pointer_capture_transition = false;
    bool has_gesture_event = false;
    bool has_wheel_delta = false;
    bool has_ime_payload = false;

    std::string target_id;
    std::string target_id_before;
    std::string target_id_after;
    bool target_changed = false;
    bool had_focus_before = false;
    bool has_focus_after = false;
    normalized_input_replay_focus_timeline_kind focus_kind =
        normalized_input_replay_focus_timeline_kind::caret_moved;
    keyboard_shortcut_intent keyboard_intent = keyboard_shortcut_intent::none;

    std::size_t text_byte_count = 0;
    std::size_t text_byte_count_before = 0;
    std::size_t text_byte_count_after = 0;
    std::int64_t text_byte_delta = 0;
    text_range caret_before;
    text_range caret_after;
    bool caret_changed = false;
    bool had_selection_before = false;
    bool has_selection_after = false;
    text_range selection_before;
    text_range selection_after;
    bool selection_changed = false;

    normalized_input_replay_ime_timeline_phase ime_phase =
        normalized_input_replay_ime_timeline_phase::preedit_update;
    std::size_t utf8_text_bytes = 0;
    std::size_t committed_text_bytes = 0;
    bool preedit_text_valid = true;
    bool preedit_range_valid = true;
    bool stale_preedit_cleared_after = true;
    bool ime_composition_active = false;

    normalized_input_replay_pointer_timeline_kind pointer_kind =
        normalized_input_replay_pointer_timeline_kind::gesture_suppressed;
    std::int32_t pointer_id = 0;
    pointer_phase event_phase = pointer_phase::down;
    pointer_contact_kind pointer_contact = pointer_contact_kind::unknown;
    pointer_arbitration_decision pointer_decision = pointer_arbitration_decision::none;
    pointer_capture_lifecycle capture_before_lifecycle = pointer_capture_lifecycle::idle;
    pointer_capture_lifecycle capture_after_lifecycle = pointer_capture_lifecycle::idle;
    bool capture_before_active = false;
    bool capture_after_active = false;
    bool capture_changed = false;
    bool capture_ended_cleanly_after = true;
    std::size_t tracked_pointer_count_before = 0;
    std::size_t tracked_pointer_count_after = 0;
    gesture_policy_decision gesture_decision = gesture_policy_decision::none;
    gesture_direction gesture_direction_hint = gesture_direction::none;
    input_event_summary_kind normalized_event_kind = input_event_summary_kind::tap;

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

struct input_action_resolution_batch_summary {
    std::string label;
    std::vector<input_action_resolution_result_summary> selected;
    std::vector<input_action_resolution_result_summary> supporting_evidence;
    std::vector<input_action_resolution_result_summary> rejected;
    std::vector<input_action_resolution_result_summary> diagnostic_only;
    input_action_candidate_result_counts counts;
    std::size_t selected_candidate_index = input_action_candidate_npos;
    bool has_selected_candidate = false;
};

struct input_action_resolution_replay_summary {
    std::vector<input_action_resolution_batch_summary> batches;
    std::vector<input_action_resolution_result_summary> selected;
    std::vector<input_action_resolution_result_summary> supporting_evidence;
    std::vector<input_action_resolution_result_summary> rejected;
    std::vector<input_action_resolution_result_summary> diagnostic_only;
    input_action_candidate_result_counts counts;
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

inline void count_input_action_candidate_result(
    input_action_candidate_result_counts& counts,
    const input_action_candidate_result& result)
{
    ++counts.total;
    switch (result.status) {
    case input_action_candidate_result_status::selected:
        count_input_action_candidate_kind(counts.selected, result.candidate.kind);
        return;
    case input_action_candidate_result_status::supporting_evidence:
        count_input_action_candidate_kind(counts.supporting_evidence, result.candidate.kind);
        return;
    case input_action_candidate_result_status::diagnostic_only:
        count_input_action_candidate_kind(counts.diagnostic_only, result.candidate.kind);
        return;
    case input_action_candidate_result_status::rejected:
        count_input_action_candidate_kind(counts.rejected, result.candidate.kind);
        return;
    }
}

inline void accumulate_input_action_candidate_counts(
    input_action_candidate_counts& target,
    const input_action_candidate_counts& source)
{
    target.text_edit += source.text_edit;
    target.focus_move += source.focus_move;
    target.pointer_capture += source.pointer_capture;
    target.gesture_candidate += source.gesture_candidate;
    target.wheel_scroll += source.wheel_scroll;
    target.ime_composition_start += source.ime_composition_start;
    target.ime_preedit += source.ime_preedit;
    target.ime_commit += source.ime_commit;
    target.ime_cancel += source.ime_cancel;
    target.total += source.total;
}

inline void accumulate_input_action_candidate_result_counts(
    input_action_candidate_result_counts& target,
    const input_action_candidate_result_counts& source)
{
    accumulate_input_action_candidate_counts(target.selected, source.selected);
    accumulate_input_action_candidate_counts(
        target.supporting_evidence,
        source.supporting_evidence);
    accumulate_input_action_candidate_counts(target.diagnostic_only, source.diagnostic_only);
    accumulate_input_action_candidate_counts(target.rejected, source.rejected);
    target.total += source.total;
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

[[nodiscard]] inline bool input_action_candidate_text_has_delta(
    const input_action_candidate& candidate)
{
    return candidate.text_presentation_diff.changed
        || candidate.text_presentation_diff.committed_text_changed
        || candidate.text_presentation_diff.display_text_changed
        || candidate.text_presentation_diff.preedit_changed
        || candidate.text_presentation_diff.caret_changed
        || candidate.text_presentation_diff.selection_changed
        || candidate.text_presentation_diff.byte_counts_changed
        || candidate.text_byte_count_before != candidate.text_byte_count_after
        || candidate.caret_changed
        || candidate.selection_changed;
}

[[nodiscard]] inline bool input_action_candidate_focus_has_transition(
    const input_action_candidate& candidate)
{
    return candidate.target_changed
        || candidate.had_focus_before != candidate.has_focus_after
        || candidate.target_id_before != candidate.target_id_after
        || candidate.caret_changed
        || candidate.selection_changed
        || candidate.emits_input_event;
}

[[nodiscard]] inline bool input_action_candidate_capture_snapshot_changed(
    const pointer_capture_snapshot& before,
    const pointer_capture_snapshot& after)
{
    return before.lifecycle != after.lifecycle
        || before.active != after.active
        || before.pointer_id != after.pointer_id
        || before.tracked_pointer_count != after.tracked_pointer_count;
}

[[nodiscard]] inline bool input_action_candidate_pointer_has_capture_transition(
    const input_action_candidate& candidate)
{
    return candidate.capture_changed
        || candidate.tracked_pointer_count_before != candidate.tracked_pointer_count_after
        || input_action_candidate_capture_snapshot_changed(
            candidate.capture_before,
            candidate.capture_after);
}

[[nodiscard]] inline bool input_action_candidate_wheel_has_delta(
    const input_action_candidate& candidate)
{
    return candidate.pixel_delta_x != 0.0f
        || candidate.pixel_delta_y != 0.0f
        || candidate.line_delta_x != 0.0f
        || candidate.line_delta_y != 0.0f;
}

[[nodiscard]] inline bool input_action_candidate_gesture_emits_event(
    const input_action_candidate& candidate)
{
    return candidate.emits_input_event || candidate.gesture_policy.emitted_input_event;
}

[[nodiscard]] inline bool input_action_candidate_gesture_policy_rejected(
    gesture_policy_decision decision)
{
    return decision == gesture_policy_decision::swipe_rejected_distance
        || decision == gesture_policy_decision::swipe_rejected_cross_axis
        || decision == gesture_policy_decision::swipe_rejected_duration;
}

[[nodiscard]] inline bool input_action_candidate_gesture_policy_suppressed(
    gesture_policy_decision decision)
{
    return decision == gesture_policy_decision::release_suppressed
        || decision == gesture_policy_decision::ignored_by_capture;
}

[[nodiscard]] inline bool input_action_candidate_ime_has_payload(
    const input_action_candidate& candidate)
{
    return !candidate.utf8_text.empty()
        || !candidate.committed_text.empty()
        || candidate.composition.active
        || !candidate.composition.preedit_text.empty()
        || candidate.text_byte_count_before != candidate.text_byte_count_after
        || candidate.caret_before.start_byte != candidate.caret_after.start_byte
        || candidate.caret_before.end_byte != candidate.caret_after.end_byte
        || candidate.had_selection_before != candidate.has_selection_after
        || candidate.selection_before.start_byte != candidate.selection_after.start_byte
        || candidate.selection_before.end_byte != candidate.selection_after.end_byte;
}

[[nodiscard]] inline bool input_action_candidate_ime_evidence_clean(
    const input_action_candidate& candidate)
{
    return candidate.preedit_text_valid
        && candidate.preedit_range_valid
        && candidate.stale_preedit_cleared_after;
}

[[nodiscard]] inline int input_action_candidate_result_priority(
    input_action_candidate_kind kind)
{
    switch (kind) {
    case input_action_candidate_kind::ime_commit:
        return 900;
    case input_action_candidate_kind::text_edit:
        return 850;
    case input_action_candidate_kind::ime_cancel:
        return 800;
    case input_action_candidate_kind::ime_preedit:
        return 750;
    case input_action_candidate_kind::ime_composition_start:
        return 700;
    case input_action_candidate_kind::focus_move:
        return 600;
    case input_action_candidate_kind::wheel_scroll:
        return 550;
    case input_action_candidate_kind::gesture_candidate:
        return 500;
    case input_action_candidate_kind::pointer_capture:
        return 200;
    }
    return 0;
}

[[nodiscard]] inline input_action_candidate_result resolve_input_action_candidate(
    const input_action_candidate& candidate,
    std::size_t candidate_index = 0)
{
    input_action_candidate_result result{
        .candidate = candidate,
        .candidate_index = candidate_index,
        .priority = input_action_candidate_result_priority(candidate.kind),
        .emits_input_event = candidate.emits_input_event,
        .has_text_delta = input_action_candidate_text_has_delta(candidate),
        .has_focus_transition = input_action_candidate_focus_has_transition(candidate),
        .has_pointer_capture_transition =
            input_action_candidate_pointer_has_capture_transition(candidate),
        .has_gesture_event = input_action_candidate_gesture_emits_event(candidate),
        .has_wheel_delta = input_action_candidate_wheel_has_delta(candidate),
        .has_ime_payload = input_action_candidate_ime_has_payload(candidate),
        .evidence_clean = candidate.capture_ended_cleanly_after
            && input_action_candidate_ime_evidence_clean(candidate),
    };

    switch (candidate.kind) {
    case input_action_candidate_kind::text_edit:
        if (result.has_text_delta) {
            result.status = input_action_candidate_result_status::selected;
            result.reason = input_action_candidate_result_reason::text_edit_diff;
        }
        return result;
    case input_action_candidate_kind::focus_move:
        if (result.has_focus_transition) {
            result.status = input_action_candidate_result_status::selected;
            result.reason = input_action_candidate_result_reason::focus_transition;
        }
        return result;
    case input_action_candidate_kind::pointer_capture:
        if (result.has_pointer_capture_transition) {
            result.status = input_action_candidate_result_status::selected;
            result.reason = input_action_candidate_result_reason::pointer_capture_transition;
        }
        return result;
    case input_action_candidate_kind::gesture_candidate:
        if (result.has_gesture_event) {
            result.status = input_action_candidate_result_status::selected;
            result.reason = input_action_candidate_result_reason::gesture_emitted;
        } else if (input_action_candidate_gesture_policy_rejected(candidate.gesture_policy.decision)) {
            result.status = input_action_candidate_result_status::rejected;
            result.reason = input_action_candidate_result_reason::gesture_policy_rejected;
        } else if (input_action_candidate_gesture_policy_suppressed(candidate.gesture_policy.decision)) {
            result.status = input_action_candidate_result_status::rejected;
            result.reason = input_action_candidate_result_reason::gesture_policy_suppressed;
        }
        return result;
    case input_action_candidate_kind::wheel_scroll:
        if (result.has_wheel_delta || candidate.emits_input_event) {
            result.status = input_action_candidate_result_status::selected;
            result.reason = input_action_candidate_result_reason::wheel_delta;
        }
        return result;
    case input_action_candidate_kind::ime_composition_start:
        if (candidate.composition.active || result.has_ime_payload || candidate.emits_input_event) {
            result.status = input_action_candidate_result_status::selected;
            result.reason = input_action_candidate_result_reason::ime_composition_started;
        }
        return result;
    case input_action_candidate_kind::ime_preedit:
        if (!candidate.preedit_text_valid || !candidate.preedit_range_valid) {
            result.status = input_action_candidate_result_status::rejected;
            result.reason = input_action_candidate_result_reason::ime_preedit_invalid;
        } else if (result.has_ime_payload || candidate.emits_input_event) {
            result.status = input_action_candidate_result_status::selected;
            result.reason = input_action_candidate_result_reason::ime_preedit_valid;
        }
        return result;
    case input_action_candidate_kind::ime_commit:
        if (!candidate.stale_preedit_cleared_after) {
            result.status = input_action_candidate_result_status::rejected;
            result.reason = input_action_candidate_result_reason::ime_commit_invalid;
        } else if (result.has_ime_payload || candidate.emits_input_event) {
            result.status = input_action_candidate_result_status::selected;
            result.reason = input_action_candidate_result_reason::ime_committed;
        }
        return result;
    case input_action_candidate_kind::ime_cancel:
        if (!candidate.stale_preedit_cleared_after) {
            result.status = input_action_candidate_result_status::rejected;
            result.reason = input_action_candidate_result_reason::ime_cancel_invalid;
        } else if (result.has_ime_payload || candidate.emits_input_event) {
            result.status = input_action_candidate_result_status::selected;
            result.reason = input_action_candidate_result_reason::ime_canceled;
        }
        return result;
    }

    return result;
}

[[nodiscard]] inline bool input_action_candidate_result_is_primary_candidate(
    const input_action_candidate_result& result)
{
    return result.status == input_action_candidate_result_status::selected;
}

[[nodiscard]] inline bool input_action_candidate_result_precedes(
    const input_action_candidate_result& lhs,
    const input_action_candidate_result& rhs)
{
    if (lhs.priority != rhs.priority) {
        return lhs.priority > rhs.priority;
    }
    if (lhs.candidate.timestamp_ms != rhs.candidate.timestamp_ms) {
        return lhs.candidate.timestamp_ms < rhs.candidate.timestamp_ms;
    }
    if (lhs.candidate.timeline_index != rhs.candidate.timeline_index) {
        return lhs.candidate.timeline_index < rhs.candidate.timeline_index;
    }
    return lhs.candidate_index < rhs.candidate_index;
}

inline void finalize_input_action_candidate_batch_resolution(
    input_action_candidate_batch_resolution& resolution)
{
    resolution.counts = {};
    std::size_t selected_index = input_action_candidate_npos;
    for (std::size_t index = 0; index < resolution.results.size(); ++index) {
        const input_action_candidate_result& result = resolution.results[index];
        if (!input_action_candidate_result_is_primary_candidate(result)) {
            continue;
        }
        if (selected_index == input_action_candidate_npos
            || input_action_candidate_result_precedes(result, resolution.results[selected_index])) {
            selected_index = index;
        }
    }

    resolution.has_selected_candidate = selected_index != input_action_candidate_npos;
    resolution.selected_candidate_index = selected_index;

    for (std::size_t index = 0; index < resolution.results.size(); ++index) {
        input_action_candidate_result& result = resolution.results[index];
        result.selected_candidate_index = selected_index;
        result.selected = index == selected_index;
        if (result.status != input_action_candidate_result_status::selected || result.selected) {
            count_input_action_candidate_result(resolution.counts, result);
            continue;
        }

        result.status = input_action_candidate_result_status::supporting_evidence;
        result.supports_selected_candidate = true;
        if (result.candidate.kind == input_action_candidate_kind::pointer_capture) {
            result.reason = input_action_candidate_result_reason::pointer_capture_supports_selected;
        } else {
            result.reason = input_action_candidate_result_reason::coalesced_by_selected_candidate;
        }
        count_input_action_candidate_result(resolution.counts, result);
    }
}

[[nodiscard]] inline input_action_candidate_batch_resolution
resolve_input_action_candidate_batch(
    const input_action_candidate_batch_plan& batch_plan)
{
    input_action_candidate_batch_resolution resolution{
        .label = batch_plan.label,
    };
    resolution.results.reserve(batch_plan.candidates.size());
    for (std::size_t index = 0; index < batch_plan.candidates.size(); ++index) {
        resolution.results.push_back(resolve_input_action_candidate(
            batch_plan.candidates[index],
            index));
    }
    finalize_input_action_candidate_batch_resolution(resolution);
    return resolution;
}

inline void append_input_action_candidate_batch_resolution(
    input_action_candidate_resolution& resolution,
    input_action_candidate_batch_resolution batch_resolution)
{
    for (const input_action_candidate_result& result : batch_resolution.results) {
        count_input_action_candidate_result(resolution.counts, result);
        resolution.results.push_back(result);
    }
    resolution.batches.push_back(std::move(batch_resolution));
}

[[nodiscard]] inline input_action_candidate_resolution resolve_input_action_candidate_plan(
    const input_action_candidate_plan& plan)
{
    input_action_candidate_resolution resolution{
        .final_state = plan.final_state,
    };
    resolution.batches.reserve(plan.batches.size());
    for (const input_action_candidate_batch_plan& batch_plan : plan.batches) {
        append_input_action_candidate_batch_resolution(
            resolution,
            resolve_input_action_candidate_batch(batch_plan));
    }
    return resolution;
}

[[nodiscard]] inline input_action_candidate_resolution resolve_input_action_candidates(
    std::span<const normalized_input_replay_batch> batches)
{
    return resolve_input_action_candidate_plan(plan_input_action_candidates(batches));
}

[[nodiscard]] inline input_action_candidate_resolution resolve_input_action_candidates(
    const normalized_input_replay_recording& recording)
{
    input_action_candidate_resolution resolution =
        resolve_input_action_candidate_plan(plan_input_action_candidates(recording));
    resolution.final_state = recording.final_state;
    return resolution;
}

[[nodiscard]] inline std::int64_t input_action_resolution_byte_delta(
    std::size_t before_count,
    std::size_t after_count)
{
    if (after_count >= before_count) {
        return static_cast<std::int64_t>(after_count - before_count);
    }
    return -static_cast<std::int64_t>(before_count - after_count);
}

[[nodiscard]] inline input_action_resolution_result_summary
summarize_input_action_candidate_result(
    const input_action_candidate_result& result)
{
    const input_action_candidate& candidate = result.candidate;
    return input_action_resolution_result_summary{
        .kind = candidate.kind,
        .status = result.status,
        .reason = result.reason,
        .batch_label = candidate.batch_label,
        .batch_index = candidate.batch_index,
        .candidate_index = result.candidate_index,
        .selected_candidate_index = result.selected_candidate_index,
        .event_index = candidate.event_index,
        .priority = result.priority,
        .timestamp_ms = candidate.timestamp_ms,
        .selected = result.selected,
        .supports_selected_candidate = result.supports_selected_candidate,
        .emits_input_event = result.emits_input_event,
        .evidence_clean = result.evidence_clean,
        .has_text_delta = result.has_text_delta,
        .has_focus_transition = result.has_focus_transition,
        .has_pointer_capture_transition = result.has_pointer_capture_transition,
        .has_gesture_event = result.has_gesture_event,
        .has_wheel_delta = result.has_wheel_delta,
        .has_ime_payload = result.has_ime_payload,
        .target_id = candidate.target_id,
        .target_id_before = candidate.target_id_before,
        .target_id_after = candidate.target_id_after,
        .target_changed = candidate.target_changed,
        .had_focus_before = candidate.had_focus_before,
        .has_focus_after = candidate.has_focus_after,
        .focus_kind = candidate.focus_kind,
        .keyboard_intent = candidate.keyboard.intent,
        .text_byte_count = candidate.text_byte_count,
        .text_byte_count_before = candidate.text_byte_count_before,
        .text_byte_count_after = candidate.text_byte_count_after,
        .text_byte_delta = input_action_resolution_byte_delta(
            candidate.text_byte_count_before,
            candidate.text_byte_count_after),
        .caret_before = candidate.caret_before,
        .caret_after = candidate.caret_after,
        .caret_changed = candidate.caret_changed,
        .had_selection_before = candidate.had_selection_before,
        .has_selection_after = candidate.has_selection_after,
        .selection_before = candidate.selection_before,
        .selection_after = candidate.selection_after,
        .selection_changed = candidate.selection_changed,
        .ime_phase = candidate.ime_phase,
        .utf8_text_bytes = candidate.utf8_text.size(),
        .committed_text_bytes = candidate.committed_text.size(),
        .preedit_text_valid = candidate.preedit_text_valid,
        .preedit_range_valid = candidate.preedit_range_valid,
        .stale_preedit_cleared_after = candidate.stale_preedit_cleared_after,
        .ime_composition_active = candidate.composition.active,
        .pointer_kind = candidate.pointer_kind,
        .pointer_id = candidate.pointer_id,
        .event_phase = candidate.event_phase,
        .pointer_contact = candidate.pointer_contact,
        .pointer_decision = candidate.pointer_decision,
        .capture_before_lifecycle = candidate.capture_before.lifecycle,
        .capture_after_lifecycle = candidate.capture_after.lifecycle,
        .capture_before_active = candidate.capture_before.active,
        .capture_after_active = candidate.capture_after.active,
        .capture_changed = candidate.capture_changed,
        .capture_ended_cleanly_after = candidate.capture_ended_cleanly_after,
        .tracked_pointer_count_before = candidate.tracked_pointer_count_before,
        .tracked_pointer_count_after = candidate.tracked_pointer_count_after,
        .gesture_decision = candidate.gesture_policy.decision,
        .gesture_direction_hint = candidate.gesture_policy.direction,
        .normalized_event_kind = candidate.normalized_event.kind,
        .duration_ms = candidate.duration_ms,
        .start_x = candidate.start_x,
        .start_y = candidate.start_y,
        .x = candidate.x,
        .y = candidate.y,
        .delta_x = candidate.delta_x,
        .delta_y = candidate.delta_y,
        .pixel_delta_x = candidate.pixel_delta_x,
        .pixel_delta_y = candidate.pixel_delta_y,
        .line_delta_x = candidate.line_delta_x,
        .line_delta_y = candidate.line_delta_y,
    };
}

inline void append_input_action_resolution_result_summary(
    input_action_resolution_batch_summary& batch_summary,
    input_action_resolution_result_summary summary)
{
    switch (summary.status) {
    case input_action_candidate_result_status::selected:
        batch_summary.selected.push_back(std::move(summary));
        return;
    case input_action_candidate_result_status::supporting_evidence:
        batch_summary.supporting_evidence.push_back(std::move(summary));
        return;
    case input_action_candidate_result_status::diagnostic_only:
        batch_summary.diagnostic_only.push_back(std::move(summary));
        return;
    case input_action_candidate_result_status::rejected:
        batch_summary.rejected.push_back(std::move(summary));
        return;
    }
}

[[nodiscard]] inline input_action_resolution_batch_summary
summarize_input_action_candidate_batch_resolution(
    const input_action_candidate_batch_resolution& batch_resolution)
{
    input_action_resolution_batch_summary summary{
        .label = batch_resolution.label,
        .counts = batch_resolution.counts,
        .selected_candidate_index = batch_resolution.selected_candidate_index,
        .has_selected_candidate = batch_resolution.has_selected_candidate,
    };
    summary.selected.reserve(batch_resolution.counts.selected.total);
    summary.supporting_evidence.reserve(batch_resolution.counts.supporting_evidence.total);
    summary.rejected.reserve(batch_resolution.counts.rejected.total);
    summary.diagnostic_only.reserve(batch_resolution.counts.diagnostic_only.total);
    for (const input_action_candidate_result& result : batch_resolution.results) {
        append_input_action_resolution_result_summary(
            summary,
            summarize_input_action_candidate_result(result));
    }
    return summary;
}

inline void append_input_action_resolution_batch_summary(
    input_action_resolution_replay_summary& replay_summary,
    input_action_resolution_batch_summary batch_summary)
{
    accumulate_input_action_candidate_result_counts(replay_summary.counts, batch_summary.counts);
    replay_summary.selected.insert(
        replay_summary.selected.end(),
        batch_summary.selected.begin(),
        batch_summary.selected.end());
    replay_summary.supporting_evidence.insert(
        replay_summary.supporting_evidence.end(),
        batch_summary.supporting_evidence.begin(),
        batch_summary.supporting_evidence.end());
    replay_summary.rejected.insert(
        replay_summary.rejected.end(),
        batch_summary.rejected.begin(),
        batch_summary.rejected.end());
    replay_summary.diagnostic_only.insert(
        replay_summary.diagnostic_only.end(),
        batch_summary.diagnostic_only.begin(),
        batch_summary.diagnostic_only.end());
    replay_summary.batches.push_back(std::move(batch_summary));
}

[[nodiscard]] inline input_action_resolution_replay_summary
summarize_input_action_candidate_resolution(
    const input_action_candidate_resolution& resolution)
{
    input_action_resolution_replay_summary summary{
        .final_state = resolution.final_state,
    };
    summary.batches.reserve(resolution.batches.size());
    summary.selected.reserve(resolution.counts.selected.total);
    summary.supporting_evidence.reserve(resolution.counts.supporting_evidence.total);
    summary.rejected.reserve(resolution.counts.rejected.total);
    summary.diagnostic_only.reserve(resolution.counts.diagnostic_only.total);
    for (const input_action_candidate_batch_resolution& batch_resolution : resolution.batches) {
        append_input_action_resolution_batch_summary(
            summary,
            summarize_input_action_candidate_batch_resolution(batch_resolution));
    }
    return summary;
}

[[nodiscard]] inline input_action_resolution_replay_summary
summarize_input_action_resolution_replay(
    std::span<const normalized_input_replay_batch> batches)
{
    return summarize_input_action_candidate_resolution(resolve_input_action_candidates(batches));
}

[[nodiscard]] inline input_action_resolution_replay_summary
summarize_input_action_resolution_replay(
    const normalized_input_replay_recording& recording)
{
    input_action_resolution_replay_summary summary =
        summarize_input_action_candidate_resolution(resolve_input_action_candidates(recording));
    summary.final_state = recording.final_state;
    return summary;
}

} // namespace quiz_vulkan::input
