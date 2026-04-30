#include "assets/asset_pack_validation.h"

#include <concepts>
#include <filesystem>
#include <string>
#include <type_traits>
#include <vector>

namespace quiz_vulkan::assets::tests {
namespace {

static_assert(std::is_enum_v<asset_pack_validation_issue_kind>);

static_assert(requires(asset_pack_required_root root) {
    { root.root_id } -> std::same_as<std::string&>;
});

static_assert(requires(asset_pack_validation_issue issue) {
    { issue.kind } -> std::same_as<asset_pack_validation_issue_kind&>;
    { issue.id } -> std::same_as<std::string&>;
    { issue.related_id } -> std::same_as<std::string&>;
    { issue.type } -> std::same_as<asset_type&>;
    { issue.cache_key } -> std::same_as<asset_cache_key&>;
    { issue.path } -> std::same_as<std::filesystem::path&>;
    { issue.diagnostic } -> std::same_as<std::string&>;
});

static_assert(requires(asset_pack_build_external_placement placement) {
    { placement.id } -> std::same_as<std::string&>;
    { placement.root_id } -> std::same_as<std::string&>;
    { placement.type } -> std::same_as<asset_type&>;
    { placement.cache_key } -> std::same_as<asset_cache_key&>;
    { placement.root_path } -> std::same_as<std::filesystem::path&>;
    { placement.asset_path } -> std::same_as<std::filesystem::path&>;
});

static_assert(requires(
    asset_pack_validation_report report,
    const asset_pack_validation_report& const_report) {
    { report.resolver_policy } -> std::same_as<asset_runtime_resolver_policy_summary&>;
    { report.issues } -> std::same_as<std::vector<asset_pack_validation_issue>&>;
    { report.build_external_placements } -> std::same_as<std::vector<asset_pack_build_external_placement>&>;
    { const_report.ok() } -> std::same_as<bool>;
});

static_assert(requires(
    const asset_manifest& manifest,
    const asset_resolver_interface& resolver,
    const std::vector<asset_pack_required_root>& required_roots) {
    { validate_asset_pack(manifest, resolver, required_roots) } -> std::same_as<asset_pack_validation_report>;
});

} // namespace
} // namespace quiz_vulkan::assets::tests
