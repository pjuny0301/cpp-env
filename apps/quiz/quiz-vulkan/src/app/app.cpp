#include "app/app.h"

#include <chrono>
#include <iostream>
#include <thread>
#include <utility>

namespace quiz_vulkan {

app::app(app_config config)
    : config_(std::move(config))
{
}

int app::run()
{
    shell_ = create_platform_shell();
    if (!shell_) {
        std::cerr << "failed to create platform shell\n";
        return 1;
    }

    if (!shell_->create(config_.shell)) {
        std::cerr << "failed to initialize platform shell\n";
        return 1;
    }

    shell_->show_message("quiz_vulkan platform shell ready");

    while (shell_->pump_events() == platform_shell_status::keep_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    return 0;
}

} // namespace quiz_vulkan
