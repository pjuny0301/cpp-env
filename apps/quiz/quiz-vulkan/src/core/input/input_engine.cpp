#include "core/input/input_engine.h"

#include <type_traits>
#include <utility>
#include <variant>

namespace quiz_vulkan::input {
namespace {

bool is_primary_pointer(const raw_platform_pointer_event& event)
{
    return event.button == raw_platform_pointer_button::none
        || event.button == raw_platform_pointer_button::primary;
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

void append_gestures(std::vector<input_event>& events, const std::vector<gesture_event>& gestures)
{
    for (const gesture_event& gesture : gestures) {
        events.emplace_back(gesture);
    }
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
            } else {
                return process_focus_event(raw_event);
            }
        },
        event);
}

std::vector<input_event> input_engine::update_time(std::int64_t timestamp_ms)
{
    std::vector<input_event> events;
    append_gestures(events, gestures_.update_time(timestamp_ms));
    return events;
}

std::vector<input_event> input_engine::process_scroll_event(const raw_scroll_event& event)
{
    std::vector<input_event> events;
    if (event.delta_x == 0.0f && event.delta_y == 0.0f) {
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

    events.emplace_back(normalized);
    return events;
}

void input_engine::reset()
{
    gestures_.reset();
    text_ = text_input_model{};
    ime_composing_ = false;
}

std::vector<input_event> input_engine::process_pointer_event(const raw_platform_pointer_event& event)
{
    std::vector<input_event> events;
    if (!is_primary_pointer(event)) {
        return events;
    }

    append_gestures(
        events,
        gestures_.process_pointer_event(pointer_event{
            .timestamp_ms = event.timestamp_ms,
            .pointer_id = event.pointer_id,
            .phase = to_pointer_phase(event.phase),
            .x = event.x,
            .y = event.y,
        }));
    return events;
}

std::vector<input_event> input_engine::process_text_event(const raw_platform_text_event& event)
{
    std::vector<input_event> events;
    if (ime_composing_ || !text_.commit_utf8(event.utf8_text)) {
        return events;
    }

    events.emplace_back(text_event{
        .kind = text_event_kind::commit,
        .timestamp_ms = event.timestamp_ms,
        .target_id = text_.focus_id(),
        .utf8_text = event.utf8_text,
    });
    return events;
}

std::vector<input_event> input_engine::process_ime_event(const raw_platform_ime_event& event)
{
    std::vector<input_event> events;
    if (!text_.has_focus()) {
        ime_composing_ = false;
        return events;
    }

    const std::string target_id = text_.focus_id();
    const bool had_composition = ime_composing_ || !text_.preedit_text().empty();

    if (event.phase == raw_platform_ime_phase::composition_start) {
        const bool had_preedit = !text_.preedit_text().empty();
        ime_composing_ = true;
        if (had_preedit) {
            text_.cancel_ime();
            events.emplace_back(ime_event{
                .kind = ime_event_kind::cancel,
                .timestamp_ms = event.timestamp_ms,
                .target_id = target_id,
                .utf8_text = {},
            });
        } else {
            text_.set_preedit("");
        }
        return events;
    }

    if (event.phase == raw_platform_ime_phase::preedit_update) {
        ime_composing_ = true;
        text_.set_preedit(event.utf8_text);
        events.emplace_back(ime_event{
            .kind = ime_event_kind::preedit,
            .timestamp_ms = event.timestamp_ms,
            .target_id = target_id,
            .utf8_text = event.utf8_text,
        });
        return events;
    }

    if (event.phase == raw_platform_ime_phase::commit
        || (event.phase == raw_platform_ime_phase::composition_end && !event.utf8_text.empty())) {
        ime_composing_ = false;
        if (text_.commit_ime(event.utf8_text)) {
            events.emplace_back(ime_event{
                .kind = ime_event_kind::commit,
                .timestamp_ms = event.timestamp_ms,
                .target_id = target_id,
                .utf8_text = event.utf8_text,
            });
            return events;
        }
    }

    if (event.phase == raw_platform_ime_phase::cancel
        || event.phase == raw_platform_ime_phase::composition_end
        || (event.phase == raw_platform_ime_phase::commit && event.utf8_text.empty())) {
        ime_composing_ = false;
        text_.cancel_ime();
        if (had_composition) {
            events.emplace_back(ime_event{
                .kind = ime_event_kind::cancel,
                .timestamp_ms = event.timestamp_ms,
                .target_id = target_id,
                .utf8_text = {},
            });
        }
    }

    return events;
}

std::vector<input_event> input_engine::process_key_event(const raw_platform_key_event& event)
{
    std::vector<input_event> events;
    if (event.phase != raw_platform_key_phase::down || ime_composing_ || !text_.has_focus()) {
        return events;
    }

    if (is_backspace_key(event)) {
        if (text_.backspace()) {
            events.emplace_back(text_event{
                .kind = text_event_kind::backspace,
                .timestamp_ms = event.timestamp_ms,
                .target_id = text_.focus_id(),
                .utf8_text = {},
            });
        }
        return events;
    }

    if (is_submit_key(event)) {
        if (event.repeat) {
            return events;
        }

        const std::string submitted_text = text_.text();
        if (text_.submit()) {
            events.emplace_back(text_event{
                .kind = text_event_kind::submit,
                .timestamp_ms = event.timestamp_ms,
                .target_id = text_.focus_id(),
                .utf8_text = submitted_text,
            });
        }
    }

    return events;
}

std::vector<input_event> input_engine::process_focus_event(const raw_platform_focus_event& event)
{
    std::vector<input_event> events;
    if (event.phase == raw_platform_focus_phase::gained) {
        if (text_.has_focus()) {
            events.emplace_back(text_event{
                .kind = text_event_kind::focus_gained,
                .timestamp_ms = event.timestamp_ms,
                .target_id = text_.focus_id(),
                .utf8_text = {},
            });
        }
        return events;
    }

    const bool had_focus = text_.has_focus();
    const bool had_composition = ime_composing_ || !text_.preedit_text().empty();
    const std::string target_id = text_.focus_id();

    gestures_.reset();
    text_.clear_focus();
    ime_composing_ = false;

    if (had_composition) {
        events.emplace_back(ime_event{
            .kind = ime_event_kind::cancel,
            .timestamp_ms = event.timestamp_ms,
            .target_id = target_id,
            .utf8_text = {},
        });
    }

    if (had_focus) {
        events.emplace_back(text_event{
            .kind = text_event_kind::focus_lost,
            .timestamp_ms = event.timestamp_ms,
            .target_id = target_id,
            .utf8_text = {},
        });
    }

    return events;
}

} // namespace quiz_vulkan::input
