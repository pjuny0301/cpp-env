#pragma once

#include "assets/asset_bytes_contract.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace quiz_vulkan::assets {

struct asset_typed_materialized_bytes_entry {
    std::string id;
    asset_type type = asset_type::generic;
    asset_cache_key cache_key;
    std::string source_uri;
    std::optional<std::filesystem::path> rooted_path;
    std::string materialized_source_path;
    std::filesystem::path materialized_path;
    std::size_t byte_count = 0U;
    std::string content_hash;
    runtime_materialized_asset_lookup_status materialized_status =
        runtime_materialized_asset_lookup_status::missing_id;
    asset_bytes_load_status load_status = asset_bytes_load_status::missing_bytes;
    std::vector<asset_bytes_integrity_issue> issues;
    std::string diagnostic;

    [[nodiscard]] bool ok() const
    {
        return materialized_status == runtime_materialized_asset_lookup_status::materialized
            && load_status == asset_bytes_load_status::loaded && issues.empty();
    }
};

struct asset_typed_materialized_bytes_summary {
    asset_materialized_bytes_cache_policy_summary cache_policy;
    std::vector<asset_typed_materialized_bytes_entry> fonts;
    std::vector<asset_typed_materialized_bytes_entry> images;
    std::vector<asset_typed_materialized_bytes_entry> sounds;
    std::vector<asset_typed_materialized_bytes_entry> shaders;
    std::vector<asset_typed_materialized_bytes_entry> decks;
    std::size_t skipped_generic_count = 0U;

    [[nodiscard]] bool ok() const
    {
        return cache_policy.ok();
    }

    [[nodiscard]] std::size_t entry_count() const
    {
        return fonts.size() + images.size() + sounds.size() + shaders.size() + decks.size();
    }

    [[nodiscard]] const std::vector<asset_typed_materialized_bytes_entry>& entries_for_type(
        asset_type type) const
    {
        switch (type) {
            case asset_type::font:
                return fonts;
            case asset_type::image:
                return images;
            case asset_type::sound:
                return sounds;
            case asset_type::shader:
                return shaders;
            case asset_type::deck:
                return decks;
            case asset_type::generic:
                break;
        }

        static const std::vector<asset_typed_materialized_bytes_entry> empty_entries;
        return empty_entries;
    }

    [[nodiscard]] const asset_typed_materialized_bytes_entry* find_entry(std::string_view id) const
    {
        const auto find_in = [id](const std::vector<asset_typed_materialized_bytes_entry>& entries)
            -> const asset_typed_materialized_bytes_entry* {
            for (const asset_typed_materialized_bytes_entry& entry : entries) {
                if (entry.id == id) {
                    return &entry;
                }
            }
            return nullptr;
        };

        if (const asset_typed_materialized_bytes_entry* entry = find_in(fonts); entry != nullptr) {
            return entry;
        }
        if (const asset_typed_materialized_bytes_entry* entry = find_in(images); entry != nullptr) {
            return entry;
        }
        if (const asset_typed_materialized_bytes_entry* entry = find_in(sounds); entry != nullptr) {
            return entry;
        }
        if (const asset_typed_materialized_bytes_entry* entry = find_in(shaders); entry != nullptr) {
            return entry;
        }
        if (const asset_typed_materialized_bytes_entry* entry = find_in(decks); entry != nullptr) {
            return entry;
        }
        return nullptr;
    }
};

enum class asset_typed_materialized_bytes_delta_kind {
    added,
    removed,
    changed,
};

struct asset_typed_materialized_bytes_diff_entry {
    asset_typed_materialized_bytes_delta_kind kind = asset_typed_materialized_bytes_delta_kind::changed;
    std::string id;
    asset_type type = asset_type::generic;
    std::optional<asset_typed_materialized_bytes_entry> before;
    std::optional<asset_typed_materialized_bytes_entry> after;
    bool type_changed = false;
    bool cache_key_changed = false;
    bool source_uri_changed = false;
    bool materialized_path_changed = false;
    bool content_hash_changed = false;
    bool integrity_status_changed = false;

    [[nodiscard]] bool has_field_delta() const
    {
        return type_changed || cache_key_changed || source_uri_changed || materialized_path_changed
            || content_hash_changed || integrity_status_changed;
    }
};

struct asset_typed_materialized_bytes_diff_summary {
    std::vector<asset_typed_materialized_bytes_diff_entry> added;
    std::vector<asset_typed_materialized_bytes_diff_entry> removed;
    std::vector<asset_typed_materialized_bytes_diff_entry> changed;

    [[nodiscard]] bool empty() const
    {
        return added.empty() && removed.empty() && changed.empty();
    }

    [[nodiscard]] std::size_t change_count() const
    {
        return added.size() + removed.size() + changed.size();
    }

    [[nodiscard]] const asset_typed_materialized_bytes_diff_entry* find_added(std::string_view id) const
    {
        for (const asset_typed_materialized_bytes_diff_entry& entry : added) {
            if (entry.id == id) {
                return &entry;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const asset_typed_materialized_bytes_diff_entry* find_removed(std::string_view id) const
    {
        for (const asset_typed_materialized_bytes_diff_entry& entry : removed) {
            if (entry.id == id) {
                return &entry;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const asset_typed_materialized_bytes_diff_entry* find_changed(std::string_view id) const
    {
        for (const asset_typed_materialized_bytes_diff_entry& entry : changed) {
            if (entry.id == id) {
                return &entry;
            }
        }
        return nullptr;
    }
};

enum class asset_materialized_bytes_handoff_status {
    ready,
    materialization_blocked,
    load_blocked,
    integrity_blocked,
};

struct asset_materialized_bytes_handoff_payload {
    std::string id;
    asset_type type = asset_type::generic;
    asset_cache_key cache_key;
    std::string source_uri;
    std::filesystem::path materialized_path;
    std::size_t byte_count = 0U;
    std::string content_hash;
    asset_materialized_bytes_handoff_status status =
        asset_materialized_bytes_handoff_status::materialization_blocked;
    runtime_materialized_asset_lookup_status materialized_status =
        runtime_materialized_asset_lookup_status::missing_id;
    asset_bytes_load_status load_status = asset_bytes_load_status::missing_bytes;
    std::vector<asset_bytes_integrity_issue> issues;
    std::string diagnostic;

    [[nodiscard]] bool ready() const
    {
        return status == asset_materialized_bytes_handoff_status::ready;
    }
};

struct asset_materialized_bytes_handoff_group {
    std::vector<asset_materialized_bytes_handoff_payload> ready;
    std::vector<asset_materialized_bytes_handoff_payload> blocked;

    [[nodiscard]] std::size_t payload_count() const
    {
        return ready.size() + blocked.size();
    }

    [[nodiscard]] bool ok() const
    {
        return blocked.empty();
    }

    [[nodiscard]] const asset_materialized_bytes_handoff_payload* find_ready(std::string_view id) const
    {
        for (const asset_materialized_bytes_handoff_payload& payload : ready) {
            if (payload.id == id) {
                return &payload;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const asset_materialized_bytes_handoff_payload* find_blocked(std::string_view id) const
    {
        for (const asset_materialized_bytes_handoff_payload& payload : blocked) {
            if (payload.id == id) {
                return &payload;
            }
        }
        return nullptr;
    }
};

struct asset_materialized_bytes_handoff_summary {
    asset_materialized_bytes_handoff_group fonts;
    asset_materialized_bytes_handoff_group images;
    asset_materialized_bytes_handoff_group sounds;
    asset_materialized_bytes_handoff_group shaders;
    asset_materialized_bytes_handoff_group decks;
    std::size_t skipped_generic_count = 0U;

    [[nodiscard]] bool ok() const
    {
        return blocked_count() == 0U;
    }

    [[nodiscard]] std::size_t ready_count() const
    {
        return fonts.ready.size() + images.ready.size() + sounds.ready.size() + shaders.ready.size()
            + decks.ready.size();
    }

    [[nodiscard]] std::size_t blocked_count() const
    {
        return fonts.blocked.size() + images.blocked.size() + sounds.blocked.size() + shaders.blocked.size()
            + decks.blocked.size();
    }

    [[nodiscard]] std::size_t payload_count() const
    {
        return ready_count() + blocked_count();
    }

    [[nodiscard]] const asset_materialized_bytes_handoff_group& group_for_type(asset_type type) const
    {
        switch (type) {
            case asset_type::font:
                return fonts;
            case asset_type::image:
                return images;
            case asset_type::sound:
                return sounds;
            case asset_type::shader:
                return shaders;
            case asset_type::deck:
                return decks;
            case asset_type::generic:
                break;
        }

        static const asset_materialized_bytes_handoff_group empty_group;
        return empty_group;
    }

    [[nodiscard]] const asset_materialized_bytes_handoff_payload* find_ready(std::string_view id) const
    {
        if (const asset_materialized_bytes_handoff_payload* payload = fonts.find_ready(id); payload != nullptr) {
            return payload;
        }
        if (const asset_materialized_bytes_handoff_payload* payload = images.find_ready(id); payload != nullptr) {
            return payload;
        }
        if (const asset_materialized_bytes_handoff_payload* payload = sounds.find_ready(id); payload != nullptr) {
            return payload;
        }
        if (const asset_materialized_bytes_handoff_payload* payload = shaders.find_ready(id); payload != nullptr) {
            return payload;
        }
        if (const asset_materialized_bytes_handoff_payload* payload = decks.find_ready(id); payload != nullptr) {
            return payload;
        }
        return nullptr;
    }

    [[nodiscard]] const asset_materialized_bytes_handoff_payload* find_blocked(std::string_view id) const
    {
        if (const asset_materialized_bytes_handoff_payload* payload = fonts.find_blocked(id); payload != nullptr) {
            return payload;
        }
        if (const asset_materialized_bytes_handoff_payload* payload = images.find_blocked(id); payload != nullptr) {
            return payload;
        }
        if (const asset_materialized_bytes_handoff_payload* payload = sounds.find_blocked(id); payload != nullptr) {
            return payload;
        }
        if (const asset_materialized_bytes_handoff_payload* payload = shaders.find_blocked(id); payload != nullptr) {
            return payload;
        }
        if (const asset_materialized_bytes_handoff_payload* payload = decks.find_blocked(id); payload != nullptr) {
            return payload;
        }
        return nullptr;
    }
};

struct asset_materialized_byte_payload {
    std::string id;
    asset_type type = asset_type::generic;
    asset_cache_key cache_key;
    std::string source_uri;
    std::optional<std::filesystem::path> rooted_path;
    std::string materialized_source_path;
    std::filesystem::path materialized_path;
    std::size_t byte_count = 0U;
    std::string content_hash;
    std::vector<std::byte> bytes;
    asset_materialized_bytes_handoff_status status =
        asset_materialized_bytes_handoff_status::materialization_blocked;
    runtime_materialized_asset_lookup_status materialized_status =
        runtime_materialized_asset_lookup_status::missing_id;
    asset_bytes_load_status load_status = asset_bytes_load_status::missing_bytes;
    std::vector<asset_bytes_integrity_issue> issues;
    std::string diagnostic;

    [[nodiscard]] bool ready() const
    {
        return status == asset_materialized_bytes_handoff_status::ready;
    }
};

struct asset_materialized_byte_payload_group {
    std::vector<asset_materialized_byte_payload> ready;
    std::vector<asset_materialized_byte_payload> blocked;

    [[nodiscard]] std::size_t payload_count() const
    {
        return ready.size() + blocked.size();
    }

    [[nodiscard]] bool ok() const
    {
        return blocked.empty();
    }

    [[nodiscard]] const asset_materialized_byte_payload* find_ready(std::string_view id) const
    {
        for (const asset_materialized_byte_payload& payload : ready) {
            if (payload.id == id) {
                return &payload;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const asset_materialized_byte_payload* find_blocked(std::string_view id) const
    {
        for (const asset_materialized_byte_payload& payload : blocked) {
            if (payload.id == id) {
                return &payload;
            }
        }
        return nullptr;
    }
};

struct asset_materialized_byte_payload_bundle {
    asset_materialized_bytes_cache_policy_summary cache_policy;
    asset_materialized_bytes_handoff_summary handoff;
    asset_materialized_byte_payload_group fonts;
    asset_materialized_byte_payload_group images;
    asset_materialized_byte_payload_group sounds;
    asset_materialized_byte_payload_group shaders;
    asset_materialized_byte_payload_group decks;
    std::size_t skipped_generic_count = 0U;

    [[nodiscard]] bool ok() const
    {
        return cache_policy.ok() && handoff.ok() && blocked_count() == 0U;
    }

    [[nodiscard]] std::size_t ready_count() const
    {
        return fonts.ready.size() + images.ready.size() + sounds.ready.size() + shaders.ready.size()
            + decks.ready.size();
    }

    [[nodiscard]] std::size_t blocked_count() const
    {
        return fonts.blocked.size() + images.blocked.size() + sounds.blocked.size() + shaders.blocked.size()
            + decks.blocked.size();
    }

    [[nodiscard]] std::size_t payload_count() const
    {
        return ready_count() + blocked_count();
    }

    [[nodiscard]] const asset_materialized_byte_payload_group& group_for_type(asset_type type) const
    {
        switch (type) {
            case asset_type::font:
                return fonts;
            case asset_type::image:
                return images;
            case asset_type::sound:
                return sounds;
            case asset_type::shader:
                return shaders;
            case asset_type::deck:
                return decks;
            case asset_type::generic:
                break;
        }

        static const asset_materialized_byte_payload_group empty_group;
        return empty_group;
    }

    [[nodiscard]] const asset_materialized_byte_payload* find_ready(std::string_view id) const
    {
        if (const asset_materialized_byte_payload* payload = fonts.find_ready(id); payload != nullptr) {
            return payload;
        }
        if (const asset_materialized_byte_payload* payload = images.find_ready(id); payload != nullptr) {
            return payload;
        }
        if (const asset_materialized_byte_payload* payload = sounds.find_ready(id); payload != nullptr) {
            return payload;
        }
        if (const asset_materialized_byte_payload* payload = shaders.find_ready(id); payload != nullptr) {
            return payload;
        }
        if (const asset_materialized_byte_payload* payload = decks.find_ready(id); payload != nullptr) {
            return payload;
        }
        return nullptr;
    }

    [[nodiscard]] const asset_materialized_byte_payload* find_blocked(std::string_view id) const
    {
        if (const asset_materialized_byte_payload* payload = fonts.find_blocked(id); payload != nullptr) {
            return payload;
        }
        if (const asset_materialized_byte_payload* payload = images.find_blocked(id); payload != nullptr) {
            return payload;
        }
        if (const asset_materialized_byte_payload* payload = sounds.find_blocked(id); payload != nullptr) {
            return payload;
        }
        if (const asset_materialized_byte_payload* payload = shaders.find_blocked(id); payload != nullptr) {
            return payload;
        }
        if (const asset_materialized_byte_payload* payload = decks.find_blocked(id); payload != nullptr) {
            return payload;
        }
        return nullptr;
    }
};

namespace detail {

inline bool asset_type_is_engine_facing(asset_type type)
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

inline std::vector<asset_bytes_catalog_request> make_typed_materialized_asset_byte_requests(
    const runtime_asset_catalog& catalog,
    std::size_t& skipped_generic_count)
{
    std::vector<asset_bytes_catalog_request> requests;
    requests.reserve(catalog.assets.size());
    for (const runtime_asset_catalog_snapshot& asset : catalog.assets) {
        if (!asset_type_is_engine_facing(asset.entry.type)) {
            ++skipped_generic_count;
            continue;
        }
        requests.push_back(asset_bytes_catalog_request{
            .id = asset.entry.id,
            .expected_type = asset.entry.type,
        });
    }
    return requests;
}

inline asset_typed_materialized_bytes_entry make_typed_materialized_bytes_entry(
    const asset_materialized_bytes_cache_policy_entry& policy_entry,
    const runtime_asset_catalog& catalog)
{
    const runtime_asset_catalog_snapshot* snapshot = catalog.find(policy_entry.id);
    asset_typed_materialized_bytes_entry entry{
        .id = policy_entry.id,
        .type = snapshot == nullptr ? policy_entry.expected_type : snapshot->entry.type,
        .cache_key = policy_entry.cache_key,
        .source_uri = policy_entry.source_uri,
        .materialized_source_path = policy_entry.materialized_source_path,
        .materialized_path = policy_entry.materialized_path,
        .byte_count = policy_entry.byte_count,
        .content_hash = policy_entry.content_hash,
        .materialized_status = policy_entry.materialized_status,
        .load_status = policy_entry.load_status,
        .issues = policy_entry.issues,
        .diagnostic = policy_entry.diagnostic,
    };
    if (snapshot != nullptr && snapshot->rooted_path.has_value()) {
        entry.rooted_path = snapshot->rooted_path->lexically_normal();
    }
    return entry;
}

inline void add_typed_materialized_bytes_entry(
    asset_typed_materialized_bytes_summary& summary,
    asset_typed_materialized_bytes_entry entry)
{
    switch (entry.type) {
        case asset_type::font:
            summary.fonts.push_back(std::move(entry));
            break;
        case asset_type::image:
            summary.images.push_back(std::move(entry));
            break;
        case asset_type::sound:
            summary.sounds.push_back(std::move(entry));
            break;
        case asset_type::shader:
            summary.shaders.push_back(std::move(entry));
            break;
        case asset_type::deck:
            summary.decks.push_back(std::move(entry));
            break;
        case asset_type::generic:
            ++summary.skipped_generic_count;
            break;
    }
}

template <typename Visitor>
inline void for_each_typed_materialized_bytes_entry(
    const asset_typed_materialized_bytes_summary& summary,
    Visitor&& visitor)
{
    for (const asset_typed_materialized_bytes_entry& entry : summary.fonts) {
        visitor(entry);
    }
    for (const asset_typed_materialized_bytes_entry& entry : summary.images) {
        visitor(entry);
    }
    for (const asset_typed_materialized_bytes_entry& entry : summary.sounds) {
        visitor(entry);
    }
    for (const asset_typed_materialized_bytes_entry& entry : summary.shaders) {
        visitor(entry);
    }
    for (const asset_typed_materialized_bytes_entry& entry : summary.decks) {
        visitor(entry);
    }
}

inline bool typed_materialized_bytes_integrity_status_matches(
    const asset_typed_materialized_bytes_entry& before,
    const asset_typed_materialized_bytes_entry& after)
{
    return before.ok() == after.ok() && before.materialized_status == after.materialized_status
        && before.load_status == after.load_status && before.issues.size() == after.issues.size();
}

inline asset_typed_materialized_bytes_diff_entry make_added_typed_materialized_bytes_diff_entry(
    const asset_typed_materialized_bytes_entry& after)
{
    return asset_typed_materialized_bytes_diff_entry{
        .kind = asset_typed_materialized_bytes_delta_kind::added,
        .id = after.id,
        .type = after.type,
        .after = after,
    };
}

inline asset_typed_materialized_bytes_diff_entry make_removed_typed_materialized_bytes_diff_entry(
    const asset_typed_materialized_bytes_entry& before)
{
    return asset_typed_materialized_bytes_diff_entry{
        .kind = asset_typed_materialized_bytes_delta_kind::removed,
        .id = before.id,
        .type = before.type,
        .before = before,
    };
}

inline asset_typed_materialized_bytes_diff_entry make_changed_typed_materialized_bytes_diff_entry(
    const asset_typed_materialized_bytes_entry& before,
    const asset_typed_materialized_bytes_entry& after)
{
    return asset_typed_materialized_bytes_diff_entry{
        .kind = asset_typed_materialized_bytes_delta_kind::changed,
        .id = after.id,
        .type = after.type,
        .before = before,
        .after = after,
        .type_changed = before.type != after.type,
        .cache_key_changed = before.cache_key != after.cache_key,
        .source_uri_changed = before.source_uri != after.source_uri,
        .materialized_path_changed = before.materialized_path != after.materialized_path,
        .content_hash_changed = before.content_hash != after.content_hash,
        .integrity_status_changed = !typed_materialized_bytes_integrity_status_matches(before, after),
    };
}

inline asset_materialized_bytes_handoff_status materialized_bytes_handoff_status_for_entry(
    const asset_typed_materialized_bytes_entry& entry)
{
    if (entry.materialized_status != runtime_materialized_asset_lookup_status::materialized) {
        return asset_materialized_bytes_handoff_status::materialization_blocked;
    }
    if (entry.load_status != asset_bytes_load_status::loaded) {
        return asset_materialized_bytes_handoff_status::load_blocked;
    }
    if (!entry.issues.empty()) {
        return asset_materialized_bytes_handoff_status::integrity_blocked;
    }
    return asset_materialized_bytes_handoff_status::ready;
}

inline asset_materialized_bytes_handoff_payload make_materialized_bytes_handoff_payload(
    const asset_typed_materialized_bytes_entry& entry)
{
    return asset_materialized_bytes_handoff_payload{
        .id = entry.id,
        .type = entry.type,
        .cache_key = entry.cache_key,
        .source_uri = entry.source_uri,
        .materialized_path = entry.materialized_path,
        .byte_count = entry.byte_count,
        .content_hash = entry.content_hash,
        .status = materialized_bytes_handoff_status_for_entry(entry),
        .materialized_status = entry.materialized_status,
        .load_status = entry.load_status,
        .issues = entry.issues,
        .diagnostic = entry.diagnostic,
    };
}

inline void add_materialized_bytes_handoff_payload(
    asset_materialized_bytes_handoff_summary& summary,
    asset_materialized_bytes_handoff_payload payload)
{
    asset_materialized_bytes_handoff_group* group = nullptr;
    switch (payload.type) {
        case asset_type::font:
            group = &summary.fonts;
            break;
        case asset_type::image:
            group = &summary.images;
            break;
        case asset_type::sound:
            group = &summary.sounds;
            break;
        case asset_type::shader:
            group = &summary.shaders;
            break;
        case asset_type::deck:
            group = &summary.decks;
            break;
        case asset_type::generic:
            ++summary.skipped_generic_count;
            return;
    }

    if (payload.ready()) {
        group->ready.push_back(std::move(payload));
    } else {
        group->blocked.push_back(std::move(payload));
    }
}

inline asset_materialized_byte_payload make_materialized_byte_payload(
    const asset_typed_materialized_bytes_entry& entry,
    asset_bytes_load_result load)
{
    return asset_materialized_byte_payload{
        .id = entry.id,
        .type = entry.type,
        .cache_key = entry.cache_key,
        .source_uri = entry.source_uri,
        .rooted_path = entry.rooted_path,
        .materialized_source_path = entry.materialized_source_path,
        .materialized_path = entry.materialized_path,
        .byte_count = entry.byte_count,
        .content_hash = entry.content_hash,
        .bytes = std::move(load.bytes),
        .status = materialized_bytes_handoff_status_for_entry(entry),
        .materialized_status = entry.materialized_status,
        .load_status = entry.load_status,
        .issues = entry.issues,
        .diagnostic = entry.diagnostic,
    };
}

inline void add_materialized_byte_payload(
    asset_materialized_byte_payload_bundle& bundle,
    asset_materialized_byte_payload payload)
{
    asset_materialized_byte_payload_group* group = nullptr;
    switch (payload.type) {
        case asset_type::font:
            group = &bundle.fonts;
            break;
        case asset_type::image:
            group = &bundle.images;
            break;
        case asset_type::sound:
            group = &bundle.sounds;
            break;
        case asset_type::shader:
            group = &bundle.shaders;
            break;
        case asset_type::deck:
            group = &bundle.decks;
            break;
        case asset_type::generic:
            ++bundle.skipped_generic_count;
            return;
    }

    if (payload.ready()) {
        group->ready.push_back(std::move(payload));
    } else {
        group->blocked.push_back(std::move(payload));
    }
}

} // namespace detail

inline asset_typed_materialized_bytes_diff_summary diff_typed_materialized_asset_bytes(
    const asset_typed_materialized_bytes_summary& before,
    const asset_typed_materialized_bytes_summary& after)
{
    asset_typed_materialized_bytes_diff_summary diff;

    detail::for_each_typed_materialized_bytes_entry(before, [&](const asset_typed_materialized_bytes_entry& before_entry) {
        const asset_typed_materialized_bytes_entry* after_entry = after.find_entry(before_entry.id);
        if (after_entry == nullptr) {
            diff.removed.push_back(detail::make_removed_typed_materialized_bytes_diff_entry(before_entry));
            return;
        }

        asset_typed_materialized_bytes_diff_entry changed =
            detail::make_changed_typed_materialized_bytes_diff_entry(before_entry, *after_entry);
        if (changed.has_field_delta()) {
            diff.changed.push_back(std::move(changed));
        }
    });

    detail::for_each_typed_materialized_bytes_entry(after, [&](const asset_typed_materialized_bytes_entry& after_entry) {
        if (before.find_entry(after_entry.id) == nullptr) {
            diff.added.push_back(detail::make_added_typed_materialized_bytes_diff_entry(after_entry));
        }
    });

    return diff;
}

inline asset_materialized_bytes_handoff_summary make_materialized_asset_bytes_handoff_summary(
    const asset_typed_materialized_bytes_summary& typed_summary)
{
    asset_materialized_bytes_handoff_summary handoff{
        .skipped_generic_count = typed_summary.skipped_generic_count,
    };

    detail::for_each_typed_materialized_bytes_entry(
        typed_summary,
        [&](const asset_typed_materialized_bytes_entry& entry) {
            detail::add_materialized_bytes_handoff_payload(
                handoff,
                detail::make_materialized_bytes_handoff_payload(entry));
        });

    return handoff;
}

} // namespace quiz_vulkan::assets
