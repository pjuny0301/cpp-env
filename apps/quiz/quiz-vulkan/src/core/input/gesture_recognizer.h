#pragma once

#include "core/input/input_event.h"

#include <cstddef>
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
    float drag_start_slop = 8.0f;
};

enum class pointer_capture_lifecycle {
    idle,
    tracking,
    captured,
};

struct pointer_capture_snapshot {
    pointer_capture_lifecycle lifecycle = pointer_capture_lifecycle::idle;
    bool active = false;
    std::int32_t pointer_id = 0;
    std::size_t tracked_pointer_count = 0;
};

enum class gesture_direction {
    none,
    left,
    right,
    up,
    down,
};

enum class gesture_policy_decision {
    none,
    tracking_started,
    tap_accepted,
    long_press_accepted,
    swipe_accepted,
    swipe_rejected_distance,
    swipe_rejected_cross_axis,
    swipe_rejected_duration,
    drag_started,
    drag_updated,
    drag_released,
    drag_canceled,
    release_suppressed,
    ignored_by_capture,
};

struct gesture_policy_snapshot {
    gesture_policy_decision decision = gesture_policy_decision::none;
    gesture_direction direction = gesture_direction::none;
    pointer_phase phase = pointer_phase::down;
    std::int64_t timestamp_ms = 0;
    std::int64_t duration_ms = 0;
    std::int32_t pointer_id = 0;
    float start_x = 0.0f;
    float start_y = 0.0f;
    float x = 0.0f;
    float y = 0.0f;
    float delta_x = 0.0f;
    float delta_y = 0.0f;
    float distance = 0.0f;
    float swipe_min_dx = 0.0f;
    float swipe_max_dy = 0.0f;
    std::int64_t swipe_max_duration_ms = 0;
    float tap_slop = 0.0f;
    float drag_start_slop = 0.0f;
    bool emitted_input_event = false;
    gesture_kind emitted_kind = gesture_kind::tap;
};

class gesture_recognizer {
public:
    explicit gesture_recognizer(gesture_thresholds thresholds = {});

    [[nodiscard]] std::vector<gesture_event> process_pointer_event(const pointer_event& event);
    [[nodiscard]] std::vector<gesture_event> update_time(std::int64_t timestamp_ms);
    [[nodiscard]] pointer_capture_snapshot capture_snapshot() const;
    [[nodiscard]] const std::vector<gesture_policy_snapshot>& policy_snapshots() const;
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
        bool dragging = false;
        bool long_press_emitted = false;
        bool suppress_release_gesture = false;
    };

    [[nodiscard]] std::optional<gesture_event> maybe_emit_long_press(
        std::int32_t pointer_id,
        pointer_state& state,
        std::int64_t timestamp_ms);
    [[nodiscard]] bool captured_by_other_pointer(std::int32_t pointer_id) const;
    [[nodiscard]] bool inside_drag_slop(const pointer_state& state, float x, float y) const;
    [[nodiscard]] bool inside_tap_slop(const pointer_state& state, float x, float y) const;
    void capture_pointer(std::int32_t pointer_id);
    void release_pointer_capture(std::int32_t pointer_id);

    gesture_thresholds thresholds_;
    std::unordered_map<std::int32_t, pointer_state> pointers_;
    std::optional<std::int32_t> captured_pointer_id_;
    std::vector<gesture_policy_snapshot> policy_snapshots_;
};

} // namespace quiz_vulkan::input
