#pragma once

#include "assets/asset_resolver.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace quiz_vulkan::assets {

struct asset_manifest_root {
    std::string id;
    std::filesystem::path root_path;

    [[nodiscard]] bool valid() const
    {
        return !id.empty() && !root_path.empty();
    }
};

struct asset_manifest_entry {
    std::string id;
    asset_type type = asset_type::generic;
    std::string uri;
    std::string root_id;
    std::string cache_revision;

    [[nodiscard]] bool valid() const
    {
        return !id.empty() && !uri.empty();
    }
};

struct asset_manifest {
    std::vector<asset_manifest_root> roots;
    std::vector<asset_manifest_entry> entries;

    [[nodiscard]] const asset_manifest_root* find_root(std::string_view id) const
    {
        for (const asset_manifest_root& root : roots) {
            if (root.id == id) {
                return &root;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const asset_manifest_entry* find_entry(std::string_view id) const
    {
        for (const asset_manifest_entry& entry : entries) {
            if (entry.id == id) {
                return &entry;
            }
        }
        return nullptr;
    }
};

struct asset_manifest_resolve_request {
    std::string id;
    asset_type expected_type = asset_type::generic;
};

enum class asset_manifest_resolve_status {
    resolved,
    missing_entry,
    type_mismatch,
    invalid_entry,
    asset_resolve_failed,
    root_not_configured,
    invalid_root_path,
};

struct resolved_asset_manifest_entry {
    asset_manifest_entry entry;
    resolved_asset_source source;
    asset_cache_key cache_key;
    std::optional<std::filesystem::path> rooted_path;
};

struct asset_manifest_resolve_result {
    asset_manifest_resolve_status status = asset_manifest_resolve_status::missing_entry;
    resolved_asset_manifest_entry asset;
    std::string diagnostic;

    [[nodiscard]] bool ok() const
    {
        return status == asset_manifest_resolve_status::resolved;
    }
};

inline asset_cache_key make_manifest_asset_cache_key(
    const asset_manifest_entry& entry,
    const resolved_asset_source& source)
{
    asset_cache_key key = source.cache_key();
    if (!entry.cache_revision.empty()) {
        key.append("|rev=");
        key.append(entry.cache_revision);
    }
    return key;
}

inline bool asset_manifest_path_is_within_root(
    const std::filesystem::path& candidate,
    const std::filesystem::path& root)
{
    auto candidate_part = candidate.begin();
    auto root_part = root.begin();
    for (; root_part != root.end(); ++root_part, ++candidate_part) {
        if (candidate_part == candidate.end() || *candidate_part != *root_part) {
            return false;
        }
    }
    return true;
}

inline std::string_view asset_manifest_asset_path_from_uri(std::string_view normalized_uri)
{
    constexpr std::string_view prefix = "asset://";
    if (!normalized_uri.starts_with(prefix)) {
        return {};
    }
    return normalized_uri.substr(prefix.size());
}

inline std::string_view asset_manifest_root_relative_path(const resolved_asset_source& source)
{
    if (source.kind == asset_source_kind::asset_uri) {
        return asset_manifest_asset_path_from_uri(source.normalized_uri);
    }
    if (source.kind == asset_source_kind::local_path) {
        return source.normalized_uri;
    }
    return {};
}

inline std::optional<std::filesystem::path> make_manifest_rooted_path(
    const std::filesystem::path& root,
    std::string_view relative_path)
{
    if (root.empty() || relative_path.empty()) {
        return std::nullopt;
    }

    const std::filesystem::path relative{std::string(relative_path)};
    if (relative.is_absolute() || relative.has_root_name()) {
        return std::nullopt;
    }

    const std::filesystem::path normalized_root = std::filesystem::absolute(root).lexically_normal();
    const std::filesystem::path candidate = (normalized_root / relative).lexically_normal();
    if (!asset_manifest_path_is_within_root(candidate, normalized_root)) {
        return std::nullopt;
    }
    return candidate;
}

inline asset_manifest_resolve_result resolve_asset_manifest_entry(
    const asset_manifest& manifest,
    const asset_manifest_resolve_request& request,
    const asset_resolver_interface& resolver)
{
    asset_manifest_resolve_result result;
    const asset_manifest_entry* entry = manifest.find_entry(request.id);
    if (entry == nullptr) {
        result.status = asset_manifest_resolve_status::missing_entry;
        result.diagnostic = "asset manifest entry was not found";
        return result;
    }

    result.asset.entry = *entry;
    if (!entry->valid()) {
        result.status = asset_manifest_resolve_status::invalid_entry;
        result.diagnostic = "asset manifest entry requires an id and uri";
        return result;
    }
    if (request.expected_type != asset_type::generic && entry->type != request.expected_type) {
        result.status = asset_manifest_resolve_status::type_mismatch;
        result.diagnostic = "asset manifest entry type does not match request";
        return result;
    }

    const asset_resolve_result resolved = resolver.resolve(asset_resolve_request{
        .type = entry->type,
        .uri = entry->uri,
    });
    if (!resolved.ok()) {
        result.status = asset_manifest_resolve_status::asset_resolve_failed;
        result.asset.source = resolved.source;
        result.diagnostic = resolved.diagnostic;
        return result;
    }

    result.asset.source = resolved.source;
    result.asset.cache_key = make_manifest_asset_cache_key(*entry, result.asset.source);

    if (!entry->root_id.empty()) {
        const asset_manifest_root* root = manifest.find_root(entry->root_id);
        if (root == nullptr || !root->valid()) {
            result.status = asset_manifest_resolve_status::root_not_configured;
            result.diagnostic = "asset manifest root is not configured";
            return result;
        }

        const std::string_view relative_path = asset_manifest_root_relative_path(result.asset.source);
        result.asset.rooted_path = make_manifest_rooted_path(root->root_path, relative_path);
        if (!result.asset.rooted_path.has_value()) {
            result.status = asset_manifest_resolve_status::invalid_root_path;
            result.diagnostic = "asset manifest entry cannot be rooted under the configured path";
            return result;
        }
    }

    result.status = asset_manifest_resolve_status::resolved;
    return result;
}

} // namespace quiz_vulkan::assets
