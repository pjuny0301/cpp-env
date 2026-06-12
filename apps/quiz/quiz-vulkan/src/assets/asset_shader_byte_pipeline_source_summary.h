#pragma once

#include "assets/asset_typed_materialized_bytes.h"
#include "assets/asset_pack_index.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace quiz_vulkan::assets {

enum class asset_shader_byte_pipeline_source_kind {
    manifest,
    fallback,
    local_fixture,
    build_external,
    missing_source,
};

enum class asset_shader_byte_pipeline_blocker_kind {
    none,
    missing_manifest_entry,
    missing_source,
    stale_manifest_entry,
    traversal_rejected,
    missing_root,
    invalid_root_path,
    materialization_blocked,
    byte_load_blocked,
    integrity_failure,
    empty_shader_bytes,
    non_spirv_magic,
    duplicate_id,
};

struct asset_shader_byte_pipeline_source_entry {
    std::string id;
    asset_shader_byte_pipeline_source_kind source_kind =
        asset_shader_byte_pipeline_source_kind::missing_source;
    asset_shader_byte_pipeline_blocker_kind blocker =
        asset_shader_byte_pipeline_blocker_kind::missing_source;
    bool manifest_entry_found = false;
    bool fallback_selected = false;
    bool traversal_rejected = false;
    asset_runtime_resolver_root_space root_space = asset_runtime_resolver_root_space::none;
    std::string requested_root_id;
    std::string selected_root_id;
    asset_cache_key cache_key;
    std::string source_uri;
    std::filesystem::path materialized_path;
    std::size_t byte_count = 0U;
    std::size_t payload_byte_count = 0U;
    std::string content_hash;
    std::string materialized_byte_identity;
    bool ready = false;
    std::string diagnostic;

    [[nodiscard]] bool ok() const
    {
        return ready && blocker == asset_shader_byte_pipeline_blocker_kind::none;
    }
};

struct asset_shader_byte_pipeline_source_summary {
    std::vector<asset_shader_byte_pipeline_source_entry> ready;
    std::vector<asset_shader_byte_pipeline_source_entry> blocked;
    std::size_t input_shader_count = 0U;
    std::size_t requested_shader_count = 0U;
    std::size_t manifest_source_count = 0U;
    std::size_t fallback_source_count = 0U;
    std::size_t local_fixture_source_count = 0U;
    std::size_t build_external_source_count = 0U;
    std::size_t missing_source_count = 0U;
    std::size_t traversal_rejection_count = 0U;
    std::size_t missing_manifest_count = 0U;
    std::size_t stale_manifest_count = 0U;
    std::size_t missing_root_count = 0U;

    [[nodiscard]] bool ok() const
    {
        return blocked.empty();
    }

    [[nodiscard]] std::size_t ready_count() const
    {
        return ready.size();
    }

    [[nodiscard]] std::size_t blocked_count() const
    {
        return blocked.size();
    }

    [[nodiscard]] std::size_t entry_count() const
    {
        return ready_count() + blocked_count();
    }

    [[nodiscard]] const asset_shader_byte_pipeline_source_entry* find_ready(
        std::string_view id) const
    {
        for (const asset_shader_byte_pipeline_source_entry& entry : ready) {
            if (entry.id == id) {
                return &entry;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const asset_shader_byte_pipeline_source_entry* find_blocked(
        std::string_view id) const
    {
        for (const asset_shader_byte_pipeline_source_entry& entry : blocked) {
            if (entry.id == id) {
                return &entry;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const asset_shader_byte_pipeline_source_entry* find_entry(
        std::string_view id) const
    {
        if (const asset_shader_byte_pipeline_source_entry* entry = find_ready(id); entry != nullptr) {
            return entry;
        }
        return find_blocked(id);
    }
};

enum class asset_shader_payload_runtime_stage {
    unknown,
    vertex,
    fragment,
    compute,
    geometry,
    tessellation_control,
    tessellation_evaluation,
};

enum class asset_shader_payload_runtime_delta_kind {
    reused,
    added,
    removed,
    replaced,
    invalidated,
};

struct asset_shader_payload_runtime_entry {
    std::string id;
    asset_shader_payload_runtime_stage stage = asset_shader_payload_runtime_stage::unknown;
    asset_shader_byte_pipeline_source_kind source_kind =
        asset_shader_byte_pipeline_source_kind::missing_source;
    asset_shader_byte_pipeline_blocker_kind blocker =
        asset_shader_byte_pipeline_blocker_kind::missing_source;
    asset_cache_key cache_key;
    std::string source_uri;
    std::filesystem::path materialized_path;
    std::size_t byte_count = 0U;
    std::size_t payload_byte_count = 0U;
    std::string content_hash;
    std::string cache_revision;
    std::string materialized_byte_identity;
    std::string runtime_identity;
    bool ready = false;
    std::string diagnostic;

    [[nodiscard]] bool ok() const
    {
        return ready && blocker == asset_shader_byte_pipeline_blocker_kind::none
            && !runtime_identity.empty();
    }
};

struct asset_shader_payload_runtime_summary {
    std::vector<asset_shader_payload_runtime_entry> ready;
    std::vector<asset_shader_payload_runtime_entry> blocked;
    std::size_t input_shader_count = 0U;
    std::size_t requested_shader_count = 0U;
    std::size_t vertex_stage_count = 0U;
    std::size_t fragment_stage_count = 0U;
    std::size_t compute_stage_count = 0U;
    std::size_t geometry_stage_count = 0U;
    std::size_t tessellation_control_stage_count = 0U;
    std::size_t tessellation_evaluation_stage_count = 0U;
    std::size_t unknown_stage_count = 0U;
    std::size_t revisioned_count = 0U;
    std::size_t missing_revision_count = 0U;

    [[nodiscard]] bool ok() const
    {
        return blocked.empty();
    }

    [[nodiscard]] std::size_t ready_count() const
    {
        return ready.size();
    }

    [[nodiscard]] std::size_t blocked_count() const
    {
        return blocked.size();
    }

    [[nodiscard]] std::size_t entry_count() const
    {
        return ready_count() + blocked_count();
    }

    [[nodiscard]] const asset_shader_payload_runtime_entry* find_ready(
        std::string_view id) const
    {
        for (const asset_shader_payload_runtime_entry& entry : ready) {
            if (entry.id == id) {
                return &entry;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const asset_shader_payload_runtime_entry* find_blocked(
        std::string_view id) const
    {
        for (const asset_shader_payload_runtime_entry& entry : blocked) {
            if (entry.id == id) {
                return &entry;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const asset_shader_payload_runtime_entry* find_entry(
        std::string_view id) const
    {
        if (const asset_shader_payload_runtime_entry* entry = find_ready(id); entry != nullptr) {
            return entry;
        }
        return find_blocked(id);
    }

    [[nodiscard]] const asset_shader_payload_runtime_entry* find_runtime_identity(
        std::string_view runtime_identity) const
    {
        for (const asset_shader_payload_runtime_entry& entry : ready) {
            if (entry.runtime_identity == runtime_identity) {
                return &entry;
            }
        }
        return nullptr;
    }
};

struct asset_shader_payload_runtime_diff_entry {
    asset_shader_payload_runtime_delta_kind kind =
        asset_shader_payload_runtime_delta_kind::reused;
    std::string id;
    asset_shader_payload_runtime_stage stage = asset_shader_payload_runtime_stage::unknown;
    std::optional<asset_shader_payload_runtime_entry> before;
    std::optional<asset_shader_payload_runtime_entry> after;
    bool stage_changed = false;
    bool cache_key_changed = false;
    bool cache_revision_changed = false;
    bool missing_revision_changed = false;
    bool byte_count_changed = false;
    bool payload_byte_count_changed = false;
    bool content_hash_changed = false;
    bool runtime_identity_changed = false;
    bool materialized_path_changed = false;
    bool readiness_changed = false;
    bool blocker_changed = false;
    bool before_missing_revision = false;
    bool after_missing_revision = false;
    std::string invalidated_runtime_identity;
    std::string replacement_runtime_identity;
    std::string diagnostic;

    [[nodiscard]] bool invalidates() const
    {
        return kind == asset_shader_payload_runtime_delta_kind::removed
            || kind == asset_shader_payload_runtime_delta_kind::replaced
            || kind == asset_shader_payload_runtime_delta_kind::invalidated;
    }

    [[nodiscard]] bool has_field_delta() const
    {
        return stage_changed || cache_key_changed || cache_revision_changed || missing_revision_changed
            || byte_count_changed || payload_byte_count_changed || content_hash_changed
            || runtime_identity_changed || materialized_path_changed || readiness_changed
            || blocker_changed;
    }
};

struct asset_shader_payload_runtime_diff_summary {
    std::vector<asset_shader_payload_runtime_diff_entry> reused;
    std::vector<asset_shader_payload_runtime_diff_entry> added;
    std::vector<asset_shader_payload_runtime_diff_entry> removed;
    std::vector<asset_shader_payload_runtime_diff_entry> replaced;
    std::vector<asset_shader_payload_runtime_diff_entry> invalidated;
    std::ptrdiff_t requested_delta = 0;
    std::ptrdiff_t ready_delta = 0;
    std::ptrdiff_t blocked_delta = 0;
    std::ptrdiff_t missing_revision_delta = 0;

    [[nodiscard]] bool empty() const
    {
        return added.empty() && removed.empty() && replaced.empty() && invalidated.empty();
    }

    [[nodiscard]] std::size_t change_count() const
    {
        return added.size() + removed.size() + replaced.size() + invalidated.size();
    }

    [[nodiscard]] std::size_t invalidation_count() const
    {
        return removed.size() + replaced.size() + invalidated.size();
    }

    [[nodiscard]] const asset_shader_payload_runtime_diff_entry* find_reused(
        std::string_view id) const
    {
        for (const asset_shader_payload_runtime_diff_entry& entry : reused) {
            if (entry.id == id) {
                return &entry;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const asset_shader_payload_runtime_diff_entry* find_added(
        std::string_view id) const
    {
        for (const asset_shader_payload_runtime_diff_entry& entry : added) {
            if (entry.id == id) {
                return &entry;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const asset_shader_payload_runtime_diff_entry* find_removed(
        std::string_view id) const
    {
        for (const asset_shader_payload_runtime_diff_entry& entry : removed) {
            if (entry.id == id) {
                return &entry;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const asset_shader_payload_runtime_diff_entry* find_replaced(
        std::string_view id) const
    {
        for (const asset_shader_payload_runtime_diff_entry& entry : replaced) {
            if (entry.id == id) {
                return &entry;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const asset_shader_payload_runtime_diff_entry* find_invalidated(
        std::string_view id) const
    {
        for (const asset_shader_payload_runtime_diff_entry& entry : invalidated) {
            if (entry.id == id) {
                return &entry;
            }
        }
        return nullptr;
    }
};

inline std::string asset_shader_payload_runtime_stage_name(
    asset_shader_payload_runtime_stage stage)
{
    switch (stage) {
        case asset_shader_payload_runtime_stage::unknown:
            return "unknown";
        case asset_shader_payload_runtime_stage::vertex:
            return "vertex";
        case asset_shader_payload_runtime_stage::fragment:
            return "fragment";
        case asset_shader_payload_runtime_stage::compute:
            return "compute";
        case asset_shader_payload_runtime_stage::geometry:
            return "geometry";
        case asset_shader_payload_runtime_stage::tessellation_control:
            return "tessellation_control";
        case asset_shader_payload_runtime_stage::tessellation_evaluation:
            return "tessellation_evaluation";
    }
    return "unknown";
}

namespace detail {

inline const asset_pack_index_root_selection_entry* find_shader_byte_pipeline_root_selection_entry(
    const asset_pack_index_root_selection_summary& root_selection,
    std::string_view id)
{
    for (const asset_pack_index_root_selection_entry& entry : root_selection.entries) {
        if (entry.id == id) {
            return &entry;
        }
    }
    return nullptr;
}

inline const asset_runtime_resolver_policy_diagnostic* find_shader_byte_pipeline_resolver_diagnostic(
    const asset_runtime_resolver_policy_summary& resolver_policy,
    std::string_view id)
{
    for (const asset_runtime_resolver_policy_diagnostic& diagnostic : resolver_policy.diagnostics) {
        if (diagnostic.id == id) {
            return &diagnostic;
        }
    }
    return nullptr;
}

inline std::string make_shader_byte_pipeline_materialized_identity(
    const asset_cache_key& cache_key,
    std::string_view content_hash,
    std::size_t payload_byte_count)
{
    if (cache_key.empty() && content_hash.empty() && payload_byte_count == 0U) {
        return {};
    }
    return cache_key + "|hash=" + std::string(content_hash) + "|bytes=" + std::to_string(payload_byte_count);
}

inline asset_shader_byte_pipeline_source_kind shader_byte_pipeline_source_kind_for_resolver_entry(
    const asset_runtime_resolver_policy_entry& resolver_entry,
    const asset_pack_index_root_selection_entry* root_selection)
{
    if (root_selection != nullptr
        && root_selection->status == asset_pack_index_root_selection_status::selected_fallback_root) {
        return asset_shader_byte_pipeline_source_kind::fallback;
    }

    switch (resolver_entry.root_space) {
        case asset_runtime_resolver_root_space::local_fixture:
            return asset_shader_byte_pipeline_source_kind::local_fixture;
        case asset_runtime_resolver_root_space::build_external:
            return asset_shader_byte_pipeline_source_kind::build_external;
        case asset_runtime_resolver_root_space::none:
            return asset_shader_byte_pipeline_source_kind::manifest;
    }
    return asset_shader_byte_pipeline_source_kind::manifest;
}

inline asset_shader_byte_pipeline_blocker_kind shader_byte_pipeline_blocker_from_resolver_diagnostic(
    const asset_runtime_resolver_policy_diagnostic& diagnostic)
{
    switch (diagnostic.status) {
        case asset_runtime_resolver_policy_status::path_traversal_rejected:
            return asset_shader_byte_pipeline_blocker_kind::traversal_rejected;
        case asset_runtime_resolver_policy_status::missing_root:
            return asset_shader_byte_pipeline_blocker_kind::missing_root;
        case asset_runtime_resolver_policy_status::invalid_root_path:
            return asset_shader_byte_pipeline_blocker_kind::invalid_root_path;
        case asset_runtime_resolver_policy_status::accepted:
        case asset_runtime_resolver_policy_status::duplicate_manifest_id:
        case asset_runtime_resolver_policy_status::invalid_manifest_entry:
        case asset_runtime_resolver_policy_status::resolver_rejected:
        case asset_runtime_resolver_policy_status::cache_key_mismatch:
            break;
    }
    return asset_shader_byte_pipeline_blocker_kind::missing_source;
}

inline asset_shader_byte_pipeline_blocker_kind shader_byte_pipeline_blocker_from_shader_entry(
    const asset_shader_materialized_byte_pipeline_entry& entry)
{
    if (entry.has_issue(asset_shader_materialized_byte_issue_kind::blocked_materialization)) {
        return asset_shader_byte_pipeline_blocker_kind::materialization_blocked;
    }
    if (entry.has_issue(asset_shader_materialized_byte_issue_kind::blocked_byte_load)) {
        return asset_shader_byte_pipeline_blocker_kind::byte_load_blocked;
    }
    if (entry.has_issue(asset_shader_materialized_byte_issue_kind::integrity_failure)) {
        return asset_shader_byte_pipeline_blocker_kind::integrity_failure;
    }
    if (entry.has_issue(asset_shader_materialized_byte_issue_kind::empty_shader_bytes)) {
        return asset_shader_byte_pipeline_blocker_kind::empty_shader_bytes;
    }
    if (entry.has_issue(asset_shader_materialized_byte_issue_kind::non_spirv_magic)) {
        return asset_shader_byte_pipeline_blocker_kind::non_spirv_magic;
    }
    if (entry.has_issue(asset_shader_materialized_byte_issue_kind::duplicate_id)) {
        return asset_shader_byte_pipeline_blocker_kind::duplicate_id;
    }
    return asset_shader_byte_pipeline_blocker_kind::none;
}

inline asset_shader_byte_pipeline_source_entry make_shader_byte_pipeline_source_entry(
    std::string_view id,
    const asset_shader_materialized_byte_pipeline_entry* shader_entry,
    const asset_runtime_resolver_policy_summary& resolver_policy,
    const asset_pack_index_root_selection_summary& root_selection)
{
    const asset_runtime_resolver_policy_entry* resolver_entry = resolver_policy.find_entry(id);
    const asset_runtime_resolver_policy_diagnostic* resolver_diagnostic =
        find_shader_byte_pipeline_resolver_diagnostic(resolver_policy, id);
    const asset_pack_index_root_selection_entry* root_selection_entry =
        find_shader_byte_pipeline_root_selection_entry(root_selection, id);
    if (resolver_entry != nullptr && resolver_diagnostic != nullptr
        && resolver_diagnostic->status == asset_runtime_resolver_policy_status::duplicate_manifest_id) {
        resolver_diagnostic = nullptr;
    }

    asset_shader_byte_pipeline_source_entry entry{
        .id = std::string(id),
    };

    if (shader_entry != nullptr) {
        entry.cache_key = shader_entry->cache_key;
        entry.source_uri = shader_entry->source_uri;
        entry.materialized_path = shader_entry->materialized_path;
        entry.byte_count = shader_entry->byte_count;
        entry.payload_byte_count = shader_entry->payload_byte_count;
        entry.content_hash = shader_entry->content_hash;
        entry.materialized_byte_identity = make_shader_byte_pipeline_materialized_identity(
            entry.cache_key,
            entry.content_hash,
            entry.payload_byte_count);
        entry.diagnostic = shader_entry->diagnostic;
    }

    if (resolver_entry != nullptr) {
        entry.manifest_entry_found = true;
        entry.root_space = resolver_entry->root_space;
        entry.source_kind = shader_byte_pipeline_source_kind_for_resolver_entry(
            *resolver_entry,
            root_selection_entry);
        if (entry.cache_key.empty()) {
            entry.cache_key = resolver_entry->cache_key;
        }
        if (entry.source_uri.empty()) {
            entry.source_uri = resolver_entry->source_uri;
        }
        if (entry.materialized_path.empty() && resolver_entry->rooted_path.has_value()) {
            entry.materialized_path = resolver_entry->rooted_path->lexically_normal();
        }
    }

    if (root_selection_entry != nullptr) {
        entry.requested_root_id = root_selection_entry->requested_root_id;
        entry.selected_root_id = root_selection_entry->selected_root_id;
        entry.fallback_selected =
            root_selection_entry->status == asset_pack_index_root_selection_status::selected_fallback_root;
    }

    if (resolver_diagnostic != nullptr && entry.cache_key.empty()) {
        entry.cache_key = resolver_diagnostic->cache_key;
    }

    if (resolver_diagnostic != nullptr) {
        entry.blocker = shader_byte_pipeline_blocker_from_resolver_diagnostic(*resolver_diagnostic);
    } else if (resolver_entry == nullptr && shader_entry != nullptr) {
        entry.blocker = asset_shader_byte_pipeline_blocker_kind::stale_manifest_entry;
    } else if (resolver_entry == nullptr) {
        entry.blocker = asset_shader_byte_pipeline_blocker_kind::missing_manifest_entry;
    } else if (shader_entry == nullptr) {
        entry.blocker = asset_shader_byte_pipeline_blocker_kind::missing_source;
    } else {
        entry.blocker = shader_byte_pipeline_blocker_from_shader_entry(*shader_entry);
    }

    entry.traversal_rejected = entry.blocker == asset_shader_byte_pipeline_blocker_kind::traversal_rejected;
    entry.ready = shader_entry != nullptr && shader_entry->ready()
        && entry.blocker == asset_shader_byte_pipeline_blocker_kind::none;

    if (entry.source_kind == asset_shader_byte_pipeline_source_kind::missing_source
        && resolver_entry != nullptr) {
        entry.source_kind = shader_byte_pipeline_source_kind_for_resolver_entry(
            *resolver_entry,
            root_selection_entry);
    }

    if (resolver_diagnostic != nullptr) {
        entry.diagnostic = resolver_diagnostic->diagnostic;
    } else if (entry.blocker == asset_shader_byte_pipeline_blocker_kind::missing_manifest_entry) {
        entry.diagnostic = "shader byte pipeline source is missing from manifest evidence";
    } else if (entry.blocker == asset_shader_byte_pipeline_blocker_kind::stale_manifest_entry) {
        entry.diagnostic = "shader byte pipeline payload has no current manifest evidence";
    } else if (entry.blocker == asset_shader_byte_pipeline_blocker_kind::missing_source) {
        entry.diagnostic = "shader byte pipeline source has no materialized byte payload";
    } else if (entry.diagnostic.empty()) {
        entry.diagnostic =
            entry.ready ? "shader byte pipeline source is ready" : "shader byte pipeline source is blocked";
    }

    return entry;
}

inline bool shader_byte_pipeline_source_summary_has_id(
    const asset_shader_byte_pipeline_source_summary& summary,
    std::string_view id)
{
    return summary.find_entry(id) != nullptr;
}

inline void count_shader_byte_pipeline_source_entry(
    asset_shader_byte_pipeline_source_summary& summary,
    const asset_shader_byte_pipeline_source_entry& entry)
{
    switch (entry.source_kind) {
        case asset_shader_byte_pipeline_source_kind::manifest:
            ++summary.manifest_source_count;
            break;
        case asset_shader_byte_pipeline_source_kind::fallback:
            ++summary.fallback_source_count;
            break;
        case asset_shader_byte_pipeline_source_kind::local_fixture:
            ++summary.local_fixture_source_count;
            break;
        case asset_shader_byte_pipeline_source_kind::build_external:
            ++summary.build_external_source_count;
            break;
        case asset_shader_byte_pipeline_source_kind::missing_source:
            ++summary.missing_source_count;
            break;
    }

    switch (entry.blocker) {
        case asset_shader_byte_pipeline_blocker_kind::none:
            break;
        case asset_shader_byte_pipeline_blocker_kind::missing_manifest_entry:
            ++summary.missing_manifest_count;
            break;
        case asset_shader_byte_pipeline_blocker_kind::stale_manifest_entry:
            ++summary.stale_manifest_count;
            break;
        case asset_shader_byte_pipeline_blocker_kind::traversal_rejected:
            ++summary.traversal_rejection_count;
            break;
        case asset_shader_byte_pipeline_blocker_kind::missing_root:
            ++summary.missing_root_count;
            break;
        case asset_shader_byte_pipeline_blocker_kind::missing_source:
        case asset_shader_byte_pipeline_blocker_kind::invalid_root_path:
        case asset_shader_byte_pipeline_blocker_kind::materialization_blocked:
        case asset_shader_byte_pipeline_blocker_kind::byte_load_blocked:
        case asset_shader_byte_pipeline_blocker_kind::integrity_failure:
        case asset_shader_byte_pipeline_blocker_kind::empty_shader_bytes:
        case asset_shader_byte_pipeline_blocker_kind::non_spirv_magic:
        case asset_shader_byte_pipeline_blocker_kind::duplicate_id:
            break;
    }
}

inline void add_shader_byte_pipeline_source_entry(
    asset_shader_byte_pipeline_source_summary& summary,
    asset_shader_byte_pipeline_source_entry entry)
{
    count_shader_byte_pipeline_source_entry(summary, entry);
    if (entry.ready) {
        summary.ready.push_back(std::move(entry));
    } else {
        summary.blocked.push_back(std::move(entry));
    }
}

inline std::string shader_payload_runtime_lowercase(std::string_view value)
{
    std::string result(value);
    std::ranges::transform(result, result.begin(), [](char character) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    });
    return result;
}

inline bool shader_payload_runtime_has_stage_suffix(
    std::string_view identity,
    std::string_view suffix)
{
    return identity.ends_with(suffix)
        || identity.ends_with(std::string(suffix) + ".spv");
}

inline asset_shader_payload_runtime_stage infer_shader_payload_runtime_stage(
    std::string_view source_uri,
    const std::filesystem::path& materialized_path)
{
    const std::string identity = shader_payload_runtime_lowercase(
        source_uri.empty() ? materialized_path.generic_string() : std::string(source_uri));
    if (shader_payload_runtime_has_stage_suffix(identity, ".vert")) {
        return asset_shader_payload_runtime_stage::vertex;
    }
    if (shader_payload_runtime_has_stage_suffix(identity, ".frag")) {
        return asset_shader_payload_runtime_stage::fragment;
    }
    if (shader_payload_runtime_has_stage_suffix(identity, ".comp")) {
        return asset_shader_payload_runtime_stage::compute;
    }
    if (shader_payload_runtime_has_stage_suffix(identity, ".geom")) {
        return asset_shader_payload_runtime_stage::geometry;
    }
    if (shader_payload_runtime_has_stage_suffix(identity, ".tesc")) {
        return asset_shader_payload_runtime_stage::tessellation_control;
    }
    if (shader_payload_runtime_has_stage_suffix(identity, ".tese")) {
        return asset_shader_payload_runtime_stage::tessellation_evaluation;
    }
    return asset_shader_payload_runtime_stage::unknown;
}

inline std::string shader_payload_runtime_cache_revision(
    const asset_cache_key& cache_key)
{
    const asset_cache_key_classification classification = classify_asset_cache_key(cache_key);
    return classification.cache_revision;
}

inline std::string make_shader_payload_runtime_identity(
    asset_shader_payload_runtime_stage stage,
    const asset_cache_key& cache_key,
    std::string_view content_hash,
    std::size_t payload_byte_count)
{
    if (cache_key.empty() && content_hash.empty() && payload_byte_count == 0U) {
        return {};
    }

    return "shader-runtime|stage=" + asset_shader_payload_runtime_stage_name(stage)
        + "|key=" + cache_key + "|hash=" + std::string(content_hash)
        + "|bytes=" + std::to_string(payload_byte_count);
}

inline asset_shader_payload_runtime_entry make_shader_payload_runtime_entry(
    const asset_shader_byte_pipeline_source_entry& source)
{
    asset_shader_payload_runtime_entry entry{
        .id = source.id,
        .stage = infer_shader_payload_runtime_stage(source.source_uri, source.materialized_path),
        .source_kind = source.source_kind,
        .blocker = source.blocker,
        .cache_key = source.cache_key,
        .source_uri = source.source_uri,
        .materialized_path = source.materialized_path,
        .byte_count = source.byte_count,
        .payload_byte_count = source.payload_byte_count,
        .content_hash = source.content_hash,
        .cache_revision = shader_payload_runtime_cache_revision(source.cache_key),
        .materialized_byte_identity = source.materialized_byte_identity,
        .ready = source.ready,
        .diagnostic = source.diagnostic,
    };

    if (entry.ready) {
        entry.runtime_identity = make_shader_payload_runtime_identity(
            entry.stage,
            entry.cache_key,
            entry.content_hash,
            entry.payload_byte_count);
    }
    if (entry.diagnostic.empty()) {
        entry.diagnostic =
            entry.ready ? "shader payload runtime entry is ready" : "shader payload runtime entry is blocked";
    }

    return entry;
}

inline void count_shader_payload_runtime_entry(
    asset_shader_payload_runtime_summary& summary,
    const asset_shader_payload_runtime_entry& entry)
{
    switch (entry.stage) {
        case asset_shader_payload_runtime_stage::vertex:
            ++summary.vertex_stage_count;
            break;
        case asset_shader_payload_runtime_stage::fragment:
            ++summary.fragment_stage_count;
            break;
        case asset_shader_payload_runtime_stage::compute:
            ++summary.compute_stage_count;
            break;
        case asset_shader_payload_runtime_stage::geometry:
            ++summary.geometry_stage_count;
            break;
        case asset_shader_payload_runtime_stage::tessellation_control:
            ++summary.tessellation_control_stage_count;
            break;
        case asset_shader_payload_runtime_stage::tessellation_evaluation:
            ++summary.tessellation_evaluation_stage_count;
            break;
        case asset_shader_payload_runtime_stage::unknown:
            ++summary.unknown_stage_count;
            break;
    }

    if (entry.cache_revision.empty()) {
        ++summary.missing_revision_count;
    } else {
        ++summary.revisioned_count;
    }
}

inline void add_shader_payload_runtime_entry(
    asset_shader_payload_runtime_summary& summary,
    asset_shader_payload_runtime_entry entry)
{
    count_shader_payload_runtime_entry(summary, entry);
    if (entry.ready) {
        summary.ready.push_back(std::move(entry));
    } else {
        summary.blocked.push_back(std::move(entry));
    }
}

inline std::ptrdiff_t shader_payload_runtime_count_delta(
    std::size_t before,
    std::size_t after)
{
    return static_cast<std::ptrdiff_t>(after) - static_cast<std::ptrdiff_t>(before);
}

template <typename Callback>
inline void for_each_shader_payload_runtime_entry(
    const asset_shader_payload_runtime_summary& summary,
    Callback&& callback)
{
    for (const asset_shader_payload_runtime_entry& entry : summary.ready) {
        callback(entry);
    }
    for (const asset_shader_payload_runtime_entry& entry : summary.blocked) {
        callback(entry);
    }
}

inline const asset_shader_payload_runtime_entry* find_shader_payload_runtime_entry(
    const asset_shader_payload_runtime_summary& summary,
    std::string_view id)
{
    return summary.find_entry(id);
}

inline asset_shader_payload_runtime_delta_kind classify_shader_payload_runtime_delta(
    const asset_shader_payload_runtime_entry& before,
    const asset_shader_payload_runtime_entry& after)
{
    if (before.ready && after.ready) {
        return before.runtime_identity == after.runtime_identity
            ? asset_shader_payload_runtime_delta_kind::reused
            : asset_shader_payload_runtime_delta_kind::replaced;
    }
    if (before.ready && !after.ready) {
        return asset_shader_payload_runtime_delta_kind::invalidated;
    }
    if (!before.ready && after.ready) {
        return asset_shader_payload_runtime_delta_kind::added;
    }
    if (before.blocker == after.blocker && before.stage == after.stage
        && before.cache_key == after.cache_key && before.content_hash == after.content_hash) {
        return asset_shader_payload_runtime_delta_kind::reused;
    }
    return asset_shader_payload_runtime_delta_kind::invalidated;
}

inline asset_shader_payload_runtime_diff_entry make_shader_payload_runtime_diff_entry(
    asset_shader_payload_runtime_delta_kind kind,
    const std::optional<asset_shader_payload_runtime_entry>& before,
    const std::optional<asset_shader_payload_runtime_entry>& after)
{
    const asset_shader_payload_runtime_entry* before_entry =
        before.has_value() ? &*before : nullptr;
    const asset_shader_payload_runtime_entry* after_entry =
        after.has_value() ? &*after : nullptr;
    const asset_shader_payload_runtime_entry& anchor =
        after_entry != nullptr ? *after_entry : *before_entry;

    asset_shader_payload_runtime_diff_entry entry{
        .kind = kind,
        .id = anchor.id,
        .stage = anchor.stage,
        .before = before,
        .after = after,
        .before_missing_revision =
            before_entry != nullptr ? before_entry->cache_revision.empty() : false,
        .after_missing_revision =
            after_entry != nullptr ? after_entry->cache_revision.empty() : false,
        .diagnostic = "shader payload runtime entry reused",
    };

    if (before_entry != nullptr && after_entry != nullptr) {
        entry.stage_changed = before_entry->stage != after_entry->stage;
        entry.cache_key_changed = before_entry->cache_key != after_entry->cache_key;
        entry.cache_revision_changed = before_entry->cache_revision != after_entry->cache_revision;
        entry.missing_revision_changed = entry.before_missing_revision != entry.after_missing_revision;
        entry.byte_count_changed = before_entry->byte_count != after_entry->byte_count;
        entry.payload_byte_count_changed =
            before_entry->payload_byte_count != after_entry->payload_byte_count;
        entry.content_hash_changed = before_entry->content_hash != after_entry->content_hash;
        entry.runtime_identity_changed =
            before_entry->runtime_identity != after_entry->runtime_identity;
        entry.materialized_path_changed = before_entry->materialized_path != after_entry->materialized_path;
        entry.readiness_changed = before_entry->ready != after_entry->ready;
        entry.blocker_changed = before_entry->blocker != after_entry->blocker;
    } else {
        entry.stage_changed = before_entry != nullptr || after_entry != nullptr;
        entry.cache_key_changed = before_entry != nullptr || after_entry != nullptr;
        entry.cache_revision_changed = before_entry != nullptr || after_entry != nullptr;
        entry.missing_revision_changed = entry.before_missing_revision != entry.after_missing_revision;
        entry.byte_count_changed = before_entry != nullptr || after_entry != nullptr;
        entry.payload_byte_count_changed = before_entry != nullptr || after_entry != nullptr;
        entry.content_hash_changed = before_entry != nullptr || after_entry != nullptr;
        entry.runtime_identity_changed = before_entry != nullptr || after_entry != nullptr;
        entry.materialized_path_changed = before_entry != nullptr || after_entry != nullptr;
        entry.readiness_changed = before_entry != nullptr || after_entry != nullptr;
        entry.blocker_changed = before_entry != nullptr || after_entry != nullptr;
    }

    if (before_entry != nullptr && !before_entry->runtime_identity.empty()) {
        entry.invalidated_runtime_identity = before_entry->runtime_identity;
    }
    if (after_entry != nullptr && !after_entry->runtime_identity.empty()) {
        entry.replacement_runtime_identity = after_entry->runtime_identity;
    }

    switch (kind) {
        case asset_shader_payload_runtime_delta_kind::reused:
            entry.diagnostic = "shader payload runtime entry reused";
            break;
        case asset_shader_payload_runtime_delta_kind::added:
            entry.diagnostic = "shader payload runtime entry added or became ready";
            break;
        case asset_shader_payload_runtime_delta_kind::removed:
            entry.diagnostic = "shader payload runtime entry removed";
            break;
        case asset_shader_payload_runtime_delta_kind::replaced:
            entry.diagnostic = "shader payload runtime entry replaced by stage revision or content change";
            break;
        case asset_shader_payload_runtime_delta_kind::invalidated:
            entry.diagnostic = "shader payload runtime entry invalidated by readiness or blocker change";
            break;
    }

    return entry;
}

inline void add_shader_payload_runtime_diff_entry(
    asset_shader_payload_runtime_diff_summary& diff,
    asset_shader_payload_runtime_diff_entry entry)
{
    switch (entry.kind) {
        case asset_shader_payload_runtime_delta_kind::reused:
            diff.reused.push_back(std::move(entry));
            break;
        case asset_shader_payload_runtime_delta_kind::added:
            diff.added.push_back(std::move(entry));
            break;
        case asset_shader_payload_runtime_delta_kind::removed:
            diff.removed.push_back(std::move(entry));
            break;
        case asset_shader_payload_runtime_delta_kind::replaced:
            diff.replaced.push_back(std::move(entry));
            break;
        case asset_shader_payload_runtime_delta_kind::invalidated:
            diff.invalidated.push_back(std::move(entry));
            break;
    }
}

} // namespace detail

inline asset_shader_byte_pipeline_source_summary summarize_shader_byte_pipeline_sources(
    const asset_shader_materialized_byte_pipeline_summary& shader_pipeline,
    const asset_runtime_resolver_policy_summary& resolver_policy,
    const asset_pack_index_root_selection_summary& root_selection,
    const std::vector<std::string>& expected_shader_ids)
{
    asset_shader_byte_pipeline_source_summary summary{
        .input_shader_count = shader_pipeline.input_shader_count,
        .requested_shader_count = expected_shader_ids.size(),
    };

    const auto add_shader_entry = [&](const asset_shader_materialized_byte_pipeline_entry& entry) {
        detail::add_shader_byte_pipeline_source_entry(
            summary,
            detail::make_shader_byte_pipeline_source_entry(
                entry.id,
                &entry,
                resolver_policy,
                root_selection));
    };

    for (const asset_shader_materialized_byte_pipeline_entry& entry : shader_pipeline.ready) {
        add_shader_entry(entry);
    }
    for (const asset_shader_materialized_byte_pipeline_entry& entry : shader_pipeline.blocked) {
        add_shader_entry(entry);
    }

    for (const std::string& id : expected_shader_ids) {
        if (detail::shader_byte_pipeline_source_summary_has_id(summary, id)) {
            continue;
        }
        detail::add_shader_byte_pipeline_source_entry(
            summary,
            detail::make_shader_byte_pipeline_source_entry(
                id,
                nullptr,
                resolver_policy,
                root_selection));
    }

    return summary;
}

inline asset_shader_byte_pipeline_source_summary summarize_shader_byte_pipeline_sources(
    const asset_shader_materialized_byte_pipeline_summary& shader_pipeline,
    const asset_runtime_resolver_policy_summary& resolver_policy,
    const asset_pack_index_root_selection_summary& root_selection)
{
    const std::vector<std::string> expected_shader_ids;
    return summarize_shader_byte_pipeline_sources(
        shader_pipeline,
        resolver_policy,
        root_selection,
        expected_shader_ids);
}

inline asset_shader_byte_pipeline_source_summary summarize_shader_byte_pipeline_sources(
    const asset_shader_materialized_byte_pipeline_summary& shader_pipeline,
    const asset_runtime_resolver_policy_summary& resolver_policy)
{
    const asset_pack_index_root_selection_summary root_selection;
    return summarize_shader_byte_pipeline_sources(shader_pipeline, resolver_policy, root_selection);
}

inline asset_shader_payload_runtime_summary summarize_shader_payload_runtime(
    const asset_shader_byte_pipeline_source_summary& source_summary)
{
    asset_shader_payload_runtime_summary summary{
        .input_shader_count = source_summary.input_shader_count,
        .requested_shader_count = source_summary.requested_shader_count,
    };

    for (const asset_shader_byte_pipeline_source_entry& entry : source_summary.ready) {
        detail::add_shader_payload_runtime_entry(
            summary,
            detail::make_shader_payload_runtime_entry(entry));
    }
    for (const asset_shader_byte_pipeline_source_entry& entry : source_summary.blocked) {
        detail::add_shader_payload_runtime_entry(
            summary,
            detail::make_shader_payload_runtime_entry(entry));
    }

    return summary;
}

inline asset_shader_payload_runtime_diff_summary diff_shader_payload_runtime_summaries(
    const asset_shader_payload_runtime_summary& before,
    const asset_shader_payload_runtime_summary& after)
{
    asset_shader_payload_runtime_diff_summary diff{
        .requested_delta = detail::shader_payload_runtime_count_delta(
            before.requested_shader_count,
            after.requested_shader_count),
        .ready_delta = detail::shader_payload_runtime_count_delta(
            before.ready_count(),
            after.ready_count()),
        .blocked_delta = detail::shader_payload_runtime_count_delta(
            before.blocked_count(),
            after.blocked_count()),
        .missing_revision_delta = detail::shader_payload_runtime_count_delta(
            before.missing_revision_count,
            after.missing_revision_count),
    };

    detail::for_each_shader_payload_runtime_entry(
        before,
        [&](const asset_shader_payload_runtime_entry& before_entry) {
            const asset_shader_payload_runtime_entry* after_entry =
                detail::find_shader_payload_runtime_entry(after, before_entry.id);
            if (after_entry == nullptr) {
                detail::add_shader_payload_runtime_diff_entry(
                    diff,
                    detail::make_shader_payload_runtime_diff_entry(
                        asset_shader_payload_runtime_delta_kind::removed,
                        before_entry,
                        std::nullopt));
                return;
            }

            detail::add_shader_payload_runtime_diff_entry(
                diff,
                detail::make_shader_payload_runtime_diff_entry(
                    detail::classify_shader_payload_runtime_delta(before_entry, *after_entry),
                    before_entry,
                    *after_entry));
        });

    detail::for_each_shader_payload_runtime_entry(
        after,
        [&](const asset_shader_payload_runtime_entry& after_entry) {
            if (detail::find_shader_payload_runtime_entry(before, after_entry.id) != nullptr) {
                return;
            }
            detail::add_shader_payload_runtime_diff_entry(
                diff,
                detail::make_shader_payload_runtime_diff_entry(
                    asset_shader_payload_runtime_delta_kind::added,
                    std::nullopt,
                    after_entry));
        });

    return diff;
}

inline std::string asset_shader_byte_pipeline_source_kind_name(
    asset_shader_byte_pipeline_source_kind kind)
{
    switch (kind) {
        case asset_shader_byte_pipeline_source_kind::manifest:
            return "manifest";
        case asset_shader_byte_pipeline_source_kind::fallback:
            return "fallback";
        case asset_shader_byte_pipeline_source_kind::local_fixture:
            return "local_fixture";
        case asset_shader_byte_pipeline_source_kind::build_external:
            return "build_external";
        case asset_shader_byte_pipeline_source_kind::missing_source:
            return "missing_source";
    }
    return "missing_source";
}

inline std::string asset_shader_byte_pipeline_blocker_kind_name(
    asset_shader_byte_pipeline_blocker_kind kind)
{
    switch (kind) {
        case asset_shader_byte_pipeline_blocker_kind::none:
            return "none";
        case asset_shader_byte_pipeline_blocker_kind::missing_manifest_entry:
            return "missing_manifest_entry";
        case asset_shader_byte_pipeline_blocker_kind::missing_source:
            return "missing_source";
        case asset_shader_byte_pipeline_blocker_kind::stale_manifest_entry:
            return "stale_manifest_entry";
        case asset_shader_byte_pipeline_blocker_kind::traversal_rejected:
            return "traversal_rejected";
        case asset_shader_byte_pipeline_blocker_kind::missing_root:
            return "missing_root";
        case asset_shader_byte_pipeline_blocker_kind::invalid_root_path:
            return "invalid_root_path";
        case asset_shader_byte_pipeline_blocker_kind::materialization_blocked:
            return "materialization_blocked";
        case asset_shader_byte_pipeline_blocker_kind::byte_load_blocked:
            return "byte_load_blocked";
        case asset_shader_byte_pipeline_blocker_kind::integrity_failure:
            return "integrity_failure";
        case asset_shader_byte_pipeline_blocker_kind::empty_shader_bytes:
            return "empty_shader_bytes";
        case asset_shader_byte_pipeline_blocker_kind::non_spirv_magic:
            return "non_spirv_magic";
        case asset_shader_byte_pipeline_blocker_kind::duplicate_id:
            return "duplicate_id";
    }
    return "missing_source";
}

inline std::string asset_shader_payload_runtime_delta_kind_name(
    asset_shader_payload_runtime_delta_kind kind)
{
    switch (kind) {
        case asset_shader_payload_runtime_delta_kind::reused:
            return "reused";
        case asset_shader_payload_runtime_delta_kind::added:
            return "added";
        case asset_shader_payload_runtime_delta_kind::removed:
            return "removed";
        case asset_shader_payload_runtime_delta_kind::replaced:
            return "replaced";
        case asset_shader_payload_runtime_delta_kind::invalidated:
            return "invalidated";
    }
    return "invalidated";
}

} // namespace quiz_vulkan::assets
