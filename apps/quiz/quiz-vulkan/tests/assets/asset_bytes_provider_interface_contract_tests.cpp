#include "assets/asset_bytes_contract.h"
#include "assets/asset_bytes_provider.h"
#include "assets/asset_typed_materialized_bytes.h"

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

static_assert(requires(
    asset_materialized_bytes_cache_policy_entry entry,
    const asset_materialized_bytes_cache_policy_entry& const_entry) {
    { entry.id } -> std::same_as<std::string&>;
    { entry.expected_type } -> std::same_as<asset_type&>;
    { entry.materialized_status } -> std::same_as<runtime_materialized_asset_lookup_status&>;
    { entry.load_status } -> std::same_as<asset_bytes_load_status&>;
    { entry.cache_key } -> std::same_as<asset_cache_key&>;
    { entry.source_uri } -> std::same_as<std::string&>;
    { entry.materialized_source_path } -> std::same_as<std::string&>;
    { entry.materialized_path } -> std::same_as<std::filesystem::path&>;
    { entry.byte_count } -> std::same_as<std::size_t&>;
    { entry.content_hash } -> std::same_as<std::string&>;
    { entry.issues } -> std::same_as<std::vector<asset_bytes_integrity_issue>&>;
    { entry.diagnostic } -> std::same_as<std::string&>;
    { const_entry.ok() } -> std::same_as<bool>;
});

static_assert(requires(
    asset_materialized_bytes_cache_policy_summary summary,
    const asset_materialized_bytes_cache_policy_summary& const_summary,
    std::string_view id) {
    { summary.entries } -> std::same_as<std::vector<asset_materialized_bytes_cache_policy_entry>&>;
    { summary.request_count } -> std::same_as<std::size_t&>;
    { summary.loaded_count } -> std::same_as<std::size_t&>;
    { summary.failed_count } -> std::same_as<std::size_t&>;
    { summary.total_byte_count } -> std::same_as<std::size_t&>;
    { summary.load_failed_count } -> std::same_as<std::size_t&>;
    { summary.cache_key_mismatch_count } -> std::same_as<std::size_t&>;
    { summary.source_uri_mismatch_count } -> std::same_as<std::size_t&>;
    { summary.byte_count_mismatch_count } -> std::same_as<std::size_t&>;
    { summary.content_hash_mismatch_count } -> std::same_as<std::size_t&>;
    { summary.missing_content_count } -> std::same_as<std::size_t&>;
    { const_summary.ok() } -> std::same_as<bool>;
    { const_summary.find_entry(id) } -> std::same_as<const asset_materialized_bytes_cache_policy_entry*>;
});

static_assert(requires(
    asset_typed_materialized_bytes_entry entry,
    const asset_typed_materialized_bytes_entry& const_entry) {
    { entry.id } -> std::same_as<std::string&>;
    { entry.type } -> std::same_as<asset_type&>;
    { entry.cache_key } -> std::same_as<asset_cache_key&>;
    { entry.source_uri } -> std::same_as<std::string&>;
    { entry.rooted_path } -> std::same_as<std::optional<std::filesystem::path>&>;
    { entry.materialized_source_path } -> std::same_as<std::string&>;
    { entry.materialized_path } -> std::same_as<std::filesystem::path&>;
    { entry.byte_count } -> std::same_as<std::size_t&>;
    { entry.content_hash } -> std::same_as<std::string&>;
    { entry.materialized_status } -> std::same_as<runtime_materialized_asset_lookup_status&>;
    { entry.load_status } -> std::same_as<asset_bytes_load_status&>;
    { entry.issues } -> std::same_as<std::vector<asset_bytes_integrity_issue>&>;
    { entry.diagnostic } -> std::same_as<std::string&>;
    { const_entry.ok() } -> std::same_as<bool>;
});

static_assert(requires(
    asset_typed_materialized_bytes_summary summary,
    const asset_typed_materialized_bytes_summary& const_summary,
    std::string_view id,
    asset_type type) {
    { summary.cache_policy } -> std::same_as<asset_materialized_bytes_cache_policy_summary&>;
    { summary.fonts } -> std::same_as<std::vector<asset_typed_materialized_bytes_entry>&>;
    { summary.images } -> std::same_as<std::vector<asset_typed_materialized_bytes_entry>&>;
    { summary.sounds } -> std::same_as<std::vector<asset_typed_materialized_bytes_entry>&>;
    { summary.shaders } -> std::same_as<std::vector<asset_typed_materialized_bytes_entry>&>;
    { summary.decks } -> std::same_as<std::vector<asset_typed_materialized_bytes_entry>&>;
    { summary.skipped_generic_count } -> std::same_as<std::size_t&>;
    { const_summary.ok() } -> std::same_as<bool>;
    { const_summary.entry_count() } -> std::same_as<std::size_t>;
    { const_summary.entries_for_type(type) } ->
        std::same_as<const std::vector<asset_typed_materialized_bytes_entry>&>;
    { const_summary.find_entry(id) } -> std::same_as<const asset_typed_materialized_bytes_entry*>;
});

static_assert(std::is_enum_v<asset_typed_materialized_bytes_delta_kind>);

static_assert(requires(
    asset_typed_materialized_bytes_diff_entry entry,
    const asset_typed_materialized_bytes_diff_entry& const_entry) {
    { entry.kind } -> std::same_as<asset_typed_materialized_bytes_delta_kind&>;
    { entry.id } -> std::same_as<std::string&>;
    { entry.type } -> std::same_as<asset_type&>;
    { entry.before } -> std::same_as<std::optional<asset_typed_materialized_bytes_entry>&>;
    { entry.after } -> std::same_as<std::optional<asset_typed_materialized_bytes_entry>&>;
    { entry.type_changed } -> std::same_as<bool&>;
    { entry.cache_key_changed } -> std::same_as<bool&>;
    { entry.source_uri_changed } -> std::same_as<bool&>;
    { entry.materialized_path_changed } -> std::same_as<bool&>;
    { entry.content_hash_changed } -> std::same_as<bool&>;
    { entry.integrity_status_changed } -> std::same_as<bool&>;
    { const_entry.has_field_delta() } -> std::same_as<bool>;
});

static_assert(requires(
    asset_typed_materialized_bytes_diff_summary summary,
    const asset_typed_materialized_bytes_diff_summary& const_summary,
    std::string_view id) {
    { summary.added } -> std::same_as<std::vector<asset_typed_materialized_bytes_diff_entry>&>;
    { summary.removed } -> std::same_as<std::vector<asset_typed_materialized_bytes_diff_entry>&>;
    { summary.changed } -> std::same_as<std::vector<asset_typed_materialized_bytes_diff_entry>&>;
    { const_summary.empty() } -> std::same_as<bool>;
    { const_summary.change_count() } -> std::same_as<std::size_t>;
    { const_summary.find_added(id) } -> std::same_as<const asset_typed_materialized_bytes_diff_entry*>;
    { const_summary.find_removed(id) } -> std::same_as<const asset_typed_materialized_bytes_diff_entry*>;
    { const_summary.find_changed(id) } -> std::same_as<const asset_typed_materialized_bytes_diff_entry*>;
});

static_assert(std::is_enum_v<asset_materialized_bytes_handoff_status>);

static_assert(requires(
    asset_materialized_bytes_handoff_payload payload,
    const asset_materialized_bytes_handoff_payload& const_payload) {
    { payload.id } -> std::same_as<std::string&>;
    { payload.type } -> std::same_as<asset_type&>;
    { payload.cache_key } -> std::same_as<asset_cache_key&>;
    { payload.source_uri } -> std::same_as<std::string&>;
    { payload.materialized_path } -> std::same_as<std::filesystem::path&>;
    { payload.byte_count } -> std::same_as<std::size_t&>;
    { payload.content_hash } -> std::same_as<std::string&>;
    { payload.status } -> std::same_as<asset_materialized_bytes_handoff_status&>;
    { payload.materialized_status } -> std::same_as<runtime_materialized_asset_lookup_status&>;
    { payload.load_status } -> std::same_as<asset_bytes_load_status&>;
    { payload.issues } -> std::same_as<std::vector<asset_bytes_integrity_issue>&>;
    { payload.diagnostic } -> std::same_as<std::string&>;
    { const_payload.ready() } -> std::same_as<bool>;
});

static_assert(requires(
    asset_materialized_bytes_handoff_group group,
    const asset_materialized_bytes_handoff_group& const_group,
    std::string_view id) {
    { group.ready } -> std::same_as<std::vector<asset_materialized_bytes_handoff_payload>&>;
    { group.blocked } -> std::same_as<std::vector<asset_materialized_bytes_handoff_payload>&>;
    { const_group.payload_count() } -> std::same_as<std::size_t>;
    { const_group.ok() } -> std::same_as<bool>;
    { const_group.find_ready(id) } -> std::same_as<const asset_materialized_bytes_handoff_payload*>;
    { const_group.find_blocked(id) } -> std::same_as<const asset_materialized_bytes_handoff_payload*>;
});

static_assert(requires(
    asset_materialized_bytes_handoff_summary summary,
    const asset_materialized_bytes_handoff_summary& const_summary,
    std::string_view id,
    asset_type type) {
    { summary.fonts } -> std::same_as<asset_materialized_bytes_handoff_group&>;
    { summary.images } -> std::same_as<asset_materialized_bytes_handoff_group&>;
    { summary.sounds } -> std::same_as<asset_materialized_bytes_handoff_group&>;
    { summary.shaders } -> std::same_as<asset_materialized_bytes_handoff_group&>;
    { summary.decks } -> std::same_as<asset_materialized_bytes_handoff_group&>;
    { summary.skipped_generic_count } -> std::same_as<std::size_t&>;
    { const_summary.ok() } -> std::same_as<bool>;
    { const_summary.ready_count() } -> std::same_as<std::size_t>;
    { const_summary.blocked_count() } -> std::same_as<std::size_t>;
    { const_summary.payload_count() } -> std::same_as<std::size_t>;
    { const_summary.group_for_type(type) } -> std::same_as<const asset_materialized_bytes_handoff_group&>;
    { const_summary.find_ready(id) } -> std::same_as<const asset_materialized_bytes_handoff_payload*>;
    { const_summary.find_blocked(id) } -> std::same_as<const asset_materialized_bytes_handoff_payload*>;
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
    const asset_typed_materialized_bytes_summary& typed_summary,
    const std::vector<asset_bytes_catalog_request>& requests,
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
    { summarize_materialized_asset_bytes_cache_policy(provider, catalog, requests) } ->
        std::same_as<asset_materialized_bytes_cache_policy_summary>;
    { summarize_typed_materialized_asset_bytes(provider, catalog) } ->
        std::same_as<asset_typed_materialized_bytes_summary>;
    { diff_typed_materialized_asset_bytes(typed_summary, typed_summary) } ->
        std::same_as<asset_typed_materialized_bytes_diff_summary>;
    { make_materialized_asset_bytes_handoff_summary(typed_summary) } ->
        std::same_as<asset_materialized_bytes_handoff_summary>;
});

} // namespace
} // namespace quiz_vulkan::assets::tests
