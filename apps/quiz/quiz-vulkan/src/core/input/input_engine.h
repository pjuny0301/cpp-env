#pragma once

#include "core/input/gesture_recognizer.h"
#include "core/input/input_event.h"
#include "core/input/text_input_model.h"
#include "platform/platform_input_event.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace quiz_vulkan::input {

enum class action_route_policy_kind {
    pointer_capture_reset,
    wheel_summary,
    gesture_route_snapshot,
    text_commit_boundary,
    text_backspace_boundary,
    caret_moved,
    selection_changed,
    text_submit_boundary,
    focus_loss,
    ime_commit,
    ime_cancel,
};

struct action_route_policy_diagnostic {
    action_route_policy_kind kind = action_route_policy_kind::pointer_capture_reset;
    std::int64_t timestamp_ms = 0;
    bool emits_input_event = false;
    std::size_t event_index = 0;
    std::string target_id;
    std::size_t text_byte_count = 0;
    std::size_t text_byte_count_before = 0;
    std::size_t text_byte_count_after = 0;
    text_range caret_before;
    text_range caret_after;
    bool had_selection_before = false;
    bool has_selection_after = false;
    text_range selection_before;
    text_range selection_after;
    normalized_input_event_summary normalized_event;
    ime_composition_state composition;
    pointer_capture_snapshot pointer_capture_before;
    pointer_capture_snapshot pointer_capture_after;
};

struct input_routing_diagnostics {
    std::vector<normalized_input_event_summary> normalized_events;
    std::vector<action_route_policy_diagnostic> action_routes;
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
    void append_policy(action_route_policy_diagnostic diagnostic);

    gesture_recognizer gestures_;
    text_input_model text_;
    input_routing_diagnostics diagnostics_;
    bool ime_composing_ = false;
};

} // namespace quiz_vulkan::input
