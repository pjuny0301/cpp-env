#pragma once

#include "platform/platform_shell.h"

#include <memory>

namespace quiz_vulkan {

struct app_config {
    platform_shell_config shell;
};

class app {
public:
    explicit app(app_config config = {});

    int run();

private:
    app_config config_;
    std::unique_ptr<platform_shell> shell_;
};

} // namespace quiz_vulkan
