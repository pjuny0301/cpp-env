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

} // namespace detail

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
