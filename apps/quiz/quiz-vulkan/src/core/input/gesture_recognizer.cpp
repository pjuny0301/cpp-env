#include "core/input/gesture_recognizer.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace quiz_vulkan::input {
namespace {

float abs_float(float value)
{
    return std::fabs(value);
}

gesture_direction direction_for(float dx, float dy)
{
    if (dx == 0.0f && dy == 0.0f) {
        return gesture_direction::none;
    }

    if (abs_float(dx) >= abs_float(dy)) {
        return dx < 0.0f ? gesture_direction::left : gesture_direction::right;
    }

    return dy < 0.0f ? gesture_direction::up : gesture_direction::down;
}

gesture_policy_snapshot gesture_policy(
    gesture_policy_decision decision,
    const pointer_event& event,
    std::int64_t start_ms,
    float start_x,
    float start_y,
    const gesture_thresholds& thresholds,
    bool emitted_input_event = false,
    gesture_kind emitted_kind = gesture_kind::tap)
{
    const float dx = event.x - start_x;
    const float dy = event.y - start_y;
    return gesture_policy_snapshot{
        .decision = decision,
        .direction = direction_for(dx, dy),
        .phase = event.phase,
        .timestamp_ms = event.timestamp_ms,
        .duration_ms = event.timestamp_ms - start_ms,
        .pointer_id = event.pointer_id,
        .start_x = start_x,
        .start_y = start_y,
        .x = event.x,
        .y = event.y,
        .delta_x = dx,
        .delta_y = dy,
        .distance = std::hypot(dx, dy),
        .swipe_min_dx = thresholds.swipe_min_dx,
        .swipe_max_dy = thresholds.swipe_max_dy,
        .swipe_max_duration_ms = thresholds.swipe_max_duration_ms,
        .long_press_min_duration_ms = thresholds.long_press_min_duration_ms,
        .tap_slop = thresholds.tap_slop,
        .drag_start_slop = thresholds.drag_start_slop,
        .emitted_input_event = emitted_input_event,
        .emitted_kind = emitted_kind,
    };
}

gesture_policy_snapshot gesture_policy_at_position(
    gesture_policy_decision decision,
    const pointer_event& event,
    std::int64_t start_ms,
    float start_x,
    float start_y,
    float x,
    float y,
    const gesture_thresholds& thresholds,
    bool emitted_input_event = false,
    gesture_kind emitted_kind = gesture_kind::tap)
{
    pointer_event positioned = event;
    positioned.x = x;
    positioned.y = y;
    return gesture_policy(
        decision,
        positioned,
        start_ms,
        start_x,
        start_y,
        thresholds,
        emitted_input_event,
        emitted_kind);
}

gesture_policy_decision rejected_swipe_decision(
    float dx,
    float dy,
    std::int64_t duration_ms,
    const gesture_thresholds& thresholds)
{
    if (abs_float(dx) < thresholds.swipe_min_dx) {
        return gesture_policy_decision::swipe_rejected_distance;
    }

    if (abs_float(dy) > thresholds.swipe_max_dy) {
        return gesture_policy_decision::swipe_rejected_cross_axis;
    }

    if (duration_ms > thresholds.swipe_max_duration_ms) {
        return gesture_policy_decision::swipe_rejected_duration;
    }

    return gesture_policy_decision::swipe_rejected_distance;
}

gesture_event drag_event(
    gesture_kind kind,
    const pointer_event& event,
    std::int64_t start_ms,
    float start_x,
    float start_y,
    float previous_x,
    float previous_y)
{
    const bool start = kind == gesture_kind::drag_start;
    return gesture_event{
        .kind = kind,
        .timestamp_ms = event.timestamp_ms,
        .duration_ms = event.timestamp_ms - start_ms,
        .pointer_id = event.pointer_id,
        .start_x = start_x,
        .start_y = start_y,
        .x = event.x,
        .y = event.y,
        .delta_x = event.x - (start ? start_x : previous_x),
        .delta_y = event.y - (start ? start_y : previous_y),
    };
}

gesture_event cancel_drag_event(
    const pointer_event& event,
    std::int64_t start_ms,
    float start_x,
    float start_y,
    float last_x,
    float last_y)
{
    return gesture_event{
        .kind = gesture_kind::drag_cancel,
        .timestamp_ms = event.timestamp_ms,
        .duration_ms = event.timestamp_ms - start_ms,
        .pointer_id = event.pointer_id,
        .start_x = start_x,
        .start_y = start_y,
        .x = last_x,
        .y = last_y,
        .delta_x = 0.0f,
        .delta_y = 0.0f,
    };
}

} // namespace

gesture_recognizer::gesture_recognizer(gesture_thresholds thresholds)
    : thresholds_(thresholds)
{
}

std::vector<gesture_event> gesture_recognizer::process_pointer_event(const pointer_event& event)
{
    std::vector<gesture_event> gestures;
    policy_snapshots_.clear();

    if (captured_by_other_pointer(event.pointer_id)) {
        policy_snapshots_.push_back(gesture_policy(
            gesture_policy_decision::ignored_by_capture,
            event,
            event.timestamp_ms,
            event.x,
            event.y,
            thresholds_));
        return gestures;
    }

    if (event.phase == pointer_phase::down) {
        if (auto old_pointer = pointers_.find(event.pointer_id);
            old_pointer != pointers_.end() && old_pointer->second.dragging) {
            const pointer_state& old_state = old_pointer->second;
            gestures.push_back(cancel_drag_event(
                event,
                old_state.start_ms,
                old_state.start_x,
                old_state.start_y,
                old_state.last_x,
                old_state.last_y));
            policy_snapshots_.push_back(gesture_policy_at_position(
                gesture_policy_decision::drag_canceled,
                event,
                old_state.start_ms,
                old_state.start_x,
                old_state.start_y,
                old_state.last_x,
                old_state.last_y,
                thresholds_,
                true,
                gesture_kind::drag_cancel));
            release_pointer_capture(event.pointer_id);
        }

        pointers_[event.pointer_id] = pointer_state{
            .start_ms = event.timestamp_ms,
            .last_ms = event.timestamp_ms,
            .start_x = event.x,
            .start_y = event.y,
            .last_x = event.x,
            .last_y = event.y,
        };
        if (policy_snapshots_.empty()) {
            policy_snapshots_.push_back(gesture_policy(
                gesture_policy_decision::tracking_started,
                event,
                event.timestamp_ms,
                event.x,
                event.y,
                thresholds_));
        }
        return gestures;
    }

    auto pointer = pointers_.find(event.pointer_id);
    if (pointer == pointers_.end()) {
        return gestures;
    }

    pointer_state& state = pointer->second;
    const float previous_x = state.last_x;
    const float previous_y = state.last_y;
    state.last_ms = event.timestamp_ms;
    state.last_x = event.x;
    state.last_y = event.y;
    state.moved_outside_tap_slop = state.moved_outside_tap_slop || !inside_tap_slop(state, event.x, event.y);

    if (event.phase == pointer_phase::cancel) {
        if (state.dragging) {
            gestures.push_back(drag_event(
                gesture_kind::drag_cancel,
                event,
                state.start_ms,
                state.start_x,
                state.start_y,
                previous_x,
                previous_y));
            policy_snapshots_.push_back(gesture_policy(
                gesture_policy_decision::drag_canceled,
                event,
                state.start_ms,
                state.start_x,
                state.start_y,
                thresholds_,
                true,
                gesture_kind::drag_cancel));
        } else {
            policy_snapshots_.push_back(gesture_policy(
                gesture_policy_decision::release_suppressed,
                event,
                state.start_ms,
                state.start_x,
                state.start_y,
                thresholds_));
        }
        pointers_.erase(pointer);
        release_pointer_capture(event.pointer_id);
        return gestures;
    }

    if (event.phase == pointer_phase::move) {
        if (state.long_press_emitted) {
            return gestures;
        }

        if (state.dragging) {
            gestures.push_back(drag_event(
                gesture_kind::drag_update,
                event,
                state.start_ms,
                state.start_x,
                state.start_y,
                previous_x,
                previous_y));
            policy_snapshots_.push_back(gesture_policy(
                gesture_policy_decision::drag_updated,
                event,
                state.start_ms,
                state.start_x,
                state.start_y,
                thresholds_,
                true,
                gesture_kind::drag_update));
            return gestures;
        }

        if (!inside_drag_slop(state, event.x, event.y)) {
            state.dragging = true;
            state.suppress_release_gesture = true;
            capture_pointer(event.pointer_id);
            gestures.push_back(drag_event(
                gesture_kind::drag_start,
                event,
                state.start_ms,
                state.start_x,
                state.start_y,
                previous_x,
                previous_y));
            policy_snapshots_.push_back(gesture_policy(
                gesture_policy_decision::drag_started,
                event,
                state.start_ms,
                state.start_x,
                state.start_y,
                thresholds_,
                true,
                gesture_kind::drag_start));
        }
        return gestures;
    }

    if (auto long_press = maybe_emit_long_press(event.pointer_id, state, event.timestamp_ms)) {
        gestures.push_back(*long_press);
    }

    if (event.phase != pointer_phase::up) {
        return gestures;
    }

    if (state.dragging) {
        gestures.push_back(drag_event(
            gesture_kind::drag_end,
            event,
            state.start_ms,
            state.start_x,
            state.start_y,
            previous_x,
            previous_y));
        policy_snapshots_.push_back(gesture_policy(
            gesture_policy_decision::drag_released,
            event,
            state.start_ms,
            state.start_x,
            state.start_y,
            thresholds_,
            true,
            gesture_kind::drag_end));
        pointers_.erase(pointer);
        release_pointer_capture(event.pointer_id);
        return gestures;
    }

    if (!state.suppress_release_gesture) {
        const std::int64_t duration_ms = event.timestamp_ms - state.start_ms;
        const float dx = event.x - state.start_x;
        const float dy = event.y - state.start_y;

        if (abs_float(dx) >= thresholds_.swipe_min_dx
            && abs_float(dy) <= thresholds_.swipe_max_dy
            && duration_ms <= thresholds_.swipe_max_duration_ms) {
            gestures.push_back(gesture_event{
                .kind = dx < 0.0f ? gesture_kind::swipe_left : gesture_kind::swipe_right,
                .timestamp_ms = event.timestamp_ms,
                .duration_ms = duration_ms,
                .pointer_id = event.pointer_id,
                .start_x = state.start_x,
                .start_y = state.start_y,
                .x = event.x,
                .y = event.y,
            });
            policy_snapshots_.push_back(gesture_policy(
                gesture_policy_decision::swipe_accepted,
                event,
                state.start_ms,
                state.start_x,
                state.start_y,
                thresholds_,
                true,
                dx < 0.0f ? gesture_kind::swipe_left : gesture_kind::swipe_right));
        } else if (inside_tap_slop(state, event.x, event.y)
                   && duration_ms < thresholds_.long_press_min_duration_ms) {
            gestures.push_back(gesture_event{
                .kind = gesture_kind::tap,
                .timestamp_ms = event.timestamp_ms,
                .duration_ms = duration_ms,
                .pointer_id = event.pointer_id,
                .start_x = state.start_x,
                .start_y = state.start_y,
                .x = event.x,
                .y = event.y,
            });
            policy_snapshots_.push_back(gesture_policy(
                gesture_policy_decision::tap_accepted,
                event,
                state.start_ms,
                state.start_x,
                state.start_y,
                thresholds_,
                true,
                gesture_kind::tap));
        } else {
            policy_snapshots_.push_back(gesture_policy(
                rejected_swipe_decision(dx, dy, duration_ms, thresholds_),
                event,
                state.start_ms,
                state.start_x,
                state.start_y,
                thresholds_));
        }
    } else {
        policy_snapshots_.push_back(gesture_policy(
            gesture_policy_decision::release_suppressed,
            event,
            state.start_ms,
            state.start_x,
            state.start_y,
            thresholds_));
    }

    pointers_.erase(pointer);
    release_pointer_capture(event.pointer_id);
    return gestures;
}

std::vector<gesture_event> gesture_recognizer::update_time(std::int64_t timestamp_ms)
{
    std::vector<gesture_event> gestures;
    policy_snapshots_.clear();
    for (auto& [pointer_id, state] : pointers_) {
        if (auto long_press = maybe_emit_long_press(pointer_id, state, timestamp_ms)) {
            gestures.push_back(*long_press);
            policy_snapshots_.push_back(gesture_policy(
                gesture_policy_decision::long_press_accepted,
                pointer_event{
                    .timestamp_ms = timestamp_ms,
                    .pointer_id = pointer_id,
                    .phase = pointer_phase::move,
                    .x = state.last_x,
                    .y = state.last_y,
                },
                state.start_ms,
                state.start_x,
                state.start_y,
                thresholds_,
                true,
                gesture_kind::long_press));
        }
    }
    std::ranges::sort(gestures, [](const gesture_event& lhs, const gesture_event& rhs) {
        return lhs.pointer_id < rhs.pointer_id;
    });
    std::ranges::sort(policy_snapshots_, [](const gesture_policy_snapshot& lhs, const gesture_policy_snapshot& rhs) {
        return lhs.pointer_id < rhs.pointer_id;
    });
    return gestures;
}

void gesture_recognizer::reset()
{
    pointers_.clear();
    captured_pointer_id_.reset();
    policy_snapshots_.clear();
}

pointer_capture_snapshot gesture_recognizer::capture_snapshot() const
{
    if (captured_pointer_id_.has_value()) {
        return pointer_capture_snapshot{
            .lifecycle = pointer_capture_lifecycle::captured,
            .active = true,
            .pointer_id = *captured_pointer_id_,
            .tracked_pointer_count = pointers_.size(),
        };
    }

    if (pointers_.empty()) {
        return pointer_capture_snapshot{};
    }

    std::int32_t pointer_id = pointers_.begin()->first;
    for (const auto& [tracked_pointer_id, state] : pointers_) {
        static_cast<void>(state);
        pointer_id = std::min(pointer_id, tracked_pointer_id);
    }

    return pointer_capture_snapshot{
        .lifecycle = pointer_capture_lifecycle::tracking,
        .active = false,
        .pointer_id = pointer_id,
        .tracked_pointer_count = pointers_.size(),
    };
}

const std::vector<gesture_policy_snapshot>& gesture_recognizer::policy_snapshots() const
{
    return policy_snapshots_;
}

std::optional<gesture_event> gesture_recognizer::maybe_emit_long_press(
    std::int32_t pointer_id,
    pointer_state& state,
    std::int64_t timestamp_ms)
{
    if (state.long_press_emitted || state.moved_outside_tap_slop || state.dragging) {
        return std::nullopt;
    }

    const std::int64_t duration_ms = timestamp_ms - state.start_ms;
    if (duration_ms < thresholds_.long_press_min_duration_ms) {
        return std::nullopt;
    }

    state.long_press_emitted = true;
    state.suppress_release_gesture = true;
    return gesture_event{
        .kind = gesture_kind::long_press,
        .timestamp_ms = timestamp_ms,
        .duration_ms = duration_ms,
        .pointer_id = pointer_id,
        .start_x = state.start_x,
        .start_y = state.start_y,
        .x = state.last_x,
        .y = state.last_y,
    };
}

bool gesture_recognizer::inside_tap_slop(const pointer_state& state, float x, float y) const
{
    return abs_float(x - state.start_x) <= thresholds_.tap_slop
        && abs_float(y - state.start_y) <= thresholds_.tap_slop;
}

bool gesture_recognizer::captured_by_other_pointer(std::int32_t pointer_id) const
{
    return captured_pointer_id_.has_value() && *captured_pointer_id_ != pointer_id;
}

bool gesture_recognizer::inside_drag_slop(const pointer_state& state, float x, float y) const
{
    return abs_float(x - state.start_x) <= thresholds_.drag_start_slop
        && abs_float(y - state.start_y) <= thresholds_.drag_start_slop;
}

void gesture_recognizer::capture_pointer(std::int32_t pointer_id)
{
    captured_pointer_id_ = pointer_id;
    for (auto pointer = pointers_.begin(); pointer != pointers_.end();) {
        if (pointer->first == pointer_id) {
            ++pointer;
        } else {
            pointer = pointers_.erase(pointer);
        }
    }
}

void gesture_recognizer::release_pointer_capture(std::int32_t pointer_id)
{
    if (captured_pointer_id_ == pointer_id) {
        captured_pointer_id_.reset();
    }
}

} // namespace quiz_vulkan::input
