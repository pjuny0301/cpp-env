#pragma once

#include "core/input/text_edit_transaction_diagnostics.h"
#include "core/input/text_input_model.h"
#include "core/input/text_input_presentation.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::input {

struct text_edit_transaction_replay_step {
    std::string label;
    text_edit_transaction_operation operation = text_edit_transaction_operation::none;
    std::string text;
    text_range range;
};

struct text_edit_transaction_replay_operation_counts {
    std::size_t none = 0;
    std::size_t focus = 0;
    std::size_t clear_focus = 0;
    std::size_t move_caret_to_start = 0;
    std::size_t move_caret_to_end = 0;
    std::size_t move_caret_left = 0;
    std::size_t move_caret_right = 0;
    std::size_t extend_selection_left = 0;
    std::size_t extend_selection_right = 0;
    std::size_t select_all = 0;
    std::size_t clear_selection = 0;
    std::size_t set_selection = 0;
    std::size_t commit_utf8 = 0;
    std::size_t backspace = 0;
    std::size_t delete_forward = 0;
    std::size_t set_preedit = 0;
    std::size_t commit_ime = 0;
    std::size_t cancel_ime = 0;
    std::size_t submit = 0;
};

struct text_edit_transaction_replay_step_diagnostics {
    std::string label;
    text_edit_transaction_operation operation = text_edit_transaction_operation::none;
    text_edit_transaction_diagnostics transaction;
    text_input_presentation_snapshot before_presentation;
    text_input_presentation_snapshot after_presentation;
    text_input_presentation_diff presentation_diff;
    std::size_t committed_text_bytes_before = 0;
    std::size_t committed_text_bytes_after = 0;
    std::size_t display_text_bytes_before = 0;
    std::size_t display_text_bytes_after = 0;
    text_range caret_before;
    text_range caret_after;
    bool had_selection_before = false;
    bool has_selection_after = false;
    bool had_preedit_before = false;
    bool has_preedit_after = false;
    bool had_submit_before = false;
    bool has_submit_after = false;
    bool accepted = false;
    bool rejected = false;
    bool invalid_edit_rejected = false;
    bool utf8_boundary_safe = true;
    bool selection_replaced_committed_text = false;
    bool selection_replaced_display_text = false;
    bool selection_deleted = false;
    bool selection_cleared = false;
    bool ime_preedit_started = false;
    bool ime_preedit_updated = false;
    bool ime_preedit_cleared = false;
    bool ime_committed = false;
    bool ime_canceled = false;
};

struct text_edit_transaction_replay_aggregate_counts {
    text_edit_transaction_replay_operation_counts operations;
    std::size_t step_count = 0;
    std::size_t accepted_count = 0;
    std::size_t rejected_count = 0;
    std::size_t invalid_edit_rejected_count = 0;
    std::size_t utf8_boundary_safe_count = 0;
    std::size_t utf8_boundary_unsafe_count = 0;
    std::size_t text_changed_count = 0;
    std::size_t display_text_changed_count = 0;
    std::size_t caret_changed_count = 0;
    std::size_t selection_changed_count = 0;
    std::size_t preedit_changed_count = 0;
    std::size_t submit_available_after_count = 0;
    std::size_t selection_replaced_committed_text_count = 0;
    std::size_t selection_replaced_display_text_count = 0;
    std::size_t selection_deleted_count = 0;
    std::size_t selection_cleared_count = 0;
    std::size_t ime_preedit_started_count = 0;
    std::size_t ime_preedit_updated_count = 0;
    std::size_t ime_preedit_cleared_count = 0;
    std::size_t ime_committed_count = 0;
    std::size_t ime_canceled_count = 0;
    std::size_t inserted_byte_count = 0;
    std::size_t deleted_byte_count = 0;
    std::size_t replaced_byte_count = 0;
    std::int64_t committed_byte_delta = 0;
    std::int64_t display_byte_delta = 0;
};

struct text_edit_transaction_replay_summary {
    std::vector<text_edit_transaction_replay_step_diagnostics> steps;
    text_edit_transaction_replay_aggregate_counts counts;
    text_input_presentation_snapshot final_presentation;
    text_edit_transaction_diagnostics final_transaction;
    bool final_submit_available = false;
    bool final_utf8_boundary_safe = true;
};

struct text_edit_transaction_replay_count_delta {
    std::size_t before_count = 0;
    std::size_t after_count = 0;
    std::int64_t delta = 0;
    bool changed = false;
};

struct text_edit_transaction_replay_int_delta {
    std::int64_t before_value = 0;
    std::int64_t after_value = 0;
    std::int64_t delta = 0;
    bool changed = false;
};

struct text_edit_transaction_replay_operation_count_deltas {
    text_edit_transaction_replay_count_delta none;
    text_edit_transaction_replay_count_delta focus;
    text_edit_transaction_replay_count_delta clear_focus;
    text_edit_transaction_replay_count_delta move_caret_to_start;
    text_edit_transaction_replay_count_delta move_caret_to_end;
    text_edit_transaction_replay_count_delta move_caret_left;
    text_edit_transaction_replay_count_delta move_caret_right;
    text_edit_transaction_replay_count_delta extend_selection_left;
    text_edit_transaction_replay_count_delta extend_selection_right;
    text_edit_transaction_replay_count_delta select_all;
    text_edit_transaction_replay_count_delta clear_selection;
    text_edit_transaction_replay_count_delta set_selection;
    text_edit_transaction_replay_count_delta commit_utf8;
    text_edit_transaction_replay_count_delta backspace;
    text_edit_transaction_replay_count_delta delete_forward;
    text_edit_transaction_replay_count_delta set_preedit;
    text_edit_transaction_replay_count_delta commit_ime;
    text_edit_transaction_replay_count_delta cancel_ime;
    text_edit_transaction_replay_count_delta submit;
    bool changed = false;
};

struct text_edit_transaction_replay_aggregate_count_deltas {
    text_edit_transaction_replay_operation_count_deltas operations;
    text_edit_transaction_replay_count_delta step_count;
    text_edit_transaction_replay_count_delta accepted_count;
    text_edit_transaction_replay_count_delta rejected_count;
    text_edit_transaction_replay_count_delta invalid_edit_rejected_count;
    text_edit_transaction_replay_count_delta utf8_boundary_safe_count;
    text_edit_transaction_replay_count_delta utf8_boundary_unsafe_count;
    text_edit_transaction_replay_count_delta text_changed_count;
    text_edit_transaction_replay_count_delta display_text_changed_count;
    text_edit_transaction_replay_count_delta caret_changed_count;
    text_edit_transaction_replay_count_delta selection_changed_count;
    text_edit_transaction_replay_count_delta preedit_changed_count;
    text_edit_transaction_replay_count_delta submit_available_after_count;
    text_edit_transaction_replay_count_delta selection_replaced_committed_text_count;
    text_edit_transaction_replay_count_delta selection_replaced_display_text_count;
    text_edit_transaction_replay_count_delta selection_deleted_count;
    text_edit_transaction_replay_count_delta selection_cleared_count;
    text_edit_transaction_replay_count_delta ime_preedit_started_count;
    text_edit_transaction_replay_count_delta ime_preedit_updated_count;
    text_edit_transaction_replay_count_delta ime_preedit_cleared_count;
    text_edit_transaction_replay_count_delta ime_committed_count;
    text_edit_transaction_replay_count_delta ime_canceled_count;
    text_edit_transaction_replay_count_delta inserted_byte_count;
    text_edit_transaction_replay_count_delta deleted_byte_count;
    text_edit_transaction_replay_count_delta replaced_byte_count;
    text_edit_transaction_replay_int_delta committed_byte_delta;
    text_edit_transaction_replay_int_delta display_byte_delta;
    bool changed = false;
};

struct text_edit_transaction_replay_diff {
    text_edit_transaction_replay_aggregate_count_deltas counts;
    text_input_presentation_diff final_presentation;
    bool final_presentation_changed = false;
    bool invalid_edit_changed = false;
    bool utf8_boundary_changed = false;
    bool replacement_changed = false;
    bool ime_transition_changed = false;
    bool submit_changed = false;
    std::size_t changed_category_count = 0;
    bool changed = false;
};

inline void count_text_edit_transaction_replay_operation(
    text_edit_transaction_replay_operation_counts& counts,
    text_edit_transaction_operation operation)
{
    switch (operation) {
    case text_edit_transaction_operation::none:
        ++counts.none;
        return;
    case text_edit_transaction_operation::focus:
        ++counts.focus;
        return;
    case text_edit_transaction_operation::clear_focus:
        ++counts.clear_focus;
        return;
    case text_edit_transaction_operation::move_caret_to_start:
        ++counts.move_caret_to_start;
        return;
    case text_edit_transaction_operation::move_caret_to_end:
        ++counts.move_caret_to_end;
        return;
    case text_edit_transaction_operation::move_caret_left:
        ++counts.move_caret_left;
        return;
    case text_edit_transaction_operation::move_caret_right:
        ++counts.move_caret_right;
        return;
    case text_edit_transaction_operation::extend_selection_left:
        ++counts.extend_selection_left;
        return;
    case text_edit_transaction_operation::extend_selection_right:
        ++counts.extend_selection_right;
        return;
    case text_edit_transaction_operation::select_all:
        ++counts.select_all;
        return;
    case text_edit_transaction_operation::clear_selection:
        ++counts.clear_selection;
        return;
    case text_edit_transaction_operation::set_selection:
        ++counts.set_selection;
        return;
    case text_edit_transaction_operation::commit_utf8:
        ++counts.commit_utf8;
        return;
    case text_edit_transaction_operation::backspace:
        ++counts.backspace;
        return;
    case text_edit_transaction_operation::delete_forward:
        ++counts.delete_forward;
        return;
    case text_edit_transaction_operation::set_preedit:
        ++counts.set_preedit;
        return;
    case text_edit_transaction_operation::commit_ime:
        ++counts.commit_ime;
        return;
    case text_edit_transaction_operation::cancel_ime:
        ++counts.cancel_ime;
        return;
    case text_edit_transaction_operation::submit:
        ++counts.submit;
        return;
    }
}

[[nodiscard]] inline text_edit_transaction_snapshot make_text_edit_transaction_replay_snapshot(
    const text_input_presentation_snapshot& presentation)
{
    const text_range replacement_range = presentation.has_selection
        ? presentation.selection_range
        : text_range{
            .start_byte = presentation.preedit_anchor_byte_offset,
            .end_byte = presentation.preedit_anchor_byte_offset,
        };
    return text_edit_transaction_snapshot{
        .has_focus = presentation.has_focus,
        .target_id = presentation.target_id,
        .text = presentation.committed_text,
        .display_text = presentation.display_text,
        .caret_byte_offset = presentation.caret_byte_offset,
        .caret_range = presentation.caret_range,
        .has_selection = presentation.has_selection,
        .selection = presentation.selection_range,
        .has_preedit = presentation.has_preedit,
        .preedit_text = presentation.preedit_text,
        .preedit_range = presentation.preedit_range,
        .preedit_anchor_byte_offset = presentation.preedit_anchor_byte_offset,
        .composition = ime_composition_state{
            .active = presentation.has_preedit,
            .preedit_text = presentation.preedit_text,
            .replacement_range = replacement_range,
            .preedit_range = presentation.preedit_range,
            .caret_range = presentation.caret_range,
        },
    };
}

[[nodiscard]] inline text_edit_transaction_diagnostics make_text_edit_transaction_replay_noop_transaction(
    const text_input_presentation_snapshot& presentation)
{
    const text_edit_transaction_snapshot snapshot =
        make_text_edit_transaction_replay_snapshot(presentation);
    return text_edit_transaction_diagnostics{
        .operation = text_edit_transaction_operation::none,
        .accepted = false,
        .rejected = true,
        .rejection_reason = text_edit_transaction_rejection_reason::unchanged,
        .before = snapshot,
        .after = snapshot,
        .bytes = text_edit_transaction_byte_diagnostics{
            .committed_text_bytes_before = presentation.byte_counts.committed_text_bytes,
            .committed_text_bytes_after = presentation.byte_counts.committed_text_bytes,
            .display_text_bytes_before = presentation.byte_counts.display_text_bytes,
            .display_text_bytes_after = presentation.byte_counts.display_text_bytes,
        },
    };
}

inline void apply_text_edit_transaction_replay_step(
    text_input_model& model,
    const text_edit_transaction_replay_step& step)
{
    switch (step.operation) {
    case text_edit_transaction_operation::none:
        return;
    case text_edit_transaction_operation::focus:
        model.focus(step.text);
        return;
    case text_edit_transaction_operation::clear_focus:
        model.clear_focus();
        return;
    case text_edit_transaction_operation::move_caret_to_start:
        static_cast<void>(model.move_caret_to_start());
        return;
    case text_edit_transaction_operation::move_caret_to_end:
        static_cast<void>(model.move_caret_to_end());
        return;
    case text_edit_transaction_operation::move_caret_left:
        static_cast<void>(model.move_caret_left());
        return;
    case text_edit_transaction_operation::move_caret_right:
        static_cast<void>(model.move_caret_right());
        return;
    case text_edit_transaction_operation::extend_selection_left:
        static_cast<void>(model.extend_selection_left());
        return;
    case text_edit_transaction_operation::extend_selection_right:
        static_cast<void>(model.extend_selection_right());
        return;
    case text_edit_transaction_operation::select_all:
        static_cast<void>(model.select_all());
        return;
    case text_edit_transaction_operation::clear_selection:
        static_cast<void>(model.clear_selection());
        return;
    case text_edit_transaction_operation::set_selection:
        static_cast<void>(model.set_selection(step.range));
        return;
    case text_edit_transaction_operation::commit_utf8:
        static_cast<void>(model.commit_utf8(step.text));
        return;
    case text_edit_transaction_operation::backspace:
        static_cast<void>(model.backspace());
        return;
    case text_edit_transaction_operation::delete_forward:
        static_cast<void>(model.delete_forward());
        return;
    case text_edit_transaction_operation::set_preedit:
        static_cast<void>(model.set_preedit(step.text));
        return;
    case text_edit_transaction_operation::commit_ime:
        static_cast<void>(model.commit_ime(step.text));
        return;
    case text_edit_transaction_operation::cancel_ime:
        static_cast<void>(model.cancel_ime());
        return;
    case text_edit_transaction_operation::submit:
        static_cast<void>(model.submit());
        return;
    }
}

[[nodiscard]] inline text_edit_transaction_replay_step_diagnostics record_text_edit_transaction_replay_step(
    text_input_model& model,
    const text_edit_transaction_replay_step& step)
{
    text_input_presentation_snapshot before = make_text_input_presentation_snapshot(model);
    apply_text_edit_transaction_replay_step(model, step);
    text_input_presentation_snapshot after = make_text_input_presentation_snapshot(model);
    const text_edit_transaction_diagnostics transaction =
        step.operation == text_edit_transaction_operation::none
            ? make_text_edit_transaction_replay_noop_transaction(before)
            : model.last_transaction_diagnostics();

    return text_edit_transaction_replay_step_diagnostics{
        .label = step.label,
        .operation = step.operation,
        .transaction = transaction,
        .before_presentation = before,
        .after_presentation = after,
        .presentation_diff = diff_text_input_presentation_snapshots(before, after),
        .committed_text_bytes_before = before.byte_counts.committed_text_bytes,
        .committed_text_bytes_after = after.byte_counts.committed_text_bytes,
        .display_text_bytes_before = before.byte_counts.display_text_bytes,
        .display_text_bytes_after = after.byte_counts.display_text_bytes,
        .caret_before = before.caret_range,
        .caret_after = after.caret_range,
        .had_selection_before = before.has_selection,
        .has_selection_after = after.has_selection,
        .had_preedit_before = before.has_preedit,
        .has_preedit_after = after.has_preedit,
        .had_submit_before = before.has_submit_text,
        .has_submit_after = after.has_submit_text,
        .accepted = transaction.accepted,
        .rejected = transaction.rejected,
        .invalid_edit_rejected = transaction.invalid_edit_rejected,
        .utf8_boundary_safe = transaction.utf8_boundary_safe,
        .selection_replaced_committed_text = transaction.selection_replaced_committed_text,
        .selection_replaced_display_text = transaction.selection_replaced_display_text,
        .selection_deleted = transaction.selection_deleted,
        .selection_cleared = transaction.selection_cleared,
        .ime_preedit_started = transaction.ime_preedit_started,
        .ime_preedit_updated = transaction.ime_preedit_updated,
        .ime_preedit_cleared = transaction.ime_preedit_cleared,
        .ime_committed = transaction.ime_committed,
        .ime_canceled = transaction.ime_canceled,
    };
}

inline void accumulate_text_edit_transaction_replay_counts(
    text_edit_transaction_replay_aggregate_counts& counts,
    const text_edit_transaction_replay_step_diagnostics& step)
{
    ++counts.step_count;
    count_text_edit_transaction_replay_operation(counts.operations, step.operation);
    if (step.accepted) {
        ++counts.accepted_count;
    }
    if (step.rejected) {
        ++counts.rejected_count;
    }
    if (step.invalid_edit_rejected) {
        ++counts.invalid_edit_rejected_count;
    }
    if (step.utf8_boundary_safe) {
        ++counts.utf8_boundary_safe_count;
    } else {
        ++counts.utf8_boundary_unsafe_count;
    }
    if (step.transaction.text_changed) {
        ++counts.text_changed_count;
    }
    if (step.transaction.display_text_changed) {
        ++counts.display_text_changed_count;
    }
    if (step.transaction.caret_changed) {
        ++counts.caret_changed_count;
    }
    if (step.transaction.selection_changed) {
        ++counts.selection_changed_count;
    }
    if (step.transaction.preedit_changed) {
        ++counts.preedit_changed_count;
    }
    if (step.has_submit_after) {
        ++counts.submit_available_after_count;
    }
    if (step.selection_replaced_committed_text) {
        ++counts.selection_replaced_committed_text_count;
    }
    if (step.selection_replaced_display_text) {
        ++counts.selection_replaced_display_text_count;
    }
    if (step.selection_deleted) {
        ++counts.selection_deleted_count;
    }
    if (step.selection_cleared) {
        ++counts.selection_cleared_count;
    }
    if (step.ime_preedit_started) {
        ++counts.ime_preedit_started_count;
    }
    if (step.ime_preedit_updated) {
        ++counts.ime_preedit_updated_count;
    }
    if (step.ime_preedit_cleared) {
        ++counts.ime_preedit_cleared_count;
    }
    if (step.ime_committed) {
        ++counts.ime_committed_count;
    }
    if (step.ime_canceled) {
        ++counts.ime_canceled_count;
    }
    counts.inserted_byte_count += step.transaction.bytes.inserted_byte_count;
    counts.deleted_byte_count += step.transaction.bytes.deleted_byte_count;
    counts.replaced_byte_count += step.transaction.bytes.replaced_byte_count;
    counts.committed_byte_delta += step.transaction.bytes.committed_byte_delta;
    counts.display_byte_delta += step.transaction.bytes.display_byte_delta;
}

[[nodiscard]] inline text_edit_transaction_replay_summary replay_text_edit_transactions(
    text_input_model& model,
    std::span<const text_edit_transaction_replay_step> steps)
{
    text_edit_transaction_replay_summary summary;
    summary.steps.reserve(steps.size());
    for (const text_edit_transaction_replay_step& step : steps) {
        text_edit_transaction_replay_step_diagnostics diagnostics =
            record_text_edit_transaction_replay_step(model, step);
        accumulate_text_edit_transaction_replay_counts(summary.counts, diagnostics);
        summary.final_transaction = diagnostics.transaction;
        summary.steps.push_back(std::move(diagnostics));
    }
    summary.final_presentation = make_text_input_presentation_snapshot(model);
    summary.final_submit_available = summary.final_presentation.has_submit_text;
    summary.final_utf8_boundary_safe =
        summary.steps.empty() ? true : summary.steps.back().utf8_boundary_safe;
    return summary;
}

[[nodiscard]] inline text_edit_transaction_replay_summary replay_text_edit_transactions(
    std::span<const text_edit_transaction_replay_step> steps)
{
    text_input_model model;
    return replay_text_edit_transactions(model, steps);
}

[[nodiscard]] inline std::int64_t text_edit_transaction_replay_size_delta(
    std::size_t before_count,
    std::size_t after_count)
{
    if (after_count >= before_count) {
        return static_cast<std::int64_t>(after_count - before_count);
    }
    return -static_cast<std::int64_t>(before_count - after_count);
}

[[nodiscard]] inline text_edit_transaction_replay_count_delta diff_text_edit_transaction_replay_count(
    std::size_t before_count,
    std::size_t after_count)
{
    return text_edit_transaction_replay_count_delta{
        .before_count = before_count,
        .after_count = after_count,
        .delta = text_edit_transaction_replay_size_delta(before_count, after_count),
        .changed = before_count != after_count,
    };
}

[[nodiscard]] inline text_edit_transaction_replay_int_delta diff_text_edit_transaction_replay_int(
    std::int64_t before_value,
    std::int64_t after_value)
{
    return text_edit_transaction_replay_int_delta{
        .before_value = before_value,
        .after_value = after_value,
        .delta = after_value - before_value,
        .changed = before_value != after_value,
    };
}

[[nodiscard]] inline bool text_edit_transaction_replay_operation_deltas_changed(
    const text_edit_transaction_replay_operation_count_deltas& deltas)
{
    return deltas.none.changed
        || deltas.focus.changed
        || deltas.clear_focus.changed
        || deltas.move_caret_to_start.changed
        || deltas.move_caret_to_end.changed
        || deltas.move_caret_left.changed
        || deltas.move_caret_right.changed
        || deltas.extend_selection_left.changed
        || deltas.extend_selection_right.changed
        || deltas.select_all.changed
        || deltas.clear_selection.changed
        || deltas.set_selection.changed
        || deltas.commit_utf8.changed
        || deltas.backspace.changed
        || deltas.delete_forward.changed
        || deltas.set_preedit.changed
        || deltas.commit_ime.changed
        || deltas.cancel_ime.changed
        || deltas.submit.changed;
}

[[nodiscard]] inline text_edit_transaction_replay_operation_count_deltas
diff_text_edit_transaction_replay_operation_counts(
    const text_edit_transaction_replay_operation_counts& before,
    const text_edit_transaction_replay_operation_counts& after)
{
    text_edit_transaction_replay_operation_count_deltas deltas{
        .none = diff_text_edit_transaction_replay_count(before.none, after.none),
        .focus = diff_text_edit_transaction_replay_count(before.focus, after.focus),
        .clear_focus = diff_text_edit_transaction_replay_count(before.clear_focus, after.clear_focus),
        .move_caret_to_start =
            diff_text_edit_transaction_replay_count(before.move_caret_to_start, after.move_caret_to_start),
        .move_caret_to_end =
            diff_text_edit_transaction_replay_count(before.move_caret_to_end, after.move_caret_to_end),
        .move_caret_left =
            diff_text_edit_transaction_replay_count(before.move_caret_left, after.move_caret_left),
        .move_caret_right =
            diff_text_edit_transaction_replay_count(before.move_caret_right, after.move_caret_right),
        .extend_selection_left =
            diff_text_edit_transaction_replay_count(before.extend_selection_left, after.extend_selection_left),
        .extend_selection_right =
            diff_text_edit_transaction_replay_count(before.extend_selection_right, after.extend_selection_right),
        .select_all = diff_text_edit_transaction_replay_count(before.select_all, after.select_all),
        .clear_selection =
            diff_text_edit_transaction_replay_count(before.clear_selection, after.clear_selection),
        .set_selection =
            diff_text_edit_transaction_replay_count(before.set_selection, after.set_selection),
        .commit_utf8 = diff_text_edit_transaction_replay_count(before.commit_utf8, after.commit_utf8),
        .backspace = diff_text_edit_transaction_replay_count(before.backspace, after.backspace),
        .delete_forward =
            diff_text_edit_transaction_replay_count(before.delete_forward, after.delete_forward),
        .set_preedit = diff_text_edit_transaction_replay_count(before.set_preedit, after.set_preedit),
        .commit_ime = diff_text_edit_transaction_replay_count(before.commit_ime, after.commit_ime),
        .cancel_ime = diff_text_edit_transaction_replay_count(before.cancel_ime, after.cancel_ime),
        .submit = diff_text_edit_transaction_replay_count(before.submit, after.submit),
    };
    deltas.changed = text_edit_transaction_replay_operation_deltas_changed(deltas);
    return deltas;
}

[[nodiscard]] inline text_edit_transaction_replay_aggregate_count_deltas
diff_text_edit_transaction_replay_aggregate_counts(
    const text_edit_transaction_replay_aggregate_counts& before,
    const text_edit_transaction_replay_aggregate_counts& after)
{
    text_edit_transaction_replay_aggregate_count_deltas deltas{
        .operations = diff_text_edit_transaction_replay_operation_counts(before.operations, after.operations),
        .step_count = diff_text_edit_transaction_replay_count(before.step_count, after.step_count),
        .accepted_count = diff_text_edit_transaction_replay_count(before.accepted_count, after.accepted_count),
        .rejected_count = diff_text_edit_transaction_replay_count(before.rejected_count, after.rejected_count),
        .invalid_edit_rejected_count =
            diff_text_edit_transaction_replay_count(
                before.invalid_edit_rejected_count,
                after.invalid_edit_rejected_count),
        .utf8_boundary_safe_count =
            diff_text_edit_transaction_replay_count(
                before.utf8_boundary_safe_count,
                after.utf8_boundary_safe_count),
        .utf8_boundary_unsafe_count =
            diff_text_edit_transaction_replay_count(
                before.utf8_boundary_unsafe_count,
                after.utf8_boundary_unsafe_count),
        .text_changed_count =
            diff_text_edit_transaction_replay_count(before.text_changed_count, after.text_changed_count),
        .display_text_changed_count =
            diff_text_edit_transaction_replay_count(
                before.display_text_changed_count,
                after.display_text_changed_count),
        .caret_changed_count =
            diff_text_edit_transaction_replay_count(before.caret_changed_count, after.caret_changed_count),
        .selection_changed_count =
            diff_text_edit_transaction_replay_count(
                before.selection_changed_count,
                after.selection_changed_count),
        .preedit_changed_count =
            diff_text_edit_transaction_replay_count(before.preedit_changed_count, after.preedit_changed_count),
        .submit_available_after_count =
            diff_text_edit_transaction_replay_count(
                before.submit_available_after_count,
                after.submit_available_after_count),
        .selection_replaced_committed_text_count =
            diff_text_edit_transaction_replay_count(
                before.selection_replaced_committed_text_count,
                after.selection_replaced_committed_text_count),
        .selection_replaced_display_text_count =
            diff_text_edit_transaction_replay_count(
                before.selection_replaced_display_text_count,
                after.selection_replaced_display_text_count),
        .selection_deleted_count =
            diff_text_edit_transaction_replay_count(
                before.selection_deleted_count,
                after.selection_deleted_count),
        .selection_cleared_count =
            diff_text_edit_transaction_replay_count(
                before.selection_cleared_count,
                after.selection_cleared_count),
        .ime_preedit_started_count =
            diff_text_edit_transaction_replay_count(
                before.ime_preedit_started_count,
                after.ime_preedit_started_count),
        .ime_preedit_updated_count =
            diff_text_edit_transaction_replay_count(
                before.ime_preedit_updated_count,
                after.ime_preedit_updated_count),
        .ime_preedit_cleared_count =
            diff_text_edit_transaction_replay_count(
                before.ime_preedit_cleared_count,
                after.ime_preedit_cleared_count),
        .ime_committed_count =
            diff_text_edit_transaction_replay_count(before.ime_committed_count, after.ime_committed_count),
        .ime_canceled_count =
            diff_text_edit_transaction_replay_count(before.ime_canceled_count, after.ime_canceled_count),
        .inserted_byte_count =
            diff_text_edit_transaction_replay_count(before.inserted_byte_count, after.inserted_byte_count),
        .deleted_byte_count =
            diff_text_edit_transaction_replay_count(before.deleted_byte_count, after.deleted_byte_count),
        .replaced_byte_count =
            diff_text_edit_transaction_replay_count(before.replaced_byte_count, after.replaced_byte_count),
        .committed_byte_delta =
            diff_text_edit_transaction_replay_int(before.committed_byte_delta, after.committed_byte_delta),
        .display_byte_delta =
            diff_text_edit_transaction_replay_int(before.display_byte_delta, after.display_byte_delta),
    };
    deltas.changed =
        deltas.operations.changed
        || deltas.step_count.changed
        || deltas.accepted_count.changed
        || deltas.rejected_count.changed
        || deltas.invalid_edit_rejected_count.changed
        || deltas.utf8_boundary_safe_count.changed
        || deltas.utf8_boundary_unsafe_count.changed
        || deltas.text_changed_count.changed
        || deltas.display_text_changed_count.changed
        || deltas.caret_changed_count.changed
        || deltas.selection_changed_count.changed
        || deltas.preedit_changed_count.changed
        || deltas.submit_available_after_count.changed
        || deltas.selection_replaced_committed_text_count.changed
        || deltas.selection_replaced_display_text_count.changed
        || deltas.selection_deleted_count.changed
        || deltas.selection_cleared_count.changed
        || deltas.ime_preedit_started_count.changed
        || deltas.ime_preedit_updated_count.changed
        || deltas.ime_preedit_cleared_count.changed
        || deltas.ime_committed_count.changed
        || deltas.ime_canceled_count.changed
        || deltas.inserted_byte_count.changed
        || deltas.deleted_byte_count.changed
        || deltas.replaced_byte_count.changed
        || deltas.committed_byte_delta.changed
        || deltas.display_byte_delta.changed;
    return deltas;
}

inline void count_text_edit_transaction_replay_diff_category(
    bool changed,
    std::size_t& changed_category_count)
{
    if (changed) {
        ++changed_category_count;
    }
}

[[nodiscard]] inline text_edit_transaction_replay_diff diff_text_edit_transaction_replay_summaries(
    const text_edit_transaction_replay_summary& before,
    const text_edit_transaction_replay_summary& after)
{
    text_edit_transaction_replay_diff diff{
        .counts = diff_text_edit_transaction_replay_aggregate_counts(before.counts, after.counts),
        .final_presentation =
            diff_text_input_presentation_snapshots(before.final_presentation, after.final_presentation),
    };
    diff.final_presentation_changed = diff.final_presentation.changed;
    diff.invalid_edit_changed = diff.counts.invalid_edit_rejected_count.changed;
    diff.utf8_boundary_changed =
        diff.counts.utf8_boundary_safe_count.changed
        || diff.counts.utf8_boundary_unsafe_count.changed;
    diff.replacement_changed =
        diff.counts.selection_replaced_committed_text_count.changed
        || diff.counts.selection_replaced_display_text_count.changed
        || diff.counts.selection_deleted_count.changed
        || diff.counts.selection_cleared_count.changed;
    diff.ime_transition_changed =
        diff.counts.ime_preedit_started_count.changed
        || diff.counts.ime_preedit_updated_count.changed
        || diff.counts.ime_preedit_cleared_count.changed
        || diff.counts.ime_committed_count.changed
        || diff.counts.ime_canceled_count.changed;
    diff.submit_changed =
        diff.counts.submit_available_after_count.changed
        || diff.final_presentation.submit_changed;
    count_text_edit_transaction_replay_diff_category(
        diff.final_presentation_changed,
        diff.changed_category_count);
    count_text_edit_transaction_replay_diff_category(diff.invalid_edit_changed, diff.changed_category_count);
    count_text_edit_transaction_replay_diff_category(diff.utf8_boundary_changed, diff.changed_category_count);
    count_text_edit_transaction_replay_diff_category(diff.replacement_changed, diff.changed_category_count);
    count_text_edit_transaction_replay_diff_category(diff.ime_transition_changed, diff.changed_category_count);
    count_text_edit_transaction_replay_diff_category(diff.submit_changed, diff.changed_category_count);
    diff.changed = diff.counts.changed || diff.changed_category_count > 0;
    return diff;
}

} // namespace quiz_vulkan::input
