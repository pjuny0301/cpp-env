#pragma once

#include "assets/asset_manifest.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace quiz_vulkan::assets {

enum class runtime_asset_catalog_lookup_status {
    found,
    missing_id,
    type_mismatch,
    unresolved_asset,
};

enum class runtime_materialized_asset_lookup_status {
    materialized,
    missing_id,
    type_mismatch,
    unresolved_asset,
    unsupported_source_kind,
    missing_rooted_path,
    invalid_rooted_path,
    source_path_mismatch,
    cache_key_mismatch,
    noncanonical_cache_key,
};

struct runtime_asset_catalog_snapshot {
    asset_manifest_entry entry;
    resolved_asset_source source;
    asset_cache_key cache_key;
    std::string resolved_root_id;
    std::optional<std::filesystem::path> rooted_path;
};

struct runtime_asset_catalog_lookup_result {
    runtime_asset_catalog_lookup_status status = runtime_asset_catalog_lookup_status::missing_id;
    runtime_asset_catalog_snapshot asset;
    std::string diagnostic;

    [[nodiscard]] bool ok() const
    {
        return status == runtime_asset_catalog_lookup_status::found;
    }
};

struct runtime_materialized_asset_snapshot {
    runtime_asset_catalog_snapshot asset;
    asset_cache_key_classification cache_key_policy;
    std::filesystem::path local_path;
    std::string source_path;
};

struct runtime_materialized_asset_lookup_request {
    std::string id;
    asset_type expected_type = asset_type::generic;
};

struct runtime_materialized_asset_lookup_result {
    runtime_materialized_asset_lookup_status status =
        runtime_materialized_asset_lookup_status::missing_id;
    runtime_materialized_asset_snapshot materialized;
    std::string diagnostic;

    [[nodiscard]] bool ok() const
    {
        return status == runtime_materialized_asset_lookup_status::materialized;
    }
};

struct runtime_asset_catalog_diagnostic {
    asset_manifest_validation_issue_kind kind = asset_manifest_validation_issue_kind::invalid_entry;
    std::string id;
    std::string related_id;
    asset_cache_key cache_key;
    std::string diagnostic;
};

struct runtime_asset_catalog {
    std::vector<runtime_asset_catalog_snapshot> assets;
    std::vector<runtime_asset_catalog_diagnostic> diagnostics;

    [[nodiscard]] bool ok() const
    {
        return diagnostics.empty();
    }

    [[nodiscard]] const runtime_asset_catalog_snapshot* find(std::string_view id) const
    {
        for (const runtime_asset_catalog_snapshot& asset : assets) {
            if (asset.entry.id == id) {
                return &asset;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const runtime_asset_catalog_diagnostic* find_diagnostic(std::string_view id) const
    {
        for (const runtime_asset_catalog_diagnostic& diagnostic : diagnostics) {
            if (diagnostic.id == id) {
                return &diagnostic;
            }
        }
        return nullptr;
    }

    [[nodiscard]] runtime_asset_catalog_lookup_result lookup(
        std::string_view id,
        asset_type expected_type) const
    {
        runtime_asset_catalog_lookup_result result;
        if (const runtime_asset_catalog_snapshot* asset = find(id); asset != nullptr) {
            result.asset = *asset;
            if (expected_type != asset_type::generic && asset->entry.type != expected_type) {
                result.status = runtime_asset_catalog_lookup_status::type_mismatch;
                result.diagnostic = "runtime asset catalog entry type does not match request";
                return result;
            }

            result.status = runtime_asset_catalog_lookup_status::found;
            return result;
        }

        if (const runtime_asset_catalog_diagnostic* diagnostic = find_diagnostic(id); diagnostic != nullptr) {
            result.status = runtime_asset_catalog_lookup_status::unresolved_asset;
            result.diagnostic = diagnostic->diagnostic;
            return result;
        }

        result.status = runtime_asset_catalog_lookup_status::missing_id;
        result.diagnostic = "runtime asset catalog entry was not found";
        return result;
    }

    [[nodiscard]] runtime_asset_catalog_lookup_result lookup_font(std::string_view id) const
    {
        return lookup(id, asset_type::font);
    }

    [[nodiscard]] runtime_asset_catalog_lookup_result lookup_image(std::string_view id) const
    {
        return lookup(id, asset_type::image);
    }

    [[nodiscard]] runtime_asset_catalog_lookup_result lookup_sound(std::string_view id) const
    {
        return lookup(id, asset_type::sound);
    }

    [[nodiscard]] runtime_asset_catalog_lookup_result lookup_shader(std::string_view id) const
    {
        return lookup(id, asset_type::shader);
    }

    [[nodiscard]] runtime_asset_catalog_lookup_result lookup_deck(std::string_view id) const
    {
        return lookup(id, asset_type::deck);
    }
};

namespace detail {

inline void add_runtime_asset_catalog_diagnostic(
    runtime_asset_catalog& catalog,
    asset_manifest_validation_issue_kind kind,
    std::string id,
    std::string diagnostic,
    std::string related_id = {},
    asset_cache_key cache_key = {})
{
    catalog.diagnostics.push_back(runtime_asset_catalog_diagnostic{
        .kind = kind,
        .id = std::move(id),
        .related_id = std::move(related_id),
        .cache_key = std::move(cache_key),
        .diagnostic = std::move(diagnostic),
    });
}

inline void add_runtime_asset_catalog_validation_diagnostics(
    runtime_asset_catalog& catalog,
    const asset_manifest_validation_result& validation)
{
    for (const asset_manifest_validation_issue& issue : validation.issues) {
        if (issue.kind == asset_manifest_validation_issue_kind::cache_key_collision) {
            add_runtime_asset_catalog_diagnostic(
                catalog,
                issue.kind,
                issue.id,
                issue.diagnostic,
                issue.related_id,
                issue.cache_key);
        }
    }
}

inline bool runtime_asset_path_has_parent_reference(const std::filesystem::path& path)
{
    for (const std::filesystem::path& part : path) {
        if (part == "..") {
            return true;
        }
    }
    return false;
}

inline bool runtime_asset_rooted_path_is_safe(const std::filesystem::path& path)
{
    return !path.empty() && path.is_absolute() && !runtime_asset_path_has_parent_reference(path);
}

inline bool runtime_asset_rooted_path_matches_source_path(
    const std::filesystem::path& rooted_path,
    std::string_view source_path)
{
    if (source_path.empty()) {
        return false;
    }

    const std::filesystem::path source{std::string(source_path)};
    if (source.empty() || source.is_absolute() || source.has_root_name()
        || runtime_asset_path_has_parent_reference(source)) {
        return false;
    }

    const std::filesystem::path normalized_rooted_path = rooted_path.lexically_normal();
    auto rooted_part = normalized_rooted_path.end();
    auto source_part = source.end();
    while (source_part != source.begin()) {
        --source_part;
        if (rooted_part == normalized_rooted_path.begin()) {
            return false;
        }
        --rooted_part;
        if (*rooted_part != *source_part) {
            return false;
        }
    }

    return true;
}

inline runtime_materialized_asset_lookup_status materialized_status_from_catalog_lookup_status(
    runtime_asset_catalog_lookup_status status)
{
    switch (status) {
        case runtime_asset_catalog_lookup_status::found:
            return runtime_materialized_asset_lookup_status::missing_rooted_path;
        case runtime_asset_catalog_lookup_status::missing_id:
            return runtime_materialized_asset_lookup_status::missing_id;
        case runtime_asset_catalog_lookup_status::type_mismatch:
            return runtime_materialized_asset_lookup_status::type_mismatch;
        case runtime_asset_catalog_lookup_status::unresolved_asset:
            return runtime_materialized_asset_lookup_status::unresolved_asset;
    }
    return runtime_materialized_asset_lookup_status::missing_id;
}

} // namespace detail

inline runtime_materialized_asset_lookup_result materialize_runtime_asset(
    const runtime_asset_catalog_snapshot& snapshot)
{
    runtime_materialized_asset_lookup_result result{
        .materialized = runtime_materialized_asset_snapshot{
            .asset = snapshot,
        },
    };

    const asset_cache_key expected_cache_key = make_manifest_asset_cache_key(snapshot.entry, snapshot.source);
    if (snapshot.cache_key != expected_cache_key) {
        result.status = runtime_materialized_asset_lookup_status::cache_key_mismatch;
        result.diagnostic = "runtime materialized asset cache key does not match the normalized source";
        return result;
    }

    result.materialized.cache_key_policy = classify_asset_cache_key(snapshot.cache_key);
    if (!result.materialized.cache_key_policy.ok()) {
        result.status = runtime_materialized_asset_lookup_status::noncanonical_cache_key;
        result.diagnostic = result.materialized.cache_key_policy.diagnostic;
        return result;
    }

    if (snapshot.source.kind != asset_source_kind::asset_uri && snapshot.source.kind != asset_source_kind::local_path) {
        result.status = runtime_materialized_asset_lookup_status::unsupported_source_kind;
        result.diagnostic = "runtime materialized assets require asset uri or local path sources";
        return result;
    }

    if (!snapshot.rooted_path.has_value()) {
        result.status = runtime_materialized_asset_lookup_status::missing_rooted_path;
        result.diagnostic = "runtime materialized asset requires a rooted local path";
        return result;
    }

    if (!detail::runtime_asset_rooted_path_is_safe(*snapshot.rooted_path)) {
        result.status = runtime_materialized_asset_lookup_status::invalid_rooted_path;
        result.diagnostic = "runtime materialized asset rooted path must be absolute and cannot contain parent traversal";
        return result;
    }

    if (!detail::runtime_asset_rooted_path_matches_source_path(
            *snapshot.rooted_path,
            result.materialized.cache_key_policy.source_path)) {
        result.status = runtime_materialized_asset_lookup_status::source_path_mismatch;
        result.diagnostic = "runtime materialized asset rooted path does not match the normalized source path";
        return result;
    }

    result.status = runtime_materialized_asset_lookup_status::materialized;
    result.materialized.local_path = snapshot.rooted_path->lexically_normal();
    result.materialized.source_path = result.materialized.cache_key_policy.source_path;
    return result;
}

inline runtime_materialized_asset_lookup_result lookup_runtime_materialized_asset(
    const runtime_asset_catalog& catalog,
    const runtime_materialized_asset_lookup_request& request)
{
    const runtime_asset_catalog_lookup_result lookup = catalog.lookup(request.id, request.expected_type);
    if (!lookup.ok()) {
        runtime_materialized_asset_lookup_result result;
        result.status = detail::materialized_status_from_catalog_lookup_status(lookup.status);
        result.diagnostic = lookup.diagnostic;
        if (!lookup.asset.entry.id.empty()) {
            result.materialized.asset = lookup.asset;
        }
        return result;
    }

    return materialize_runtime_asset(lookup.asset);
}

inline runtime_asset_catalog build_runtime_asset_catalog(
    const asset_manifest& manifest,
    const asset_resolver_interface& resolver)
{
    runtime_asset_catalog catalog;
    detail::add_runtime_asset_catalog_validation_diagnostics(catalog, validate_asset_manifest(manifest, resolver));

    for (const asset_manifest_entry& entry : manifest.entries) {
        if (!entry.valid()) {
            detail::add_runtime_asset_catalog_diagnostic(
                catalog,
                asset_manifest_validation_issue_kind::invalid_entry,
                entry.id,
                "asset manifest entry requires an id and uri");
            continue;
        }

        const asset_resolve_result resolved = resolver.resolve(asset_resolve_request{
            .type = entry.type,
            .uri = entry.uri,
        });
        if (!resolved.ok()) {
            detail::add_runtime_asset_catalog_diagnostic(
                catalog,
                asset_manifest_validation_issue_kind::asset_resolve_failed,
                entry.id,
                resolved.diagnostic);
            continue;
        }

        runtime_asset_catalog_snapshot snapshot{
            .entry = entry,
            .source = resolved.source,
            .cache_key = make_manifest_asset_cache_key(entry, resolved.source),
        };

        if (!entry.root_id.empty()) {
            const asset_manifest_root* root = manifest.find_root(entry.root_id);
            if (root == nullptr || !root->valid()) {
                detail::add_runtime_asset_catalog_diagnostic(
                    catalog,
                    asset_manifest_validation_issue_kind::missing_root,
                    entry.id,
                    "asset manifest entry references an unconfigured root",
                    entry.root_id);
                continue;
            }

            snapshot.resolved_root_id = root->id;
            snapshot.rooted_path = make_manifest_rooted_path(
                root->root_path,
                asset_manifest_root_relative_path(snapshot.source));
            if (!snapshot.rooted_path.has_value()) {
                detail::add_runtime_asset_catalog_diagnostic(
                    catalog,
                    asset_manifest_validation_issue_kind::invalid_root_path,
                    entry.id,
                    "asset manifest entry cannot be rooted under the configured path",
                    root->id);
                continue;
            }
        }

        catalog.assets.push_back(std::move(snapshot));
    }

    return catalog;
}

} // namespace quiz_vulkan::assets
