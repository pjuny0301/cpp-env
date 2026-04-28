#include "assets/asset_resolver.h"

#include <concepts>
#include <string>
#include <string_view>

namespace {

using namespace quiz_vulkan;

template <typename T>
concept AssetResolverInterface = requires(
    const T& resolver,
    const assets::asset_resolve_request& request) {
    { resolver.resolve(request) } -> std::same_as<assets::asset_resolve_result>;
};

static_assert(AssetResolverInterface<assets::asset_resolver_interface>);
static_assert(AssetResolverInterface<assets::normalizing_asset_resolver>);

static_assert(requires(assets::resolved_asset_source source) {
    { source.original_uri } -> std::same_as<std::string&>;
    { source.normalized_uri } -> std::same_as<std::string&>;
    { source.cache_key() } -> std::same_as<assets::asset_cache_key>;
    { source.is_remote() } -> std::same_as<bool>;
    { source.is_packaged_asset() } -> std::same_as<bool>;
});

static_assert(requires(assets::asset_type type, std::string_view uri) {
    { assets::asset_type_token(type) } -> std::same_as<std::string_view>;
    { assets::make_asset_cache_key(type, uri) } -> std::same_as<assets::asset_cache_key>;
});

} // namespace
