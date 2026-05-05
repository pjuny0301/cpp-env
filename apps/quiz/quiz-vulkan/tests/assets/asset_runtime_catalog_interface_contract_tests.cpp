#include "assets/asset_runtime_catalog.h"

#include <concepts>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace quiz_vulkan::assets::tests {
namespace {

static_assert(std::is_enum_v<runtime_asset_catalog_lookup_status>);
static_assert(std::is_enum_v<runtime_materialized_asset_lookup_status>);

static_assert(requires(runtime_asset_catalog_snapshot snapshot) {
    { snapshot.entry } -> std::same_as<asset_manifest_entry&>;
    { snapshot.source } -> std::same_as<resolved_asset_source&>;
    { snapshot.cache_key } -> std::same_as<asset_cache_key&>;
    { snapshot.resolved_root_id } -> std::same_as<std::string&>;
    { snapshot.rooted_path } -> std::same_as<std::optional<std::filesystem::path>&>;
});

static_assert(requires(runtime_asset_catalog_lookup_result result) {
    { result.status } -> std::same_as<runtime_asset_catalog_lookup_status&>;
    { result.asset } -> std::same_as<runtime_asset_catalog_snapshot&>;
    { result.diagnostic } -> std::same_as<std::string&>;
    { result.ok() } -> std::same_as<bool>;
});

static_assert(requires(runtime_materialized_asset_snapshot snapshot) {
    { snapshot.asset } -> std::same_as<runtime_asset_catalog_snapshot&>;
    { snapshot.cache_key_policy } -> std::same_as<asset_cache_key_classification&>;
    { snapshot.local_path } -> std::same_as<std::filesystem::path&>;
    { snapshot.source_path } -> std::same_as<std::string&>;
});

static_assert(requires(runtime_materialized_asset_lookup_request request) {
    { request.id } -> std::same_as<std::string&>;
    { request.expected_type } -> std::same_as<asset_type&>;
});

static_assert(requires(runtime_materialized_asset_lookup_result result) {
    { result.status } -> std::same_as<runtime_materialized_asset_lookup_status&>;
    { result.materialized } -> std::same_as<runtime_materialized_asset_snapshot&>;
    { result.diagnostic } -> std::same_as<std::string&>;
    { result.ok() } -> std::same_as<bool>;
});

static_assert(requires(runtime_asset_catalog_diagnostic diagnostic) {
    { diagnostic.kind } -> std::same_as<asset_manifest_validation_issue_kind&>;
    { diagnostic.id } -> std::same_as<std::string&>;
    { diagnostic.related_id } -> std::same_as<std::string&>;
    { diagnostic.cache_key } -> std::same_as<asset_cache_key&>;
    { diagnostic.diagnostic } -> std::same_as<std::string&>;
});

static_assert(requires(const runtime_asset_catalog& catalog, std::string_view id, asset_type type) {
    { catalog.assets } -> std::same_as<const std::vector<runtime_asset_catalog_snapshot>&>;
    { catalog.diagnostics } -> std::same_as<const std::vector<runtime_asset_catalog_diagnostic>&>;
    { catalog.ok() } -> std::same_as<bool>;
    { catalog.find(id) } -> std::same_as<const runtime_asset_catalog_snapshot*>;
    { catalog.find_diagnostic(id) } -> std::same_as<const runtime_asset_catalog_diagnostic*>;
    { catalog.lookup(id, type) } -> std::same_as<runtime_asset_catalog_lookup_result>;
    { catalog.lookup_font(id) } -> std::same_as<runtime_asset_catalog_lookup_result>;
    { catalog.lookup_image(id) } -> std::same_as<runtime_asset_catalog_lookup_result>;
    { catalog.lookup_sound(id) } -> std::same_as<runtime_asset_catalog_lookup_result>;
    { catalog.lookup_shader(id) } -> std::same_as<runtime_asset_catalog_lookup_result>;
    { catalog.lookup_deck(id) } -> std::same_as<runtime_asset_catalog_lookup_result>;
});

static_assert(requires(const asset_manifest& manifest, const asset_resolver_interface& resolver) {
    { build_runtime_asset_catalog(manifest, resolver) } -> std::same_as<runtime_asset_catalog>;
});

static_assert(requires(
    const runtime_asset_catalog& catalog,
    const runtime_asset_catalog_snapshot& snapshot,
    const runtime_materialized_asset_lookup_request& request) {
    { materialize_runtime_asset(snapshot) } -> std::same_as<runtime_materialized_asset_lookup_result>;
    { lookup_runtime_materialized_asset(catalog, request) } -> std::same_as<runtime_materialized_asset_lookup_result>;
});

} // namespace
} // namespace quiz_vulkan::assets::tests
