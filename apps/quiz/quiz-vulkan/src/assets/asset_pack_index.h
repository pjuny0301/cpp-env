#pragma once

#include "assets/asset_pack_validation.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace quiz_vulkan::assets {

enum class asset_pack_manifest_version_status {
    accepted,
    missing_version,
    version_mismatch,
};

enum class asset_pack_index_lookup_status {
    found,
    missing_id,
    type_mismatch,
};

struct asset_pack_index_request {
    std::string manifest_version;
    std::string expected_manifest_version;
    bool require_manifest_version = false;
    std::vector<asset_pack_required_root> required_roots;
};

struct asset_pack_manifest_version_diagnostic {
    asset_pack_manifest_version_status status = asset_pack_manifest_version_status::accepted;
    std::string manifest_version;
    std::string expected_manifest_version;
    std::string diagnostic;

    [[nodiscard]] bool ok() const
    {
        return status == asset_pack_manifest_version_status::accepted;
    }
};

struct asset_pack_index_entry {
    std::size_t manifest_index = 0U;
    std::string id;
    asset_type type = asset_type::generic;
    asset_source_kind source_kind = asset_source_kind::unsupported;
    asset_runtime_resolver_root_space root_space = asset_runtime_resolver_root_space::none;
    std::string source_uri;
    std::string resolved_root_id;
    asset_cache_key cache_key;
    std::optional<std::filesystem::path> rooted_path;
};

struct asset_pack_index_cache_key_group {
    asset_cache_key cache_key;
    asset_type type = asset_type::generic;
    std::vector<std::string> entry_ids;

    [[nodiscard]] bool duplicate() const
    {
        return entry_ids.size() > 1U;
    }
};

struct asset_pack_index_invalid_summary {
    bool valid = true;
    std::size_t manifest_version_issue_count = 0U;
    std::size_t resolver_diagnostic_count = 0U;
    std::size_t validation_issue_count = 0U;
    std::size_t missing_required_root_count = 0U;
    std::size_t invalid_required_root_count = 0U;
    std::size_t duplicate_cache_key_count = 0U;
    std::size_t missing_local_fixture_file_count = 0U;
    std::size_t build_external_placement_count = 0U;

    [[nodiscard]] bool ok() const
    {
        return valid;
    }
};

struct asset_pack_index_lookup_result {
    asset_pack_index_lookup_status status = asset_pack_index_lookup_status::missing_id;
    asset_pack_index_entry entry;
    std::string diagnostic;

    [[nodiscard]] bool ok() const
    {
        return status == asset_pack_index_lookup_status::found;
    }
};

struct asset_pack_index_catalog {
    asset_pack_validation_report validation;
    asset_pack_manifest_version_diagnostic manifest_version;
    std::vector<asset_pack_index_entry> entries;
    std::vector<asset_pack_index_cache_key_group> cache_key_groups;
    asset_pack_index_invalid_summary invalid_summary;

    [[nodiscard]] bool ok() const
    {
        return invalid_summary.ok();
    }

    [[nodiscard]] const asset_pack_index_entry* find(std::string_view id) const
    {
        for (const asset_pack_index_entry& entry : entries) {
            if (entry.id == id) {
                return &entry;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const asset_pack_index_cache_key_group* find_cache_key_group(std::string_view cache_key) const
    {
        for (const asset_pack_index_cache_key_group& group : cache_key_groups) {
            if (group.cache_key == cache_key) {
                return &group;
            }
        }
        return nullptr;
    }

    [[nodiscard]] std::vector<asset_pack_index_entry> entries_for_type(asset_type type) const
    {
        std::vector<asset_pack_index_entry> result;
        for (const asset_pack_index_entry& entry : entries) {
            if (type == asset_type::generic || entry.type == type) {
                result.push_back(entry);
            }
        }
        return result;
    }

    [[nodiscard]] asset_pack_index_lookup_result lookup(std::string_view id, asset_type expected_type) const
    {
        asset_pack_index_lookup_result result;
        const asset_pack_index_entry* entry = find(id);
        if (entry == nullptr) {
            result.status = asset_pack_index_lookup_status::missing_id;
            result.diagnostic = "asset pack index entry was not found";
            return result;
        }

        result.entry = *entry;
        if (expected_type != asset_type::generic && entry->type != expected_type) {
            result.status = asset_pack_index_lookup_status::type_mismatch;
            result.diagnostic = "asset pack index entry type does not match request";
            return result;
        }

        result.status = asset_pack_index_lookup_status::found;
        return result;
    }
};

namespace detail {

inline int asset_pack_index_type_rank(asset_type type)
{
    switch (type) {
        case asset_type::font:
            return 1;
        case asset_type::image:
            return 2;
        case asset_type::sound:
            return 3;
        case asset_type::shader:
            return 4;
        case asset_type::deck:
            return 5;
        case asset_type::generic:
            return 6;
    }
    return 6;
}

inline asset_pack_index_cache_key_group& find_or_add_pack_index_cache_key_group(
    std::vector<asset_pack_index_cache_key_group>& groups,
    const asset_pack_index_entry& entry)
{
    for (asset_pack_index_cache_key_group& group : groups) {
        if (group.cache_key == entry.cache_key) {
            return group;
        }
    }
    groups.push_back(asset_pack_index_cache_key_group{
        .cache_key = entry.cache_key,
        .type = entry.type,
    });
    return groups.back();
}

inline void sort_pack_index_cache_key_groups(std::vector<asset_pack_index_cache_key_group>& groups)
{
    for (asset_pack_index_cache_key_group& group : groups) {
        std::ranges::sort(group.entry_ids);
        group.entry_ids.erase(std::ranges::unique(group.entry_ids).begin(), group.entry_ids.end());
    }
    std::ranges::sort(groups, [](const auto& left, const auto& right) {
        return std::tuple(asset_pack_index_type_rank(left.type), std::string_view(left.cache_key))
            < std::tuple(asset_pack_index_type_rank(right.type), std::string_view(right.cache_key));
    });
}

inline asset_pack_index_entry make_pack_index_entry(const asset_runtime_resolver_policy_entry& entry)
{
    return asset_pack_index_entry{
        .manifest_index = entry.manifest_index,
        .id = entry.id,
        .type = entry.type,
        .source_kind = entry.source_kind,
        .root_space = entry.root_space,
        .source_uri = entry.source_uri,
        .resolved_root_id = entry.resolved_root_id,
        .cache_key = entry.cache_key,
        .rooted_path = entry.rooted_path,
    };
}

inline asset_pack_index_invalid_summary summarize_pack_index_invalid_state(
    const asset_pack_validation_report& validation,
    const asset_pack_manifest_version_diagnostic& manifest_version)
{
    asset_pack_index_invalid_summary summary{
        .valid = validation.ok() && manifest_version.ok(),
        .manifest_version_issue_count = manifest_version.ok() ? 0U : 1U,
        .resolver_diagnostic_count = validation.resolver_policy.diagnostics.size(),
        .validation_issue_count = validation.issues.size(),
        .build_external_placement_count = validation.build_external_placements.size(),
    };

    for (const asset_pack_validation_issue& issue : validation.issues) {
        switch (issue.kind) {
            case asset_pack_validation_issue_kind::missing_required_root:
                ++summary.missing_required_root_count;
                break;
            case asset_pack_validation_issue_kind::invalid_required_root:
                ++summary.invalid_required_root_count;
                break;
            case asset_pack_validation_issue_kind::duplicate_cache_key:
                ++summary.duplicate_cache_key_count;
                break;
            case asset_pack_validation_issue_kind::missing_local_fixture_file:
                ++summary.missing_local_fixture_file_count;
                break;
        }
    }

    return summary;
}

} // namespace detail

inline asset_pack_manifest_version_diagnostic validate_asset_pack_manifest_version(
    std::string_view manifest_version,
    std::string_view expected_manifest_version,
    bool require_manifest_version)
{
    asset_pack_manifest_version_diagnostic diagnostic{
        .manifest_version = std::string(manifest_version),
        .expected_manifest_version = std::string(expected_manifest_version),
    };

    if (manifest_version.empty() && (require_manifest_version || !expected_manifest_version.empty())) {
        diagnostic.status = asset_pack_manifest_version_status::missing_version;
        diagnostic.diagnostic = "asset pack manifest version is required";
        return diagnostic;
    }
    if (!expected_manifest_version.empty() && manifest_version != expected_manifest_version) {
        diagnostic.status = asset_pack_manifest_version_status::version_mismatch;
        diagnostic.diagnostic = "asset pack manifest version does not match the expected version";
        return diagnostic;
    }

    diagnostic.status = asset_pack_manifest_version_status::accepted;
    return diagnostic;
}

inline asset_pack_index_catalog build_asset_pack_index(
    const asset_manifest& manifest,
    const asset_resolver_interface& resolver,
    const asset_pack_index_request& request)
{
    asset_pack_index_catalog catalog;
    catalog.validation = validate_asset_pack(manifest, resolver, request.required_roots);
    catalog.manifest_version = validate_asset_pack_manifest_version(
        request.manifest_version,
        request.expected_manifest_version,
        request.require_manifest_version);

    for (const asset_runtime_resolver_policy_entry& entry : catalog.validation.resolver_policy.entries) {
        asset_pack_index_entry index_entry = detail::make_pack_index_entry(entry);
        asset_pack_index_cache_key_group& group =
            detail::find_or_add_pack_index_cache_key_group(catalog.cache_key_groups, index_entry);
        group.entry_ids.push_back(index_entry.id);
        catalog.entries.push_back(std::move(index_entry));
    }

    detail::sort_pack_index_cache_key_groups(catalog.cache_key_groups);
    catalog.invalid_summary = detail::summarize_pack_index_invalid_state(catalog.validation, catalog.manifest_version);
    return catalog;
}

} // namespace quiz_vulkan::assets
