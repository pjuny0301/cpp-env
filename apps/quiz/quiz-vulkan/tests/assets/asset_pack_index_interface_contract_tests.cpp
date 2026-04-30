#include "assets/asset_pack_index.h"

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

static_assert(std::is_enum_v<asset_pack_manifest_version_status>);
static_assert(std::is_enum_v<asset_pack_index_lookup_status>);

static_assert(requires(asset_pack_index_request request) {
    { request.manifest_version } -> std::same_as<std::string&>;
    { request.expected_manifest_version } -> std::same_as<std::string&>;
    { request.require_manifest_version } -> std::same_as<bool&>;
    { request.required_roots } -> std::same_as<std::vector<asset_pack_required_root>&>;
});

static_assert(requires(asset_pack_manifest_version_diagnostic diagnostic) {
    { diagnostic.status } -> std::same_as<asset_pack_manifest_version_status&>;
    { diagnostic.manifest_version } -> std::same_as<std::string&>;
    { diagnostic.expected_manifest_version } -> std::same_as<std::string&>;
    { diagnostic.diagnostic } -> std::same_as<std::string&>;
    { diagnostic.ok() } -> std::same_as<bool>;
});

static_assert(requires(asset_pack_index_entry entry) {
    { entry.manifest_index } -> std::same_as<std::size_t&>;
    { entry.id } -> std::same_as<std::string&>;
    { entry.type } -> std::same_as<asset_type&>;
    { entry.source_kind } -> std::same_as<asset_source_kind&>;
    { entry.root_space } -> std::same_as<asset_runtime_resolver_root_space&>;
    { entry.source_uri } -> std::same_as<std::string&>;
    { entry.resolved_root_id } -> std::same_as<std::string&>;
    { entry.cache_key } -> std::same_as<asset_cache_key&>;
    { entry.rooted_path } -> std::same_as<std::optional<std::filesystem::path>&>;
});

static_assert(requires(asset_pack_index_cache_key_group group) {
    { group.cache_key } -> std::same_as<asset_cache_key&>;
    { group.type } -> std::same_as<asset_type&>;
    { group.entry_ids } -> std::same_as<std::vector<std::string>&>;
    { group.duplicate() } -> std::same_as<bool>;
});

static_assert(requires(asset_pack_index_invalid_summary summary) {
    { summary.valid } -> std::same_as<bool&>;
    { summary.manifest_version_issue_count } -> std::same_as<std::size_t&>;
    { summary.resolver_diagnostic_count } -> std::same_as<std::size_t&>;
    { summary.validation_issue_count } -> std::same_as<std::size_t&>;
    { summary.missing_required_root_count } -> std::same_as<std::size_t&>;
    { summary.invalid_required_root_count } -> std::same_as<std::size_t&>;
    { summary.duplicate_cache_key_count } -> std::same_as<std::size_t&>;
    { summary.missing_local_fixture_file_count } -> std::same_as<std::size_t&>;
    { summary.build_external_placement_count } -> std::same_as<std::size_t&>;
    { summary.ok() } -> std::same_as<bool>;
});

static_assert(requires(asset_pack_index_lookup_result result) {
    { result.status } -> std::same_as<asset_pack_index_lookup_status&>;
    { result.entry } -> std::same_as<asset_pack_index_entry&>;
    { result.diagnostic } -> std::same_as<std::string&>;
    { result.ok() } -> std::same_as<bool>;
});

static_assert(requires(asset_pack_index_lookup_request request) {
    { request.id } -> std::same_as<std::string&>;
    { request.expected_type } -> std::same_as<asset_type&>;
});

static_assert(requires(asset_pack_index_lookup_diagnostic diagnostic) {
    { diagnostic.request_index } -> std::same_as<std::size_t&>;
    { diagnostic.status } -> std::same_as<asset_pack_index_lookup_status&>;
    { diagnostic.id } -> std::same_as<std::string&>;
    { diagnostic.expected_type } -> std::same_as<asset_type&>;
    { diagnostic.entry } -> std::same_as<asset_pack_index_entry&>;
    { diagnostic.diagnostic } -> std::same_as<std::string&>;
    { diagnostic.ok() } -> std::same_as<bool>;
});

static_assert(requires(asset_pack_index_lookup_report report) {
    { report.requests } -> std::same_as<std::vector<asset_pack_index_lookup_request>&>;
    { report.diagnostics } -> std::same_as<std::vector<asset_pack_index_lookup_diagnostic>&>;
    { report.cache_key_groups } -> std::same_as<std::vector<asset_pack_index_cache_key_group>&>;
    { report.found_count } -> std::same_as<std::size_t&>;
    { report.missing_id_count } -> std::same_as<std::size_t&>;
    { report.type_mismatch_count } -> std::same_as<std::size_t&>;
    { report.ok() } -> std::same_as<bool>;
});

static_assert(requires(asset_pack_index_catalog_snapshot_view view) {
    { view.entries } -> std::same_as<std::vector<asset_pack_index_entry>&>;
    { view.cache_key_groups } -> std::same_as<std::vector<asset_pack_index_cache_key_group>&>;
});

static_assert(requires(
    asset_pack_index_catalog catalog,
    const asset_pack_index_catalog& const_catalog,
    std::string_view id,
    std::string_view cache_key,
    asset_type type) {
    { catalog.validation } -> std::same_as<asset_pack_validation_report&>;
    { catalog.manifest_version } -> std::same_as<asset_pack_manifest_version_diagnostic&>;
    { catalog.entries } -> std::same_as<std::vector<asset_pack_index_entry>&>;
    { catalog.cache_key_groups } -> std::same_as<std::vector<asset_pack_index_cache_key_group>&>;
    { catalog.invalid_summary } -> std::same_as<asset_pack_index_invalid_summary&>;
    { const_catalog.ok() } -> std::same_as<bool>;
    { const_catalog.find(id) } -> std::same_as<const asset_pack_index_entry*>;
    { const_catalog.find_cache_key_group(cache_key) } -> std::same_as<const asset_pack_index_cache_key_group*>;
    { const_catalog.entries_for_type(type) } -> std::same_as<std::vector<asset_pack_index_entry>>;
    { const_catalog.lookup(id, type) } -> std::same_as<asset_pack_index_lookup_result>;
    { const_catalog.lookup_font(id) } -> std::same_as<asset_pack_index_lookup_result>;
    { const_catalog.lookup_image(id) } -> std::same_as<asset_pack_index_lookup_result>;
    { const_catalog.lookup_sound(id) } -> std::same_as<asset_pack_index_lookup_result>;
    { const_catalog.lookup_shader(id) } -> std::same_as<asset_pack_index_lookup_result>;
    { const_catalog.lookup_deck(id) } -> std::same_as<asset_pack_index_lookup_result>;
});

static_assert(requires(
    const asset_manifest& manifest,
    const asset_resolver_interface& resolver,
    const asset_pack_index_request& request,
    std::string_view version,
    std::string_view expected) {
    { validate_asset_pack_manifest_version(version, expected, true) } ->
        std::same_as<asset_pack_manifest_version_diagnostic>;
    { build_asset_pack_index(manifest, resolver, request) } -> std::same_as<asset_pack_index_catalog>;
    { sorted_asset_pack_index_entries(build_asset_pack_index(manifest, resolver, request)) } ->
        std::same_as<std::vector<asset_pack_index_entry>>;
    { sorted_asset_pack_index_cache_key_groups(build_asset_pack_index(manifest, resolver, request)) } ->
        std::same_as<std::vector<asset_pack_index_cache_key_group>>;
    { make_asset_pack_index_catalog_snapshot_view(build_asset_pack_index(manifest, resolver, request)) } ->
        std::same_as<asset_pack_index_catalog_snapshot_view>;
    { summarize_asset_pack_index_lookup_requests(
        build_asset_pack_index(manifest, resolver, request),
        std::vector<asset_pack_index_lookup_request>{}) } -> std::same_as<asset_pack_index_lookup_report>;
});

} // namespace
} // namespace quiz_vulkan::assets::tests
