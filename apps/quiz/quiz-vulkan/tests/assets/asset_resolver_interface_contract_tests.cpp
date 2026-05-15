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

static_assert(requires(assets::asset_cache_key_classification classification) {
    { classification.status } -> std::same_as<assets::asset_cache_key_policy_status&>;
    { classification.type } -> std::same_as<assets::asset_type&>;
    { classification.source_kind } -> std::same_as<assets::asset_source_kind&>;
    { classification.type_component } -> std::same_as<std::string&>;
    { classification.source_component } -> std::same_as<std::string&>;
    { classification.revision_component } -> std::same_as<std::string&>;
    { classification.normalized_uri } -> std::same_as<std::string&>;
    { classification.source_path } -> std::same_as<std::string&>;
    { classification.cache_revision } -> std::same_as<std::string&>;
    { classification.diagnostic } -> std::same_as<std::string&>;
    { classification.ok() } -> std::same_as<bool>;
    { classification.has_cache_revision() } -> std::same_as<bool>;
});

static_assert(requires(
    std::string_view token,
    std::string_view cache_key,
    assets::asset_type& type,
    const assets::resolved_asset_source& source) {
    { assets::parse_asset_type_token(token, type) } -> std::same_as<bool>;
    { assets::asset_cache_revision_is_valid(token) } -> std::same_as<bool>;
    { assets::asset_cache_key_source_path(source) } -> std::same_as<std::string>;
    { assets::classify_asset_cache_key(cache_key) } -> std::same_as<assets::asset_cache_key_classification>;
    { assets::asset_cache_key_is_canonical(cache_key) } -> std::same_as<bool>;
});

} // namespace
