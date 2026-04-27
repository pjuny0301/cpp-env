#include "platform/platform_shell.h"

#include <iostream>
#include <memory>
#include <string_view>

namespace quiz_vulkan {
namespace {

class null_platform_shell final : public platform_shell {
public:
    bool create(const platform_shell_config& config) override
    {
        std::cout << "null platform shell placeholder: " << config.title << " "
                  << config.width << "x" << config.height << '\n';
        return true;
    }

    platform_shell_status pump_events() override
    {
        return platform_shell_status::exit_requested;
    }

    void show_message(std::string_view message) override
    {
        std::cout << message << '\n';
    }
};

} // namespace

std::unique_ptr<platform_shell> create_platform_shell()
{
    return std::make_unique<null_platform_shell>();
}

} // namespace quiz_vulkan
