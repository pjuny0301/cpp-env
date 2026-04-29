#include "assets/asset_manifest.h"

#include <concepts>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace quiz_vulkan;

static_assert(requires(
    assets::asset_manifest_root root,
    const assets::asset_manifest_root& const_root,
    std::string_view id) {
    { root.id } -> std::same_as<std::string&>;
    { root.aliases } -> std::same_as<std::vector<std::string>&>;
    { root.root_path } -> std::same_as<std::filesystem::path&>;
    { const_root.aliases_valid() } -> std::same_as<bool>;
    { const_root.matches(id) } -> std::same_as<bool>;
    { const_root.valid() } -> std::same_as<bool>;
});

static_assert(requires(assets::asset_manifest_entry entry, const assets::asset_manifest_entry& const_entry) {
    { entry.id } -> std::same_as<std::string&>;
    { entry.type } -> std::same_as<assets::asset_type&>;
    { entry.uri } -> std::same_as<std::string&>;
    { entry.root_id } -> std::same_as<std::string&>;
    { entry.cache_revision } -> std::same_as<std::string&>;
    { const_entry.valid() } -> std::same_as<bool>;
});

static_assert(requires(
    assets::asset_manifest manifest,
    const assets::asset_manifest& const_manifest,
    std::string_view id) {
    { manifest.roots } -> std::same_as<std::vector<assets::asset_manifest_root>&>;
    { manifest.entries } -> std::same_as<std::vector<assets::asset_manifest_entry>&>;
    { const_manifest.find_root(id) } -> std::same_as<const assets::asset_manifest_root*>;
    { const_manifest.find_entry(id) } -> std::same_as<const assets::asset_manifest_entry*>;
});

static_assert(requires(assets::asset_manifest_resolve_request request) {
    { request.id } -> std::same_as<std::string&>;
    { request.expected_type } -> std::same_as<assets::asset_type&>;
});

static_assert(requires(assets::resolved_asset_manifest_entry entry) {
    { entry.entry } -> std::same_as<assets::asset_manifest_entry&>;
    { entry.source } -> std::same_as<assets::resolved_asset_source&>;
    { entry.cache_key } -> std::same_as<assets::asset_cache_key&>;
    { entry.rooted_path } -> std::same_as<std::optional<std::filesystem::path>&>;
});

static_assert(requires(
    assets::asset_manifest_resolve_result result,
    const assets::asset_manifest_resolve_result& const_result) {
    { result.status } -> std::same_as<assets::asset_manifest_resolve_status&>;
    { result.asset } -> std::same_as<assets::resolved_asset_manifest_entry&>;
    { result.diagnostic } -> std::same_as<std::string&>;
    { const_result.ok() } -> std::same_as<bool>;
});

static_assert(requires(assets::asset_manifest_validation_issue issue) {
    { issue.kind } -> std::same_as<assets::asset_manifest_validation_issue_kind&>;
    { issue.id } -> std::same_as<std::string&>;
    { issue.related_id } -> std::same_as<std::string&>;
    { issue.cache_key } -> std::same_as<assets::asset_cache_key&>;
    { issue.diagnostic } -> std::same_as<std::string&>;
});

static_assert(requires(
    assets::asset_manifest_validation_result result,
    const assets::asset_manifest_validation_result& const_result) {
    { result.issues } -> std::same_as<std::vector<assets::asset_manifest_validation_issue>&>;
    { const_result.ok() } -> std::same_as<bool>;
});

static_assert(requires(assets::asset_manifest_parse_issue issue) {
    { issue.kind } -> std::same_as<assets::asset_manifest_parse_issue_kind&>;
    { issue.line } -> std::same_as<std::size_t&>;
    { issue.diagnostic } -> std::same_as<std::string&>;
});

static_assert(requires(
    assets::asset_manifest_parse_result result,
    const assets::asset_manifest_parse_result& const_result) {
    { result.manifest } -> std::same_as<assets::asset_manifest&>;
    { result.issues } -> std::same_as<std::vector<assets::asset_manifest_parse_issue>&>;
    { const_result.ok() } -> std::same_as<bool>;
});

static_assert(requires(
    const assets::asset_manifest& manifest,
    const assets::asset_manifest_resolve_request& request,
    const assets::asset_resolver_interface& resolver,
    const assets::asset_manifest_entry& entry,
    const assets::resolved_asset_source& source,
    std::filesystem::path root,
    std::string_view relative_path,
    std::string_view normalized_uri) {
    { assets::make_manifest_asset_cache_key(entry, source) } -> std::same_as<assets::asset_cache_key>;
    { assets::asset_manifest_path_is_within_root(root, root) } -> std::same_as<bool>;
    { assets::asset_manifest_asset_path_from_uri(normalized_uri) } -> std::same_as<std::string_view>;
    { assets::asset_manifest_root_relative_path(source) } -> std::same_as<std::string_view>;
    { assets::make_manifest_rooted_path(root, relative_path) } -> std::same_as<std::optional<std::filesystem::path>>;
    { assets::format_asset_manifest(manifest) } -> std::same_as<std::string>;
    { assets::parse_asset_manifest(normalized_uri) } -> std::same_as<assets::asset_manifest_parse_result>;
    { assets::load_asset_manifest_file(root) } -> std::same_as<assets::asset_manifest_parse_result>;
    { assets::resolve_asset_manifest_entry(manifest, request, resolver) } ->
        std::same_as<assets::asset_manifest_resolve_result>;
    { assets::validate_asset_manifest(manifest, resolver) } -> std::same_as<assets::asset_manifest_validation_result>;
});

static_assert(requires(assets::asset_manifest_normalized_entry entry) {
    { entry.entry } -> std::same_as<assets::asset_manifest_entry&>;
    { entry.source } -> std::same_as<assets::resolved_asset_source&>;
    { entry.cache_key } -> std::same_as<assets::asset_cache_key&>;
    { entry.resolved_root_id } -> std::same_as<std::string&>;
    { entry.rooted_path } -> std::same_as<std::optional<std::filesystem::path>&>;
});

static_assert(requires(
    assets::asset_manifest_normalization_result result,
    const assets::asset_manifest_normalization_result& const_result,
    std::string_view id,
    std::string_view cache_key) {
    { result.entries } -> std::same_as<std::vector<assets::asset_manifest_normalized_entry>&>;
    { result.validation } -> std::same_as<assets::asset_manifest_validation_result&>;
    { const_result.ok() } -> std::same_as<bool>;
    { const_result.find_entry(id) } -> std::same_as<const assets::asset_manifest_normalized_entry*>;
    { const_result.find_cache_key(cache_key) } -> std::same_as<const assets::asset_manifest_normalized_entry*>;
});

static_assert(requires(const assets::asset_manifest& manifest, const assets::asset_resolver_interface& resolver) {
    { assets::normalize_asset_manifest(manifest, resolver) } ->
        std::same_as<assets::asset_manifest_normalization_result>;
});

static_assert(requires(assets::asset_manifest_catalog_root_summary root) {
    { root.root_id } -> std::same_as<std::string&>;
    { root.entry_ids } -> std::same_as<std::vector<std::string>&>;
    { root.cache_keys } -> std::same_as<std::vector<assets::asset_cache_key>&>;
});

static_assert(requires(assets::asset_manifest_catalog_cache_key_summary cache_key) {
    { cache_key.cache_key } -> std::same_as<assets::asset_cache_key&>;
    { cache_key.entry_ids } -> std::same_as<std::vector<std::string>&>;
    { cache_key.root_ids } -> std::same_as<std::vector<std::string>&>;
});

static_assert(requires(
    assets::asset_manifest_catalog_type_summary type,
    const assets::asset_manifest_catalog_type_summary& const_type,
    std::string_view id,
    std::string_view cache_key) {
    { type.type } -> std::same_as<assets::asset_type&>;
    { type.entry_ids } -> std::same_as<std::vector<std::string>&>;
    { type.roots } -> std::same_as<std::vector<assets::asset_manifest_catalog_root_summary>&>;
    { type.cache_keys } -> std::same_as<std::vector<assets::asset_manifest_catalog_cache_key_summary>&>;
    { const_type.find_root(id) } -> std::same_as<const assets::asset_manifest_catalog_root_summary*>;
    { const_type.find_cache_key(cache_key) } ->
        std::same_as<const assets::asset_manifest_catalog_cache_key_summary*>;
});

static_assert(requires(
    assets::asset_manifest_catalog_summary summary,
    const assets::asset_manifest_catalog_summary& const_summary,
    assets::asset_type type) {
    { summary.types } -> std::same_as<std::vector<assets::asset_manifest_catalog_type_summary>&>;
    { summary.validation } -> std::same_as<assets::asset_manifest_validation_result&>;
    { const_summary.ok() } -> std::same_as<bool>;
    { const_summary.find_type(type) } -> std::same_as<const assets::asset_manifest_catalog_type_summary*>;
});

static_assert(requires(
    const assets::asset_manifest& manifest,
    const assets::asset_resolver_interface& resolver,
    const assets::asset_manifest_normalization_result& normalized) {
    { assets::summarize_asset_manifest_catalog(normalized) } ->
        std::same_as<assets::asset_manifest_catalog_summary>;
    { assets::summarize_asset_manifest_catalog(manifest, resolver) } ->
        std::same_as<assets::asset_manifest_catalog_summary>;
});

static_assert(requires(assets::asset_manifest_missing_root_report report) {
    { report.entry_id } -> std::same_as<std::string&>;
    { report.root_id } -> std::same_as<std::string&>;
});

static_assert(requires(assets::asset_manifest_unsupported_scheme_report report) {
    { report.entry_id } -> std::same_as<std::string&>;
    { report.uri } -> std::same_as<std::string&>;
    { report.diagnostic } -> std::same_as<std::string&>;
});

static_assert(requires(assets::asset_manifest_cache_key_collision_report report) {
    { report.entry_id } -> std::same_as<std::string&>;
    { report.related_entry_id } -> std::same_as<std::string&>;
    { report.cache_key } -> std::same_as<assets::asset_cache_key&>;
});

static_assert(requires(
    assets::asset_manifest_diagnostic_report report,
    const assets::asset_manifest_diagnostic_report& const_report) {
    { report.missing_roots } -> std::same_as<std::vector<assets::asset_manifest_missing_root_report>&>;
    { report.unsupported_schemes } -> std::same_as<std::vector<assets::asset_manifest_unsupported_scheme_report>&>;
    { report.cache_key_collisions } ->
        std::same_as<std::vector<assets::asset_manifest_cache_key_collision_report>&>;
    { report.validation } -> std::same_as<assets::asset_manifest_validation_result&>;
    { const_report.ok() } -> std::same_as<bool>;
});

static_assert(requires(const assets::asset_manifest& manifest, const assets::asset_resolver_interface& resolver) {
    { assets::make_asset_manifest_diagnostic_report(manifest, resolver) } ->
        std::same_as<assets::asset_manifest_diagnostic_report>;
});

} // namespace
