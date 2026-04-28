#include "assets/deck_source_asset_adapter.h"
#include "core/domain/deck_source.h"

#include <concepts>
#include <filesystem>

namespace {

using namespace quiz_vulkan;

template <typename T>
concept AssetDeckSourceProviderInterface = requires(
    T& provider,
    const domain::deck_source_request& request) {
    { provider.load_decks(request) } -> std::same_as<domain::deck_source_result>;
};

static_assert(AssetDeckSourceProviderInterface<assets::asset_deck_source_provider>);

static_assert(requires(assets::deck_source_asset_adapter_config config) {
    { config.local_root } -> std::same_as<std::filesystem::path&>;
    { config.asset_root } -> std::same_as<std::filesystem::path&>;
});

} // namespace
