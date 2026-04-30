#pragma once

#include "assets/asset_runtime_catalog.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace quiz_vulkan::assets {

enum class asset_runtime_resolver_policy_status {
    accepted,
    duplicate_manifest_id,
    invalid_manifest_entry,
    resolver_rejected,
    path_traversal_rejected,
    missing_root,
    invalid_root_path,
    cache_key_mismatch,
};

enum class asset_runtime_resolver_root_space {
    none,
    local_fixture,
    build_external,
};

struct asset_runtime_resolver_policy_entry {
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

struct asset_runtime_resolver_policy_diagnostic {
    asset_runtime_resolver_policy_status status = asset_runtime_resolver_policy_status::resolver_rejected;
    std::size_t manifest_index = 0U;
    std::size_t related_manifest_index = 0U;
    std::string id;
    std::string related_id;
    asset_type type = asset_type::generic;
    asset_source_kind source_kind = asset_source_kind::unsupported;
    asset_cache_key cache_key;
    std::string diagnostic;
};

struct asset_runtime_resolver_policy_summary {
    std::vector<asset_runtime_resolver_policy_entry> entries;
    std::vector<asset_runtime_resolver_policy_diagnostic> diagnostics;
    std::size_t asset_uri_count = 0U;
    std::size_t local_path_count = 0U;
    std::size_t build_external_root_count = 0U;
    std::size_t local_fixture_root_count = 0U;
    std::size_t path_traversal_rejection_count = 0U;

    [[nodiscard]] bool ok() const
    {
        return diagnostics.empty();
    }

    [[nodiscard]] const asset_runtime_resolver_policy_entry* find_entry(std::string_view id) const
    {
        for (const asset_runtime_resolver_policy_entry& entry : entries) {
            if (entry.id == id) {
                return &entry;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const asset_runtime_resolver_policy_entry* find_cache_key(std::string_view cache_key) const
    {
        for (const asset_runtime_resolver_policy_entry& entry : entries) {
            if (entry.cache_key == cache_key) {
                return &entry;
            }
        }
        return nullptr;
    }
};

namespace detail {

inline bool asset_runtime_resolver_path_is_build_external(const std::filesystem::path& path)
{
    bool saw_build = false;
    for (const std::filesystem::path& part : path.lexically_normal()) {
        const std::string token = part.generic_string();
        if (saw_build && token == "external") {
            return true;
        }
        saw_build = token == "build";
    }
    return false;
}

inline asset_runtime_resolver_root_space classify_asset_runtime_resolver_root_space(
    const std::filesystem::path& root_path)
{
    if (root_path.empty()) {
        return asset_runtime_resolver_root_space::none;
    }
    if (asset_runtime_resolver_path_is_build_external(root_path)) {
        return asset_runtime_resolver_root_space::build_external;
    }
    return asset_runtime_resolver_root_space::local_fixture;
}

inline void add_asset_runtime_resolver_policy_diagnostic(
    asset_runtime_resolver_policy_summary& summary,
    asset_runtime_resolver_policy_status status,
    std::size_t manifest_index,
    std::string id,
    asset_type type,
    std::string diagnostic,
    asset_source_kind source_kind = asset_source_kind::unsupported,
    asset_cache_key cache_key = {},
    std::size_t related_manifest_index = 0U,
    std::string related_id = {})
{
    summary.diagnostics.push_back(asset_runtime_resolver_policy_diagnostic{
        .status = status,
        .manifest_index = manifest_index,
        .related_manifest_index = related_manifest_index,
        .id = std::move(id),
        .related_id = std::move(related_id),
        .type = type,
        .source_kind = source_kind,
        .cache_key = std::move(cache_key),
        .diagnostic = std::move(diagnostic),
    });
    if (status == asset_runtime_resolver_policy_status::path_traversal_rejected) {
        ++summary.path_traversal_rejection_count;
    }
}

inline std::optional<std::size_t> find_prior_manifest_entry_index(
    const asset_manifest& manifest,
    std::size_t current_index,
    std::string_view id)
{
    if (id.empty()) {
        return std::nullopt;
    }
    for (std::size_t index = 0U; index < current_index; ++index) {
        if (manifest.entries[index].id == id) {
            return index;
        }
    }
    return std::nullopt;
}

inline asset_runtime_resolver_policy_status policy_status_from_resolve_failure(asset_resolve_status status)
{
    if (status == asset_resolve_status::path_traversal) {
        return asset_runtime_resolver_policy_status::path_traversal_rejected;
    }
    return asset_runtime_resolver_policy_status::resolver_rejected;
}

inline void count_asset_runtime_resolver_source_kind(
    asset_runtime_resolver_policy_summary& summary,
    asset_source_kind source_kind)
{
    if (source_kind == asset_source_kind::asset_uri) {
        ++summary.asset_uri_count;
    } else if (source_kind == asset_source_kind::local_path) {
        ++summary.local_path_count;
    }
}

inline void count_asset_runtime_resolver_root_space(
    asset_runtime_resolver_policy_summary& summary,
    asset_runtime_resolver_root_space root_space)
{
    if (root_space == asset_runtime_resolver_root_space::build_external) {
        ++summary.build_external_root_count;
    } else if (root_space == asset_runtime_resolver_root_space::local_fixture) {
        ++summary.local_fixture_root_count;
    }
}

} // namespace detail

inline asset_runtime_resolver_policy_summary summarize_asset_runtime_resolver_policy(
    const asset_manifest& manifest,
    const asset_resolver_interface& resolver)
{
    asset_runtime_resolver_policy_summary summary;

    for (std::size_t index = 0U; index < manifest.entries.size(); ++index) {
        const asset_manifest_entry& entry = manifest.entries[index];
        if (const std::optional<std::size_t> prior =
                detail::find_prior_manifest_entry_index(manifest, index, entry.id);
            prior.has_value()) {
            detail::add_asset_runtime_resolver_policy_diagnostic(
                summary,
                asset_runtime_resolver_policy_status::duplicate_manifest_id,
                index,
                entry.id,
                entry.type,
                "asset manifest lookup uses the first entry with a duplicated id",
                asset_source_kind::unsupported,
                {},
                *prior,
                manifest.entries[*prior].id);
        }

        if (!entry.valid()) {
            detail::add_asset_runtime_resolver_policy_diagnostic(
                summary,
                asset_runtime_resolver_policy_status::invalid_manifest_entry,
                index,
                entry.id,
                entry.type,
                "asset manifest entry requires an id and uri");
            continue;
        }

        const asset_resolve_result resolved = resolver.resolve(asset_resolve_request{
            .type = entry.type,
            .uri = entry.uri,
        });
        if (!resolved.ok()) {
            detail::add_asset_runtime_resolver_policy_diagnostic(
                summary,
                detail::policy_status_from_resolve_failure(resolved.status),
                index,
                entry.id,
                entry.type,
                resolved.diagnostic,
                resolved.source.kind);
            continue;
        }

        asset_runtime_resolver_policy_entry policy_entry{
            .manifest_index = index,
            .id = entry.id,
            .type = entry.type,
            .source_kind = resolved.source.kind,
            .source_uri = resolved.source.normalized_uri,
            .cache_key = make_manifest_asset_cache_key(entry, resolved.source),
        };

        const asset_cache_key expected_source_key = make_asset_cache_key(entry.type, resolved.source.normalized_uri);
        if (resolved.source.cache_key() != expected_source_key) {
            detail::add_asset_runtime_resolver_policy_diagnostic(
                summary,
                asset_runtime_resolver_policy_status::cache_key_mismatch,
                index,
                entry.id,
                entry.type,
                "asset resolver source cache key does not match the normalized source",
                resolved.source.kind,
                resolved.source.cache_key());
            continue;
        }

        if (!entry.root_id.empty()) {
            const asset_manifest_root* root = manifest.find_root(entry.root_id);
            if (root == nullptr || !root->valid()) {
                detail::add_asset_runtime_resolver_policy_diagnostic(
                    summary,
                    asset_runtime_resolver_policy_status::missing_root,
                    index,
                    entry.id,
                    entry.type,
                    "asset manifest entry references an unconfigured root",
                    resolved.source.kind,
                    policy_entry.cache_key);
                continue;
            }

            policy_entry.resolved_root_id = root->id;
            policy_entry.root_space = detail::classify_asset_runtime_resolver_root_space(root->root_path);
            policy_entry.rooted_path = make_manifest_rooted_path(
                root->root_path,
                asset_manifest_root_relative_path(resolved.source));
            if (!policy_entry.rooted_path.has_value()) {
                detail::add_asset_runtime_resolver_policy_diagnostic(
                    summary,
                    asset_runtime_resolver_policy_status::invalid_root_path,
                    index,
                    entry.id,
                    entry.type,
                    "asset manifest entry cannot be rooted under the configured path",
                    resolved.source.kind,
                    policy_entry.cache_key);
                continue;
            }
        }

        detail::count_asset_runtime_resolver_source_kind(summary, policy_entry.source_kind);
        detail::count_asset_runtime_resolver_root_space(summary, policy_entry.root_space);
        summary.entries.push_back(std::move(policy_entry));
    }

    return summary;
}

} // namespace quiz_vulkan::assets
