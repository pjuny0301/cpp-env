#pragma once

#include "assets/asset_runtime_resolver_policy.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace quiz_vulkan::assets {

enum class asset_render_resource_address_status {
    accepted,
    unsupported_asset_type,
    unsupported_source_kind,
    resolver_rejected,
    path_traversal_rejected,
    missing_root,
    invalid_root_path,
    cache_key_component_mismatch,
    duplicate_manifest_id,
    invalid_manifest_entry,
};

enum class asset_render_resource_address_kind {
    canonical_asset_uri,
    local_file,
    local_fixture_file,
    build_external_file,
    unsupported,
};

struct asset_render_resource_cache_key_components {
    asset_cache_key_policy_status status = asset_cache_key_policy_status::empty_key;
    asset_type type = asset_type::generic;
    asset_source_kind source_kind = asset_source_kind::unsupported;
    std::string type_component;
    std::string source_component;
    std::string revision_component;
    std::string source_path;
    std::string cache_revision;

    [[nodiscard]] bool ok() const
    {
        return status == asset_cache_key_policy_status::accepted;
    }

    [[nodiscard]] bool has_cache_revision() const
    {
        return !cache_revision.empty();
    }
};

struct asset_render_resource_address_entry {
    std::size_t manifest_index = 0U;
    std::string id;
    asset_type type = asset_type::generic;
    asset_render_resource_address_kind address_kind = asset_render_resource_address_kind::unsupported;
    asset_source_kind source_kind = asset_source_kind::unsupported;
    asset_runtime_resolver_root_space root_space = asset_runtime_resolver_root_space::none;
    std::string canonical_identity;
    std::string source_uri;
    std::string source_path;
    std::string resolved_root_id;
    asset_cache_key cache_key;
    asset_render_resource_cache_key_components cache_key_components;

    [[nodiscard]] bool ok() const
    {
        return address_kind != asset_render_resource_address_kind::unsupported
            && cache_key_components.ok();
    }
};

struct asset_render_resource_address_diagnostic {
    asset_render_resource_address_status status = asset_render_resource_address_status::resolver_rejected;
    std::size_t manifest_index = 0U;
    std::string id;
    std::string related_id;
    asset_type type = asset_type::generic;
    asset_source_kind source_kind = asset_source_kind::unsupported;
    asset_cache_key cache_key;
    std::string source_uri;
    std::string diagnostic;
};

struct asset_render_resource_address_summary {
    std::vector<asset_render_resource_address_entry> entries;
    std::vector<asset_render_resource_address_diagnostic> diagnostics;
    std::size_t accepted_count = 0U;
    std::size_t rejected_count = 0U;
    std::size_t canonical_asset_uri_count = 0U;
    std::size_t local_file_count = 0U;
    std::size_t local_fixture_boundary_count = 0U;
    std::size_t build_external_boundary_count = 0U;
    std::size_t path_traversal_rejection_count = 0U;
    std::size_t unsupported_asset_type_count = 0U;
    std::size_t cache_key_component_mismatch_count = 0U;

    [[nodiscard]] bool ok() const
    {
        return diagnostics.empty();
    }

    [[nodiscard]] std::size_t entry_count() const
    {
        return entries.size();
    }

    [[nodiscard]] std::size_t diagnostic_count() const
    {
        return diagnostics.size();
    }

    [[nodiscard]] const asset_render_resource_address_entry* find_entry(std::string_view id) const
    {
        for (const asset_render_resource_address_entry& entry : entries) {
            if (entry.id == id) {
                return &entry;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const asset_render_resource_address_entry* find_cache_key(
        std::string_view cache_key) const
    {
        for (const asset_render_resource_address_entry& entry : entries) {
            if (entry.cache_key == cache_key) {
                return &entry;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const asset_render_resource_address_entry* find_canonical_identity(
        std::string_view canonical_identity) const
    {
        for (const asset_render_resource_address_entry& entry : entries) {
            if (entry.canonical_identity == canonical_identity) {
                return &entry;
            }
        }
        return nullptr;
    }
};

inline bool asset_render_resource_address_type_supported(asset_type type)
{
    switch (type) {
        case asset_type::font:
        case asset_type::image:
        case asset_type::sound:
        case asset_type::shader:
        case asset_type::deck:
            return true;
        case asset_type::generic:
            return false;
    }
    return false;
}

inline asset_render_resource_address_status asset_render_resource_address_status_from_resolver_policy(
    asset_runtime_resolver_policy_status status)
{
    switch (status) {
        case asset_runtime_resolver_policy_status::accepted:
            return asset_render_resource_address_status::accepted;
        case asset_runtime_resolver_policy_status::duplicate_manifest_id:
            return asset_render_resource_address_status::duplicate_manifest_id;
        case asset_runtime_resolver_policy_status::invalid_manifest_entry:
            return asset_render_resource_address_status::invalid_manifest_entry;
        case asset_runtime_resolver_policy_status::path_traversal_rejected:
            return asset_render_resource_address_status::path_traversal_rejected;
        case asset_runtime_resolver_policy_status::missing_root:
            return asset_render_resource_address_status::missing_root;
        case asset_runtime_resolver_policy_status::invalid_root_path:
            return asset_render_resource_address_status::invalid_root_path;
        case asset_runtime_resolver_policy_status::cache_key_mismatch:
            return asset_render_resource_address_status::cache_key_component_mismatch;
        case asset_runtime_resolver_policy_status::resolver_rejected:
            return asset_render_resource_address_status::resolver_rejected;
    }
    return asset_render_resource_address_status::resolver_rejected;
}

inline asset_render_resource_cache_key_components make_asset_render_resource_cache_key_components(
    std::string_view cache_key)
{
    const asset_cache_key_classification classification = classify_asset_cache_key(cache_key);
    return asset_render_resource_cache_key_components{
        .status = classification.status,
        .type = classification.type,
        .source_kind = classification.source_kind,
        .type_component = classification.type_component,
        .source_component = classification.source_component,
        .revision_component = classification.revision_component,
        .source_path = classification.source_path,
        .cache_revision = classification.cache_revision,
    };
}

inline asset_render_resource_address_kind asset_render_resource_address_kind_for_source(
    asset_source_kind source_kind,
    asset_runtime_resolver_root_space root_space)
{
    if (source_kind == asset_source_kind::asset_uri) {
        return asset_render_resource_address_kind::canonical_asset_uri;
    }
    if (source_kind != asset_source_kind::local_path) {
        return asset_render_resource_address_kind::unsupported;
    }
    if (root_space == asset_runtime_resolver_root_space::build_external) {
        return asset_render_resource_address_kind::build_external_file;
    }
    if (root_space == asset_runtime_resolver_root_space::local_fixture) {
        return asset_render_resource_address_kind::local_fixture_file;
    }
    return asset_render_resource_address_kind::local_file;
}

inline std::string make_asset_render_resource_local_identity(
    asset_render_resource_address_kind kind,
    std::string_view root_id,
    std::string_view source_path)
{
    std::string identity;
    if (kind == asset_render_resource_address_kind::build_external_file) {
        identity = "build-external://";
    } else if (kind == asset_render_resource_address_kind::local_fixture_file) {
        identity = "local-fixture://";
    } else {
        identity = "local://";
    }
    if (!root_id.empty()) {
        identity.append(root_id);
        identity.push_back('/');
    }
    identity.append(source_path);
    return identity;
}

inline std::string make_asset_render_resource_canonical_identity(
    asset_render_resource_address_kind kind,
    std::string_view source_uri,
    std::string_view root_id,
    std::string_view source_path)
{
    if (kind == asset_render_resource_address_kind::canonical_asset_uri) {
        return std::string(source_uri);
    }
    if (kind == asset_render_resource_address_kind::unsupported) {
        return {};
    }
    return make_asset_render_resource_local_identity(kind, root_id, source_path);
}

inline void count_asset_render_resource_address_entry(
    asset_render_resource_address_summary& summary,
    const asset_render_resource_address_entry& entry)
{
    ++summary.accepted_count;
    switch (entry.address_kind) {
        case asset_render_resource_address_kind::canonical_asset_uri:
            ++summary.canonical_asset_uri_count;
            break;
        case asset_render_resource_address_kind::local_file:
            ++summary.local_file_count;
            break;
        case asset_render_resource_address_kind::local_fixture_file:
            ++summary.local_file_count;
            ++summary.local_fixture_boundary_count;
            break;
        case asset_render_resource_address_kind::build_external_file:
            ++summary.local_file_count;
            ++summary.build_external_boundary_count;
            break;
        case asset_render_resource_address_kind::unsupported:
            break;
    }
}

inline void count_asset_render_resource_address_diagnostic(
    asset_render_resource_address_summary& summary,
    const asset_render_resource_address_diagnostic& diagnostic)
{
    ++summary.rejected_count;
    if (diagnostic.status == asset_render_resource_address_status::path_traversal_rejected) {
        ++summary.path_traversal_rejection_count;
    } else if (diagnostic.status == asset_render_resource_address_status::unsupported_asset_type) {
        ++summary.unsupported_asset_type_count;
    } else if (diagnostic.status == asset_render_resource_address_status::cache_key_component_mismatch) {
        ++summary.cache_key_component_mismatch_count;
    }
}

inline void add_asset_render_resource_address_entry(
    asset_render_resource_address_summary& summary,
    asset_render_resource_address_entry entry)
{
    count_asset_render_resource_address_entry(summary, entry);
    summary.entries.push_back(std::move(entry));
}

inline void add_asset_render_resource_address_diagnostic(
    asset_render_resource_address_summary& summary,
    asset_render_resource_address_diagnostic diagnostic)
{
    count_asset_render_resource_address_diagnostic(summary, diagnostic);
    summary.diagnostics.push_back(std::move(diagnostic));
}

inline asset_render_resource_address_diagnostic make_asset_render_resource_diagnostic_from_resolver_policy(
    const asset_runtime_resolver_policy_diagnostic& diagnostic)
{
    return asset_render_resource_address_diagnostic{
        .status = asset_render_resource_address_status_from_resolver_policy(diagnostic.status),
        .manifest_index = diagnostic.manifest_index,
        .id = diagnostic.id,
        .related_id = diagnostic.related_id,
        .type = diagnostic.type,
        .source_kind = diagnostic.source_kind,
        .cache_key = diagnostic.cache_key,
        .diagnostic = diagnostic.diagnostic,
    };
}

inline asset_render_resource_address_diagnostic make_asset_render_resource_address_diagnostic(
    asset_render_resource_address_status status,
    const asset_runtime_resolver_policy_entry& entry,
    std::string diagnostic)
{
    return asset_render_resource_address_diagnostic{
        .status = status,
        .manifest_index = entry.manifest_index,
        .id = entry.id,
        .type = entry.type,
        .source_kind = entry.source_kind,
        .cache_key = entry.cache_key,
        .source_uri = entry.source_uri,
        .diagnostic = std::move(diagnostic),
    };
}

inline void add_asset_render_resource_address_from_resolver_entry(
    asset_render_resource_address_summary& summary,
    const asset_runtime_resolver_policy_entry& resolver_entry)
{
    if (!asset_render_resource_address_type_supported(resolver_entry.type)) {
        add_asset_render_resource_address_diagnostic(
            summary,
            make_asset_render_resource_address_diagnostic(
                asset_render_resource_address_status::unsupported_asset_type,
                resolver_entry,
                "render resource addressing requires a concrete asset type"));
        return;
    }

    const asset_render_resource_address_kind address_kind =
        asset_render_resource_address_kind_for_source(resolver_entry.source_kind, resolver_entry.root_space);
    if (address_kind == asset_render_resource_address_kind::unsupported) {
        add_asset_render_resource_address_diagnostic(
            summary,
            make_asset_render_resource_address_diagnostic(
                asset_render_resource_address_status::unsupported_source_kind,
                resolver_entry,
                "render resource addressing supports asset uri and local path sources"));
        return;
    }

    const asset_render_resource_cache_key_components cache_key_components =
        make_asset_render_resource_cache_key_components(resolver_entry.cache_key);
    if (!cache_key_components.ok() || cache_key_components.type != resolver_entry.type
        || cache_key_components.source_component != resolver_entry.source_uri) {
        add_asset_render_resource_address_diagnostic(
            summary,
            make_asset_render_resource_address_diagnostic(
                asset_render_resource_address_status::cache_key_component_mismatch,
                resolver_entry,
                "render resource cache key components do not match the resolved source"));
        return;
    }

    add_asset_render_resource_address_entry(
        summary,
        asset_render_resource_address_entry{
            .manifest_index = resolver_entry.manifest_index,
            .id = resolver_entry.id,
            .type = resolver_entry.type,
            .address_kind = address_kind,
            .source_kind = resolver_entry.source_kind,
            .root_space = resolver_entry.root_space,
            .canonical_identity = make_asset_render_resource_canonical_identity(
                address_kind,
                resolver_entry.source_uri,
                resolver_entry.resolved_root_id,
                cache_key_components.source_path),
            .source_uri = resolver_entry.source_uri,
            .source_path = cache_key_components.source_path,
            .resolved_root_id = resolver_entry.resolved_root_id,
            .cache_key = resolver_entry.cache_key,
            .cache_key_components = cache_key_components,
        });
}

inline asset_render_resource_address_summary summarize_asset_render_resource_addresses(
    const asset_manifest& manifest,
    const asset_resolver_interface& resolver)
{
    const asset_runtime_resolver_policy_summary resolver_policy =
        summarize_asset_runtime_resolver_policy(manifest, resolver);
    asset_render_resource_address_summary summary;

    for (const asset_runtime_resolver_policy_entry& entry : resolver_policy.entries) {
        add_asset_render_resource_address_from_resolver_entry(summary, entry);
    }
    for (const asset_runtime_resolver_policy_diagnostic& diagnostic : resolver_policy.diagnostics) {
        add_asset_render_resource_address_diagnostic(
            summary,
            make_asset_render_resource_diagnostic_from_resolver_policy(diagnostic));
    }

    return summary;
}

inline std::string asset_render_resource_address_status_name(asset_render_resource_address_status status)
{
    switch (status) {
        case asset_render_resource_address_status::accepted:
            return "accepted";
        case asset_render_resource_address_status::unsupported_asset_type:
            return "unsupported_asset_type";
        case asset_render_resource_address_status::unsupported_source_kind:
            return "unsupported_source_kind";
        case asset_render_resource_address_status::resolver_rejected:
            return "resolver_rejected";
        case asset_render_resource_address_status::path_traversal_rejected:
            return "path_traversal_rejected";
        case asset_render_resource_address_status::missing_root:
            return "missing_root";
        case asset_render_resource_address_status::invalid_root_path:
            return "invalid_root_path";
        case asset_render_resource_address_status::cache_key_component_mismatch:
            return "cache_key_component_mismatch";
        case asset_render_resource_address_status::duplicate_manifest_id:
            return "duplicate_manifest_id";
        case asset_render_resource_address_status::invalid_manifest_entry:
            return "invalid_manifest_entry";
    }
    return "resolver_rejected";
}

inline std::string asset_render_resource_address_kind_name(asset_render_resource_address_kind kind)
{
    switch (kind) {
        case asset_render_resource_address_kind::canonical_asset_uri:
            return "canonical_asset_uri";
        case asset_render_resource_address_kind::local_file:
            return "local_file";
        case asset_render_resource_address_kind::local_fixture_file:
            return "local_fixture_file";
        case asset_render_resource_address_kind::build_external_file:
            return "build_external_file";
        case asset_render_resource_address_kind::unsupported:
            return "unsupported";
    }
    return "unsupported";
}

} // namespace quiz_vulkan::assets
