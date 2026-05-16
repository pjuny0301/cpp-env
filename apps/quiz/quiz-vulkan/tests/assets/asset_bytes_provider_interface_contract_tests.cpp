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

static_assert(requires(
    asset_materialized_byte_payload payload,
    const asset_materialized_byte_payload& const_payload) {
    { payload.id } -> std::same_as<std::string&>;
    { payload.type } -> std::same_as<asset_type&>;
    { payload.cache_key } -> std::same_as<asset_cache_key&>;
    { payload.source_uri } -> std::same_as<std::string&>;
    { payload.rooted_path } -> std::same_as<std::optional<std::filesystem::path>&>;
    { payload.materialized_source_path } -> std::same_as<std::string&>;
    { payload.materialized_path } -> std::same_as<std::filesystem::path&>;
    { payload.byte_count } -> std::same_as<std::size_t&>;
    { payload.content_hash } -> std::same_as<std::string&>;
    { payload.bytes } -> std::same_as<std::vector<std::byte>&>;
    { payload.status } -> std::same_as<asset_materialized_bytes_handoff_status&>;
    { payload.materialized_status } -> std::same_as<runtime_materialized_asset_lookup_status&>;
    { payload.load_status } -> std::same_as<asset_bytes_load_status&>;
    { payload.issues } -> std::same_as<std::vector<asset_bytes_integrity_issue>&>;
    { payload.diagnostic } -> std::same_as<std::string&>;
    { const_payload.ready() } -> std::same_as<bool>;
});

static_assert(requires(
    asset_materialized_byte_payload_group group,
    const asset_materialized_byte_payload_group& const_group,
    std::string_view id) {
    { group.ready } -> std::same_as<std::vector<asset_materialized_byte_payload>&>;
    { group.blocked } -> std::same_as<std::vector<asset_materialized_byte_payload>&>;
    { const_group.payload_count() } -> std::same_as<std::size_t>;
    { const_group.ok() } -> std::same_as<bool>;
    { const_group.find_ready(id) } -> std::same_as<const asset_materialized_byte_payload*>;
    { const_group.find_blocked(id) } -> std::same_as<const asset_materialized_byte_payload*>;
});

static_assert(requires(
    asset_materialized_byte_payload_bundle bundle,
    const asset_materialized_byte_payload_bundle& const_bundle,
    std::string_view id,
    asset_type type) {
    { bundle.cache_policy } -> std::same_as<asset_materialized_bytes_cache_policy_summary&>;
    { bundle.handoff } -> std::same_as<asset_materialized_bytes_handoff_summary&>;
    { bundle.fonts } -> std::same_as<asset_materialized_byte_payload_group&>;
    { bundle.images } -> std::same_as<asset_materialized_byte_payload_group&>;
    { bundle.sounds } -> std::same_as<asset_materialized_byte_payload_group&>;
    { bundle.shaders } -> std::same_as<asset_materialized_byte_payload_group&>;
    { bundle.decks } -> std::same_as<asset_materialized_byte_payload_group&>;
    { bundle.skipped_generic_count } -> std::same_as<std::size_t&>;
    { const_bundle.ok() } -> std::same_as<bool>;
    { const_bundle.ready_count() } -> std::same_as<std::size_t>;
    { const_bundle.blocked_count() } -> std::same_as<std::size_t>;
    { const_bundle.payload_count() } -> std::same_as<std::size_t>;
    { const_bundle.group_for_type(type) } -> std::same_as<const asset_materialized_byte_payload_group&>;
    { const_bundle.find_ready(id) } -> std::same_as<const asset_materialized_byte_payload*>;
    { const_bundle.find_blocked(id) } -> std::same_as<const asset_materialized_byte_payload*>;
});

static_assert(requires(asset_materialized_byte_payload_snapshot snapshot) {
    { snapshot.id } -> std::same_as<std::string&>;
    { snapshot.type } -> std::same_as<asset_type&>;
    { snapshot.cache_key } -> std::same_as<asset_cache_key&>;
    { snapshot.source_uri } -> std::same_as<std::string&>;
    { snapshot.materialized_path } -> std::same_as<std::filesystem::path&>;
    { snapshot.byte_count } -> std::same_as<std::size_t&>;
    { snapshot.payload_byte_count } -> std::same_as<std::size_t&>;
    { snapshot.content_hash } -> std::same_as<std::string&>;
    { snapshot.status } -> std::same_as<asset_materialized_bytes_handoff_status&>;
    { snapshot.materialized_status } -> std::same_as<runtime_materialized_asset_lookup_status&>;
    { snapshot.load_status } -> std::same_as<asset_bytes_load_status&>;
    { snapshot.ready } -> std::same_as<bool&>;
});

static_assert(requires(
    asset_materialized_byte_payload_bundle_snapshot snapshot,
    const asset_materialized_byte_payload_bundle_snapshot& const_snapshot,
    std::string_view id) {
    { snapshot.payloads } -> std::same_as<std::vector<asset_materialized_byte_payload_snapshot>&>;
    { snapshot.skipped_generic_count } -> std::same_as<std::size_t&>;
    { const_snapshot.ok() } -> std::same_as<bool>;
    { const_snapshot.ready_count() } -> std::same_as<std::size_t>;
    { const_snapshot.blocked_count() } -> std::same_as<std::size_t>;
    { const_snapshot.payload_count() } -> std::same_as<std::size_t>;
    { const_snapshot.find_payload(id) } -> std::same_as<const asset_materialized_byte_payload_snapshot*>;
});

static_assert(std::is_enum_v<asset_materialized_byte_payload_delta_kind>);

static_assert(requires(
    asset_materialized_byte_payload_diff_entry entry,
    const asset_materialized_byte_payload_diff_entry& const_entry) {
    { entry.kind } -> std::same_as<asset_materialized_byte_payload_delta_kind&>;
    { entry.id } -> std::same_as<std::string&>;
    { entry.type } -> std::same_as<asset_type&>;
    { entry.before } -> std::same_as<std::optional<asset_materialized_byte_payload_snapshot>&>;
    { entry.after } -> std::same_as<std::optional<asset_materialized_byte_payload_snapshot>&>;
    { entry.type_changed } -> std::same_as<bool&>;
    { entry.cache_key_changed } -> std::same_as<bool&>;
    { entry.source_uri_changed } -> std::same_as<bool&>;
    { entry.materialized_path_changed } -> std::same_as<bool&>;
    { entry.byte_count_changed } -> std::same_as<bool&>;
    { entry.payload_byte_count_changed } -> std::same_as<bool&>;
    { entry.content_hash_changed } -> std::same_as<bool&>;
    { entry.status_changed } -> std::same_as<bool&>;
    { entry.readiness_changed } -> std::same_as<bool&>;
    { const_entry.has_field_delta() } -> std::same_as<bool>;
});

static_assert(requires(
    asset_materialized_byte_payload_diff_summary summary,
    const asset_materialized_byte_payload_diff_summary& const_summary,
    std::string_view id) {
    { summary.added } -> std::same_as<std::vector<asset_materialized_byte_payload_diff_entry>&>;
    { summary.removed } -> std::same_as<std::vector<asset_materialized_byte_payload_diff_entry>&>;
    { summary.changed } -> std::same_as<std::vector<asset_materialized_byte_payload_diff_entry>&>;
    { const_summary.empty() } -> std::same_as<bool>;
    { const_summary.change_count() } -> std::same_as<std::size_t>;
    { const_summary.find_added(id) } -> std::same_as<const asset_materialized_byte_payload_diff_entry*>;
    { const_summary.find_removed(id) } -> std::same_as<const asset_materialized_byte_payload_diff_entry*>;
    { const_summary.find_changed(id) } -> std::same_as<const asset_materialized_byte_payload_diff_entry*>;
});

static_assert(std::is_enum_v<asset_materialized_byte_payload_selection_status>);

static_assert(requires(asset_materialized_byte_payload_selection_request request) {
    { request.id } -> std::same_as<std::string&>;
    { request.expected_type } -> std::same_as<asset_type&>;
    { request.expected_cache_key } -> std::same_as<std::optional<asset_cache_key>&>;
    { request.require_ready } -> std::same_as<bool&>;
    { request.require_integrity_ok } -> std::same_as<bool&>;
});

static_assert(requires(
    asset_materialized_byte_payload_selection_result result,
    const asset_materialized_byte_payload_selection_result& const_result) {
    { result.status } -> std::same_as<asset_materialized_byte_payload_selection_status&>;
    { result.payload } -> std::same_as<const asset_materialized_byte_payload*&>;
    { result.snapshot } -> std::same_as<std::optional<asset_materialized_byte_payload_snapshot>&>;
    { result.id } -> std::same_as<std::string&>;
    { result.expected_type } -> std::same_as<asset_type&>;
    { result.expected_cache_key } -> std::same_as<std::optional<asset_cache_key>&>;
    { result.actual_type } -> std::same_as<asset_type&>;
    { result.actual_cache_key } -> std::same_as<asset_cache_key&>;
    { result.match_count } -> std::same_as<std::size_t&>;
    { result.diagnostic } -> std::same_as<std::string&>;
    { const_result.selected() } -> std::same_as<bool>;
});

static_assert(requires(asset_materialized_byte_payload_filter filter) {
    { filter.type } -> std::same_as<std::optional<asset_type>&>;
    { filter.id } -> std::same_as<std::optional<std::string>&>;
    { filter.cache_key } -> std::same_as<std::optional<asset_cache_key>&>;
    { filter.ready } -> std::same_as<std::optional<bool>&>;
    { filter.integrity_ok } -> std::same_as<std::optional<bool>&>;
});

static_assert(requires(
    asset_materialized_byte_payload_filter_result result,
    const asset_materialized_byte_payload_filter_result& const_result) {
    { result.payloads } -> std::same_as<std::vector<const asset_materialized_byte_payload*>&>;
    { result.snapshots } -> std::same_as<std::vector<asset_materialized_byte_payload_snapshot>&>;
    { const_result.empty() } -> std::same_as<bool>;
    { const_result.match_count() } -> std::same_as<std::size_t>;
    { const_result.first_payload() } -> std::same_as<const asset_materialized_byte_payload*>;
});

static_assert(requires(
    asset_materialized_byte_payload_request_transaction_item item,
    const asset_materialized_byte_payload_request_transaction_item& const_item) {
    { item.request_index } -> std::same_as<std::size_t&>;
    { item.request } -> std::same_as<asset_materialized_byte_payload_selection_request&>;
    { item.selection } -> std::same_as<asset_materialized_byte_payload_selection_result&>;
    { item.selected_snapshot } -> std::same_as<std::optional<asset_materialized_byte_payload_snapshot>&>;
    { const_item.selected() } -> std::same_as<bool>;
});

static_assert(requires(
    asset_materialized_byte_payload_request_transaction_summary summary,
    const asset_materialized_byte_payload_request_transaction_summary& const_summary) {
    { summary.request_count } -> std::same_as<std::size_t&>;
    { summary.selected_count } -> std::same_as<std::size_t&>;
    { summary.ready_count } -> std::same_as<std::size_t&>;
    { summary.blocked_count } -> std::same_as<std::size_t&>;
    { summary.missing_count } -> std::same_as<std::size_t&>;
    { summary.wrong_type_count } -> std::same_as<std::size_t&>;
    { summary.cache_key_mismatch_count } -> std::same_as<std::size_t&>;
    { summary.integrity_failure_count } -> std::same_as<std::size_t&>;
    { summary.duplicate_count } -> std::same_as<std::size_t&>;
    { const_summary.failed_count() } -> std::same_as<std::size_t>;
    { const_summary.ok() } -> std::same_as<bool>;
});

static_assert(requires(
    asset_materialized_byte_payload_request_transaction transaction,
    const asset_materialized_byte_payload_request_transaction& const_transaction,
    std::size_t index,
    std::string_view id) {
    { transaction.items } -> std::same_as<std::vector<asset_materialized_byte_payload_request_transaction_item>&>;
    { transaction.summary } -> std::same_as<asset_materialized_byte_payload_request_transaction_summary&>;
    { const_transaction.ok() } -> std::same_as<bool>;
    { const_transaction.request_count() } -> std::same_as<std::size_t>;
    { const_transaction.item_at(index) } ->
        std::same_as<const asset_materialized_byte_payload_request_transaction_item*>;
    { const_transaction.find_request(id) } ->
        std::same_as<const asset_materialized_byte_payload_request_transaction_item*>;
});

static_assert(requires(
    asset_materialized_byte_payload_request_transaction_count_delta delta,
    const asset_materialized_byte_payload_request_transaction_count_delta& const_delta) {
    { delta.request_delta } -> std::same_as<std::ptrdiff_t&>;
    { delta.selected_delta } -> std::same_as<std::ptrdiff_t&>;
    { delta.ready_delta } -> std::same_as<std::ptrdiff_t&>;
    { delta.blocked_delta } -> std::same_as<std::ptrdiff_t&>;
    { delta.missing_delta } -> std::same_as<std::ptrdiff_t&>;
    { delta.wrong_type_delta } -> std::same_as<std::ptrdiff_t&>;
    { delta.cache_key_mismatch_delta } -> std::same_as<std::ptrdiff_t&>;
    { delta.integrity_failure_delta } -> std::same_as<std::ptrdiff_t&>;
    { delta.duplicate_delta } -> std::same_as<std::ptrdiff_t&>;
    { delta.failed_delta } -> std::same_as<std::ptrdiff_t&>;
    { const_delta.empty() } -> std::same_as<bool>;
});

static_assert(std::is_enum_v<asset_materialized_byte_payload_request_transaction_delta_kind>);

static_assert(requires(
    asset_materialized_byte_payload_request_transaction_diff_entry entry,
    const asset_materialized_byte_payload_request_transaction_diff_entry& const_entry) {
    { entry.kind } -> std::same_as<asset_materialized_byte_payload_request_transaction_delta_kind&>;
    { entry.id } -> std::same_as<std::string&>;
    { entry.occurrence } -> std::same_as<std::size_t&>;
    { entry.before_index } -> std::same_as<std::optional<std::size_t>&>;
    { entry.after_index } -> std::same_as<std::optional<std::size_t>&>;
    { entry.before_status } -> std::same_as<std::optional<asset_materialized_byte_payload_selection_status>&>;
    { entry.after_status } -> std::same_as<std::optional<asset_materialized_byte_payload_selection_status>&>;
    { entry.before_snapshot } -> std::same_as<std::optional<asset_materialized_byte_payload_snapshot>&>;
    { entry.after_snapshot } -> std::same_as<std::optional<asset_materialized_byte_payload_snapshot>&>;
    { entry.before_selected_snapshot } -> std::same_as<std::optional<asset_materialized_byte_payload_snapshot>&>;
    { entry.after_selected_snapshot } -> std::same_as<std::optional<asset_materialized_byte_payload_snapshot>&>;
    { entry.status_changed } -> std::same_as<bool&>;
    { entry.selected_snapshot_changed } -> std::same_as<bool&>;
    { entry.readiness_changed } -> std::same_as<bool&>;
    { entry.integrity_failure_changed } -> std::same_as<bool&>;
    { entry.cache_key_mismatch_changed } -> std::same_as<bool&>;
    { const_entry.has_field_delta() } -> std::same_as<bool>;
});

static_assert(requires(
    asset_materialized_byte_payload_request_transaction_diff_summary summary,
    const asset_materialized_byte_payload_request_transaction_diff_summary& const_summary,
    std::string_view id) {
    { summary.before_summary } -> std::same_as<asset_materialized_byte_payload_request_transaction_summary&>;
    { summary.after_summary } -> std::same_as<asset_materialized_byte_payload_request_transaction_summary&>;
    { summary.count_delta } -> std::same_as<asset_materialized_byte_payload_request_transaction_count_delta&>;
    { summary.added } -> std::same_as<std::vector<asset_materialized_byte_payload_request_transaction_diff_entry>&>;
    { summary.removed } -> std::same_as<std::vector<asset_materialized_byte_payload_request_transaction_diff_entry>&>;
    { summary.changed } -> std::same_as<std::vector<asset_materialized_byte_payload_request_transaction_diff_entry>&>;
    { summary.diagnostic } -> std::same_as<std::string&>;
    { const_summary.empty() } -> std::same_as<bool>;
    { const_summary.change_count() } -> std::same_as<std::size_t>;
    { const_summary.find_added(id) } ->
        std::same_as<const asset_materialized_byte_payload_request_transaction_diff_entry*>;
    { const_summary.find_removed(id) } ->
        std::same_as<const asset_materialized_byte_payload_request_transaction_diff_entry*>;
    { const_summary.find_changed(id) } ->
        std::same_as<const asset_materialized_byte_payload_request_transaction_diff_entry*>;
});

static_assert(std::is_enum_v<asset_shader_materialized_byte_issue_kind>);

static_assert(requires(asset_shader_materialized_byte_issue issue) {
    { issue.kind } -> std::same_as<asset_shader_materialized_byte_issue_kind&>;
    { issue.id } -> std::same_as<std::string&>;
    { issue.cache_key } -> std::same_as<asset_cache_key&>;
    { issue.source_uri } -> std::same_as<std::string&>;
    { issue.materialized_path } -> std::same_as<std::filesystem::path&>;
    { issue.diagnostic } -> std::same_as<std::string&>;
});

static_assert(requires(
    asset_shader_materialized_byte_pipeline_entry entry,
    const asset_shader_materialized_byte_pipeline_entry& const_entry) {
    { entry.id } -> std::same_as<std::string&>;
    { entry.type } -> std::same_as<asset_type&>;
    { entry.cache_key } -> std::same_as<asset_cache_key&>;
    { entry.source_uri } -> std::same_as<std::string&>;
    { entry.materialized_path } -> std::same_as<std::filesystem::path&>;
    { entry.byte_count } -> std::same_as<std::size_t&>;
    { entry.payload_byte_count } -> std::same_as<std::size_t&>;
    { entry.content_hash } -> std::same_as<std::string&>;
    { entry.payload_status } -> std::same_as<asset_materialized_bytes_handoff_status&>;
    { entry.materialized_status } -> std::same_as<runtime_materialized_asset_lookup_status&>;
    { entry.load_status } -> std::same_as<asset_bytes_load_status&>;
    { entry.spirv_expected } -> std::same_as<bool&>;
    { entry.spirv_magic_checked } -> std::same_as<bool&>;
    { entry.spirv_magic_valid } -> std::same_as<bool&>;
    { entry.duplicate_count } -> std::same_as<std::size_t&>;
    { entry.issues } -> std::same_as<std::vector<asset_shader_materialized_byte_issue>&>;
    { entry.diagnostic } -> std::same_as<std::string&>;
    { const_entry.ready() } -> std::same_as<bool>;
});

static_assert(requires(
    asset_shader_materialized_byte_pipeline_summary summary,
    const asset_shader_materialized_byte_pipeline_summary& const_summary,
    std::string_view id) {
    { summary.ready } -> std::same_as<std::vector<asset_shader_materialized_byte_pipeline_entry>&>;
    { summary.blocked } -> std::same_as<std::vector<asset_shader_materialized_byte_pipeline_entry>&>;
    { summary.input_shader_count } -> std::same_as<std::size_t&>;
    { summary.blocked_materialization_count } -> std::same_as<std::size_t&>;
    { summary.blocked_byte_load_count } -> std::same_as<std::size_t&>;
    { summary.integrity_failure_count } -> std::same_as<std::size_t&>;
    { summary.empty_shader_bytes_count } -> std::same_as<std::size_t&>;
    { summary.non_spirv_magic_count } -> std::same_as<std::size_t&>;
    { summary.duplicate_id_count } -> std::same_as<std::size_t&>;
    { const_summary.ok() } -> std::same_as<bool>;
    { const_summary.ready_count() } -> std::same_as<std::size_t>;
    { const_summary.blocked_count() } -> std::same_as<std::size_t>;
    { const_summary.entry_count() } -> std::same_as<std::size_t>;
    { const_summary.find_ready(id) } -> std::same_as<const asset_shader_materialized_byte_pipeline_entry*>;
    { const_summary.find_blocked(id) } -> std::same_as<const asset_shader_materialized_byte_pipeline_entry*>;
    { const_summary.find_entry(id) } -> std::same_as<const asset_shader_materialized_byte_pipeline_entry*>;
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
    const asset_materialized_byte_payload& payload,
    const asset_materialized_byte_payload_bundle& payload_bundle,
    const asset_materialized_byte_payload_bundle_snapshot& payload_snapshot,
    const asset_materialized_byte_payload_selection_request& selection_request,
    const asset_materialized_byte_payload_filter& payload_filter,
    const std::vector<asset_materialized_byte_payload_selection_request>& selection_requests,
    const asset_materialized_byte_payload_request_transaction& payload_transaction,
    asset_materialized_byte_payload_selection_status selection_status,
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
    { make_materialized_asset_byte_payload_bundle(provider, catalog) } ->
        std::same_as<asset_materialized_byte_payload_bundle>;
    { make_materialized_asset_byte_payload_snapshot(payload) } ->
        std::same_as<asset_materialized_byte_payload_snapshot>;
    { snapshot_materialized_asset_byte_payload_bundle(payload_bundle) } ->
        std::same_as<asset_materialized_byte_payload_bundle_snapshot>;
    { diff_materialized_asset_byte_payload_snapshots(payload_snapshot, payload_snapshot) } ->
        std::same_as<asset_materialized_byte_payload_diff_summary>;
    { diff_materialized_asset_byte_payload_bundles(payload_bundle, payload_bundle) } ->
        std::same_as<asset_materialized_byte_payload_diff_summary>;
    { asset_materialized_byte_payload_selection_status_name(selection_status) } -> std::same_as<std::string>;
    { materialized_asset_byte_payload_integrity_ok(payload) } -> std::same_as<bool>;
    { select_materialized_asset_byte_payload(payload_bundle, selection_request) } ->
        std::same_as<asset_materialized_byte_payload_selection_result>;
    { filter_materialized_asset_byte_payloads(payload_bundle, payload_filter) } ->
        std::same_as<asset_materialized_byte_payload_filter_result>;
    { make_materialized_asset_byte_payload_request_transaction(payload_bundle, selection_requests) } ->
        std::same_as<asset_materialized_byte_payload_request_transaction>;
    { diff_materialized_asset_byte_payload_request_transactions(payload_transaction, payload_transaction) } ->
        std::same_as<asset_materialized_byte_payload_request_transaction_diff_summary>;
    { summarize_shader_materialized_byte_pipeline(payload_bundle) } ->
        std::same_as<asset_shader_materialized_byte_pipeline_summary>;
});

} // namespace
} // namespace quiz_vulkan::assets::tests
