#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace quiz_vulkan {

struct platform_shell_config {
    std::string title = "quiz_vulkan";
    int width = 1280;
    int height = 720;
};

struct platform_client_size {
    int width = 0;
    int height = 0;
};

struct platform_shell_state {
    platform_client_size client_size;
    std::string frame_status;
};

struct platform_text_overlay {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    std::string text;
    std::uint8_t red = 255;
    std::uint8_t green = 255;
    std::uint8_t blue = 255;
    std::uint8_t alpha = 255;
};

enum class platform_shell_status {
    keep_running,
    exit_requested
};

enum class platform_input_event_type {
    pointer_press,
};

struct platform_input_event {
    platform_input_event_type type = platform_input_event_type::pointer_press;
    float x = 0.0f;
    float y = 0.0f;
};

class platform_shell {
public:
    virtual ~platform_shell() = default;

    virtual bool create(const platform_shell_config& config) = 0;
    virtual platform_shell_status pump_events() = 0;
    [[nodiscard]] virtual std::vector<platform_input_event> drain_input_events() { return {}; }
    [[nodiscard]] virtual platform_shell_state state() const { return {}; }
    virtual void present_framebuffer(std::size_t width, std::size_t height, const unsigned char* rgba)
    {
        (void)width;
        (void)height;
        (void)rgba;
    }
    virtual void present_frame(
        std::size_t width,
        std::size_t height,
        const unsigned char* rgba,
        const std::vector<platform_text_overlay>& text_overlays)
    {
        (void)text_overlays;
        present_framebuffer(width, height, rgba);
    }
    virtual void set_frame_status(std::string_view status) { (void)status; }
    virtual void show_message(std::string_view message) = 0;
};

std::unique_ptr<platform_shell> create_platform_shell();

} // namespace quiz_vulkan
