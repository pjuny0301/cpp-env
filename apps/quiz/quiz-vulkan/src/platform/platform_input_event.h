#pragma once

#include <cstdint>
#include <string>
#include <variant>

namespace quiz_vulkan {

enum class raw_platform_pointer_phase {
    down,
    move,
    up,
    cancel,
};

enum class raw_platform_pointer_button {
    none,
    primary,
    secondary,
    middle,
};

enum class raw_platform_key_phase {
    down,
    up,
};

enum class raw_platform_focus_phase {
    gained,
    lost,
};

enum class raw_platform_ime_phase {
    composition_start,
    preedit_update,
    commit,
    composition_end,
    cancel,
};

struct raw_platform_pointer_event {
    std::int64_t timestamp_ms = 0;
    std::int32_t pointer_id = 0;
    raw_platform_pointer_phase phase = raw_platform_pointer_phase::down;
    raw_platform_pointer_button button = raw_platform_pointer_button::primary;
    float x = 0.0f;
    float y = 0.0f;
};

struct raw_platform_text_event {
    std::int64_t timestamp_ms = 0;
    std::string utf8_text;
};

struct raw_platform_ime_event {
    std::int64_t timestamp_ms = 0;
    raw_platform_ime_phase phase = raw_platform_ime_phase::preedit_update;
    std::string utf8_text;
};

struct raw_platform_key_event {
    std::int64_t timestamp_ms = 0;
    raw_platform_key_phase phase = raw_platform_key_phase::down;
    std::int32_t key_code = 0;
    std::string logical_key;
    bool alt = false;
    bool ctrl = false;
    bool shift = false;
    bool meta = false;
    bool repeat = false;
};

struct raw_platform_focus_event {
    std::int64_t timestamp_ms = 0;
    raw_platform_focus_phase phase = raw_platform_focus_phase::gained;
};

using raw_platform_input_event = std::variant<
    raw_platform_pointer_event,
    raw_platform_text_event,
    raw_platform_ime_event,
    raw_platform_key_event,
    raw_platform_focus_event>;

} // namespace quiz_vulkan
