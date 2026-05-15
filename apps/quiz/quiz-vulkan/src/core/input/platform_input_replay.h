#pragma once

#include "core/input/normalized_input_replay.h"
#include "core/input/normalized_input_replay_diff.h"
#include "core/input/platform_input_engine_adapter.h"
#include "core/input/platform_input_translator.h"
#include "core/input/text_edit_transaction_replay.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace quiz_vulkan::input {

enum class platform_input_replay_source_kind {
    mouse,
    touch,
    wheel,
    key,
    character,
    focus,
    ime,
    time_update,
};

using platform_input_replay_action = std::variant<
    platform_input_translation_request,
    raw_platform_focus_event,
    normalized_input_replay_time_update>;

struct platform_input_replay_step {
    std::string label;
    platform_input_replay_action action;
};

struct platform_input_replay_options {
    std::string initial_focus_target_id;
};

struct platform_input_replay_event_counts {
    std::size_t pointer = 0;
    std::size_t mouse = 0;
    std::size_t touch = 0;
    std::size_t wheel = 0;
    std::size_t key = 0;
    std::size_t character = 0;
    std::size_t focus = 0;
    std::size_t ime = 0;
    std::size_t time_update = 0;
    std::size_t total = 0;
};

struct platform_input_replay_rejection_counts {
    std::size_t invalid_timestamp = 0;
    std::size_t invalid_pointer_id = 0;
    std::size_t invalid_coordinates = 0;
    std::size_t invalid_wheel_delta = 0;
    std::size_t invalid_key = 0;
    std::size_t empty_text = 0;
    std::size_t invalid_utf8 = 0;
};

struct platform_input_replay_translation_counts {
    std::size_t total = 0;
    std::size_t accepted = 0;
    std::size_t rejected = 0;
    std::size_t emitted_event = 0;
    std::size_t dispatched_to_engine = 0;
    std::size_t rejected_without_side_effect = 0;
    platform_input_replay_rejection_counts rejections;
};

struct platform_input_replay_step_diagnostics {
    std::string label;
    platform_input_replay_source_kind source = platform_input_replay_source_kind::mouse;
    bool has_translation = false;
    platform_input_translation_diagnostic translation;
    bool dispatched_to_engine = false;
    bool rejected_without_side_effect = false;
    normalized_input_replay_batch normalized_batch;
    text_input_presentation_snapshot before_presentation;
    text_input_presentation_snapshot after_presentation;
    text_input_presentation_diff text_presentation_diff;
    bool produced_text_edit_transaction = false;
    text_edit_transaction_replay_step_diagnostics text_edit;
};

struct platform_input_replay_summary {
    std::vector<platform_input_replay_step_diagnostics> steps;
    platform_input_replay_event_counts events;
    platform_input_replay_translation_counts translations;
    normalized_input_replay_recording normalized;
    text_edit_transaction_replay_summary text_edits;
    normalized_input_replay_pointer_summary pointer;
    normalized_input_replay_gesture_policy_summary gesture_policies;
    normalized_input_replay_focus_summary focus;
    normalized_input_replay_ime_summary ime;
    normalized_input_replay_keyboard_summary keyboard;
    input_diagnostic_summary route_summary;
    text_input_presentation_snapshot final_text_presentation;
};

struct platform_input_replay_event_count_deltas {
    normalized_input_replay_count_delta pointer;
    normalized_input_replay_count_delta mouse;
    normalized_input_replay_count_delta touch;
    normalized_input_replay_count_delta wheel;
    normalized_input_replay_count_delta key;
    normalized_input_replay_count_delta character;
    normalized_input_replay_count_delta focus;
    normalized_input_replay_count_delta ime;
    normalized_input_replay_count_delta time_update;
    normalized_input_replay_count_delta total;
    bool changed = false;
};

struct platform_input_replay_rejection_count_deltas {
    normalized_input_replay_count_delta invalid_timestamp;
    normalized_input_replay_count_delta invalid_pointer_id;
    normalized_input_replay_count_delta invalid_coordinates;
    normalized_input_replay_count_delta invalid_wheel_delta;
    normalized_input_replay_count_delta invalid_key;
    normalized_input_replay_count_delta empty_text;
    normalized_input_replay_count_delta invalid_utf8;
    bool changed = false;
};

struct platform_input_replay_translation_count_deltas {
    normalized_input_replay_count_delta total;
    normalized_input_replay_count_delta accepted;
    normalized_input_replay_count_delta rejected;
    normalized_input_replay_count_delta emitted_event;
    normalized_input_replay_count_delta dispatched_to_engine;
    normalized_input_replay_count_delta rejected_without_side_effect;
    platform_input_replay_rejection_count_deltas rejections;
    bool changed = false;
};

struct platform_input_replay_diff {
    platform_input_replay_event_count_deltas events;
    platform_input_replay_translation_count_deltas translations;
    normalized_input_replay_diff normalized;
    text_edit_transaction_replay_diff text_edits;
    text_input_presentation_diff final_text_presentation;
    bool event_counts_changed = false;
    bool translation_counts_changed = false;
    bool normalized_changed = false;
    bool text_edit_changed = false;
    bool gesture_capture_focus_changed = false;
    bool final_text_presentation_changed = false;
    std::size_t changed_category_count = 0;
    bool changed = false;
};

[[nodiscard]] inline platform_input_replay_source_kind platform_input_replay_source_for(
    const platform_input_translation_request& request)
{
    return std::visit(
        [](const auto& sample) -> platform_input_replay_source_kind {
            using sample_type = std::decay_t<decltype(sample)>;
            if constexpr (std::is_same_v<sample_type, platform_mouse_sample>) {
                return platform_input_replay_source_kind::mouse;
            } else if constexpr (std::is_same_v<sample_type, platform_touch_sample>) {
                return platform_input_replay_source_kind::touch;
            } else if constexpr (std::is_same_v<sample_type, platform_wheel_sample>) {
                return platform_input_replay_source_kind::wheel;
            } else if constexpr (std::is_same_v<sample_type, platform_key_sample>) {
                return platform_input_replay_source_kind::key;
            } else if constexpr (std::is_same_v<sample_type, platform_character_sample>) {
                return platform_input_replay_source_kind::character;
            } else {
                return platform_input_replay_source_kind::ime;
            }
        },
        request.sample);
}

inline void count_platform_input_replay_event(
    platform_input_replay_event_counts& counts,
    platform_input_replay_source_kind source)
{
    ++counts.total;
    switch (source) {
    case platform_input_replay_source_kind::mouse:
        ++counts.pointer;
        ++counts.mouse;
        return;
    case platform_input_replay_source_kind::touch:
        ++counts.pointer;
        ++counts.touch;
        return;
    case platform_input_replay_source_kind::wheel:
        ++counts.wheel;
        return;
    case platform_input_replay_source_kind::key:
        ++counts.key;
        return;
    case platform_input_replay_source_kind::character:
        ++counts.character;
        return;
    case platform_input_replay_source_kind::focus:
        ++counts.focus;
        return;
    case platform_input_replay_source_kind::ime:
        ++counts.ime;
        return;
    case platform_input_replay_source_kind::time_update:
        ++counts.time_update;
        return;
    }
}

inline void count_platform_input_replay_rejection(
    platform_input_replay_rejection_counts& counts,
    platform_input_rejection_reason reason)
{
    switch (reason) {
    case platform_input_rejection_reason::none:
        return;
    case platform_input_rejection_reason::invalid_timestamp:
        ++counts.invalid_timestamp;
        return;
    case platform_input_rejection_reason::invalid_pointer_id:
        ++counts.invalid_pointer_id;
        return;
    case platform_input_rejection_reason::invalid_coordinates:
        ++counts.invalid_coordinates;
        return;
    case platform_input_rejection_reason::invalid_wheel_delta:
        ++counts.invalid_wheel_delta;
        return;
    case platform_input_rejection_reason::invalid_key:
        ++counts.invalid_key;
        return;
    case platform_input_rejection_reason::empty_text:
        ++counts.empty_text;
        return;
    case platform_input_rejection_reason::invalid_utf8:
        ++counts.invalid_utf8;
        return;
    }
}

inline void count_platform_input_replay_translation(
    platform_input_replay_translation_counts& counts,
    const platform_input_translation_diagnostic& diagnostic,
    bool dispatched_to_engine)
{
    ++counts.total;
    if (diagnostic.status == platform_input_translation_status::accepted) {
        ++counts.accepted;
    } else {
        ++counts.rejected;
    }
    if (diagnostic.emitted_event) {
        ++counts.emitted_event;
    }
    if (dispatched_to_engine) {
        ++counts.dispatched_to_engine;
    }
    if (diagnostic.status == platform_input_translation_status::rejected && !dispatched_to_engine) {
        ++counts.rejected_without_side_effect;
    }
    count_platform_input_replay_rejection(counts.rejections, diagnostic.rejection_reason);
}

[[nodiscard]] inline bool platform_input_replay_route_has_text_transaction(
    action_route_policy_kind kind)
{
    switch (kind) {
    case action_route_policy_kind::text_commit_boundary:
    case action_route_policy_kind::text_backspace_boundary:
    case action_route_policy_kind::text_delete_forward_boundary:
    case action_route_policy_kind::caret_moved:
    case action_route_policy_kind::selection_changed:
    case action_route_policy_kind::text_submit_boundary:
    case action_route_policy_kind::focus_loss:
    case action_route_policy_kind::ime_preedit:
    case action_route_policy_kind::ime_commit:
    case action_route_policy_kind::ime_cancel:
    case action_route_policy_kind::ime_composition_start:
        return true;
    case action_route_policy_kind::pointer_capture_reset:
    case action_route_policy_kind::pointer_capture_arbitration:
    case action_route_policy_kind::wheel_summary:
    case action_route_policy_kind::gesture_route_snapshot:
    case action_route_policy_kind::focus_traversal_next:
    case action_route_policy_kind::focus_traversal_previous:
    case action_route_policy_kind::keyboard_cancel_intent:
        return false;
    }

    return false;
}

[[nodiscard]] inline bool platform_input_replay_routes_have_text_transaction(
    std::span<const action_route_policy_diagnostic> routes)
{
    for (const action_route_policy_diagnostic& route : routes) {
        if (platform_input_replay_route_has_text_transaction(route.kind)) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] inline text_edit_transaction_replay_step_diagnostics make_platform_input_replay_text_edit_step(
    std::string label,
    const text_edit_transaction_diagnostics& transaction,
    const text_input_presentation_snapshot& before,
    const text_input_presentation_snapshot& after)
{
    return text_edit_transaction_replay_step_diagnostics{
        .label = std::move(label),
        .operation = transaction.operation,
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

[[nodiscard]] inline normalized_input_replay_batch make_platform_input_replay_empty_batch(
    input_engine& engine,
    std::string label)
{
    const normalized_input_replay_end_state state = capture_normalized_input_replay_end_state(engine);
    normalized_input_replay_batch batch{
        .label = std::move(label),
        .end_state = state,
        .text_presentation_diff =
            diff_text_input_presentation_snapshots(state.text_presentation, state.text_presentation),
    };
    batch.ime = summarize_normalized_input_replay_ime_routes(
        std::span<const action_route_policy_diagnostic>{},
        std::span<const input_event>{},
        state);
    batch.pointer = summarize_normalized_input_replay_pointer_routes(
        std::span<const action_route_policy_diagnostic>{},
        state);
    batch.focus = summarize_normalized_input_replay_focus_routes(
        std::span<const action_route_policy_diagnostic>{},
        std::span<const input_event>{},
        state,
        state);
    return batch;
}

[[nodiscard]] inline normalized_input_replay_batch make_platform_input_replay_batch_from_dispatch(
    std::string label,
    const normalized_input_replay_end_state& before_state,
    const platform_input_dispatch_result& dispatch,
    const input_engine& engine)
{
    const input_routing_diagnostics& diagnostics = dispatch.routing_diagnostics;
    normalized_input_replay_end_state end_state = capture_normalized_input_replay_end_state(engine);
    normalized_input_replay_ime_summary ime =
        summarize_normalized_input_replay_ime_routes(diagnostics.action_routes, dispatch.input_events, end_state);
    normalized_input_replay_pointer_summary pointer =
        summarize_normalized_input_replay_pointer_routes(diagnostics.action_routes, end_state);
    normalized_input_replay_focus_summary focus =
        summarize_normalized_input_replay_focus_routes(
            diagnostics.action_routes,
            dispatch.input_events,
            before_state,
            end_state);
    normalized_input_replay_gesture_policy_summary gesture_policies =
        summarize_normalized_input_replay_gesture_policy_routes(diagnostics.action_routes);
    const text_input_presentation_diff text_presentation_diff =
        diff_text_input_presentation_snapshots(
            before_state.text_presentation,
            end_state.text_presentation);
    return normalized_input_replay_batch{
        .label = std::move(label),
        .input_events = dispatch.input_events,
        .normalized_events = diagnostics.normalized_events,
        .summary = diagnostics.summary,
        .keyboard = summarize_normalized_input_replay_keyboard_routes(diagnostics.action_routes),
        .ime = std::move(ime),
        .pointer = std::move(pointer),
        .gesture_policies = std::move(gesture_policies),
        .focus = std::move(focus),
        .end_state = std::move(end_state),
        .text_presentation_diff = text_presentation_diff,
    };
}

inline void append_platform_input_replay_normalized_batch(
    normalized_input_replay_recording& recording,
    normalized_input_replay_batch batch)
{
    accumulate_input_diagnostic_summary(recording.summary, batch.summary);
    accumulate_normalized_input_replay_keyboard_summary(recording.keyboard, batch.keyboard);
    accumulate_normalized_input_replay_ime_summary(recording.ime, batch.ime);
    accumulate_normalized_input_replay_pointer_summary(recording.pointer, batch.pointer);
    accumulate_normalized_input_replay_gesture_policy_summary(
        recording.gesture_policies,
        batch.gesture_policies);
    accumulate_normalized_input_replay_focus_summary(recording.focus, batch.focus);
    recording.final_state = batch.end_state;
    recording.batches.push_back(std::move(batch));
}

inline void append_platform_input_replay_text_edit_if_present(
    platform_input_replay_summary& summary,
    platform_input_replay_step_diagnostics& step,
    const input_engine& engine,
    const input_routing_diagnostics& diagnostics)
{
    if (!platform_input_replay_routes_have_text_transaction(diagnostics.action_routes)) {
        return;
    }

    step.produced_text_edit_transaction = true;
    step.text_edit = make_platform_input_replay_text_edit_step(
        step.label,
        engine.text_model().last_transaction_diagnostics(),
        step.before_presentation,
        step.after_presentation);
    accumulate_text_edit_transaction_replay_counts(summary.text_edits.counts, step.text_edit);
    summary.text_edits.final_transaction = step.text_edit.transaction;
    summary.text_edits.steps.push_back(step.text_edit);
}

inline void finalize_platform_input_replay_summary(
    platform_input_replay_summary& summary,
    const input_engine& engine)
{
    summary.route_summary = summary.normalized.summary;
    summary.pointer = summary.normalized.pointer;
    summary.gesture_policies = summary.normalized.gesture_policies;
    summary.focus = summary.normalized.focus;
    summary.ime = summary.normalized.ime;
    summary.keyboard = summary.normalized.keyboard;
    summary.final_text_presentation = engine.text_presentation_snapshot();
    summary.text_edits.final_presentation = summary.final_text_presentation;
    summary.text_edits.final_submit_available = summary.final_text_presentation.has_submit_text;
    if (!summary.text_edits.steps.empty()) {
        summary.text_edits.final_utf8_boundary_safe = summary.text_edits.steps.back().utf8_boundary_safe;
    }
    if (summary.steps.empty()) {
        summary.normalized.final_state = capture_normalized_input_replay_end_state(engine);
        summary.normalized.ime = summarize_normalized_input_replay_ime_routes(
            std::span<const action_route_policy_diagnostic>{},
            std::span<const input_event>{},
            summary.normalized.final_state);
        summary.normalized.pointer = summarize_normalized_input_replay_pointer_routes(
            std::span<const action_route_policy_diagnostic>{},
            summary.normalized.final_state);
        summary.normalized.focus = summarize_normalized_input_replay_focus_routes(
            std::span<const action_route_policy_diagnostic>{},
            std::span<const input_event>{},
            summary.normalized.final_state,
            summary.normalized.final_state);
        summary.pointer = summary.normalized.pointer;
        summary.focus = summary.normalized.focus;
        summary.ime = summary.normalized.ime;
    }
}

[[nodiscard]] inline platform_input_replay_step_diagnostics replay_platform_input_step(
    input_engine& engine,
    const platform_input_translator& translator,
    platform_input_replay_summary& summary,
    const platform_input_replay_step& step)
{
    platform_input_replay_step_diagnostics diagnostics{
        .label = step.label,
        .before_presentation = engine.text_presentation_snapshot(),
    };

    std::visit(
        [&engine, &translator, &summary, &diagnostics](const auto& action) {
            using action_type = std::decay_t<decltype(action)>;
            if constexpr (std::is_same_v<action_type, platform_input_translation_request>) {
                diagnostics.source = platform_input_replay_source_for(action);
                count_platform_input_replay_event(summary.events, diagnostics.source);
                const normalized_input_replay_end_state before_state =
                    capture_normalized_input_replay_end_state(engine);
                platform_input_dispatch_result dispatch =
                    translate_and_dispatch_platform_input(engine, translator, action);
                diagnostics.has_translation = true;
                diagnostics.translation = dispatch.translation.diagnostic;
                diagnostics.dispatched_to_engine = dispatch.dispatched_to_engine;
                diagnostics.rejected_without_side_effect =
                    dispatch.translation.diagnostic.status == platform_input_translation_status::rejected
                    && !dispatch.dispatched_to_engine;
                count_platform_input_replay_translation(
                    summary.translations,
                    diagnostics.translation,
                    diagnostics.dispatched_to_engine);
                diagnostics.after_presentation = engine.text_presentation_snapshot();
                diagnostics.text_presentation_diff = diff_text_input_presentation_snapshots(
                    diagnostics.before_presentation,
                    diagnostics.after_presentation);
                if (dispatch.dispatched_to_engine) {
                    diagnostics.normalized_batch = make_platform_input_replay_batch_from_dispatch(
                        diagnostics.label,
                        before_state,
                        dispatch,
                        engine);
                    append_platform_input_replay_text_edit_if_present(
                        summary,
                        diagnostics,
                        engine,
                        dispatch.routing_diagnostics);
                } else {
                    diagnostics.normalized_batch =
                        make_platform_input_replay_empty_batch(engine, diagnostics.label);
                }
                append_platform_input_replay_normalized_batch(
                    summary.normalized,
                    diagnostics.normalized_batch);
            } else if constexpr (std::is_same_v<action_type, raw_platform_focus_event>) {
                diagnostics.source = platform_input_replay_source_kind::focus;
                count_platform_input_replay_event(summary.events, diagnostics.source);
                const normalized_input_replay_end_state before_state =
                    capture_normalized_input_replay_end_state(engine);
                platform_input_dispatch_result dispatch{
                    .dispatched_to_engine = true,
                    .input_events = engine.process_raw_event(raw_platform_input_event{action}),
                    .routing_diagnostics = engine.routing_diagnostics(),
                };
                diagnostics.dispatched_to_engine = true;
                diagnostics.after_presentation = engine.text_presentation_snapshot();
                diagnostics.text_presentation_diff = diff_text_input_presentation_snapshots(
                    diagnostics.before_presentation,
                    diagnostics.after_presentation);
                diagnostics.normalized_batch = make_platform_input_replay_batch_from_dispatch(
                    diagnostics.label,
                    before_state,
                    dispatch,
                    engine);
                append_platform_input_replay_text_edit_if_present(
                    summary,
                    diagnostics,
                    engine,
                    dispatch.routing_diagnostics);
                append_platform_input_replay_normalized_batch(
                    summary.normalized,
                    diagnostics.normalized_batch);
            } else {
                diagnostics.source = platform_input_replay_source_kind::time_update;
                count_platform_input_replay_event(summary.events, diagnostics.source);
                const normalized_input_replay_end_state before_state =
                    capture_normalized_input_replay_end_state(engine);
                platform_input_dispatch_result dispatch{
                    .dispatched_to_engine = true,
                    .input_events = engine.update_time(action.timestamp_ms),
                    .routing_diagnostics = engine.routing_diagnostics(),
                };
                diagnostics.dispatched_to_engine = true;
                diagnostics.after_presentation = engine.text_presentation_snapshot();
                diagnostics.text_presentation_diff = diff_text_input_presentation_snapshots(
                    diagnostics.before_presentation,
                    diagnostics.after_presentation);
                diagnostics.normalized_batch = make_platform_input_replay_batch_from_dispatch(
                    diagnostics.label,
                    before_state,
                    dispatch,
                    engine);
                append_platform_input_replay_normalized_batch(
                    summary.normalized,
                    diagnostics.normalized_batch);
            }
        },
        step.action);

    return diagnostics;
}

[[nodiscard]] inline platform_input_replay_summary replay_platform_input_batch(
    input_engine& engine,
    const platform_input_translator& translator,
    std::span<const platform_input_replay_step> steps,
    const platform_input_replay_options& options = {})
{
    if (!options.initial_focus_target_id.empty()) {
        engine.focus_text_target(options.initial_focus_target_id);
    }

    platform_input_replay_summary summary;
    summary.steps.reserve(steps.size());
    for (const platform_input_replay_step& step : steps) {
        summary.steps.push_back(replay_platform_input_step(engine, translator, summary, step));
    }
    finalize_platform_input_replay_summary(summary, engine);
    return summary;
}

[[nodiscard]] inline platform_input_replay_summary replay_platform_input_batch(
    std::span<const platform_input_replay_step> steps,
    const platform_input_replay_options& options = {})
{
    input_engine engine;
    platform_input_translator translator;
    return replay_platform_input_batch(engine, translator, steps, options);
}

[[nodiscard]] inline bool platform_input_replay_event_deltas_changed(
    const platform_input_replay_event_count_deltas& deltas)
{
    return deltas.pointer.changed
        || deltas.mouse.changed
        || deltas.touch.changed
        || deltas.wheel.changed
        || deltas.key.changed
        || deltas.character.changed
        || deltas.focus.changed
        || deltas.ime.changed
        || deltas.time_update.changed
        || deltas.total.changed;
}

[[nodiscard]] inline platform_input_replay_event_count_deltas diff_platform_input_replay_event_counts(
    const platform_input_replay_event_counts& before,
    const platform_input_replay_event_counts& after)
{
    platform_input_replay_event_count_deltas deltas{
        .pointer = normalized_input_replay_diff_count(before.pointer, after.pointer),
        .mouse = normalized_input_replay_diff_count(before.mouse, after.mouse),
        .touch = normalized_input_replay_diff_count(before.touch, after.touch),
        .wheel = normalized_input_replay_diff_count(before.wheel, after.wheel),
        .key = normalized_input_replay_diff_count(before.key, after.key),
        .character = normalized_input_replay_diff_count(before.character, after.character),
        .focus = normalized_input_replay_diff_count(before.focus, after.focus),
        .ime = normalized_input_replay_diff_count(before.ime, after.ime),
        .time_update = normalized_input_replay_diff_count(before.time_update, after.time_update),
        .total = normalized_input_replay_diff_count(before.total, after.total),
    };
    deltas.changed = platform_input_replay_event_deltas_changed(deltas);
    return deltas;
}

[[nodiscard]] inline bool platform_input_replay_rejection_deltas_changed(
    const platform_input_replay_rejection_count_deltas& deltas)
{
    return deltas.invalid_timestamp.changed
        || deltas.invalid_pointer_id.changed
        || deltas.invalid_coordinates.changed
        || deltas.invalid_wheel_delta.changed
        || deltas.invalid_key.changed
        || deltas.empty_text.changed
        || deltas.invalid_utf8.changed;
}

[[nodiscard]] inline platform_input_replay_rejection_count_deltas
diff_platform_input_replay_rejection_counts(
    const platform_input_replay_rejection_counts& before,
    const platform_input_replay_rejection_counts& after)
{
    platform_input_replay_rejection_count_deltas deltas{
        .invalid_timestamp =
            normalized_input_replay_diff_count(before.invalid_timestamp, after.invalid_timestamp),
        .invalid_pointer_id =
            normalized_input_replay_diff_count(before.invalid_pointer_id, after.invalid_pointer_id),
        .invalid_coordinates =
            normalized_input_replay_diff_count(before.invalid_coordinates, after.invalid_coordinates),
        .invalid_wheel_delta =
            normalized_input_replay_diff_count(before.invalid_wheel_delta, after.invalid_wheel_delta),
        .invalid_key = normalized_input_replay_diff_count(before.invalid_key, after.invalid_key),
        .empty_text = normalized_input_replay_diff_count(before.empty_text, after.empty_text),
        .invalid_utf8 = normalized_input_replay_diff_count(before.invalid_utf8, after.invalid_utf8),
    };
    deltas.changed = platform_input_replay_rejection_deltas_changed(deltas);
    return deltas;
}

[[nodiscard]] inline platform_input_replay_translation_count_deltas
diff_platform_input_replay_translation_counts(
    const platform_input_replay_translation_counts& before,
    const platform_input_replay_translation_counts& after)
{
    platform_input_replay_translation_count_deltas deltas{
        .total = normalized_input_replay_diff_count(before.total, after.total),
        .accepted = normalized_input_replay_diff_count(before.accepted, after.accepted),
        .rejected = normalized_input_replay_diff_count(before.rejected, after.rejected),
        .emitted_event = normalized_input_replay_diff_count(before.emitted_event, after.emitted_event),
        .dispatched_to_engine =
            normalized_input_replay_diff_count(before.dispatched_to_engine, after.dispatched_to_engine),
        .rejected_without_side_effect = normalized_input_replay_diff_count(
            before.rejected_without_side_effect,
            after.rejected_without_side_effect),
        .rejections = diff_platform_input_replay_rejection_counts(before.rejections, after.rejections),
    };
    deltas.changed =
        deltas.total.changed
        || deltas.accepted.changed
        || deltas.rejected.changed
        || deltas.emitted_event.changed
        || deltas.dispatched_to_engine.changed
        || deltas.rejected_without_side_effect.changed
        || deltas.rejections.changed;
    return deltas;
}

inline void count_platform_input_replay_diff_category(
    bool changed,
    std::size_t& changed_category_count)
{
    if (changed) {
        ++changed_category_count;
    }
}

[[nodiscard]] inline platform_input_replay_diff diff_platform_input_replay_summaries(
    const platform_input_replay_summary& before,
    const platform_input_replay_summary& after)
{
    platform_input_replay_diff diff{
        .events = diff_platform_input_replay_event_counts(before.events, after.events),
        .translations = diff_platform_input_replay_translation_counts(before.translations, after.translations),
        .normalized = diff_normalized_input_replay_recordings(before.normalized, after.normalized),
        .text_edits = diff_text_edit_transaction_replay_summaries(before.text_edits, after.text_edits),
        .final_text_presentation =
            diff_text_input_presentation_snapshots(
                before.final_text_presentation,
                after.final_text_presentation),
    };
    diff.event_counts_changed = diff.events.changed;
    diff.translation_counts_changed = diff.translations.changed;
    diff.normalized_changed = diff.normalized.regression.changed;
    diff.text_edit_changed = diff.text_edits.changed;
    diff.gesture_capture_focus_changed =
        diff.normalized.pointer.changed
        || diff.normalized.gesture_policies.changed
        || diff.normalized.focus.changed;
    diff.final_text_presentation_changed = diff.final_text_presentation.changed;
    count_platform_input_replay_diff_category(diff.event_counts_changed, diff.changed_category_count);
    count_platform_input_replay_diff_category(diff.translation_counts_changed, diff.changed_category_count);
    count_platform_input_replay_diff_category(diff.normalized_changed, diff.changed_category_count);
    count_platform_input_replay_diff_category(diff.text_edit_changed, diff.changed_category_count);
    count_platform_input_replay_diff_category(
        diff.gesture_capture_focus_changed,
        diff.changed_category_count);
    count_platform_input_replay_diff_category(
        diff.final_text_presentation_changed,
        diff.changed_category_count);
    diff.changed = diff.changed_category_count > 0;
    return diff;
}

} // namespace quiz_vulkan::input
