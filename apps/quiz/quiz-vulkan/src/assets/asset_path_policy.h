#pragma once

#include "assets/asset_runtime_catalog.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace quiz_vulkan::assets {

enum class asset_path_policy_status {
    accepted,
    unsupported_asset_type,
    unsupported_source_kind,
    invalid_path,
    path_traversal,
    unsupported_extension,
    invalid_rooted_path,
    cache_key_mismatch,
    unresolved_source,
    cache_key_collision,
    duplicate_catalog_path,
};

struct asset_path_policy_rule {
    asset_type type = asset_type::generic;
    std::string_view catalog_group;
};

struct asset_path_policy_snapshot {
    std::string id;
    asset_type type = asset_type::generic;
    asset_cache_key cache_key;
    std::string source_uri;
    std::string source_path;
    std::string catalog_path;
    std::optional<std::filesystem::path> rooted_path;
};

struct asset_path_policy_result {
    asset_path_policy_status status = asset_path_policy_status::invalid_path;
    asset_path_policy_snapshot asset;
    std::string diagnostic;

    [[nodiscard]] bool ok() const
    {
        return status == asset_path_policy_status::accepted;
    }
};

struct asset_path_policy_diagnostic {
    asset_path_policy_status status = asset_path_policy_status::invalid_path;
    std::string id;
    std::string related_id;
    asset_type type = asset_type::generic;
    asset_cache_key cache_key;
    std::string catalog_path;
    std::string diagnostic;
};

struct asset_path_policy_kind_counts {
    asset_type type = asset_type::generic;
    std::size_t accepted_count = 0U;
    std::size_t rejected_count = 0U;
};

struct asset_path_policy_catalog_snapshot_view {
    std::vector<asset_path_policy_snapshot> entries;
    std::vector<asset_path_policy_diagnostic> diagnostics;
    std::vector<asset_path_policy_kind_counts> kind_counts;
};

struct asset_path_policy_duplicate_source_report {
    asset_type type = asset_type::generic;
    std::string source_uri;
    std::vector<std::string> entry_ids;
};

struct asset_path_policy_duplicate_catalog_path_report {
    asset_type type = asset_type::generic;
    std::string catalog_path;
    std::vector<std::string> entry_ids;
};

struct asset_path_policy_duplicate_cache_key_report {
    asset_type type = asset_type::generic;
    asset_cache_key cache_key;
    std::vector<std::string> entry_ids;
};

struct asset_path_policy_catalog {
    std::vector<asset_path_policy_snapshot> entries;
    std::vector<asset_path_policy_diagnostic> diagnostics;

    [[nodiscard]] bool ok() const
    {
        return diagnostics.empty();
    }

    [[nodiscard]] const asset_path_policy_snapshot* find(std::string_view id) const
    {
        for (const asset_path_policy_snapshot& entry : entries) {
            if (entry.id == id) {
                return &entry;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const asset_path_policy_snapshot* find_cache_key(std::string_view cache_key) const
    {
        for (const asset_path_policy_snapshot& entry : entries) {
            if (entry.cache_key == cache_key) {
                return &entry;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const asset_path_policy_snapshot* find_catalog_path(std::string_view catalog_path) const
    {
        for (const asset_path_policy_snapshot& entry : entries) {
            if (entry.catalog_path == catalog_path) {
                return &entry;
            }
        }
        return nullptr;
    }
};

struct asset_manifest_path_policy_validation_summary {
    asset_path_policy_catalog policy_catalog;
    asset_path_policy_catalog_snapshot_view snapshot_view;
    std::vector<asset_path_policy_kind_counts> manifest_kind_counts;
    std::vector<asset_path_policy_duplicate_source_report> duplicate_sources;
    std::vector<asset_path_policy_duplicate_catalog_path_report> duplicate_catalog_paths;
    std::vector<asset_path_policy_duplicate_cache_key_report> duplicate_cache_keys;

    [[nodiscard]] bool ok() const
    {
        return policy_catalog.ok() && duplicate_sources.empty() && duplicate_catalog_paths.empty()
            && duplicate_cache_keys.empty();
    }
};

inline asset_path_policy_rule asset_path_policy_for_type(asset_type type)
{
    switch (type) {
        case asset_type::font:
            return asset_path_policy_rule{.type = type, .catalog_group = "fonts"};
        case asset_type::image:
            return asset_path_policy_rule{.type = type, .catalog_group = "images"};
        case asset_type::sound:
            return asset_path_policy_rule{.type = type, .catalog_group = "sounds"};
        case asset_type::shader:
            return asset_path_policy_rule{.type = type, .catalog_group = "shaders"};
        case asset_type::deck:
            return asset_path_policy_rule{.type = type, .catalog_group = "decks"};
        case asset_type::generic:
            return asset_path_policy_rule{};
    }
    return asset_path_policy_rule{};
}

inline bool asset_path_policy_type_supported(asset_type type)
{
    return !asset_path_policy_for_type(type).catalog_group.empty();
}

namespace detail {

struct asset_path_policy_path_result {
    asset_path_policy_status status = asset_path_policy_status::invalid_path;
    std::string path;
    std::string diagnostic;

    [[nodiscard]] bool ok() const
    {
        return status == asset_path_policy_status::accepted;
    }
};

inline std::string lowercase_asset_path_extension(std::string_view path)
{
    const std::string_view::size_type slash = path.find_last_of('/');
    const std::string_view::size_type dot = path.find_last_of('.');
    if (dot == std::string_view::npos || dot == path.size() - 1U) {
        return {};
    }
    if (slash != std::string_view::npos && dot < slash) {
        return {};
    }

    std::string extension(path.substr(dot));
    std::ranges::transform(extension, extension.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return extension;
}

inline bool extension_matches_any(std::string_view extension, std::initializer_list<std::string_view> allowed)
{
    for (std::string_view candidate : allowed) {
        if (extension == candidate) {
            return true;
        }
    }
    return false;
}

inline bool extension_allowed_for_asset_type(asset_type type, std::string_view extension)
{
    switch (type) {
        case asset_type::font:
            return extension_matches_any(extension, {".ttf", ".otf", ".ttc", ".otc", ".woff", ".woff2"});
        case asset_type::image:
            return extension_matches_any(extension, {".png", ".jpg", ".jpeg", ".webp", ".bmp", ".tga"});
        case asset_type::sound:
            return extension_matches_any(extension, {".wav", ".ogg", ".mp3", ".flac"});
        case asset_type::shader:
            return extension_matches_any(
                extension,
                {".spv", ".vert", ".frag", ".comp", ".geom", ".tesc", ".tese", ".glsl"});
        case asset_type::deck:
            return extension_matches_any(extension, {".quiz", ".json", ".yaml", ".yml"});
        case asset_type::generic:
            return false;
    }
    return false;
}

inline asset_path_policy_status policy_status_from_resolve_status(asset_resolve_status status)
{
    if (status == asset_resolve_status::path_traversal) {
        return asset_path_policy_status::path_traversal;
    }
    return asset_path_policy_status::invalid_path;
}

inline asset_path_policy_path_result normalize_policy_source_path(const resolved_asset_source& source)
{
    asset_path_policy_path_result result;
    std::string_view path;
    bool strip_leading_slashes = false;

    if (source.kind == asset_source_kind::asset_uri) {
        path = asset_manifest_asset_path_from_uri(source.normalized_uri);
        strip_leading_slashes = true;
    } else if (source.kind == asset_source_kind::local_path) {
        path = source.normalized_uri;
    } else {
        result.status = asset_path_policy_status::unsupported_source_kind;
        result.diagnostic = "asset path policy accepts only asset uri and local path sources";
        return result;
    }

    const normalized_asset_path_result normalized = normalize_asset_path_segments(
        path,
        true,
        strip_leading_slashes,
        true);
    if (!normalized.ok()) {
        result.status = policy_status_from_resolve_status(normalized.status);
        result.diagnostic = normalized.diagnostic;
        return result;
    }

    result.status = asset_path_policy_status::accepted;
    result.path = normalized.path;
    return result;
}

inline bool path_starts_with_catalog_group(std::string_view path, std::string_view catalog_group)
{
    return path == catalog_group
        || (path.size() > catalog_group.size() && path.substr(0U, catalog_group.size()) == catalog_group
            && path[catalog_group.size()] == '/');
}

inline std::string make_policy_catalog_path(std::string_view catalog_group, std::string_view source_path)
{
    if (path_starts_with_catalog_group(source_path, catalog_group)) {
        return std::string(source_path);
    }

    std::string catalog_path;
    catalog_path.reserve(catalog_group.size() + 1U + source_path.size());
    catalog_path.append(catalog_group);
    catalog_path.push_back('/');
    catalog_path.append(source_path);
    return catalog_path;
}

inline bool path_contains_parent_reference(const std::filesystem::path& path)
{
    for (const std::filesystem::path& part : path) {
        if (part == "..") {
            return true;
        }
    }
    return false;
}

inline bool rooted_path_valid_for_policy(const std::optional<std::filesystem::path>& path)
{
    return !path.has_value() || (!path->empty() && path->is_absolute() && !path_contains_parent_reference(*path));
}

inline void add_asset_path_policy_diagnostic(
    asset_path_policy_catalog& catalog,
    asset_path_policy_status status,
    std::string id,
    asset_type type,
    asset_cache_key cache_key,
    std::string diagnostic,
    std::string related_id = {},
    std::string catalog_path = {})
{
    catalog.diagnostics.push_back(asset_path_policy_diagnostic{
        .status = status,
        .id = std::move(id),
        .related_id = std::move(related_id),
        .type = type,
        .cache_key = std::move(cache_key),
        .catalog_path = std::move(catalog_path),
        .diagnostic = std::move(diagnostic),
    });
}

inline void add_runtime_catalog_diagnostics_to_policy_catalog(
    asset_path_policy_catalog& catalog,
    const runtime_asset_catalog& runtime_catalog)
{
    for (const runtime_asset_catalog_diagnostic& diagnostic : runtime_catalog.diagnostics) {
        add_asset_path_policy_diagnostic(
            catalog,
            asset_path_policy_status::unresolved_source,
            diagnostic.id,
            asset_type::generic,
            diagnostic.cache_key,
            diagnostic.diagnostic);
    }
}

inline int asset_path_policy_type_rank(asset_type type)
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

inline asset_path_policy_kind_counts& find_or_add_policy_kind_counts(
    std::vector<asset_path_policy_kind_counts>& counts,
    asset_type type)
{
    for (asset_path_policy_kind_counts& count : counts) {
        if (count.type == type) {
            return count;
        }
    }
    counts.push_back(asset_path_policy_kind_counts{.type = type});
    return counts.back();
}

inline asset_type manifest_policy_diagnostic_type(
    const asset_manifest& manifest,
    const asset_path_policy_diagnostic& diagnostic)
{
    if (diagnostic.type != asset_type::generic) {
        return diagnostic.type;
    }
    if (const asset_manifest_entry* entry = manifest.find_entry(diagnostic.id); entry != nullptr) {
        return entry->type;
    }
    return asset_type::generic;
}

inline void sort_policy_duplicate_entry_ids(std::vector<std::string>& entry_ids)
{
    std::ranges::sort(entry_ids);
    entry_ids.erase(std::ranges::unique(entry_ids).begin(), entry_ids.end());
}

inline void filter_single_policy_duplicate_sources(std::vector<asset_path_policy_duplicate_source_report>& reports)
{
    for (asset_path_policy_duplicate_source_report& report : reports) {
        sort_policy_duplicate_entry_ids(report.entry_ids);
    }
    reports.erase(
        std::ranges::remove_if(reports, [](const asset_path_policy_duplicate_source_report& report) {
            return report.entry_ids.size() < 2U;
        }).begin(),
        reports.end());
}

inline void filter_single_policy_duplicate_catalog_paths(
    std::vector<asset_path_policy_duplicate_catalog_path_report>& reports)
{
    for (asset_path_policy_duplicate_catalog_path_report& report : reports) {
        sort_policy_duplicate_entry_ids(report.entry_ids);
    }
    reports.erase(
        std::ranges::remove_if(reports, [](const asset_path_policy_duplicate_catalog_path_report& report) {
            return report.entry_ids.size() < 2U;
        }).begin(),
        reports.end());
}

inline void filter_single_policy_duplicate_cache_keys(
    std::vector<asset_path_policy_duplicate_cache_key_report>& reports)
{
    for (asset_path_policy_duplicate_cache_key_report& report : reports) {
        sort_policy_duplicate_entry_ids(report.entry_ids);
    }
    reports.erase(
        std::ranges::remove_if(reports, [](const asset_path_policy_duplicate_cache_key_report& report) {
            return report.entry_ids.size() < 2U;
        }).begin(),
        reports.end());
}

} // namespace detail

inline asset_path_policy_result apply_asset_path_policy(const runtime_asset_catalog_snapshot& snapshot)
{
    asset_path_policy_result result;
    result.asset = asset_path_policy_snapshot{
        .id = snapshot.entry.id,
        .type = snapshot.entry.type,
        .cache_key = snapshot.cache_key,
        .source_uri = snapshot.source.normalized_uri,
        .rooted_path = snapshot.rooted_path,
    };

    const asset_path_policy_rule rule = asset_path_policy_for_type(snapshot.entry.type);
    if (rule.catalog_group.empty()) {
        result.status = asset_path_policy_status::unsupported_asset_type;
        result.diagnostic = "asset path policy requires a concrete asset type";
        return result;
    }

    const detail::asset_path_policy_path_result path = detail::normalize_policy_source_path(snapshot.source);
    if (!path.ok()) {
        result.status = path.status;
        result.diagnostic = path.diagnostic;
        return result;
    }

    const std::string extension = detail::lowercase_asset_path_extension(path.path);
    if (!detail::extension_allowed_for_asset_type(snapshot.entry.type, extension)) {
        result.status = asset_path_policy_status::unsupported_extension;
        result.diagnostic = "asset path extension is not allowed for the asset type";
        return result;
    }

    if (!detail::rooted_path_valid_for_policy(snapshot.rooted_path)) {
        result.status = asset_path_policy_status::invalid_rooted_path;
        result.diagnostic = "asset rooted path must be absolute and cannot contain parent traversal";
        return result;
    }

    const asset_cache_key expected_cache_key = make_manifest_asset_cache_key(snapshot.entry, snapshot.source);
    if (snapshot.cache_key != expected_cache_key) {
        result.status = asset_path_policy_status::cache_key_mismatch;
        result.diagnostic = "asset path policy cache key does not match the normalized source";
        return result;
    }

    result.asset.source_path = path.path;
    result.asset.catalog_path = detail::make_policy_catalog_path(rule.catalog_group, result.asset.source_path);
    result.status = asset_path_policy_status::accepted;
    return result;
}

inline asset_path_policy_catalog build_asset_path_policy_catalog(const runtime_asset_catalog& runtime_catalog)
{
    asset_path_policy_catalog catalog;
    detail::add_runtime_catalog_diagnostics_to_policy_catalog(catalog, runtime_catalog);

    for (const runtime_asset_catalog_snapshot& snapshot : runtime_catalog.assets) {
        asset_path_policy_result policy = apply_asset_path_policy(snapshot);
        if (!policy.ok()) {
            detail::add_asset_path_policy_diagnostic(
                catalog,
                policy.status,
                policy.asset.id,
                policy.asset.type,
                policy.asset.cache_key,
                policy.diagnostic);
            continue;
        }

        if (const asset_path_policy_snapshot* existing = catalog.find_cache_key(policy.asset.cache_key);
            existing != nullptr && existing->catalog_path != policy.asset.catalog_path) {
            detail::add_asset_path_policy_diagnostic(
                catalog,
                asset_path_policy_status::cache_key_collision,
                policy.asset.id,
                policy.asset.type,
                policy.asset.cache_key,
                "asset path policy cache key maps to multiple catalog paths",
                existing->id,
                policy.asset.catalog_path);
        }

        if (const asset_path_policy_snapshot* existing = catalog.find_catalog_path(policy.asset.catalog_path);
            existing != nullptr && existing->id != policy.asset.id) {
            detail::add_asset_path_policy_diagnostic(
                catalog,
                asset_path_policy_status::duplicate_catalog_path,
                policy.asset.id,
                policy.asset.type,
                policy.asset.cache_key,
                "asset path policy catalog path is duplicated",
                existing->id,
                policy.asset.catalog_path);
        }

        catalog.entries.push_back(std::move(policy.asset));
    }

    return catalog;
}

inline std::vector<asset_path_policy_snapshot> sorted_asset_path_policy_entries(
    const asset_path_policy_catalog& catalog)
{
    std::vector<asset_path_policy_snapshot> entries = catalog.entries;
    std::ranges::sort(entries, [](const asset_path_policy_snapshot& left, const asset_path_policy_snapshot& right) {
        return std::tuple(
                   detail::asset_path_policy_type_rank(left.type),
                   std::string_view(left.catalog_path),
                   std::string_view(left.id),
                   std::string_view(left.cache_key),
                   std::string_view(left.source_uri))
            < std::tuple(
                   detail::asset_path_policy_type_rank(right.type),
                   std::string_view(right.catalog_path),
                   std::string_view(right.id),
                   std::string_view(right.cache_key),
                   std::string_view(right.source_uri));
    });
    return entries;
}

inline std::vector<asset_path_policy_diagnostic> sorted_asset_path_policy_diagnostics(
    const asset_path_policy_catalog& catalog)
{
    std::vector<asset_path_policy_diagnostic> diagnostics = catalog.diagnostics;
    std::ranges::sort(
        diagnostics,
        [](const asset_path_policy_diagnostic& left, const asset_path_policy_diagnostic& right) {
            return std::tuple(
                       detail::asset_path_policy_type_rank(left.type),
                       static_cast<int>(left.status),
                       std::string_view(left.catalog_path),
                       std::string_view(left.id),
                       std::string_view(left.related_id),
                       std::string_view(left.cache_key),
                       std::string_view(left.diagnostic))
                < std::tuple(
                       detail::asset_path_policy_type_rank(right.type),
                       static_cast<int>(right.status),
                       std::string_view(right.catalog_path),
                       std::string_view(right.id),
                       std::string_view(right.related_id),
                       std::string_view(right.cache_key),
                       std::string_view(right.diagnostic));
        });
    return diagnostics;
}

inline std::vector<asset_path_policy_kind_counts> summarize_asset_path_policy_kind_counts(
    const asset_path_policy_catalog& catalog)
{
    std::vector<asset_path_policy_kind_counts> counts;
    for (const asset_path_policy_snapshot& entry : catalog.entries) {
        ++detail::find_or_add_policy_kind_counts(counts, entry.type).accepted_count;
    }
    for (const asset_path_policy_diagnostic& diagnostic : catalog.diagnostics) {
        ++detail::find_or_add_policy_kind_counts(counts, diagnostic.type).rejected_count;
    }

    std::ranges::sort(counts, [](const asset_path_policy_kind_counts& left, const asset_path_policy_kind_counts& right) {
        return detail::asset_path_policy_type_rank(left.type) < detail::asset_path_policy_type_rank(right.type);
    });
    return counts;
}

inline asset_path_policy_catalog_snapshot_view make_asset_path_policy_catalog_snapshot_view(
    const asset_path_policy_catalog& catalog)
{
    return asset_path_policy_catalog_snapshot_view{
        .entries = sorted_asset_path_policy_entries(catalog),
        .diagnostics = sorted_asset_path_policy_diagnostics(catalog),
        .kind_counts = summarize_asset_path_policy_kind_counts(catalog),
    };
}

inline std::vector<asset_path_policy_kind_counts> summarize_asset_manifest_path_policy_kind_counts(
    const asset_manifest& manifest,
    const asset_path_policy_catalog& catalog)
{
    std::vector<asset_path_policy_kind_counts> counts;
    for (const asset_manifest_entry& entry : manifest.entries) {
        (void)detail::find_or_add_policy_kind_counts(counts, entry.type);
    }
    for (const asset_path_policy_snapshot& entry : catalog.entries) {
        ++detail::find_or_add_policy_kind_counts(counts, entry.type).accepted_count;
    }
    for (const asset_path_policy_diagnostic& diagnostic : catalog.diagnostics) {
        const asset_type type = detail::manifest_policy_diagnostic_type(manifest, diagnostic);
        ++detail::find_or_add_policy_kind_counts(counts, type).rejected_count;
    }

    std::ranges::sort(counts, [](const asset_path_policy_kind_counts& left, const asset_path_policy_kind_counts& right) {
        return detail::asset_path_policy_type_rank(left.type) < detail::asset_path_policy_type_rank(right.type);
    });
    return counts;
}

inline std::vector<asset_path_policy_duplicate_source_report> report_asset_path_policy_duplicate_sources(
    const asset_path_policy_catalog& catalog)
{
    std::vector<asset_path_policy_duplicate_source_report> reports;
    for (const asset_path_policy_snapshot& entry : catalog.entries) {
        auto existing = std::ranges::find_if(reports, [&entry](const asset_path_policy_duplicate_source_report& report) {
            return report.type == entry.type && report.source_uri == entry.source_uri;
        });
        if (existing == reports.end()) {
            reports.push_back(asset_path_policy_duplicate_source_report{
                .type = entry.type,
                .source_uri = entry.source_uri,
                .entry_ids = {entry.id},
            });
        } else {
            existing->entry_ids.push_back(entry.id);
        }
    }

    detail::filter_single_policy_duplicate_sources(reports);
    std::ranges::sort(reports, [](const auto& left, const auto& right) {
        return std::tuple(detail::asset_path_policy_type_rank(left.type), std::string_view(left.source_uri))
            < std::tuple(detail::asset_path_policy_type_rank(right.type), std::string_view(right.source_uri));
    });
    return reports;
}

inline std::vector<asset_path_policy_duplicate_catalog_path_report> report_asset_path_policy_duplicate_catalog_paths(
    const asset_path_policy_catalog& catalog)
{
    std::vector<asset_path_policy_duplicate_catalog_path_report> reports;
    for (const asset_path_policy_snapshot& entry : catalog.entries) {
        auto existing = std::ranges::find_if(
            reports,
            [&entry](const asset_path_policy_duplicate_catalog_path_report& report) {
                return report.type == entry.type && report.catalog_path == entry.catalog_path;
            });
        if (existing == reports.end()) {
            reports.push_back(asset_path_policy_duplicate_catalog_path_report{
                .type = entry.type,
                .catalog_path = entry.catalog_path,
                .entry_ids = {entry.id},
            });
        } else {
            existing->entry_ids.push_back(entry.id);
        }
    }

    detail::filter_single_policy_duplicate_catalog_paths(reports);
    std::ranges::sort(reports, [](const auto& left, const auto& right) {
        return std::tuple(detail::asset_path_policy_type_rank(left.type), std::string_view(left.catalog_path))
            < std::tuple(detail::asset_path_policy_type_rank(right.type), std::string_view(right.catalog_path));
    });
    return reports;
}

inline std::vector<asset_path_policy_duplicate_cache_key_report> report_asset_path_policy_duplicate_cache_keys(
    const asset_path_policy_catalog& catalog)
{
    std::vector<asset_path_policy_duplicate_cache_key_report> reports;
    for (const asset_path_policy_snapshot& entry : catalog.entries) {
        auto existing = std::ranges::find_if(
            reports,
            [&entry](const asset_path_policy_duplicate_cache_key_report& report) {
                return report.type == entry.type && report.cache_key == entry.cache_key;
            });
        if (existing == reports.end()) {
            reports.push_back(asset_path_policy_duplicate_cache_key_report{
                .type = entry.type,
                .cache_key = entry.cache_key,
                .entry_ids = {entry.id},
            });
        } else {
            existing->entry_ids.push_back(entry.id);
        }
    }

    detail::filter_single_policy_duplicate_cache_keys(reports);
    std::ranges::sort(reports, [](const auto& left, const auto& right) {
        return std::tuple(detail::asset_path_policy_type_rank(left.type), std::string_view(left.cache_key))
            < std::tuple(detail::asset_path_policy_type_rank(right.type), std::string_view(right.cache_key));
    });
    return reports;
}

inline asset_manifest_path_policy_validation_summary validate_asset_manifest_path_policy(
    const asset_manifest& manifest,
    const asset_resolver_interface& resolver)
{
    asset_manifest_path_policy_validation_summary summary;
    summary.policy_catalog = build_asset_path_policy_catalog(build_runtime_asset_catalog(manifest, resolver));
    summary.snapshot_view = make_asset_path_policy_catalog_snapshot_view(summary.policy_catalog);
    summary.manifest_kind_counts = summarize_asset_manifest_path_policy_kind_counts(manifest, summary.policy_catalog);
    summary.duplicate_sources = report_asset_path_policy_duplicate_sources(summary.policy_catalog);
    summary.duplicate_catalog_paths = report_asset_path_policy_duplicate_catalog_paths(summary.policy_catalog);
    summary.duplicate_cache_keys = report_asset_path_policy_duplicate_cache_keys(summary.policy_catalog);
    return summary;
}

} // namespace quiz_vulkan::assets
