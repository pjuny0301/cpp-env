#pragma once

#include "core/domain/deck_source.h"

#include <filesystem>
#include <string>
#include <vector>

namespace quiz_vulkan {

struct app_deck_source_config {
    std::vector<domain::deck_source_request> requests;
    std::filesystem::path local_root;
    std::filesystem::path asset_root;
};

struct app_deck_load_request {
    std::vector<std::filesystem::path> deck_artifacts;
    std::vector<domain::deck_source_request> deck_sources;
    domain::deck_source_provider_interface* deck_source_provider = nullptr;
};

struct app_deck_load_result {
    std::vector<domain::deck> decks;
    std::vector<std::string> messages;
};

[[nodiscard]] app_deck_load_result load_app_decks(const app_deck_load_request& request);

} // namespace quiz_vulkan
