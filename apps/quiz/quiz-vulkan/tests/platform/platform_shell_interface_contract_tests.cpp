#include "platform/platform_input_event.h"
#include "platform/platform_shell.h"

#include <concepts>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace {

using namespace quiz_vulkan;

template <typename T>
concept PlatformShellInterface = requires(
    T& shell,
    const platform_shell_config& config,
    std::size_t width,
    std::size_t height,
    const unsigned char* rgba,
    std::string_view text) {
    { shell.create(config) } -> std::same_as<bool>;
    { shell.pump_events() } -> std::same_as<platform_shell_status>;
    { shell.drain_input_events() } -> std::same_as<std::vector<platform_input_event>>;
    { shell.state() } -> std::same_as<platform_shell_state>;
    { shell.present_framebuffer(width, height, rgba) } -> std::same_as<void>;
    { shell.set_frame_status(text) } -> std::same_as<void>;
    { shell.show_message(text) } -> std::same_as<void>;
};

static_assert(PlatformShellInterface<platform_shell>);

static_assert(requires(platform_shell_config config) {
    { config.title } -> std::same_as<std::string&>;
    { config.width } -> std::same_as<int&>;
    { config.height } -> std::same_as<int&>;
});

static_assert(requires(platform_input_event event) {
    { event.type } -> std::same_as<platform_input_event_type&>;
    { event.x } -> std::same_as<float&>;
    { event.y } -> std::same_as<float&>;
    { event.text } -> std::same_as<std::string&>;
});

constexpr raw_platform_scroll_event platform_raw_scroll_contract{
    .timestamp_ms = 12,
    .x = 20.0f,
    .y = 30.0f,
    .delta_x = 4.0f,
    .delta_y = -8.0f,
    .unit = raw_platform_scroll_delta_unit::pixels,
};
static_assert(platform_raw_scroll_contract.unit == raw_platform_scroll_delta_unit::pixels);

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

static_assert(requires {
    { create_platform_shell() } -> std::same_as<std::unique_ptr<platform_shell>>;
});

} // namespace
