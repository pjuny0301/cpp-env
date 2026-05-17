#pragma once

#include "platform/platform_input_event.h"

#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace quiz_vulkan::input {

enum class platform_input_source_kind {
    mouse,
    touch,
    wheel,
    key,
    character,
    ime,
};

enum class platform_pointer_sample_phase {
    down,
    move,
    up,
    cancel,
};

enum class platform_mouse_button {
    primary,
    secondary,
    middle,
};

enum class platform_key_sample_phase {
    down,
    up,
};

enum class platform_scroll_delta_unit {
    pixels,
    lines,
};

enum class platform_ime_sample_phase {
    composition_start,
    preedit_update,
    commit,
    composition_end,
    cancel,
};

enum class platform_input_translation_status {
    accepted,
    rejected,
};

enum class platform_input_rejection_reason {
    none,
    invalid_timestamp,
    invalid_pointer_id,
    invalid_coordinates,
    invalid_wheel_delta,
    invalid_key,
    empty_text,
    invalid_utf8,
};

struct platform_mouse_sample {
    std::int64_t timestamp_ms = 0;
    std::int32_t pointer_id = 1;
    platform_pointer_sample_phase phase = platform_pointer_sample_phase::down;
    platform_mouse_button button = platform_mouse_button::primary;
    float x = 0.0f;
    float y = 0.0f;
};

struct platform_touch_sample {
    std::int64_t timestamp_ms = 0;
    std::int32_t contact_id = 1;
    platform_pointer_sample_phase phase = platform_pointer_sample_phase::down;
    float x = 0.0f;
    float y = 0.0f;
};

struct platform_wheel_sample {
    std::int64_t timestamp_ms = 0;
    float x = 0.0f;
    float y = 0.0f;
    float delta_x = 0.0f;
    float delta_y = 0.0f;
    platform_scroll_delta_unit unit = platform_scroll_delta_unit::pixels;
    bool alt = false;
    bool ctrl = false;
    bool shift = false;
    bool meta = false;
};

struct platform_key_sample {
    std::int64_t timestamp_ms = 0;
    platform_key_sample_phase phase = platform_key_sample_phase::down;
    std::int32_t key_code = 0;
    std::string logical_key;
    bool alt = false;
    bool ctrl = false;
    bool shift = false;
    bool meta = false;
    bool repeat = false;
};

struct platform_character_sample {
    std::int64_t timestamp_ms = 0;
    std::string utf8_text;
};

struct platform_ime_composition_sample {
    std::int64_t timestamp_ms = 0;
    platform_ime_sample_phase phase = platform_ime_sample_phase::preedit_update;
    std::string utf8_text;
};

using platform_input_sample = std::variant<
    platform_mouse_sample,
    platform_touch_sample,
    platform_wheel_sample,
    platform_key_sample,
    platform_character_sample,
    platform_ime_composition_sample>;

struct platform_input_translation_request {
    platform_input_sample sample;
};

struct platform_input_translation_diagnostic {
    platform_input_source_kind source = platform_input_source_kind::mouse;
    platform_input_translation_status status = platform_input_translation_status::rejected;
    platform_input_rejection_reason rejection_reason = platform_input_rejection_reason::none;
    std::int64_t timestamp_ms = 0;
    bool emitted_event = false;
};

struct platform_input_translation_result {
    std::optional<raw_platform_input_event> event;
    platform_input_translation_diagnostic diagnostic;
};

class platform_input_translator {
public:
    [[nodiscard]] platform_input_translation_result translate(
        const platform_input_translation_request& request) const;
};

namespace detail {

[[nodiscard]] inline bool finite_position(float x, float y)
{
    return std::isfinite(x) && std::isfinite(y);
}

[[nodiscard]] inline raw_platform_pointer_phase to_raw_pointer_phase(platform_pointer_sample_phase phase)
{
    switch (phase) {
    case platform_pointer_sample_phase::down:
        return raw_platform_pointer_phase::down;
    case platform_pointer_sample_phase::move:
        return raw_platform_pointer_phase::move;
    case platform_pointer_sample_phase::up:
        return raw_platform_pointer_phase::up;
    case platform_pointer_sample_phase::cancel:
        return raw_platform_pointer_phase::cancel;
    }

    return raw_platform_pointer_phase::cancel;
}

[[nodiscard]] inline raw_platform_pointer_button to_raw_pointer_button(platform_mouse_button button)
{
    switch (button) {
    case platform_mouse_button::primary:
        return raw_platform_pointer_button::primary;
    case platform_mouse_button::secondary:
        return raw_platform_pointer_button::secondary;
    case platform_mouse_button::middle:
        return raw_platform_pointer_button::middle;
    }

    return raw_platform_pointer_button::primary;
}

[[nodiscard]] inline raw_platform_key_phase to_raw_key_phase(platform_key_sample_phase phase)
{
    switch (phase) {
    case platform_key_sample_phase::down:
        return raw_platform_key_phase::down;
    case platform_key_sample_phase::up:
        return raw_platform_key_phase::up;
    }

    return raw_platform_key_phase::up;
}

[[nodiscard]] inline raw_platform_scroll_delta_unit to_raw_scroll_delta_unit(platform_scroll_delta_unit unit)
{
    switch (unit) {
    case platform_scroll_delta_unit::pixels:
        return raw_platform_scroll_delta_unit::pixels;
    case platform_scroll_delta_unit::lines:
        return raw_platform_scroll_delta_unit::lines;
    }

    return raw_platform_scroll_delta_unit::pixels;
}

[[nodiscard]] inline raw_platform_ime_phase to_raw_ime_phase(platform_ime_sample_phase phase)
{
    switch (phase) {
    case platform_ime_sample_phase::composition_start:
        return raw_platform_ime_phase::composition_start;
    case platform_ime_sample_phase::preedit_update:
        return raw_platform_ime_phase::preedit_update;
    case platform_ime_sample_phase::commit:
        return raw_platform_ime_phase::commit;
    case platform_ime_sample_phase::composition_end:
        return raw_platform_ime_phase::composition_end;
    case platform_ime_sample_phase::cancel:
        return raw_platform_ime_phase::cancel;
    }

    return raw_platform_ime_phase::cancel;
}

[[nodiscard]] inline bool valid_utf8(std::string_view text)
{
    std::size_t index = 0;
    while (index < text.size()) {
        const auto leading = static_cast<unsigned char>(text[index]);
        if (leading <= 0x7F) {
            ++index;
            continue;
        }

        std::size_t continuation_count = 0;
        std::uint32_t codepoint = 0;
        std::uint32_t minimum_codepoint = 0;
        if ((leading & 0xE0U) == 0xC0U) {
            continuation_count = 1;
            codepoint = leading & 0x1FU;
            minimum_codepoint = 0x80U;
        } else if ((leading & 0xF0U) == 0xE0U) {
            continuation_count = 2;
            codepoint = leading & 0x0FU;
            minimum_codepoint = 0x800U;
        } else if ((leading & 0xF8U) == 0xF0U) {
            continuation_count = 3;
            codepoint = leading & 0x07U;
            minimum_codepoint = 0x10000U;
        } else {
            return false;
        }

        if (index + continuation_count >= text.size()) {
            return false;
        }

        for (std::size_t offset = 1; offset <= continuation_count; ++offset) {
            const auto continuation = static_cast<unsigned char>(text[index + offset]);
            if ((continuation & 0xC0U) != 0x80U) {
                return false;
            }
            codepoint = (codepoint << 6U) | (continuation & 0x3FU);
        }

        if (codepoint < minimum_codepoint
            || codepoint > 0x10FFFFU
            || (codepoint >= 0xD800U && codepoint <= 0xDFFFU)) {
            return false;
        }

        index += continuation_count + 1;
    }

    return true;
}

[[nodiscard]] inline platform_input_translation_result rejected_result(
    platform_input_source_kind source,
    std::int64_t timestamp_ms,
    platform_input_rejection_reason reason)
{
    return platform_input_translation_result{
        .event = std::nullopt,
        .diagnostic = platform_input_translation_diagnostic{
            .source = source,
            .status = platform_input_translation_status::rejected,
            .rejection_reason = reason,
            .timestamp_ms = timestamp_ms,
            .emitted_event = false,
        },
    };
}

[[nodiscard]] inline platform_input_translation_result accepted_result(
    platform_input_source_kind source,
    std::int64_t timestamp_ms,
    raw_platform_input_event event)
{
    return platform_input_translation_result{
        .event = std::move(event),
        .diagnostic = platform_input_translation_diagnostic{
            .source = source,
            .status = platform_input_translation_status::accepted,
            .rejection_reason = platform_input_rejection_reason::none,
            .timestamp_ms = timestamp_ms,
            .emitted_event = true,
        },
    };
}

[[nodiscard]] inline platform_input_translation_result translate_mouse_sample(
    const platform_mouse_sample& sample)
{
    if (sample.timestamp_ms < 0) {
        return rejected_result(
            platform_input_source_kind::mouse,
            sample.timestamp_ms,
            platform_input_rejection_reason::invalid_timestamp);
    }
    if (sample.pointer_id <= 0) {
        return rejected_result(
            platform_input_source_kind::mouse,
            sample.timestamp_ms,
            platform_input_rejection_reason::invalid_pointer_id);
    }
    if (!finite_position(sample.x, sample.y)) {
        return rejected_result(
            platform_input_source_kind::mouse,
            sample.timestamp_ms,
            platform_input_rejection_reason::invalid_coordinates);
    }

    return accepted_result(
        platform_input_source_kind::mouse,
        sample.timestamp_ms,
        raw_platform_pointer_event{
            .timestamp_ms = sample.timestamp_ms,
            .pointer_id = sample.pointer_id,
            .phase = to_raw_pointer_phase(sample.phase),
            .button = to_raw_pointer_button(sample.button),
            .x = sample.x,
            .y = sample.y,
        });
}

[[nodiscard]] inline platform_input_translation_result translate_touch_sample(
    const platform_touch_sample& sample)
{
    if (sample.timestamp_ms < 0) {
        return rejected_result(
            platform_input_source_kind::touch,
            sample.timestamp_ms,
            platform_input_rejection_reason::invalid_timestamp);
    }
    if (sample.contact_id <= 0) {
        return rejected_result(
            platform_input_source_kind::touch,
            sample.timestamp_ms,
            platform_input_rejection_reason::invalid_pointer_id);
    }
    if (!finite_position(sample.x, sample.y)) {
        return rejected_result(
            platform_input_source_kind::touch,
            sample.timestamp_ms,
            platform_input_rejection_reason::invalid_coordinates);
    }

    return accepted_result(
        platform_input_source_kind::touch,
        sample.timestamp_ms,
        raw_platform_pointer_event{
            .timestamp_ms = sample.timestamp_ms,
            .pointer_id = sample.contact_id,
            .phase = to_raw_pointer_phase(sample.phase),
            .button = raw_platform_pointer_button::none,
            .x = sample.x,
            .y = sample.y,
        });
}

[[nodiscard]] inline platform_input_translation_result translate_wheel_sample(
    const platform_wheel_sample& sample)
{
    if (sample.timestamp_ms < 0) {
        return rejected_result(
            platform_input_source_kind::wheel,
            sample.timestamp_ms,
            platform_input_rejection_reason::invalid_timestamp);
    }
    if (!finite_position(sample.x, sample.y)
        || !std::isfinite(sample.delta_x)
        || !std::isfinite(sample.delta_y)) {
        return rejected_result(
            platform_input_source_kind::wheel,
            sample.timestamp_ms,
            platform_input_rejection_reason::invalid_coordinates);
    }
    if (sample.delta_x == 0.0f && sample.delta_y == 0.0f) {
        return rejected_result(
            platform_input_source_kind::wheel,
            sample.timestamp_ms,
            platform_input_rejection_reason::invalid_wheel_delta);
    }

    return accepted_result(
        platform_input_source_kind::wheel,
        sample.timestamp_ms,
        raw_platform_scroll_event{
            .timestamp_ms = sample.timestamp_ms,
            .x = sample.x,
            .y = sample.y,
            .delta_x = sample.delta_x,
            .delta_y = sample.delta_y,
            .unit = to_raw_scroll_delta_unit(sample.unit),
            .alt = sample.alt,
            .ctrl = sample.ctrl,
            .shift = sample.shift,
            .meta = sample.meta,
        });
}

[[nodiscard]] inline platform_input_translation_result translate_key_sample(
    const platform_key_sample& sample)
{
    if (sample.timestamp_ms < 0) {
        return rejected_result(
            platform_input_source_kind::key,
            sample.timestamp_ms,
            platform_input_rejection_reason::invalid_timestamp);
    }
    if (sample.key_code == 0 && sample.logical_key.empty()) {
        return rejected_result(
            platform_input_source_kind::key,
            sample.timestamp_ms,
            platform_input_rejection_reason::invalid_key);
    }

    return accepted_result(
        platform_input_source_kind::key,
        sample.timestamp_ms,
        raw_platform_key_event{
            .timestamp_ms = sample.timestamp_ms,
            .phase = to_raw_key_phase(sample.phase),
            .key_code = sample.key_code,
            .logical_key = sample.logical_key,
            .alt = sample.alt,
            .ctrl = sample.ctrl,
            .shift = sample.shift,
            .meta = sample.meta,
            .repeat = sample.repeat,
        });
}

[[nodiscard]] inline platform_input_translation_result translate_character_sample(
    const platform_character_sample& sample)
{
    if (sample.timestamp_ms < 0) {
        return rejected_result(
            platform_input_source_kind::character,
            sample.timestamp_ms,
            platform_input_rejection_reason::invalid_timestamp);
    }
    if (sample.utf8_text.empty()) {
        return rejected_result(
            platform_input_source_kind::character,
            sample.timestamp_ms,
            platform_input_rejection_reason::empty_text);
    }
    if (!valid_utf8(sample.utf8_text)) {
        return rejected_result(
            platform_input_source_kind::character,
            sample.timestamp_ms,
            platform_input_rejection_reason::invalid_utf8);
    }

    return accepted_result(
        platform_input_source_kind::character,
        sample.timestamp_ms,
        raw_platform_text_event{
            .timestamp_ms = sample.timestamp_ms,
            .utf8_text = sample.utf8_text,
        });
}

[[nodiscard]] inline platform_input_translation_result translate_ime_sample(
    const platform_ime_composition_sample& sample)
{
    if (sample.timestamp_ms < 0) {
        return rejected_result(
            platform_input_source_kind::ime,
            sample.timestamp_ms,
            platform_input_rejection_reason::invalid_timestamp);
    }
    if (sample.phase == platform_ime_sample_phase::commit && sample.utf8_text.empty()) {
        return rejected_result(
            platform_input_source_kind::ime,
            sample.timestamp_ms,
            platform_input_rejection_reason::empty_text);
    }
    if (!sample.utf8_text.empty() && !valid_utf8(sample.utf8_text)) {
        return rejected_result(
            platform_input_source_kind::ime,
            sample.timestamp_ms,
            platform_input_rejection_reason::invalid_utf8);
    }

    return accepted_result(
        platform_input_source_kind::ime,
        sample.timestamp_ms,
        raw_platform_ime_event{
            .timestamp_ms = sample.timestamp_ms,
            .phase = to_raw_ime_phase(sample.phase),
            .utf8_text = sample.utf8_text,
        });
}

} // namespace detail

inline platform_input_translation_result platform_input_translator::translate(
    const platform_input_translation_request& request) const
{
    return std::visit(
        [](const auto& sample) -> platform_input_translation_result {
            using sample_type = std::decay_t<decltype(sample)>;
            if constexpr (std::is_same_v<sample_type, platform_mouse_sample>) {
                return detail::translate_mouse_sample(sample);
            } else if constexpr (std::is_same_v<sample_type, platform_touch_sample>) {
                return detail::translate_touch_sample(sample);
            } else if constexpr (std::is_same_v<sample_type, platform_wheel_sample>) {
                return detail::translate_wheel_sample(sample);
            } else if constexpr (std::is_same_v<sample_type, platform_key_sample>) {
                return detail::translate_key_sample(sample);
            } else if constexpr (std::is_same_v<sample_type, platform_character_sample>) {
                return detail::translate_character_sample(sample);
            } else {
                return detail::translate_ime_sample(sample);
            }
        },
        request.sample);
}

} // namespace quiz_vulkan::input
