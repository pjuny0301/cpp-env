#include "platform/platform_shell.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <string_view>

namespace quiz_vulkan {
namespace {

platform_client_size configured_client_size(const platform_shell_config& config)
{
    return {
        std::max(config.width, 1),
        std::max(config.height, 1),
    };
}

class null_platform_shell final : public platform_shell {
public:
    bool create(const platform_shell_config& config) override
    {
        state_.client_size = configured_client_size(config);
        std::cout << "null platform shell placeholder: " << config.title << " "
                  << state_.client_size.width << "x" << state_.client_size.height << '\n';
        return true;
    }

    platform_shell_status pump_events() override
    {
        return platform_shell_status::exit_requested;
    }

    [[nodiscard]] platform_shell_state state() const override
    {
        return state_;
    }

    void set_frame_status(std::string_view status) override
    {
        state_.frame_status = status;
    }

    void show_message(std::string_view message) override
    {
        std::cout << message << '\n';
    }

private:
    platform_shell_state state_;
};

} // namespace

std::unique_ptr<platform_shell> create_platform_shell()
{
    return std::make_unique<null_platform_shell>();
}

} // namespace quiz_vulkan
