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
    { result.content_hash } -> std::same_as<std::string&>;
    { result.cache_key } -> std::same_as<asset_cache_key&>;
    { result.source_uri } -> std::same_as<std::string&>;
    { result.diagnostic } -> std::same_as<std::string&>;
    { result.ok() } -> std::same_as<bool>;
});

static_assert(std::is_enum_v<asset_bytes_integrity_issue_kind>);

static_assert(requires(asset_bytes_integrity_issue issue) {
    { issue.kind } -> std::same_as<asset_bytes_integrity_issue_kind&>;
    { issue.id } -> std::same_as<std::string&>;
    { issue.type } -> std::same_as<asset_type&>;
    { issue.cache_key } -> std::same_as<asset_cache_key&>;
    { issue.expected_cache_key } -> std::same_as<asset_cache_key&>;
    { issue.source_uri } -> std::same_as<std::string&>;
    { issue.expected_source_uri } -> std::same_as<std::string&>;
    { issue.reported_byte_count } -> std::same_as<std::size_t&>;
    { issue.actual_byte_count } -> std::same_as<std::size_t&>;
    { issue.reported_content_hash } -> std::same_as<std::string&>;
    { issue.actual_content_hash } -> std::same_as<std::string&>;
    { issue.diagnostic } -> std::same_as<std::string&>;
});

static_assert(requires(asset_bytes_integrity_request request) {
    { request.snapshot } -> std::same_as<runtime_asset_catalog_snapshot&>;
    { request.load } -> std::same_as<asset_bytes_load_result&>;
    { request.require_non_empty } -> std::same_as<bool&>;
});

static_assert(requires(asset_bytes_integrity_report report, const asset_bytes_integrity_report& const_report) {
    { report.load } -> std::same_as<asset_bytes_load_result&>;
    { report.issues } -> std::same_as<std::vector<asset_bytes_integrity_issue>&>;
    { const_report.ok() } -> std::same_as<bool>;
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
    const std::vector<std::byte>& bytes,
    const runtime_asset_catalog_snapshot& snapshot,
    const runtime_materialized_asset_lookup_result& materialized,
    const asset_materialized_bytes_request& materialized_request,
    const runtime_asset_catalog& catalog,
    const asset_bytes_integrity_request& integrity_request,
    const asset_bytes_catalog_request& request) {
    { make_asset_bytes_content_hash(bytes) } -> std::same_as<std::string>;
    { validate_asset_bytes_integrity(integrity_request) } -> std::same_as<asset_bytes_integrity_report>;
    { load_asset_bytes(provider, snapshot) } -> std::same_as<asset_bytes_load_result>;
    { load_asset_bytes_with_integrity(provider, snapshot) } -> std::same_as<asset_bytes_integrity_report>;
    { load_asset_bytes(provider, catalog, request) } -> std::same_as<asset_bytes_load_result>;
    { load_materialized_asset_bytes(provider, materialized_request) } -> std::same_as<asset_bytes_load_result>;
    { load_materialized_asset_bytes(provider, materialized) } -> std::same_as<asset_bytes_load_result>;
    { load_materialized_asset_bytes(provider, catalog, request) } -> std::same_as<asset_bytes_load_result>;
    { load_materialized_asset_bytes_with_integrity(provider, materialized_request) } ->
        std::same_as<asset_bytes_integrity_report>;
    { load_materialized_asset_bytes_with_integrity(provider, materialized) } ->
        std::same_as<asset_bytes_integrity_report>;
    { load_materialized_asset_bytes_with_integrity(provider, catalog, request) } ->
        std::same_as<asset_bytes_integrity_report>;
});

} // namespace
} // namespace quiz_vulkan::assets::tests
