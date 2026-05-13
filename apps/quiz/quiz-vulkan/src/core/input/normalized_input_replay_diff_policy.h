#pragma once

#include "core/input/gesture_recognizer.h"
#include "core/input/text_input_types.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

namespace quiz_vulkan::input {

struct normalized_input_replay_count_delta {
    std::size_t before_count = 0;
    std::size_t after_count = 0;
    std::int64_t delta = 0;
    bool changed = false;
};

struct normalized_input_replay_bool_delta {
    bool before_value = false;
    bool after_value = false;
    bool changed = false;
};

struct normalized_input_replay_range_delta {
    text_range before_range;
    text_range after_range;
    bool changed = false;
};

struct normalized_input_replay_string_delta {
    std::string before_value;
    std::string after_value;
    std::int64_t byte_delta = 0;
    bool changed = false;
};

struct normalized_input_replay_pointer_capture_delta {
    pointer_capture_snapshot before_capture;
    pointer_capture_snapshot after_capture;
    bool before_clean = true;
    bool after_clean = true;
    bool changed = false;
    bool clean_changed = false;
};

[[nodiscard]] inline bool pointer_capture_snapshot_clean(const pointer_capture_snapshot& snapshot)
{
    return snapshot.lifecycle == pointer_capture_lifecycle::idle
        && !snapshot.active
        && snapshot.tracked_pointer_count == 0;
}

[[nodiscard]] inline bool normalized_input_replay_same_text_range(text_range lhs, text_range rhs)
{
    return lhs.start_byte == rhs.start_byte && lhs.end_byte == rhs.end_byte;
}

[[nodiscard]] inline bool normalized_input_replay_pointer_capture_changed(
    const pointer_capture_snapshot& before,
    const pointer_capture_snapshot& after)
{
    return before.lifecycle != after.lifecycle
        || before.active != after.active
        || before.pointer_id != after.pointer_id
        || before.tracked_pointer_count != after.tracked_pointer_count;
}

[[nodiscard]] inline std::int64_t normalized_input_replay_size_delta(
    std::size_t before_count,
    std::size_t after_count)
{
    if (after_count >= before_count) {
        return static_cast<std::int64_t>(after_count - before_count);
    }
    return -static_cast<std::int64_t>(before_count - after_count);
}

[[nodiscard]] inline normalized_input_replay_count_delta normalized_input_replay_diff_count(
    std::size_t before_count,
    std::size_t after_count)
{
    return normalized_input_replay_count_delta{
        .before_count = before_count,
        .after_count = after_count,
        .delta = normalized_input_replay_size_delta(before_count, after_count),
        .changed = before_count != after_count,
    };
}

[[nodiscard]] inline normalized_input_replay_bool_delta normalized_input_replay_diff_bool(
    bool before_value,
    bool after_value)
{
    return normalized_input_replay_bool_delta{
        .before_value = before_value,
        .after_value = after_value,
        .changed = before_value != after_value,
    };
}

[[nodiscard]] inline normalized_input_replay_range_delta normalized_input_replay_diff_range(
    text_range before_range,
    text_range after_range)
{
    return normalized_input_replay_range_delta{
        .before_range = before_range,
        .after_range = after_range,
        .changed = !normalized_input_replay_same_text_range(before_range, after_range),
    };
}

[[nodiscard]] inline normalized_input_replay_string_delta normalized_input_replay_diff_string(
    std::string before_value,
    std::string after_value)
{
    const std::size_t before_size = before_value.size();
    const std::size_t after_size = after_value.size();
    const bool changed = before_value != after_value;
    return normalized_input_replay_string_delta{
        .before_value = std::move(before_value),
        .after_value = std::move(after_value),
        .byte_delta = normalized_input_replay_size_delta(before_size, after_size),
        .changed = changed,
    };
}

[[nodiscard]] inline normalized_input_replay_pointer_capture_delta normalized_input_replay_diff_pointer_capture(
    pointer_capture_snapshot before_capture,
    pointer_capture_snapshot after_capture)
{
    const bool before_clean = pointer_capture_snapshot_clean(before_capture);
    const bool after_clean = pointer_capture_snapshot_clean(after_capture);
    return normalized_input_replay_pointer_capture_delta{
        .before_capture = before_capture,
        .after_capture = after_capture,
        .before_clean = before_clean,
        .after_clean = after_clean,
        .changed = normalized_input_replay_pointer_capture_changed(before_capture, after_capture),
        .clean_changed = before_clean != after_clean,
    };
}

} // namespace quiz_vulkan::input
