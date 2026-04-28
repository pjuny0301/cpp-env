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

} // namespace

gesture_recognizer::gesture_recognizer(gesture_thresholds thresholds)
    : thresholds_(thresholds)
{
}

std::vector<gesture_event> gesture_recognizer::process_pointer_event(const pointer_event& event)
{
    std::vector<gesture_event> gestures;

    if (event.phase == pointer_phase::down) {
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
    state.last_ms = event.timestamp_ms;
    state.last_x = event.x;
    state.last_y = event.y;
    state.moved_outside_tap_slop = state.moved_outside_tap_slop || !inside_tap_slop(state, event.x, event.y);

    if (event.phase == pointer_phase::cancel) {
        pointers_.erase(pointer);
        return gestures;
    }

    if (auto long_press = maybe_emit_long_press(event.pointer_id, state, event.timestamp_ms)) {
        gestures.push_back(*long_press);
    }

    if (event.phase != pointer_phase::up) {
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
}

std::optional<gesture_event> gesture_recognizer::maybe_emit_long_press(
    std::int32_t pointer_id,
    pointer_state& state,
    std::int64_t timestamp_ms)
{
    if (state.long_press_emitted || state.moved_outside_tap_slop) {
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

} // namespace quiz_vulkan::input
