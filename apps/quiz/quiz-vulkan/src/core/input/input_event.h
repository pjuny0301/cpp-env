#pragma once

#include <cstdint>
#include <string>
#include <variant>

namespace quiz_vulkan::input {

enum class pointer_phase {
    down,
    move,
    up,
    cancel,
};

struct pointer_event {
    std::int64_t timestamp_ms = 0;
    std::int32_t pointer_id = 0;
    pointer_phase phase = pointer_phase::down;
    float x = 0.0f;
    float y = 0.0f;
};

enum class gesture_kind {
    tap,
    long_press,
    swipe_left,
    swipe_right,
    drag_start,
    drag_update,
    drag_end,
    drag_cancel,
};

struct gesture_event {
    gesture_kind kind = gesture_kind::tap;
    std::int64_t timestamp_ms = 0;
    std::int64_t duration_ms = 0;
    std::int32_t pointer_id = 0;
    float start_x = 0.0f;
    float start_y = 0.0f;
    float x = 0.0f;
    float y = 0.0f;
    float delta_x = 0.0f;
    float delta_y = 0.0f;
};

enum class text_event_kind {
    commit,
    backspace,
    submit,
    focus_gained,
    focus_lost,
};

struct text_event {
    text_event_kind kind = text_event_kind::commit;
    std::int64_t timestamp_ms = 0;
    std::string target_id;
    std::string utf8_text;
};

enum class ime_event_kind {
    preedit,
    commit,
    cancel,
};

struct ime_event {
    ime_event_kind kind = ime_event_kind::preedit;
    std::int64_t timestamp_ms = 0;
    std::string target_id;
    std::string utf8_text;
};

using input_event = std::variant<gesture_event, text_event, ime_event>;

} // namespace quiz_vulkan::input
