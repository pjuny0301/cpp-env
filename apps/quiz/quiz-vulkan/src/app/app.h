#pragma once

#include "app/app_deck_loader.h"
#include "platform/platform_shell.h"

#include <filesystem>
#include <memory>
#include <vector>

namespace quiz_vulkan {

struct app_config {
    platform_shell_config shell;
    std::vector<std::filesystem::path> deck_artifacts;
    app_deck_source_config deck_sources;
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
