#include "assets/asset_path_policy.h"

#include <concepts>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace quiz_vulkan::assets::tests {
namespace {

static_assert(std::is_enum_v<asset_path_policy_status>);

static_assert(requires(asset_path_policy_rule rule) {
    { rule.type } -> std::same_as<asset_type&>;
    { rule.catalog_group } -> std::same_as<std::string_view&>;
});

static_assert(requires(asset_path_policy_snapshot snapshot) {
    { snapshot.id } -> std::same_as<std::string&>;
    { snapshot.type } -> std::same_as<asset_type&>;
    { snapshot.cache_key } -> std::same_as<asset_cache_key&>;
    { snapshot.source_uri } -> std::same_as<std::string&>;
    { snapshot.source_path } -> std::same_as<std::string&>;
    { snapshot.catalog_path } -> std::same_as<std::string&>;
    { snapshot.rooted_path } -> std::same_as<std::optional<std::filesystem::path>&>;
});

static_assert(requires(asset_path_policy_result result) {
    { result.status } -> std::same_as<asset_path_policy_status&>;
    { result.asset } -> std::same_as<asset_path_policy_snapshot&>;
    { result.diagnostic } -> std::same_as<std::string&>;
    { result.ok() } -> std::same_as<bool>;
});

static_assert(requires(asset_path_policy_diagnostic diagnostic) {
    { diagnostic.status } -> std::same_as<asset_path_policy_status&>;
    { diagnostic.id } -> std::same_as<std::string&>;
    { diagnostic.related_id } -> std::same_as<std::string&>;
    { diagnostic.type } -> std::same_as<asset_type&>;
    { diagnostic.cache_key } -> std::same_as<asset_cache_key&>;
    { diagnostic.catalog_path } -> std::same_as<std::string&>;
    { diagnostic.diagnostic } -> std::same_as<std::string&>;
});

static_assert(requires(asset_path_policy_kind_counts counts) {
    { counts.type } -> std::same_as<asset_type&>;
    { counts.accepted_count } -> std::same_as<std::size_t&>;
    { counts.rejected_count } -> std::same_as<std::size_t&>;
});

static_assert(requires(asset_path_policy_catalog_snapshot_view view) {
    { view.entries } -> std::same_as<std::vector<asset_path_policy_snapshot>&>;
    { view.diagnostics } -> std::same_as<std::vector<asset_path_policy_diagnostic>&>;
    { view.kind_counts } -> std::same_as<std::vector<asset_path_policy_kind_counts>&>;
});

static_assert(requires(asset_path_policy_duplicate_source_report report) {
    { report.type } -> std::same_as<asset_type&>;
    { report.source_uri } -> std::same_as<std::string&>;
    { report.entry_ids } -> std::same_as<std::vector<std::string>&>;
});

static_assert(requires(asset_path_policy_duplicate_catalog_path_report report) {
    { report.type } -> std::same_as<asset_type&>;
    { report.catalog_path } -> std::same_as<std::string&>;
    { report.entry_ids } -> std::same_as<std::vector<std::string>&>;
});

static_assert(requires(asset_path_policy_duplicate_cache_key_report report) {
    { report.type } -> std::same_as<asset_type&>;
    { report.cache_key } -> std::same_as<asset_cache_key&>;
    { report.entry_ids } -> std::same_as<std::vector<std::string>&>;
});

static_assert(requires(asset_manifest_path_policy_validation_summary summary) {
    { summary.policy_catalog } -> std::same_as<asset_path_policy_catalog&>;
    { summary.snapshot_view } -> std::same_as<asset_path_policy_catalog_snapshot_view&>;
    { summary.manifest_kind_counts } -> std::same_as<std::vector<asset_path_policy_kind_counts>&>;
    { summary.duplicate_sources } -> std::same_as<std::vector<asset_path_policy_duplicate_source_report>&>;
    { summary.duplicate_catalog_paths } ->
        std::same_as<std::vector<asset_path_policy_duplicate_catalog_path_report>&>;
    { summary.duplicate_cache_keys } -> std::same_as<std::vector<asset_path_policy_duplicate_cache_key_report>&>;
    { summary.ok() } -> std::same_as<bool>;
});

static_assert(requires(
    const asset_path_policy_catalog& catalog,
    std::string_view id,
    std::string_view cache_key,
    std::string_view catalog_path) {
    { catalog.entries } -> std::same_as<const std::vector<asset_path_policy_snapshot>&>;
    { catalog.diagnostics } -> std::same_as<const std::vector<asset_path_policy_diagnostic>&>;
    { catalog.ok() } -> std::same_as<bool>;
    { catalog.find(id) } -> std::same_as<const asset_path_policy_snapshot*>;
    { catalog.find_cache_key(cache_key) } -> std::same_as<const asset_path_policy_snapshot*>;
    { catalog.find_catalog_path(catalog_path) } -> std::same_as<const asset_path_policy_snapshot*>;
});

static_assert(requires(asset_type type, const runtime_asset_catalog_snapshot& snapshot, const runtime_asset_catalog& catalog) {
    { asset_path_policy_for_type(type) } -> std::same_as<asset_path_policy_rule>;
    { asset_path_policy_type_supported(type) } -> std::same_as<bool>;
    { apply_asset_path_policy(snapshot) } -> std::same_as<asset_path_policy_result>;
    { build_asset_path_policy_catalog(catalog) } -> std::same_as<asset_path_policy_catalog>;
    { sorted_asset_path_policy_entries(build_asset_path_policy_catalog(catalog)) } ->
        std::same_as<std::vector<asset_path_policy_snapshot>>;
    { sorted_asset_path_policy_diagnostics(build_asset_path_policy_catalog(catalog)) } ->
        std::same_as<std::vector<asset_path_policy_diagnostic>>;
    { summarize_asset_path_policy_kind_counts(build_asset_path_policy_catalog(catalog)) } ->
        std::same_as<std::vector<asset_path_policy_kind_counts>>;
    { make_asset_path_policy_catalog_snapshot_view(build_asset_path_policy_catalog(catalog)) } ->
        std::same_as<asset_path_policy_catalog_snapshot_view>;
    { report_asset_path_policy_duplicate_sources(build_asset_path_policy_catalog(catalog)) } ->
        std::same_as<std::vector<asset_path_policy_duplicate_source_report>>;
    { report_asset_path_policy_duplicate_catalog_paths(build_asset_path_policy_catalog(catalog)) } ->
        std::same_as<std::vector<asset_path_policy_duplicate_catalog_path_report>>;
    { report_asset_path_policy_duplicate_cache_keys(build_asset_path_policy_catalog(catalog)) } ->
        std::same_as<std::vector<asset_path_policy_duplicate_cache_key_report>>;
});

static_assert(requires(const asset_manifest& manifest, const asset_resolver_interface& resolver) {
    { summarize_asset_manifest_path_policy_kind_counts(manifest, asset_path_policy_catalog{}) } ->
        std::same_as<std::vector<asset_path_policy_kind_counts>>;
    { validate_asset_manifest_path_policy(manifest, resolver) } ->
        std::same_as<asset_manifest_path_policy_validation_summary>;
});

} // namespace
} // namespace quiz_vulkan::assets::tests
