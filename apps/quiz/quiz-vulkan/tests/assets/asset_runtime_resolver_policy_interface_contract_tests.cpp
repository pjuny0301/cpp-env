#include "assets/asset_runtime_resolver_policy.h"

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

static_assert(std::is_enum_v<asset_runtime_resolver_policy_status>);
static_assert(std::is_enum_v<asset_runtime_resolver_root_space>);

static_assert(requires(asset_runtime_resolver_policy_entry entry) {
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

static_assert(requires(asset_runtime_resolver_policy_diagnostic diagnostic) {
    { diagnostic.status } -> std::same_as<asset_runtime_resolver_policy_status&>;
    { diagnostic.manifest_index } -> std::same_as<std::size_t&>;
    { diagnostic.related_manifest_index } -> std::same_as<std::size_t&>;
    { diagnostic.id } -> std::same_as<std::string&>;
    { diagnostic.related_id } -> std::same_as<std::string&>;
    { diagnostic.type } -> std::same_as<asset_type&>;
    { diagnostic.source_kind } -> std::same_as<asset_source_kind&>;
    { diagnostic.cache_key } -> std::same_as<asset_cache_key&>;
    { diagnostic.diagnostic } -> std::same_as<std::string&>;
});

static_assert(requires(
    asset_runtime_resolver_policy_summary summary,
    const asset_runtime_resolver_policy_summary& const_summary,
    std::string_view id,
    std::string_view cache_key) {
    { summary.entries } -> std::same_as<std::vector<asset_runtime_resolver_policy_entry>&>;
    { summary.diagnostics } -> std::same_as<std::vector<asset_runtime_resolver_policy_diagnostic>&>;
    { summary.asset_uri_count } -> std::same_as<std::size_t&>;
    { summary.local_path_count } -> std::same_as<std::size_t&>;
    { summary.build_external_root_count } -> std::same_as<std::size_t&>;
    { summary.local_fixture_root_count } -> std::same_as<std::size_t&>;
    { summary.path_traversal_rejection_count } -> std::same_as<std::size_t&>;
    { const_summary.ok() } -> std::same_as<bool>;
    { const_summary.find_entry(id) } -> std::same_as<const asset_runtime_resolver_policy_entry*>;
    { const_summary.find_cache_key(cache_key) } -> std::same_as<const asset_runtime_resolver_policy_entry*>;
});

static_assert(requires(const asset_manifest& manifest, const asset_resolver_interface& resolver) {
    { summarize_asset_runtime_resolver_policy(manifest, resolver) } ->
        std::same_as<asset_runtime_resolver_policy_summary>;
});

} // namespace
} // namespace quiz_vulkan::assets::tests
