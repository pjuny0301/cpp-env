#pragma once

#include "assets/asset_resolver.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace quiz_vulkan::assets {

struct asset_manifest_root {
    std::string id;
    std::vector<std::string> aliases;
    std::filesystem::path root_path;

    [[nodiscard]] bool aliases_valid() const
    {
        for (const std::string& alias : aliases) {
            if (alias.empty()) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool matches(std::string_view value) const
    {
        if (value.empty()) {
            return false;
        }
        if (id == value) {
            return true;
        }
        for (const std::string& alias : aliases) {
            if (alias == value) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] bool valid() const
    {
        return !id.empty() && !root_path.empty() && aliases_valid();
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
            if (root.matches(id)) {
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

enum class asset_manifest_validation_issue_kind {
    invalid_root,
    duplicate_root_id,
    invalid_entry,
    duplicate_entry_id,
    missing_root,
    asset_resolve_failed,
    invalid_root_path,
    cache_key_collision,
};

struct asset_manifest_validation_issue {
    asset_manifest_validation_issue_kind kind = asset_manifest_validation_issue_kind::invalid_entry;
    std::string id;
    std::string related_id;
    asset_cache_key cache_key;
    std::string diagnostic;
};

struct asset_manifest_validation_result {
    std::vector<asset_manifest_validation_issue> issues;

    [[nodiscard]] bool ok() const
    {
        return issues.empty();
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

namespace detail {

struct asset_manifest_cache_record {
    std::string id;
    asset_cache_key cache_key;
    std::optional<std::filesystem::path> rooted_path;
};

inline void add_manifest_validation_issue(
    asset_manifest_validation_result& result,
    asset_manifest_validation_issue_kind kind,
    std::string id,
    std::string diagnostic,
    std::string related_id = {},
    asset_cache_key cache_key = {})
{
    result.issues.push_back(asset_manifest_validation_issue{
        .kind = kind,
        .id = std::move(id),
        .related_id = std::move(related_id),
        .cache_key = std::move(cache_key),
        .diagnostic = std::move(diagnostic),
    });
}

inline bool manifest_contains_prior_root_identifier(
    const asset_manifest& manifest,
    std::size_t root_index,
    std::string_view identifier)
{
    if (identifier.empty()) {
        return false;
    }
    for (std::size_t index = 0; index < root_index; ++index) {
        if (manifest.roots[index].matches(identifier)) {
            return true;
        }
    }
    return false;
}

inline bool root_contains_prior_identifier(
    const asset_manifest_root& root,
    std::size_t alias_index,
    std::string_view identifier)
{
    if (identifier.empty() || root.id == identifier) {
        return !identifier.empty();
    }
    for (std::size_t index = 0; index < alias_index; ++index) {
        if (root.aliases[index] == identifier) {
            return true;
        }
    }
    return false;
}

inline bool manifest_contains_duplicate_entry_id(
    const asset_manifest& manifest,
    std::size_t entry_index)
{
    const std::string& id = manifest.entries[entry_index].id;
    if (id.empty()) {
        return false;
    }
    for (std::size_t index = 0; index < entry_index; ++index) {
        if (manifest.entries[index].id == id) {
            return true;
        }
    }
    return false;
}

inline bool rooted_paths_conflict(
    const std::optional<std::filesystem::path>& left,
    const std::optional<std::filesystem::path>& right)
{
    return left.has_value() && right.has_value() && left->lexically_normal() != right->lexically_normal();
}

} // namespace detail

inline asset_manifest_validation_result validate_asset_manifest(
    const asset_manifest& manifest,
    const asset_resolver_interface& resolver)
{
    asset_manifest_validation_result result;
    for (std::size_t index = 0; index < manifest.roots.size(); ++index) {
        const asset_manifest_root& root = manifest.roots[index];
        if (root.id.empty() || root.root_path.empty()) {
            detail::add_manifest_validation_issue(
                result,
                asset_manifest_validation_issue_kind::invalid_root,
                root.id,
                "asset manifest root requires an id and path");
        }
        if (!root.aliases_valid()) {
            detail::add_manifest_validation_issue(
                result,
                asset_manifest_validation_issue_kind::invalid_root,
                root.id,
                "asset manifest root aliases must not be empty");
        }
        if (detail::manifest_contains_prior_root_identifier(manifest, index, root.id)) {
            detail::add_manifest_validation_issue(
                result,
                asset_manifest_validation_issue_kind::duplicate_root_id,
                root.id,
                "asset manifest root id or alias is duplicated");
        }
        for (std::size_t alias_index = 0; alias_index < root.aliases.size(); ++alias_index) {
            const std::string& alias = root.aliases[alias_index];
            if (alias.empty()) {
                continue;
            }
            if (detail::manifest_contains_prior_root_identifier(manifest, index, alias)
                || detail::root_contains_prior_identifier(root, alias_index, alias)) {
                detail::add_manifest_validation_issue(
                    result,
                    asset_manifest_validation_issue_kind::duplicate_root_id,
                    alias,
                    "asset manifest root id or alias is duplicated",
                    root.id);
            }
        }
    }

    std::vector<detail::asset_manifest_cache_record> cache_records;
    for (std::size_t index = 0; index < manifest.entries.size(); ++index) {
        const asset_manifest_entry& entry = manifest.entries[index];
        if (!entry.valid()) {
            detail::add_manifest_validation_issue(
                result,
                asset_manifest_validation_issue_kind::invalid_entry,
                entry.id,
                "asset manifest entry requires an id and uri");
            continue;
        }
        if (detail::manifest_contains_duplicate_entry_id(manifest, index)) {
            detail::add_manifest_validation_issue(
                result,
                asset_manifest_validation_issue_kind::duplicate_entry_id,
                entry.id,
                "asset manifest entry id is duplicated");
        }

        const asset_manifest_root* root = nullptr;
        if (!entry.root_id.empty()) {
            root = manifest.find_root(entry.root_id);
            if (root == nullptr || !root->valid()) {
                detail::add_manifest_validation_issue(
                    result,
                    asset_manifest_validation_issue_kind::missing_root,
                    entry.id,
                    "asset manifest entry references an unconfigured root",
                    entry.root_id);
            }
        }

        const asset_resolve_result resolved = resolver.resolve(asset_resolve_request{
            .type = entry.type,
            .uri = entry.uri,
        });
        if (!resolved.ok()) {
            detail::add_manifest_validation_issue(
                result,
                asset_manifest_validation_issue_kind::asset_resolve_failed,
                entry.id,
                resolved.diagnostic);
            continue;
        }

        std::optional<std::filesystem::path> rooted_path;
        if (root != nullptr && root->valid()) {
            const std::string_view relative_path = asset_manifest_root_relative_path(resolved.source);
            rooted_path = make_manifest_rooted_path(root->root_path, relative_path);
            if (!rooted_path.has_value()) {
                detail::add_manifest_validation_issue(
                    result,
                    asset_manifest_validation_issue_kind::invalid_root_path,
                    entry.id,
                    "asset manifest entry cannot be rooted under the configured path",
                    root->id);
            }
        }

        const asset_cache_key cache_key = make_manifest_asset_cache_key(entry, resolved.source);
        for (const detail::asset_manifest_cache_record& record : cache_records) {
            if (record.cache_key == cache_key && detail::rooted_paths_conflict(record.rooted_path, rooted_path)) {
                detail::add_manifest_validation_issue(
                    result,
                    asset_manifest_validation_issue_kind::cache_key_collision,
                    entry.id,
                    "asset manifest cache key maps to multiple rooted paths",
                    record.id,
                    cache_key);
            }
        }
        cache_records.push_back(detail::asset_manifest_cache_record{
            .id = entry.id,
            .cache_key = cache_key,
            .rooted_path = std::move(rooted_path),
        });
    }

    return result;
}

} // namespace quiz_vulkan::assets
