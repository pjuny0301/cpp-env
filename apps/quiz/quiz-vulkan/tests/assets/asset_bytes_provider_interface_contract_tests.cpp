#include "assets/asset_bytes_provider.h"

#include <concepts>
#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace quiz_vulkan::assets::tests {
namespace {

static_assert(std::is_enum_v<asset_bytes_load_status>);

static_assert(requires(asset_bytes_snapshot_request request) {
    { request.snapshot } -> std::same_as<runtime_asset_catalog_snapshot&>;
});

static_assert(requires(asset_bytes_catalog_request request) {
    { request.id } -> std::same_as<std::string&>;
    { request.expected_type } -> std::same_as<asset_type&>;
});

static_assert(requires(asset_materialized_bytes_request request) {
    { request.materialized } -> std::same_as<runtime_materialized_asset_lookup_result&>;
});

static_assert(requires(asset_bytes_load_result result) {
    { result.status } -> std::same_as<asset_bytes_load_status&>;
    { result.bytes } -> std::same_as<std::vector<std::byte>&>;
    { result.byte_count } -> std::same_as<std::size_t&>;
    { result.cache_key } -> std::same_as<asset_cache_key&>;
    { result.source_uri } -> std::same_as<std::string&>;
    { result.diagnostic } -> std::same_as<std::string&>;
    { result.ok() } -> std::same_as<bool>;
});

static_assert(std::has_virtual_destructor_v<asset_bytes_provider_interface>);
static_assert(std::derived_from<fake_asset_bytes_provider, asset_bytes_provider_interface>);
static_assert(std::derived_from<local_file_asset_bytes_provider, asset_bytes_provider_interface>);

static_assert(requires(const asset_bytes_provider_interface& provider, const asset_bytes_snapshot_request& request) {
    { provider.load_bytes(request) } -> std::same_as<asset_bytes_load_result>;
});

static_assert(requires(fake_asset_bytes_record record) {
    { record.cache_key } -> std::same_as<asset_cache_key&>;
    { record.bytes } -> std::same_as<std::vector<std::byte>&>;
});

static_assert(requires(
    fake_asset_bytes_provider provider,
    asset_cache_key cache_key,
    std::vector<std::byte> bytes,
    std::string_view text,
    const asset_bytes_snapshot_request& request) {
    { provider.set_bytes(cache_key, bytes) } -> std::same_as<void>;
    { provider.set_text(cache_key, text) } -> std::same_as<void>;
    { provider.records() } -> std::same_as<const std::vector<fake_asset_bytes_record>&>;
    { provider.load_bytes(request) } -> std::same_as<asset_bytes_load_result>;
});

static_assert(requires(
    const asset_bytes_provider_interface& provider,
    const runtime_asset_catalog_snapshot& snapshot,
    const runtime_materialized_asset_lookup_result& materialized,
    const asset_materialized_bytes_request& materialized_request,
    const runtime_asset_catalog& catalog,
    const asset_bytes_catalog_request& request) {
    { load_asset_bytes(provider, snapshot) } -> std::same_as<asset_bytes_load_result>;
    { load_asset_bytes(provider, catalog, request) } -> std::same_as<asset_bytes_load_result>;
    { load_materialized_asset_bytes(provider, materialized_request) } -> std::same_as<asset_bytes_load_result>;
    { load_materialized_asset_bytes(provider, materialized) } -> std::same_as<asset_bytes_load_result>;
    { load_materialized_asset_bytes(provider, catalog, request) } -> std::same_as<asset_bytes_load_result>;
});

} // namespace
} // namespace quiz_vulkan::assets::tests
