#pragma once

#include "core/input/input_event.h"

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace quiz_vulkan::input {

struct gesture_thresholds {
    float swipe_min_dx = 60.0f;
    float swipe_max_dy = 40.0f;
    std::int64_t swipe_max_duration_ms = 800;
    std::int64_t long_press_min_duration_ms = 600;
    float tap_slop = 8.0f;
};

class gesture_recognizer {
public:
    explicit gesture_recognizer(gesture_thresholds thresholds = {});

    [[nodiscard]] std::vector<gesture_event> process_pointer_event(const pointer_event& event);
    [[nodiscard]] std::vector<gesture_event> update_time(std::int64_t timestamp_ms);
    void reset();

private:
    struct pointer_state {
        std::int64_t start_ms = 0;
        std::int64_t last_ms = 0;
        float start_x = 0.0f;
        float start_y = 0.0f;
        float last_x = 0.0f;
        float last_y = 0.0f;
        bool moved_outside_tap_slop = false;
        bool long_press_emitted = false;
        bool suppress_release_gesture = false;
    };

    [[nodiscard]] std::optional<gesture_event> maybe_emit_long_press(
        std::int32_t pointer_id,
        pointer_state& state,
        std::int64_t timestamp_ms);
    [[nodiscard]] bool inside_tap_slop(const pointer_state& state, float x, float y) const;

    gesture_thresholds thresholds_;
    std::unordered_map<std::int32_t, pointer_state> pointers_;
};

} // namespace quiz_vulkan::input
