#pragma once

#include "core/input/gesture_recognizer.h"
#include "core/input/input_event.h"
#include "core/input/text_input_model.h"
#include "platform/platform_input_event.h"

#include <cstdint>
#include <string>
#include <vector>

namespace quiz_vulkan::input {

struct input_routing_diagnostics {
    std::vector<normalized_input_event_summary> normalized_events;
    pointer_capture_snapshot pointer_capture;
};

class input_engine {
public:
    explicit input_engine(gesture_thresholds thresholds = {});

    void focus_text_target(std::string target_id);
    void clear_text_focus();

    [[nodiscard]] bool has_text_focus() const;
    [[nodiscard]] const std::string& text_focus_id() const;
    [[nodiscard]] const text_input_model& text_model() const;
    [[nodiscard]] const input_routing_diagnostics& routing_diagnostics() const;

    [[nodiscard]] std::vector<input_event> process_raw_event(const raw_platform_input_event& event);
    [[nodiscard]] std::vector<input_event> process_scroll_event(const raw_scroll_event& event);
    [[nodiscard]] std::vector<input_event> update_time(std::int64_t timestamp_ms);
    void reset();

private:
    [[nodiscard]] std::vector<input_event> process_pointer_event(const raw_platform_pointer_event& event);
    [[nodiscard]] std::vector<input_event> process_text_event(const raw_platform_text_event& event);
    [[nodiscard]] std::vector<input_event> process_ime_event(const raw_platform_ime_event& event);
    [[nodiscard]] std::vector<input_event> process_key_event(const raw_platform_key_event& event);
    [[nodiscard]] std::vector<input_event> process_focus_event(const raw_platform_focus_event& event);
    void begin_route_diagnostics();
    void finish_route_diagnostics();
    void append_gestures(std::vector<input_event>& events, const std::vector<gesture_event>& gestures);
    void append_scroll(std::vector<input_event>& events, const scroll_event& scroll);

    gesture_recognizer gestures_;
    text_input_model text_;
    input_routing_diagnostics diagnostics_;
    bool ime_composing_ = false;
};

} // namespace quiz_vulkan::input
