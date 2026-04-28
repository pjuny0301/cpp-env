#include "core/input/gesture_recognizer.h"
#include "core/input/text_input_model.h"
#include "platform/platform_input_event.h"

#include <concepts>
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
    { recognizer.reset() } -> std::same_as<void>;
};

template <typename T>
concept TextInputModelInterface = requires(
    T& model,
    std::string target,
    std::string_view text) {
    { model.focus(target) } -> std::same_as<void>;
    { model.clear_focus() } -> std::same_as<void>;
    { model.has_focus() } -> std::same_as<bool>;
    { model.focus_id() } -> std::same_as<const std::string&>;
    { model.text() } -> std::same_as<const std::string&>;
    { model.preedit_text() } -> std::same_as<const std::string&>;
    { model.display_text() } -> std::same_as<std::string>;
    { model.commit_utf8(text) } -> std::same_as<bool>;
    { model.backspace() } -> std::same_as<bool>;
    { model.set_preedit(text) } -> std::same_as<bool>;
    { model.commit_ime(text) } -> std::same_as<bool>;
    { model.cancel_ime() } -> std::same_as<bool>;
    { model.submit() } -> std::same_as<bool>;
    { model.has_submit_text() } -> std::same_as<bool>;
    { model.consume_submit_text() } -> std::same_as<std::optional<std::string>>;
};

static_assert(GestureRecognizerInterface<input::gesture_recognizer>);
static_assert(TextInputModelInterface<input::text_input_model>);

static_assert(std::variant_size_v<raw_platform_input_event> == 5);
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

} // namespace
