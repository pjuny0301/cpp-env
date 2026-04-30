#include "core/input/input_engine.h"

#include <algorithm>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace quiz_vulkan::input {
namespace {

struct text_edit_snapshot {
    std::size_t text_byte_count = 0;
    text_range caret;
    bool has_selection = false;
    text_range selection;
};

bool is_primary_pointer(const raw_platform_pointer_event& event)
{
    return event.button == raw_platform_pointer_button::none
        || event.button == raw_platform_pointer_button::primary;
}

pointer_contact_kind pointer_contact_for(const raw_platform_pointer_event& event)
{
    return event.button == raw_platform_pointer_button::none
        ? pointer_contact_kind::touch_like
        : pointer_contact_kind::mouse_like;
}

pointer_phase to_pointer_phase(raw_platform_pointer_phase phase)
{
    switch (phase) {
    case raw_platform_pointer_phase::down:
        return pointer_phase::down;
    case raw_platform_pointer_phase::move:
        return pointer_phase::move;
    case raw_platform_pointer_phase::up:
        return pointer_phase::up;
    case raw_platform_pointer_phase::cancel:
        return pointer_phase::cancel;
    }

    return pointer_phase::cancel;
}

scroll_delta_unit to_scroll_delta_unit(raw_platform_scroll_delta_unit unit)
{
    switch (unit) {
    case raw_platform_scroll_delta_unit::pixels:
        return scroll_delta_unit::pixels;
    case raw_platform_scroll_delta_unit::lines:
        return scroll_delta_unit::lines;
    }

    return scroll_delta_unit::pixels;
}

bool is_backspace_key(const raw_platform_key_event& event)
{
    return event.logical_key == "Backspace" || event.key_code == 8;
}

bool is_submit_key(const raw_platform_key_event& event)
{
    return event.logical_key == "Enter"
        || event.logical_key == "NumpadEnter"
        || event.logical_key == "Return"
        || event.key_code == 13;
}

bool is_arrow_left_key(const raw_platform_key_event& event)
{
    return event.logical_key == "ArrowLeft"
        || event.logical_key == "Left"
        || event.key_code == 37;
}

bool is_arrow_right_key(const raw_platform_key_event& event)
{
    return event.logical_key == "ArrowRight"
        || event.logical_key == "Right"
        || event.key_code == 39;
}

bool is_home_key(const raw_platform_key_event& event)
{
    return event.logical_key == "Home" || event.key_code == 36;
}

bool is_end_key(const raw_platform_key_event& event)
{
    return event.logical_key == "End" || event.key_code == 35;
}

bool is_tab_key(const raw_platform_key_event& event)
{
    return event.logical_key == "Tab" || event.key_code == 9;
}

bool is_a_key(const raw_platform_key_event& event)
{
    return event.logical_key == "a"
        || event.logical_key == "A"
        || event.logical_key == "KeyA"
        || event.key_code == 65;
}

bool is_select_all_key(const raw_platform_key_event& event)
{
    return !event.alt && (event.ctrl || event.meta) && is_a_key(event);
}

bool is_keyboard_navigation_key(const raw_platform_key_event& event)
{
    return is_arrow_left_key(event)
        || is_arrow_right_key(event)
        || is_home_key(event)
        || is_end_key(event)
        || is_tab_key(event)
        || is_select_all_key(event);
}

action_route_policy_kind navigation_policy_kind(const raw_platform_key_event& event)
{
    if (is_tab_key(event)) {
        return event.shift
            ? action_route_policy_kind::focus_traversal_previous
            : action_route_policy_kind::focus_traversal_next;
    }

    if (is_select_all_key(event)
        || ((is_arrow_left_key(event) || is_arrow_right_key(event)) && event.shift)) {
        return action_route_policy_kind::selection_changed;
    }

    return action_route_policy_kind::caret_moved;
}

ime_event make_ime_event(
    ime_event_kind kind,
    std::int64_t timestamp_ms,
    const std::string& target_id,
    std::string utf8_text,
    ime_composition_state composition)
{
    return ime_event{
        .kind = kind,
        .timestamp_ms = timestamp_ms,
        .target_id = target_id,
        .utf8_text = std::move(utf8_text),
        .composition = std::move(composition),
    };
}

input_event_summary_kind summary_kind_for(gesture_kind kind)
{
    switch (kind) {
    case gesture_kind::tap:
        return input_event_summary_kind::tap;
    case gesture_kind::long_press:
        return input_event_summary_kind::long_press;
    case gesture_kind::swipe_left:
        return input_event_summary_kind::swipe_left;
    case gesture_kind::swipe_right:
        return input_event_summary_kind::swipe_right;
    case gesture_kind::drag_start:
        return input_event_summary_kind::drag_start;
    case gesture_kind::drag_update:
        return input_event_summary_kind::drag_update;
    case gesture_kind::drag_end:
        return input_event_summary_kind::drag_end;
    case gesture_kind::drag_cancel:
        return input_event_summary_kind::drag_cancel;
    }

    return input_event_summary_kind::tap;
}

normalized_input_event_summary summarize_gesture(const gesture_event& gesture)
{
    return normalized_input_event_summary{
        .kind = summary_kind_for(gesture.kind),
        .timestamp_ms = gesture.timestamp_ms,
        .duration_ms = gesture.duration_ms,
        .pointer_id = gesture.pointer_id,
        .start_x = gesture.start_x,
        .start_y = gesture.start_y,
        .x = gesture.x,
        .y = gesture.y,
        .delta_x = gesture.delta_x,
        .delta_y = gesture.delta_y,
    };
}

normalized_input_event_summary summarize_scroll(const scroll_event& scroll)
{
    return normalized_input_event_summary{
        .kind = input_event_summary_kind::wheel,
        .timestamp_ms = scroll.timestamp_ms,
        .x = scroll.x,
        .y = scroll.y,
        .pixel_delta_x = scroll.pixel_delta_x,
        .pixel_delta_y = scroll.pixel_delta_y,
        .line_delta_x = scroll.line_delta_x,
        .line_delta_y = scroll.line_delta_y,
    };
}

bool has_pointer_capture_state(const pointer_capture_snapshot& snapshot)
{
    return snapshot.lifecycle != pointer_capture_lifecycle::idle
        || snapshot.active
        || snapshot.tracked_pointer_count > 0;
}

bool pointer_capture_changed(const pointer_capture_snapshot& before, const pointer_capture_snapshot& after)
{
    return before.lifecycle != after.lifecycle
        || before.active != after.active
        || before.pointer_id != after.pointer_id
        || before.tracked_pointer_count != after.tracked_pointer_count;
}

void apply_pointer_arbitration(
    action_route_policy_diagnostic& diagnostic,
    std::int32_t pointer_id,
    pointer_phase phase,
    pointer_arbitration_decision decision)
{
    diagnostic.pointer_id = pointer_id;
    diagnostic.pointer_event_phase = phase;
    diagnostic.pointer_decision = decision;
}

void apply_pointer_route_context(
    action_route_policy_diagnostic& diagnostic,
    const raw_platform_pointer_event& event,
    pointer_phase phase,
    pointer_arbitration_decision decision,
    const pointer_capture_snapshot& pointer_capture_before,
    const pointer_capture_snapshot& pointer_capture_after)
{
    apply_pointer_arbitration(diagnostic, event.pointer_id, phase, decision);
    diagnostic.pointer_contact = pointer_contact_for(event);
    diagnostic.tracked_pointer_count_before = pointer_capture_before.tracked_pointer_count;
    diagnostic.tracked_pointer_count_after = pointer_capture_after.tracked_pointer_count;
}

bool has_gesture_kind(
    const std::vector<gesture_event>& gestures,
    std::int32_t pointer_id,
    gesture_kind kind)
{
    return std::ranges::any_of(gestures, [pointer_id, kind](const gesture_event& gesture) {
        return gesture.pointer_id == pointer_id && gesture.kind == kind;
    });
}

bool is_emitless_gesture_policy(const gesture_policy_snapshot& snapshot)
{
    switch (snapshot.decision) {
    case gesture_policy_decision::swipe_rejected_distance:
    case gesture_policy_decision::swipe_rejected_cross_axis:
    case gesture_policy_decision::swipe_rejected_duration:
    case gesture_policy_decision::release_suppressed:
        return true;
    case gesture_policy_decision::none:
    case gesture_policy_decision::tracking_started:
    case gesture_policy_decision::tap_accepted:
    case gesture_policy_decision::long_press_accepted:
    case gesture_policy_decision::swipe_accepted:
    case gesture_policy_decision::drag_started:
    case gesture_policy_decision::drag_updated:
    case gesture_policy_decision::drag_released:
    case gesture_policy_decision::drag_canceled:
    case gesture_policy_decision::ignored_by_capture:
        return false;
    }

    return false;
}

const gesture_policy_snapshot* find_policy_for_gesture(
    const std::vector<gesture_policy_snapshot>& policies,
    const gesture_event& gesture)
{
    const auto policy = std::ranges::find_if(policies, [&gesture](const gesture_policy_snapshot& snapshot) {
        return snapshot.emitted_input_event
            && snapshot.emitted_kind == gesture.kind
            && snapshot.pointer_id == gesture.pointer_id
            && snapshot.timestamp_ms == gesture.timestamp_ms;
    });
    return policy == policies.end() ? nullptr : &*policy;
}

pointer_arbitration_decision pointer_decision_for(
    const raw_platform_pointer_event& event,
    pointer_phase phase,
    const pointer_capture_snapshot& before,
    const pointer_capture_snapshot& after,
    const std::vector<gesture_event>& gestures)
{
    if (before.active && before.pointer_id != event.pointer_id) {
        return pointer_arbitration_decision::ignored_by_capture;
    }

    if (phase == pointer_phase::down
        && has_gesture_kind(gestures, event.pointer_id, gesture_kind::drag_cancel)) {
        return pointer_arbitration_decision::restarted;
    }

    if (phase == pointer_phase::cancel
        && has_pointer_capture_state(before)
        && pointer_capture_changed(before, after)) {
        return pointer_arbitration_decision::canceled;
    }

    if (before.active
        && before.pointer_id == event.pointer_id
        && after.lifecycle == pointer_capture_lifecycle::idle
        && phase == pointer_phase::up) {
        return pointer_arbitration_decision::released;
    }

    if (!before.active && after.active && after.pointer_id == event.pointer_id) {
        return pointer_arbitration_decision::captured;
    }

    if (phase == pointer_phase::down
        && after.lifecycle == pointer_capture_lifecycle::tracking
        && pointer_capture_changed(before, after)) {
        return pointer_arbitration_decision::tracked;
    }

    return pointer_arbitration_decision::none;
}

action_route_policy_diagnostic make_policy(
    action_route_policy_kind kind,
    std::int64_t timestamp_ms,
    pointer_capture_snapshot pointer_capture_before,
    pointer_capture_snapshot pointer_capture_after)
{
    action_route_policy_diagnostic diagnostic{};
    diagnostic.kind = kind;
    diagnostic.timestamp_ms = timestamp_ms;
    diagnostic.pointer_capture_before = pointer_capture_before;
    diagnostic.pointer_capture_after = pointer_capture_after;
    return diagnostic;
}

action_route_policy_diagnostic make_pointer_arbitration_policy(
    std::int64_t timestamp_ms,
    std::int32_t pointer_id,
    pointer_phase phase,
    pointer_arbitration_decision decision,
    pointer_capture_snapshot pointer_capture_before,
    pointer_capture_snapshot pointer_capture_after)
{
    action_route_policy_diagnostic diagnostic = make_policy(
        action_route_policy_kind::pointer_capture_arbitration,
        timestamp_ms,
        pointer_capture_before,
        pointer_capture_after);
    apply_pointer_arbitration(diagnostic, pointer_id, phase, decision);
    return diagnostic;
}

action_route_policy_diagnostic make_event_policy(
    action_route_policy_kind kind,
    std::int64_t timestamp_ms,
    std::size_t event_index,
    pointer_capture_snapshot pointer_capture_before,
    pointer_capture_snapshot pointer_capture_after)
{
    action_route_policy_diagnostic diagnostic = make_policy(
        kind,
        timestamp_ms,
        pointer_capture_before,
        pointer_capture_after);
    diagnostic.emits_input_event = true;
    diagnostic.event_index = event_index;
    return diagnostic;
}

text_edit_snapshot capture_text_edit(const text_input_model& model)
{
    text_edit_snapshot snapshot{};
    snapshot.text_byte_count = model.text().size();
    snapshot.caret = model.caret_range();
    if (const std::optional<text_range> selection = model.selection_range()) {
        snapshot.has_selection = true;
        snapshot.selection = *selection;
    }
    return snapshot;
}

void apply_text_edit_boundary(
    action_route_policy_diagnostic& diagnostic,
    text_edit_snapshot before,
    text_edit_snapshot after)
{
    diagnostic.text_byte_count_before = before.text_byte_count;
    diagnostic.text_byte_count_after = after.text_byte_count;
    diagnostic.caret_before = before.caret;
    diagnostic.caret_after = after.caret;
    diagnostic.had_selection_before = before.has_selection;
    diagnostic.has_selection_after = after.has_selection;
    diagnostic.selection_before = before.selection;
    diagnostic.selection_after = after.selection;
}

action_route_policy_diagnostic make_text_event_policy(
    action_route_policy_kind kind,
    std::int64_t timestamp_ms,
    std::size_t event_index,
    const std::string& target_id,
    text_edit_snapshot before,
    text_edit_snapshot after,
    pointer_capture_snapshot pointer_capture_before,
    pointer_capture_snapshot pointer_capture_after)
{
    action_route_policy_diagnostic diagnostic = make_event_policy(
        kind,
        timestamp_ms,
        event_index,
        pointer_capture_before,
        pointer_capture_after);
    diagnostic.target_id = target_id;
    apply_text_edit_boundary(diagnostic, before, after);
    return diagnostic;
}

action_route_policy_diagnostic make_text_state_policy(
    action_route_policy_kind kind,
    std::int64_t timestamp_ms,
    const std::string& target_id,
    text_edit_snapshot before,
    text_edit_snapshot after,
    pointer_capture_snapshot pointer_capture_before,
    pointer_capture_snapshot pointer_capture_after)
{
    action_route_policy_diagnostic diagnostic = make_policy(
        kind,
        timestamp_ms,
        pointer_capture_before,
        pointer_capture_after);
    diagnostic.target_id = target_id;
    apply_text_edit_boundary(diagnostic, before, after);
    return diagnostic;
}

} // namespace

input_engine::input_engine(gesture_thresholds thresholds)
    : gestures_(thresholds)
{
}

void input_engine::focus_text_target(std::string target_id)
{
    ime_composing_ = false;
    text_.focus(std::move(target_id));
}

void input_engine::clear_text_focus()
{
    ime_composing_ = false;
    text_.clear_focus();
}

bool input_engine::has_text_focus() const
{
    return text_.has_focus();
}

const std::string& input_engine::text_focus_id() const
{
    return text_.focus_id();
}

const text_input_model& input_engine::text_model() const
{
    return text_;
}

const input_routing_diagnostics& input_engine::routing_diagnostics() const
{
    return diagnostics_;
}

std::vector<input_event> input_engine::process_raw_event(const raw_platform_input_event& event)
{
    return std::visit(
        [this](const auto& raw_event) -> std::vector<input_event> {
            using event_type = std::decay_t<decltype(raw_event)>;
            if constexpr (std::is_same_v<event_type, raw_platform_pointer_event>) {
                return process_pointer_event(raw_event);
            } else if constexpr (std::is_same_v<event_type, raw_platform_text_event>) {
                return process_text_event(raw_event);
            } else if constexpr (std::is_same_v<event_type, raw_platform_ime_event>) {
                return process_ime_event(raw_event);
            } else if constexpr (std::is_same_v<event_type, raw_platform_key_event>) {
                return process_key_event(raw_event);
            } else if constexpr (std::is_same_v<event_type, raw_platform_scroll_event>) {
                return process_scroll_event(raw_scroll_event{
                    .timestamp_ms = raw_event.timestamp_ms,
                    .x = raw_event.x,
                    .y = raw_event.y,
                    .delta_x = raw_event.delta_x,
                    .delta_y = raw_event.delta_y,
                    .unit = to_scroll_delta_unit(raw_event.unit),
                });
            } else {
                return process_focus_event(raw_event);
            }
        },
        event);
}

std::vector<input_event> input_engine::update_time(std::int64_t timestamp_ms)
{
    std::vector<input_event> events;
    begin_route_diagnostics();
    const std::vector<gesture_event> gestures = gestures_.update_time(timestamp_ms);
    append_gestures(events, gestures, gestures_.policy_snapshots());
    finish_route_diagnostics();
    return events;
}

std::vector<input_event> input_engine::process_scroll_event(const raw_scroll_event& event)
{
    std::vector<input_event> events;
    begin_route_diagnostics();
    if (event.delta_x == 0.0f && event.delta_y == 0.0f) {
        finish_route_diagnostics();
        return events;
    }

    scroll_event normalized{
        .timestamp_ms = event.timestamp_ms,
        .x = event.x,
        .y = event.y,
    };

    if (event.unit == scroll_delta_unit::lines) {
        normalized.line_delta_x = event.delta_x;
        normalized.line_delta_y = event.delta_y;
    } else {
        normalized.pixel_delta_x = event.delta_x;
        normalized.pixel_delta_y = event.delta_y;
    }

    append_scroll(events, normalized);
    finish_route_diagnostics();
    return events;
}

void input_engine::reset()
{
    const pointer_capture_snapshot pointer_capture_before = gestures_.capture_snapshot();
    gestures_.reset();
    text_ = text_input_model{};
    ime_composing_ = false;
    begin_route_diagnostics();
    if (has_pointer_capture_state(pointer_capture_before)) {
        append_policy(make_policy(
            action_route_policy_kind::pointer_capture_reset,
            0,
            pointer_capture_before,
            gestures_.capture_snapshot()));
    }
    finish_route_diagnostics();
}

std::vector<input_event> input_engine::process_pointer_event(const raw_platform_pointer_event& event)
{
    std::vector<input_event> events;
    begin_route_diagnostics();
    if (!is_primary_pointer(event)) {
        finish_route_diagnostics();
        return events;
    }

    const pointer_phase phase = to_pointer_phase(event.phase);
    const pointer_capture_snapshot pointer_capture_before = diagnostics_.pointer_capture;
    const std::vector<gesture_event> gestures = gestures_.process_pointer_event(pointer_event{
        .timestamp_ms = event.timestamp_ms,
        .pointer_id = event.pointer_id,
        .phase = phase,
        .x = event.x,
        .y = event.y,
    });
    const std::vector<gesture_policy_snapshot> gesture_policies = gestures_.policy_snapshots();
    const pointer_capture_snapshot pointer_capture_after = gestures_.capture_snapshot();
    const pointer_arbitration_decision decision = pointer_decision_for(
        event,
        phase,
        pointer_capture_before,
        pointer_capture_after,
        gestures);

    const std::size_t first_gesture_policy = diagnostics_.action_routes.size();
    append_gestures(events, gestures, gesture_policies);
    for (std::size_t index = first_gesture_policy; index < diagnostics_.action_routes.size(); ++index) {
        action_route_policy_diagnostic& policy = diagnostics_.action_routes[index];
        if (policy.kind == action_route_policy_kind::gesture_route_snapshot
            && policy.normalized_event.pointer_id == event.pointer_id) {
            apply_pointer_route_context(policy, event, phase, decision, pointer_capture_before, pointer_capture_after);
        }
    }

    for (const gesture_policy_snapshot& gesture_policy : gesture_policies) {
        if (!is_emitless_gesture_policy(gesture_policy)) {
            continue;
        }

        action_route_policy_diagnostic policy = make_policy(
            action_route_policy_kind::gesture_route_snapshot,
            gesture_policy.timestamp_ms,
            pointer_capture_before,
            pointer_capture_after);
        policy.gesture_policy = gesture_policy;
        apply_pointer_route_context(policy, event, phase, decision, pointer_capture_before, pointer_capture_after);
        append_policy(std::move(policy));
    }

    if (gestures.empty()
        && (decision == pointer_arbitration_decision::tracked
            || decision == pointer_arbitration_decision::ignored_by_capture)) {
        action_route_policy_diagnostic policy = make_pointer_arbitration_policy(
            event.timestamp_ms,
            event.pointer_id,
            phase,
            decision,
            pointer_capture_before,
            pointer_capture_after);
        apply_pointer_route_context(policy, event, phase, decision, pointer_capture_before, pointer_capture_after);
        if (!gesture_policies.empty()) {
            policy.gesture_policy = gesture_policies.front();
        }
        append_policy(std::move(policy));
    }

    if (phase == pointer_phase::cancel
        && has_pointer_capture_state(diagnostics_.pointer_capture)
        && pointer_capture_changed(diagnostics_.pointer_capture, pointer_capture_after)) {
        action_route_policy_diagnostic policy = make_policy(
            action_route_policy_kind::pointer_capture_reset,
            event.timestamp_ms,
            diagnostics_.pointer_capture,
            pointer_capture_after);
        apply_pointer_route_context(
            policy,
            event,
            phase,
            decision,
            diagnostics_.pointer_capture,
            pointer_capture_after);
        if (!gesture_policies.empty()) {
            policy.gesture_policy = gesture_policies.front();
        }
        append_policy(std::move(policy));
    }
    finish_route_diagnostics();
    return events;
}

std::vector<input_event> input_engine::process_text_event(const raw_platform_text_event& event)
{
    std::vector<input_event> events;
    begin_route_diagnostics();
    const text_edit_snapshot edit_before = capture_text_edit(text_);
    if (ime_composing_ || text_.ime_composition().active || !text_.commit_utf8(event.utf8_text)) {
        finish_route_diagnostics();
        return events;
    }

    const std::size_t event_index = events.size();
    events.emplace_back(text_event{
        .kind = text_event_kind::commit,
        .timestamp_ms = event.timestamp_ms,
        .target_id = text_.focus_id(),
        .utf8_text = event.utf8_text,
    });
    action_route_policy_diagnostic policy = make_text_event_policy(
        action_route_policy_kind::text_commit_boundary,
        event.timestamp_ms,
        event_index,
        text_.focus_id(),
        edit_before,
        capture_text_edit(text_),
        diagnostics_.pointer_capture,
        gestures_.capture_snapshot());
    policy.text_byte_count = event.utf8_text.size();
    append_policy(std::move(policy));
    finish_route_diagnostics();
    return events;
}

std::vector<input_event> input_engine::process_ime_event(const raw_platform_ime_event& event)
{
    std::vector<input_event> events;
    begin_route_diagnostics();
    if (!text_.has_focus()) {
        ime_composing_ = false;
        finish_route_diagnostics();
        return events;
    }

    const std::string target_id = text_.focus_id();
    const ime_composition_state initial_composition = text_.ime_composition();
    const bool had_composition = ime_composing_ || initial_composition.active;
    const text_edit_snapshot edit_before = capture_text_edit(text_);

    if (event.phase == raw_platform_ime_phase::composition_start) {
        ime_composing_ = true;
        if (had_composition) {
            text_.cancel_ime();
            const std::size_t event_index = events.size();
            events.emplace_back(make_ime_event(
                ime_event_kind::cancel,
                event.timestamp_ms,
                target_id,
                {},
                initial_composition));
            action_route_policy_diagnostic policy = make_text_event_policy(
                action_route_policy_kind::ime_cancel,
                event.timestamp_ms,
                event_index,
                target_id,
                edit_before,
                capture_text_edit(text_),
                diagnostics_.pointer_capture,
                gestures_.capture_snapshot());
            policy.composition = initial_composition;
            append_policy(std::move(policy));
        } else {
            text_.set_preedit("");
        }
        finish_route_diagnostics();
        return events;
    }

    if (event.phase == raw_platform_ime_phase::preedit_update) {
        ime_composing_ = true;
        text_.set_preedit(event.utf8_text);
        const ime_composition_state preedit_composition = text_.ime_composition();
        const std::size_t event_index = events.size();
        events.emplace_back(make_ime_event(
            ime_event_kind::preedit,
            event.timestamp_ms,
            target_id,
            event.utf8_text,
            preedit_composition));
        action_route_policy_diagnostic policy = make_text_event_policy(
            action_route_policy_kind::ime_preedit,
            event.timestamp_ms,
            event_index,
            target_id,
            edit_before,
            capture_text_edit(text_),
            diagnostics_.pointer_capture,
            gestures_.capture_snapshot());
        policy.text_byte_count = event.utf8_text.size();
        policy.composition = preedit_composition;
        append_policy(std::move(policy));
        finish_route_diagnostics();
        return events;
    }

    if ((event.phase == raw_platform_ime_phase::commit && !event.utf8_text.empty())
        || (event.phase == raw_platform_ime_phase::composition_end && !event.utf8_text.empty())) {
        const ime_composition_state committed_composition = text_.ime_composition();
        ime_composing_ = false;
        if (text_.commit_ime(event.utf8_text)) {
            const std::size_t event_index = events.size();
            events.emplace_back(make_ime_event(
                ime_event_kind::commit,
                event.timestamp_ms,
                target_id,
                event.utf8_text,
                committed_composition));
            action_route_policy_diagnostic policy = make_text_event_policy(
                action_route_policy_kind::ime_commit,
                event.timestamp_ms,
                event_index,
                target_id,
                edit_before,
                capture_text_edit(text_),
                diagnostics_.pointer_capture,
                gestures_.capture_snapshot());
            policy.text_byte_count = event.utf8_text.size();
            policy.composition = committed_composition;
            append_policy(std::move(policy));
            finish_route_diagnostics();
            return events;
        }
    }

    if (event.phase == raw_platform_ime_phase::cancel
        || event.phase == raw_platform_ime_phase::composition_end
        || (event.phase == raw_platform_ime_phase::commit && event.utf8_text.empty())) {
        const ime_composition_state canceled_composition = text_.ime_composition();
        ime_composing_ = false;
        text_.cancel_ime();
        if (had_composition) {
            const std::size_t event_index = events.size();
            events.emplace_back(make_ime_event(
                ime_event_kind::cancel,
                event.timestamp_ms,
                target_id,
                {},
                canceled_composition));
            action_route_policy_diagnostic policy = make_text_event_policy(
                action_route_policy_kind::ime_cancel,
                event.timestamp_ms,
                event_index,
                target_id,
                edit_before,
                capture_text_edit(text_),
                diagnostics_.pointer_capture,
                gestures_.capture_snapshot());
            policy.composition = canceled_composition;
            append_policy(std::move(policy));
        }
    }

    finish_route_diagnostics();
    return events;
}

std::vector<input_event> input_engine::process_key_event(const raw_platform_key_event& event)
{
    std::vector<input_event> events;
    begin_route_diagnostics();
    if (event.phase != raw_platform_key_phase::down || !text_.has_focus()) {
        finish_route_diagnostics();
        return events;
    }

    const std::string target_id = text_.focus_id();
    const text_edit_snapshot edit_before = capture_text_edit(text_);
    const pointer_capture_snapshot pointer_capture = gestures_.capture_snapshot();
    const auto append_state_policy =
        [this, &event, &target_id, &edit_before, pointer_capture](action_route_policy_kind kind) {
            append_policy(make_text_state_policy(
                kind,
                event.timestamp_ms,
                target_id,
                edit_before,
                capture_text_edit(text_),
                diagnostics_.pointer_capture,
                pointer_capture));
        };
    const auto emit_text_policy =
        [this, &events, &event, &target_id, &edit_before, pointer_capture](
            text_event_kind event_kind,
            action_route_policy_kind policy_kind) {
            const std::size_t event_index = events.size();
            events.emplace_back(text_event{
                .kind = event_kind,
                .timestamp_ms = event.timestamp_ms,
                .target_id = target_id,
                .utf8_text = {},
            });
            append_policy(make_text_event_policy(
                policy_kind,
                event.timestamp_ms,
                event_index,
                target_id,
                edit_before,
                capture_text_edit(text_),
                diagnostics_.pointer_capture,
                pointer_capture));
        };

    if (ime_composing_ || text_.ime_composition().active) {
        if (is_keyboard_navigation_key(event)) {
            action_route_policy_diagnostic policy = make_text_state_policy(
                navigation_policy_kind(event),
                event.timestamp_ms,
                target_id,
                edit_before,
                capture_text_edit(text_),
                diagnostics_.pointer_capture,
                pointer_capture);
            policy.composition = text_.ime_composition();
            append_policy(std::move(policy));
        }
        finish_route_diagnostics();
        return events;
    }

    if (is_tab_key(event)) {
        append_state_policy(navigation_policy_kind(event));
        finish_route_diagnostics();
        return events;
    }

    if (is_select_all_key(event)) {
        if (text_.select_all()) {
            emit_text_policy(text_event_kind::selection_changed, action_route_policy_kind::selection_changed);
        } else {
            append_state_policy(action_route_policy_kind::selection_changed);
        }
        finish_route_diagnostics();
        return events;
    }

    if (is_arrow_left_key(event)) {
        const bool changed = event.shift ? text_.extend_selection_left() : text_.move_caret_left();
        if (changed) {
            emit_text_policy(
                event.shift ? text_event_kind::selection_changed : text_event_kind::caret_moved,
                event.shift ? action_route_policy_kind::selection_changed : action_route_policy_kind::caret_moved);
        } else {
            append_state_policy(navigation_policy_kind(event));
        }
        finish_route_diagnostics();
        return events;
    }

    if (is_arrow_right_key(event)) {
        const bool changed = event.shift ? text_.extend_selection_right() : text_.move_caret_right();
        if (changed) {
            emit_text_policy(
                event.shift ? text_event_kind::selection_changed : text_event_kind::caret_moved,
                event.shift ? action_route_policy_kind::selection_changed : action_route_policy_kind::caret_moved);
        } else {
            append_state_policy(navigation_policy_kind(event));
        }
        finish_route_diagnostics();
        return events;
    }

    if (is_home_key(event)) {
        if (text_.move_caret_to_start()) {
            emit_text_policy(text_event_kind::caret_moved, action_route_policy_kind::caret_moved);
        } else {
            append_state_policy(action_route_policy_kind::caret_moved);
        }
        finish_route_diagnostics();
        return events;
    }

    if (is_end_key(event)) {
        if (text_.move_caret_to_end()) {
            emit_text_policy(text_event_kind::caret_moved, action_route_policy_kind::caret_moved);
        } else {
            append_state_policy(action_route_policy_kind::caret_moved);
        }
        finish_route_diagnostics();
        return events;
    }

    if (is_backspace_key(event)) {
        if (text_.backspace()) {
            const std::size_t event_index = events.size();
            events.emplace_back(text_event{
                .kind = text_event_kind::backspace,
                .timestamp_ms = event.timestamp_ms,
                .target_id = target_id,
                .utf8_text = {},
            });
            const text_edit_snapshot edit_after = capture_text_edit(text_);
            action_route_policy_diagnostic policy = make_text_event_policy(
                action_route_policy_kind::text_backspace_boundary,
                event.timestamp_ms,
                event_index,
                target_id,
                edit_before,
                edit_after,
                diagnostics_.pointer_capture,
                gestures_.capture_snapshot());
            policy.text_byte_count = edit_before.text_byte_count >= edit_after.text_byte_count
                ? edit_before.text_byte_count - edit_after.text_byte_count
                : 0;
            append_policy(std::move(policy));
        }
        finish_route_diagnostics();
        return events;
    }

    if (is_submit_key(event)) {
        if (event.repeat) {
            finish_route_diagnostics();
            return events;
        }

        const std::string submitted_text = text_.text();
        if (text_.submit()) {
            const std::size_t event_index = events.size();
            events.emplace_back(text_event{
                .kind = text_event_kind::submit,
                .timestamp_ms = event.timestamp_ms,
                .target_id = target_id,
                .utf8_text = submitted_text,
            });
            action_route_policy_diagnostic policy = make_text_event_policy(
                action_route_policy_kind::text_submit_boundary,
                event.timestamp_ms,
                event_index,
                target_id,
                edit_before,
                capture_text_edit(text_),
                diagnostics_.pointer_capture,
                gestures_.capture_snapshot());
            policy.text_byte_count = submitted_text.size();
            append_policy(std::move(policy));
        }
    }

    finish_route_diagnostics();
    return events;
}

std::vector<input_event> input_engine::process_focus_event(const raw_platform_focus_event& event)
{
    std::vector<input_event> events;
    begin_route_diagnostics();
    if (event.phase == raw_platform_focus_phase::gained) {
        if (text_.has_focus()) {
            events.emplace_back(text_event{
                .kind = text_event_kind::focus_gained,
                .timestamp_ms = event.timestamp_ms,
                .target_id = text_.focus_id(),
                .utf8_text = {},
            });
        }
        finish_route_diagnostics();
        return events;
    }

    const bool had_focus = text_.has_focus();
    const ime_composition_state canceled_composition = text_.ime_composition();
    const bool had_composition = ime_composing_ || canceled_composition.active;
    const std::string target_id = text_.focus_id();
    const pointer_capture_snapshot pointer_capture_before = gestures_.capture_snapshot();
    const text_edit_snapshot edit_before = capture_text_edit(text_);

    gestures_.reset();
    text_.clear_focus();
    ime_composing_ = false;
    const pointer_capture_snapshot pointer_capture_after = gestures_.capture_snapshot();
    const text_edit_snapshot edit_after = capture_text_edit(text_);

    if (has_pointer_capture_state(pointer_capture_before)) {
        append_policy(make_policy(
            action_route_policy_kind::pointer_capture_reset,
            event.timestamp_ms,
            pointer_capture_before,
            pointer_capture_after));
    }

    if (had_composition) {
        const std::size_t event_index = events.size();
        events.emplace_back(make_ime_event(
            ime_event_kind::cancel,
            event.timestamp_ms,
            target_id,
            {},
            canceled_composition));
        action_route_policy_diagnostic policy = make_text_event_policy(
            action_route_policy_kind::ime_cancel,
            event.timestamp_ms,
            event_index,
            target_id,
            edit_before,
            edit_after,
            pointer_capture_before,
            pointer_capture_after);
        policy.composition = canceled_composition;
        append_policy(std::move(policy));
    }

    std::size_t focus_loss_event_index = events.size();
    if (had_focus) {
        events.emplace_back(text_event{
            .kind = text_event_kind::focus_lost,
            .timestamp_ms = event.timestamp_ms,
            .target_id = target_id,
            .utf8_text = {},
        });
    }
    action_route_policy_diagnostic policy = make_policy(
        action_route_policy_kind::focus_loss,
        event.timestamp_ms,
        pointer_capture_before,
        pointer_capture_after);
    policy.emits_input_event = had_focus;
    policy.event_index = focus_loss_event_index;
    policy.target_id = target_id;
    apply_text_edit_boundary(policy, edit_before, edit_after);
    append_policy(std::move(policy));

    finish_route_diagnostics();
    return events;
}

void input_engine::begin_route_diagnostics()
{
    diagnostics_.normalized_events.clear();
    diagnostics_.action_routes.clear();
    diagnostics_.pointer_capture = gestures_.capture_snapshot();
}

void input_engine::finish_route_diagnostics()
{
    diagnostics_.pointer_capture = gestures_.capture_snapshot();
}

void input_engine::append_gestures(
    std::vector<input_event>& events,
    const std::vector<gesture_event>& gestures,
    const std::vector<gesture_policy_snapshot>& gesture_policies)
{
    for (const gesture_event& gesture : gestures) {
        const std::size_t event_index = events.size();
        const normalized_input_event_summary summary = summarize_gesture(gesture);
        diagnostics_.normalized_events.push_back(summary);
        action_route_policy_diagnostic policy = make_event_policy(
            action_route_policy_kind::gesture_route_snapshot,
            gesture.timestamp_ms,
            event_index,
            diagnostics_.pointer_capture,
            gestures_.capture_snapshot());
        policy.normalized_event = summary;
        if (const gesture_policy_snapshot* gesture_policy = find_policy_for_gesture(gesture_policies, gesture)) {
            policy.gesture_policy = *gesture_policy;
        }
        append_policy(std::move(policy));
        events.emplace_back(gesture);
    }
}

void input_engine::append_scroll(std::vector<input_event>& events, const scroll_event& scroll)
{
    const std::size_t event_index = events.size();
    const normalized_input_event_summary summary = summarize_scroll(scroll);
    diagnostics_.normalized_events.push_back(summary);
    action_route_policy_diagnostic policy = make_event_policy(
        action_route_policy_kind::wheel_summary,
        scroll.timestamp_ms,
        event_index,
        diagnostics_.pointer_capture,
        gestures_.capture_snapshot());
    policy.normalized_event = summary;
    append_policy(std::move(policy));
    events.emplace_back(scroll);
}

void input_engine::append_policy(action_route_policy_diagnostic diagnostic)
{
    diagnostics_.action_routes.push_back(std::move(diagnostic));
}

} // namespace quiz_vulkan::input
