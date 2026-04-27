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

enum class platform_shell_status {
    keep_running,
    exit_requested
};

class platform_shell {
public:
    virtual ~platform_shell() = default;

    virtual bool create(const platform_shell_config& config) = 0;
    virtual platform_shell_status pump_events() = 0;
    virtual void show_message(std::string_view message) = 0;
};

std::unique_ptr<platform_shell> create_platform_shell();

} // namespace quiz_vulkan
