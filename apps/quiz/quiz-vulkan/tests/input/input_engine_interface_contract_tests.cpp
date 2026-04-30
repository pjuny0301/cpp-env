#include "core/input/gesture_recognizer.h"
#include "core/input/input_engine.h"
#include "core/input/text_input_model.h"
#include "platform/platform_input_event.h"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace {

using namespace quiz_vulkan;

template <typename T>
concept GestureRecognizerInterface = requires(
    T& recognizer,
    const input::pointer_event& pointer,
    std::int64_t timestamp_ms) {
    { recognizer.process_pointer_event(pointer) } -> std::same_as<std::vector<input::gesture_event>>;
    { recognizer.update_time(timestamp_ms) } -> std::same_as<std::vector<input::gesture_event>>;
    { recognizer.capture_snapshot() } -> std::same_as<input::pointer_capture_snapshot>;
    { recognizer.reset() } -> std::same_as<void>;
};

template <typename T>
concept PointerCaptureSnapshotInterface = requires(T snapshot) {
    { snapshot.lifecycle } -> std::same_as<input::pointer_capture_lifecycle&>;
    { snapshot.active } -> std::same_as<bool&>;
    { snapshot.pointer_id } -> std::same_as<std::int32_t&>;
    { snapshot.tracked_pointer_count } -> std::same_as<std::size_t&>;
};

template <typename T>
concept NormalizedInputEventSummaryInterface = requires(T summary) {
    { summary.kind } -> std::same_as<input::input_event_summary_kind&>;
    { summary.timestamp_ms } -> std::same_as<std::int64_t&>;
    { summary.duration_ms } -> std::same_as<std::int64_t&>;
    { summary.pointer_id } -> std::same_as<std::int32_t&>;
    { summary.start_x } -> std::same_as<float&>;
    { summary.start_y } -> std::same_as<float&>;
    { summary.x } -> std::same_as<float&>;
    { summary.y } -> std::same_as<float&>;
    { summary.delta_x } -> std::same_as<float&>;
    { summary.delta_y } -> std::same_as<float&>;
    { summary.pixel_delta_x } -> std::same_as<float&>;
    { summary.pixel_delta_y } -> std::same_as<float&>;
    { summary.line_delta_x } -> std::same_as<float&>;
    { summary.line_delta_y } -> std::same_as<float&>;
};

template <typename T>
concept InputRoutingDiagnosticsInterface = requires(T diagnostics) {
    { diagnostics.normalized_events } -> std::same_as<std::vector<input::normalized_input_event_summary>&>;
    { diagnostics.pointer_capture } -> std::same_as<input::pointer_capture_snapshot&>;
};

template <typename T>
concept ImeCompositionStateInterface = requires(T state) {
    { state.active } -> std::same_as<bool&>;
    { state.preedit_text } -> std::same_as<std::string&>;
    { state.replacement_range } -> std::same_as<input::text_range&>;
    { state.preedit_range } -> std::same_as<input::text_range&>;
    { state.caret_range } -> std::same_as<input::text_range&>;
};

template <typename T>
concept TextInputModelInterface = requires(
    T& model,
    std::string target,
    std::string_view text,
    input::text_range range) {
    { model.focus(target) } -> std::same_as<void>;
    { model.clear_focus() } -> std::same_as<void>;
    { model.has_focus() } -> std::same_as<bool>;
    { model.focus_id() } -> std::same_as<const std::string&>;
    { model.text() } -> std::same_as<const std::string&>;
    { model.preedit_text() } -> std::same_as<const std::string&>;
    { model.display_text() } -> std::same_as<std::string>;
    { model.caret_byte_offset() } -> std::same_as<std::size_t>;
    { model.caret_range() } -> std::same_as<input::text_range>;
    { model.preedit_range() } -> std::same_as<std::optional<input::text_range>>;
    { model.selection_range() } -> std::same_as<std::optional<input::text_range>>;
    { model.ime_composition() } -> std::same_as<input::ime_composition_state>;
    { model.move_caret_to_start() } -> std::same_as<bool>;
    { model.move_caret_to_end() } -> std::same_as<bool>;
    { model.move_caret_left() } -> std::same_as<bool>;
    { model.move_caret_right() } -> std::same_as<bool>;
    { model.extend_selection_left() } -> std::same_as<bool>;
    { model.extend_selection_right() } -> std::same_as<bool>;
    { model.select_all() } -> std::same_as<bool>;
    { model.clear_selection() } -> std::same_as<bool>;
    { model.set_selection(range) } -> std::same_as<bool>;
    { model.commit_utf8(text) } -> std::same_as<bool>;
    { model.backspace() } -> std::same_as<bool>;
    { model.set_preedit(text) } -> std::same_as<bool>;
    { model.commit_ime(text) } -> std::same_as<bool>;
    { model.cancel_ime() } -> std::same_as<bool>;
    { model.submit() } -> std::same_as<bool>;
    { model.has_submit_text() } -> std::same_as<bool>;
    { model.consume_submit_text() } -> std::same_as<std::optional<std::string>>;
};

static_assert(ImeCompositionStateInterface<input::ime_composition_state>);
static_assert(std::is_default_constructible_v<input::ime_composition_state>);
static_assert(PointerCaptureSnapshotInterface<input::pointer_capture_snapshot>);
static_assert(NormalizedInputEventSummaryInterface<input::normalized_input_event_summary>);
static_assert(InputRoutingDiagnosticsInterface<input::input_routing_diagnostics>);
static_assert(GestureRecognizerInterface<input::gesture_recognizer>);
static_assert(TextInputModelInterface<input::text_input_model>);

constexpr input::text_range text_range_contract{
    .start_byte = 3,
    .end_byte = 7,
};
static_assert(text_range_contract.start_byte == 3);
static_assert(text_range_contract.end_byte == 7);

constexpr input::text_event_kind caret_moved_contract_kind = input::text_event_kind::caret_moved;
static_assert(caret_moved_contract_kind == input::text_event_kind::caret_moved);

constexpr input::text_event_kind selection_changed_contract_kind = input::text_event_kind::selection_changed;
static_assert(selection_changed_contract_kind == input::text_event_kind::selection_changed);

static_assert(std::is_default_constructible_v<input::ime_event>);
static_assert(std::is_same_v<decltype(input::ime_event{}.composition), input::ime_composition_state>);

constexpr input::pointer_capture_snapshot captured_pointer_contract{
    .lifecycle = input::pointer_capture_lifecycle::captured,
    .active = true,
    .pointer_id = 3,
    .tracked_pointer_count = 1,
};
static_assert(captured_pointer_contract.lifecycle == input::pointer_capture_lifecycle::captured);
static_assert(captured_pointer_contract.active);
static_assert(captured_pointer_contract.pointer_id == 3);

constexpr input::normalized_input_event_summary wheel_summary_contract{
    .kind = input::input_event_summary_kind::wheel,
    .timestamp_ms = 30,
    .x = 4.0f,
    .y = 5.0f,
    .pixel_delta_y = -120.0f,
};
static_assert(wheel_summary_contract.kind == input::input_event_summary_kind::wheel);
static_assert(wheel_summary_contract.pixel_delta_y == -120.0f);

constexpr input::gesture_event drag_contract_event{
    .kind = input::gesture_kind::drag_update,
    .timestamp_ms = 20,
    .duration_ms = 10,
    .pointer_id = 4,
    .start_x = 1.0f,
    .start_y = 2.0f,
    .x = 5.0f,
    .y = 7.0f,
    .delta_x = 3.0f,
    .delta_y = 4.0f,
};
static_assert(drag_contract_event.kind == input::gesture_kind::drag_update);
static_assert(drag_contract_event.delta_x == 3.0f);
static_assert(drag_contract_event.delta_y == 4.0f);

constexpr input::gesture_thresholds drag_threshold_contract{
    .swipe_min_dx = 60.0f,
    .swipe_max_dy = 40.0f,
    .swipe_max_duration_ms = 800,
    .long_press_min_duration_ms = 600,
    .tap_slop = 8.0f,
    .drag_start_slop = 12.0f,
};
static_assert(drag_threshold_contract.drag_start_slop == 12.0f);

constexpr input::raw_scroll_event raw_scroll_contract{
    .timestamp_ms = 10,
    .x = 20.0f,
    .y = 30.0f,
    .delta_x = 1.0f,
    .delta_y = -2.0f,
    .unit = input::scroll_delta_unit::lines,
};
static_assert(raw_scroll_contract.unit == input::scroll_delta_unit::lines);

constexpr raw_platform_scroll_event raw_platform_scroll_contract{
    .timestamp_ms = 12,
    .x = 20.0f,
    .y = 30.0f,
    .delta_x = 4.0f,
    .delta_y = -8.0f,
    .unit = raw_platform_scroll_delta_unit::pixels,
};
static_assert(raw_platform_scroll_contract.unit == raw_platform_scroll_delta_unit::pixels);

constexpr input::scroll_event scroll_contract{
    .timestamp_ms = 10,
    .x = 20.0f,
    .y = 30.0f,
    .pixel_delta_x = 4.0f,
    .pixel_delta_y = -8.0f,
    .line_delta_x = 1.0f,
    .line_delta_y = -2.0f,
};
static_assert(scroll_contract.pixel_delta_y == -8.0f);
static_assert(scroll_contract.line_delta_y == -2.0f);
static_assert(std::variant_size_v<input::input_event> == 4);

template <typename T>
concept InputEngineInterface = requires(
    T& engine,
    std::string target,
    const raw_platform_input_event& event,
    const input::raw_scroll_event& scroll,
    std::int64_t timestamp_ms) {
    { engine.focus_text_target(target) } -> std::same_as<void>;
    { engine.clear_text_focus() } -> std::same_as<void>;
    { engine.has_text_focus() } -> std::same_as<bool>;
    { engine.text_focus_id() } -> std::same_as<const std::string&>;
    { engine.text_model() } -> std::same_as<const input::text_input_model&>;
    { engine.routing_diagnostics() } -> std::same_as<const input::input_routing_diagnostics&>;
    { engine.process_raw_event(event) } -> std::same_as<std::vector<input::input_event>>;
    { engine.process_scroll_event(scroll) } -> std::same_as<std::vector<input::input_event>>;
    { engine.update_time(timestamp_ms) } -> std::same_as<std::vector<input::input_event>>;
    { engine.reset() } -> std::same_as<void>;
};

static_assert(std::is_constructible_v<input::input_engine>);
static_assert(InputEngineInterface<input::input_engine>);

static_assert(std::variant_size_v<raw_platform_input_event> == 6);
static_assert(std::is_same_v<
    std::variant_alternative_t<0, raw_platform_input_event>,
    raw_platform_pointer_event>);
static_assert(std::is_same_v<
    std::variant_alternative_t<1, raw_platform_input_event>,
    raw_platform_text_event>);
static_assert(std::is_same_v<
    std::variant_alternative_t<2, raw_platform_input_event>,
    raw_platform_ime_event>);
static_assert(std::is_same_v<
    std::variant_alternative_t<3, raw_platform_input_event>,
    raw_platform_key_event>);
static_assert(std::is_same_v<
    std::variant_alternative_t<4, raw_platform_input_event>,
    raw_platform_focus_event>);
static_assert(std::is_same_v<
    std::variant_alternative_t<5, raw_platform_input_event>,
    raw_platform_scroll_event>);

} // namespace
