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

    if (captured_by_other_pointer(event.pointer_id)) {
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
        }
    }

    pointers_.erase(pointer);
    release_pointer_capture(event.pointer_id);
    return gestures;
}

std::vector<gesture_event> gesture_recognizer::update_time(std::int64_t timestamp_ms)
{
    std::vector<gesture_event> gestures;
    for (auto& [pointer_id, state] : pointers_) {
        if (auto long_press = maybe_emit_long_press(pointer_id, state, timestamp_ms)) {
            gestures.push_back(*long_press);
        }
    }
    std::ranges::sort(gestures, [](const gesture_event& lhs, const gesture_event& rhs) {
        return lhs.pointer_id < rhs.pointer_id;
    });
    return gestures;
}

void gesture_recognizer::reset()
{
    pointers_.clear();
    captured_pointer_id_.reset();
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
