#pragma once

#include <memory>
#include <string>
#include <string_view>

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

enum class platform_shell_status {
    keep_running,
    exit_requested
};

class platform_shell {
public:
    virtual ~platform_shell() = default;

    virtual bool create(const platform_shell_config& config) = 0;
    virtual platform_shell_status pump_events() = 0;
    [[nodiscard]] virtual platform_shell_state state() const { return {}; }
    virtual void set_frame_status(std::string_view status) { (void)status; }
    virtual void show_message(std::string_view message) = 0;
};

std::unique_ptr<platform_shell> create_platform_shell();

} // namespace quiz_vulkan
