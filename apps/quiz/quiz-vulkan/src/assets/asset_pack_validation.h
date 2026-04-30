#pragma once

#include "assets/asset_runtime_resolver_policy.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace quiz_vulkan::assets {

enum class asset_pack_validation_issue_kind {
    missing_required_root,
    invalid_required_root,
    duplicate_cache_key,
    missing_local_fixture_file,
};

struct asset_pack_required_root {
    std::string root_id;
};

struct asset_pack_validation_issue {
    asset_pack_validation_issue_kind kind = asset_pack_validation_issue_kind::missing_required_root;
    std::string id;
    std::string related_id;
    asset_type type = asset_type::generic;
    asset_cache_key cache_key;
    std::filesystem::path path;
    std::string diagnostic;
};

struct asset_pack_build_external_placement {
    std::string id;
    std::string root_id;
    asset_type type = asset_type::generic;
    asset_cache_key cache_key;
    std::filesystem::path root_path;
    std::filesystem::path asset_path;
};

struct asset_pack_validation_report {
    asset_runtime_resolver_policy_summary resolver_policy;
    std::vector<asset_pack_validation_issue> issues;
    std::vector<asset_pack_build_external_placement> build_external_placements;

    [[nodiscard]] bool ok() const
    {
        return resolver_policy.ok() && issues.empty();
    }
};

namespace detail {

inline void add_asset_pack_validation_issue(
    asset_pack_validation_report& report,
    asset_pack_validation_issue_kind kind,
    std::string id,
    std::string diagnostic,
    std::string related_id = {},
    asset_type type = asset_type::generic,
    asset_cache_key cache_key = {},
    std::filesystem::path path = {})
{
    report.issues.push_back(asset_pack_validation_issue{
        .kind = kind,
        .id = std::move(id),
        .related_id = std::move(related_id),
        .type = type,
        .cache_key = std::move(cache_key),
        .path = std::move(path),
        .diagnostic = std::move(diagnostic),
    });
}

inline void add_asset_pack_build_external_placement(
    asset_pack_validation_report& report,
    const asset_runtime_resolver_policy_entry& entry,
    const asset_manifest_root& root)
{
    report.build_external_placements.push_back(asset_pack_build_external_placement{
        .id = entry.id,
        .root_id = root.id,
        .type = entry.type,
        .cache_key = entry.cache_key,
        .root_path = root.root_path,
        .asset_path = entry.rooted_path.value_or(std::filesystem::path{}),
    });
}

inline void sort_asset_pack_issue_ids(std::vector<std::string>& ids)
{
    std::ranges::sort(ids);
    ids.erase(std::ranges::unique(ids).begin(), ids.end());
}

struct asset_pack_cache_key_group {
    asset_cache_key cache_key;
    std::vector<std::string> ids;
    asset_type type = asset_type::generic;
};

inline asset_pack_cache_key_group& find_or_add_asset_pack_cache_key_group(
    std::vector<asset_pack_cache_key_group>& groups,
    const asset_runtime_resolver_policy_entry& entry)
{
    for (asset_pack_cache_key_group& group : groups) {
        if (group.cache_key == entry.cache_key) {
            return group;
        }
    }
    groups.push_back(asset_pack_cache_key_group{
        .cache_key = entry.cache_key,
        .ids = {},
        .type = entry.type,
    });
    return groups.back();
}

} // namespace detail

inline asset_pack_validation_report validate_asset_pack(
    const asset_manifest& manifest,
    const asset_resolver_interface& resolver,
    const std::vector<asset_pack_required_root>& required_roots)
{
    asset_pack_validation_report report{
        .resolver_policy = summarize_asset_runtime_resolver_policy(manifest, resolver),
    };

    for (const asset_pack_required_root& required_root : required_roots) {
        const asset_manifest_root* root = manifest.find_root(required_root.root_id);
        if (root == nullptr) {
            detail::add_asset_pack_validation_issue(
                report,
                asset_pack_validation_issue_kind::missing_required_root,
                required_root.root_id,
                "asset pack required root is not configured");
            continue;
        }
        if (!root->valid()) {
            detail::add_asset_pack_validation_issue(
                report,
                asset_pack_validation_issue_kind::invalid_required_root,
                root->id,
                "asset pack required root is invalid",
                required_root.root_id,
                asset_type::generic,
                {},
                root->root_path);
        }
    }

    std::vector<detail::asset_pack_cache_key_group> cache_key_groups;
    for (const asset_runtime_resolver_policy_entry& entry : report.resolver_policy.entries) {
        detail::asset_pack_cache_key_group& group =
            detail::find_or_add_asset_pack_cache_key_group(cache_key_groups, entry);
        group.ids.push_back(entry.id);

        if (entry.root_space == asset_runtime_resolver_root_space::local_fixture && entry.rooted_path.has_value()
            && !std::filesystem::exists(*entry.rooted_path)) {
            detail::add_asset_pack_validation_issue(
                report,
                asset_pack_validation_issue_kind::missing_local_fixture_file,
                entry.id,
                "asset pack local fixture file is missing",
                entry.resolved_root_id,
                entry.type,
                entry.cache_key,
                *entry.rooted_path);
        }

        if (entry.root_space == asset_runtime_resolver_root_space::build_external) {
            if (const asset_manifest_root* root = manifest.find_root(entry.resolved_root_id); root != nullptr) {
                detail::add_asset_pack_build_external_placement(report, entry, *root);
            }
        }
    }

    for (detail::asset_pack_cache_key_group& group : cache_key_groups) {
        detail::sort_asset_pack_issue_ids(group.ids);
        if (group.ids.size() < 2U) {
            continue;
        }
        detail::add_asset_pack_validation_issue(
            report,
            asset_pack_validation_issue_kind::duplicate_cache_key,
            group.ids.front(),
            "asset pack cache key is used by multiple resolved entries",
            group.ids.back(),
            group.type,
            group.cache_key);
    }

    return report;
}

} // namespace quiz_vulkan::assets
