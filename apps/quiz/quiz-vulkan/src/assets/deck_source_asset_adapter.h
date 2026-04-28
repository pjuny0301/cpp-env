#pragma once

#include "assets/asset_resolver.h"
#include "core/domain/deck_source.h"

#include <filesystem>

namespace quiz_vulkan::assets {

struct deck_source_asset_adapter_config {
    std::filesystem::path local_root;
    std::filesystem::path asset_root;
};

class asset_deck_source_provider final : public domain::deck_source_provider_interface {
public:
    asset_deck_source_provider(
        const asset_resolver_interface& resolver,
        deck_source_asset_adapter_config config);

    domain::deck_source_result load_decks(const domain::deck_source_request& request) override;

private:
    const asset_resolver_interface& resolver_;
    deck_source_asset_adapter_config config_;
};

} // namespace quiz_vulkan::assets
