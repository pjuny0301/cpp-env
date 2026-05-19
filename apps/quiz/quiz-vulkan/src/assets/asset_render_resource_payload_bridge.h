#pragma once

#include "assets/asset_render_resource_address.h"
#include "assets/asset_typed_materialized_bytes.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace quiz_vulkan::assets {

enum class asset_render_resource_payload_bridge_status {
    accepted,
    missing_materialized_bytes,
    type_mismatch,
    cache_key_mismatch,
    duplicate_canonical_identity,
    duplicate_materialized_payload_id,
};

enum class asset_render_resource_materialized_cache_status {
    ready,
    address_rejected,
    missing_render_resource_address,
    missing_materialized_bytes,
    type_mismatch,
    cache_key_mismatch,
    duplicate_canonical_identity,
    duplicate_materialized_payload_id,
};

enum class asset_render_resource_materialized_cache_delta_kind {
    reused,
    added,
    removed,
    replaced,
    invalidated,
};

struct asset_render_resource_materialized_cache_request {
    std::string id;
    asset_type expected_type = asset_type::generic;
};

struct asset_render_resource_payload_bridge_entry {
    asset_render_resource_payload_bridge_status status =
        asset_render_resource_payload_bridge_status::missing_materialized_bytes;
    asset_render_resource_address_entry address;
    asset_materialized_byte_payload_selection_result selection;
    std::optional<asset_materialized_byte_payload_snapshot> selected_snapshot;
    std::string related_id;
    std::string diagnostic;

    [[nodiscard]] bool ok() const
    {
        return status == asset_render_resource_payload_bridge_status::accepted
            && selected_snapshot.has_value();
    }
};

struct asset_render_resource_materialized_cache_entry {
    asset_render_resource_materialized_cache_status status =
        asset_render_resource_materialized_cache_status::missing_render_resource_address;
    std::string id;
    asset_type type = asset_type::generic;
    std::string canonical_identity;
    asset_cache_key cache_key;
    std::string source_uri;
    std::filesystem::path materialized_path;
    std::size_t byte_count = 0U;
    std::size_t payload_byte_count = 0U;
    std::string content_hash;
    std::string runtime_cache_key;
    std::string related_id;
    std::string diagnostic;

    [[nodiscard]] bool ready() const
    {
        return status == asset_render_resource_materialized_cache_status::ready;
    }
};

struct asset_render_resource_materialized_cache_summary {
    std::vector<asset_render_resource_materialized_cache_entry> ready;
    std::vector<asset_render_resource_materialized_cache_entry> blocked;
    std::size_t requested_count = 0U;
    std::size_t ready_count = 0U;
    std::size_t address_rejected_count = 0U;
    std::size_t missing_render_resource_address_count = 0U;
    std::size_t missing_materialized_bytes_count = 0U;
    std::size_t type_mismatch_count = 0U;
    std::size_t cache_key_mismatch_count = 0U;
    std::size_t duplicate_canonical_identity_count = 0U;
    std::size_t duplicate_materialized_payload_id_count = 0U;

    [[nodiscard]] bool ok() const
    {
        return blocked.empty();
    }

    [[nodiscard]] std::size_t entry_count() const
    {
        return ready.size() + blocked.size();
    }

    [[nodiscard]] std::size_t blocked_count() const
    {
        return blocked.size();
    }

    [[nodiscard]] const asset_render_resource_materialized_cache_entry* find_ready(
        std::string_view id) const
    {
        for (const asset_render_resource_materialized_cache_entry& entry : ready) {
            if (entry.id == id) {
                return &entry;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const asset_render_resource_materialized_cache_entry* find_blocked(
        std::string_view id) const
    {
        for (const asset_render_resource_materialized_cache_entry& entry : blocked) {
            if (entry.id == id) {
                return &entry;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const asset_render_resource_materialized_cache_entry* find_entry(
        std::string_view id) const
    {
        if (const asset_render_resource_materialized_cache_entry* entry = find_ready(id);
            entry != nullptr) {
            return entry;
        }
        return find_blocked(id);
    }

    [[nodiscard]] const asset_render_resource_materialized_cache_entry* find_runtime_cache_key(
        std::string_view runtime_cache_key) const
    {
        for (const asset_render_resource_materialized_cache_entry& entry : ready) {
            if (entry.runtime_cache_key == runtime_cache_key) {
                return &entry;
            }
        }
        return nullptr;
    }
};

struct asset_render_resource_materialized_cache_diff_entry {
    asset_render_resource_materialized_cache_delta_kind kind =
        asset_render_resource_materialized_cache_delta_kind::replaced;
    std::string id;
    asset_type type = asset_type::generic;
    std::optional<asset_render_resource_materialized_cache_entry> before;
    std::optional<asset_render_resource_materialized_cache_entry> after;
    bool logical_identity_changed = false;
    bool cache_key_changed = false;
    bool content_hash_changed = false;
    bool runtime_cache_key_changed = false;
    bool materialized_path_changed = false;
    bool status_changed = false;
    std::string invalidated_runtime_cache_key;
    std::string replacement_runtime_cache_key;
    std::string diagnostic;

    [[nodiscard]] bool invalidates() const
    {
        return kind == asset_render_resource_materialized_cache_delta_kind::replaced
            || kind == asset_render_resource_materialized_cache_delta_kind::removed
            || kind == asset_render_resource_materialized_cache_delta_kind::invalidated;
    }

    [[nodiscard]] bool has_field_delta() const
    {
        return logical_identity_changed || cache_key_changed || content_hash_changed
            || runtime_cache_key_changed || materialized_path_changed || status_changed;
    }
};

struct asset_render_resource_materialized_cache_diff_summary {
    std::vector<asset_render_resource_materialized_cache_diff_entry> reused;
    std::vector<asset_render_resource_materialized_cache_diff_entry> added;
    std::vector<asset_render_resource_materialized_cache_diff_entry> removed;
    std::vector<asset_render_resource_materialized_cache_diff_entry> replaced;
    std::vector<asset_render_resource_materialized_cache_diff_entry> invalidated;
    std::ptrdiff_t requested_delta = 0;
    std::ptrdiff_t ready_delta = 0;
    std::ptrdiff_t blocked_delta = 0;

    [[nodiscard]] bool empty() const
    {
        return added.empty() && removed.empty() && replaced.empty() && invalidated.empty()
            && requested_delta == 0 && ready_delta == 0 && blocked_delta == 0;
    }

    [[nodiscard]] std::size_t change_count() const
    {
        return added.size() + removed.size() + replaced.size() + invalidated.size();
    }

    [[nodiscard]] std::size_t invalidation_count() const
    {
        return removed.size() + replaced.size() + invalidated.size();
    }

    [[nodiscard]] const asset_render_resource_materialized_cache_diff_entry* find_reused(
        std::string_view id) const
    {
        for (const asset_render_resource_materialized_cache_diff_entry& entry : reused) {
            if (entry.id == id) {
                return &entry;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const asset_render_resource_materialized_cache_diff_entry* find_added(
        std::string_view id) const
    {
        for (const asset_render_resource_materialized_cache_diff_entry& entry : added) {
            if (entry.id == id) {
                return &entry;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const asset_render_resource_materialized_cache_diff_entry* find_removed(
        std::string_view id) const
    {
        for (const asset_render_resource_materialized_cache_diff_entry& entry : removed) {
            if (entry.id == id) {
                return &entry;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const asset_render_resource_materialized_cache_diff_entry* find_replaced(
        std::string_view id) const
    {
        for (const asset_render_resource_materialized_cache_diff_entry& entry : replaced) {
            if (entry.id == id) {
                return &entry;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const asset_render_resource_materialized_cache_diff_entry* find_invalidated(
        std::string_view id) const
    {
        for (const asset_render_resource_materialized_cache_diff_entry& entry : invalidated) {
            if (entry.id == id) {
                return &entry;
            }
        }
        return nullptr;
    }
};

struct asset_render_resource_payload_bridge_summary {
    std::vector<asset_render_resource_payload_bridge_entry> accepted;
    std::vector<asset_render_resource_payload_bridge_entry> blocked;
    std::size_t requested_address_count = 0U;
    std::size_t accepted_count = 0U;
    std::size_t missing_materialized_bytes_count = 0U;
    std::size_t type_mismatch_count = 0U;
    std::size_t cache_key_mismatch_count = 0U;
    std::size_t duplicate_canonical_identity_count = 0U;
    std::size_t duplicate_materialized_payload_id_count = 0U;

    [[nodiscard]] bool ok() const
    {
        return blocked.empty();
    }

    [[nodiscard]] std::size_t entry_count() const
    {
        return accepted.size() + blocked.size();
    }

    [[nodiscard]] std::size_t blocked_count() const
    {
        return blocked.size();
    }

    [[nodiscard]] const asset_render_resource_payload_bridge_entry* find_accepted(
        std::string_view id) const
    {
        for (const asset_render_resource_payload_bridge_entry& entry : accepted) {
            if (entry.address.id == id) {
                return &entry;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const asset_render_resource_payload_bridge_entry* find_blocked(
        std::string_view id) const
    {
        for (const asset_render_resource_payload_bridge_entry& entry : blocked) {
            if (entry.address.id == id) {
                return &entry;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const asset_render_resource_payload_bridge_entry* find_entry(
        std::string_view id) const
    {
        if (const asset_render_resource_payload_bridge_entry* entry = find_accepted(id);
            entry != nullptr) {
            return entry;
        }
        return find_blocked(id);
    }

    [[nodiscard]] const asset_render_resource_payload_bridge_entry* find_canonical_identity(
        std::string_view canonical_identity) const
    {
        for (const asset_render_resource_payload_bridge_entry& entry : accepted) {
            if (entry.address.canonical_identity == canonical_identity) {
                return &entry;
            }
        }
        for (const asset_render_resource_payload_bridge_entry& entry : blocked) {
            if (entry.address.canonical_identity == canonical_identity) {
                return &entry;
            }
        }
        return nullptr;
    }
};

inline asset_render_resource_payload_bridge_status asset_render_resource_payload_bridge_status_from_selection(
    asset_materialized_byte_payload_selection_status status)
{
    switch (status) {
        case asset_materialized_byte_payload_selection_status::selected:
            return asset_render_resource_payload_bridge_status::accepted;
        case asset_materialized_byte_payload_selection_status::missing_id:
            return asset_render_resource_payload_bridge_status::missing_materialized_bytes;
        case asset_materialized_byte_payload_selection_status::wrong_type:
            return asset_render_resource_payload_bridge_status::type_mismatch;
        case asset_materialized_byte_payload_selection_status::cache_key_mismatch:
            return asset_render_resource_payload_bridge_status::cache_key_mismatch;
        case asset_materialized_byte_payload_selection_status::duplicate_id:
            return asset_render_resource_payload_bridge_status::duplicate_materialized_payload_id;
        case asset_materialized_byte_payload_selection_status::blocked_payload:
        case asset_materialized_byte_payload_selection_status::integrity_failure:
            break;
    }
    return asset_render_resource_payload_bridge_status::missing_materialized_bytes;
}

inline void count_asset_render_resource_payload_bridge_entry(
    asset_render_resource_payload_bridge_summary& summary,
    const asset_render_resource_payload_bridge_entry& entry)
{
    switch (entry.status) {
        case asset_render_resource_payload_bridge_status::accepted:
            ++summary.accepted_count;
            break;
        case asset_render_resource_payload_bridge_status::missing_materialized_bytes:
            ++summary.missing_materialized_bytes_count;
            break;
        case asset_render_resource_payload_bridge_status::type_mismatch:
            ++summary.type_mismatch_count;
            break;
        case asset_render_resource_payload_bridge_status::cache_key_mismatch:
            ++summary.cache_key_mismatch_count;
            break;
        case asset_render_resource_payload_bridge_status::duplicate_canonical_identity:
            ++summary.duplicate_canonical_identity_count;
            break;
        case asset_render_resource_payload_bridge_status::duplicate_materialized_payload_id:
            ++summary.duplicate_materialized_payload_id_count;
            break;
    }
}

inline void add_asset_render_resource_payload_bridge_entry(
    asset_render_resource_payload_bridge_summary& summary,
    asset_render_resource_payload_bridge_entry entry)
{
    count_asset_render_resource_payload_bridge_entry(summary, entry);
    if (entry.ok()) {
        summary.accepted.push_back(std::move(entry));
    } else {
        summary.blocked.push_back(std::move(entry));
    }
}

inline asset_materialized_byte_payload_selection_request make_asset_render_resource_payload_selection_request(
    const asset_render_resource_address_entry& address)
{
    return asset_materialized_byte_payload_selection_request{
        .id = address.id,
        .expected_type = address.type,
        .expected_cache_key = address.cache_key,
        .require_ready = false,
        .require_integrity_ok = false,
    };
}

inline asset_render_resource_payload_bridge_entry make_asset_render_resource_payload_bridge_duplicate(
    const asset_render_resource_address_entry& address,
    std::string related_id)
{
    return asset_render_resource_payload_bridge_entry{
        .status = asset_render_resource_payload_bridge_status::duplicate_canonical_identity,
        .address = address,
        .related_id = std::move(related_id),
        .diagnostic = "render resource canonical identity is duplicated",
    };
}

inline asset_render_resource_payload_bridge_entry make_asset_render_resource_payload_bridge_entry(
    const asset_render_resource_address_entry& address,
    const asset_materialized_byte_payload_bundle& bundle)
{
    asset_materialized_byte_payload_selection_result selection = select_materialized_asset_byte_payload(
        bundle,
        make_asset_render_resource_payload_selection_request(address));
    const asset_render_resource_payload_bridge_status status =
        asset_render_resource_payload_bridge_status_from_selection(selection.status);
    return asset_render_resource_payload_bridge_entry{
        .status = status,
        .address = address,
        .selection = selection,
        .selected_snapshot = status == asset_render_resource_payload_bridge_status::accepted
            ? selection.snapshot
            : std::nullopt,
        .diagnostic = status == asset_render_resource_payload_bridge_status::accepted
            ? "render resource address matched materialized byte payload"
            : selection.diagnostic,
    };
}

inline asset_render_resource_payload_bridge_summary bridge_asset_render_resource_addresses_to_payloads(
    const asset_render_resource_address_summary& addresses,
    const asset_materialized_byte_payload_bundle& bundle)
{
    asset_render_resource_payload_bridge_summary summary{
        .requested_address_count = addresses.entries.size(),
    };

    for (const asset_render_resource_address_entry& address : addresses.entries) {
        if (const asset_render_resource_payload_bridge_entry* duplicate =
                summary.find_canonical_identity(address.canonical_identity);
            duplicate != nullptr) {
            add_asset_render_resource_payload_bridge_entry(
                summary,
                make_asset_render_resource_payload_bridge_duplicate(
                    address,
                    duplicate->address.id));
            continue;
        }

        add_asset_render_resource_payload_bridge_entry(
            summary,
            make_asset_render_resource_payload_bridge_entry(address, bundle));
    }

    return summary;
}

inline asset_render_resource_materialized_cache_status asset_render_resource_materialized_cache_status_from_bridge(
    asset_render_resource_payload_bridge_status status)
{
    switch (status) {
        case asset_render_resource_payload_bridge_status::accepted:
            return asset_render_resource_materialized_cache_status::ready;
        case asset_render_resource_payload_bridge_status::missing_materialized_bytes:
            return asset_render_resource_materialized_cache_status::missing_materialized_bytes;
        case asset_render_resource_payload_bridge_status::type_mismatch:
            return asset_render_resource_materialized_cache_status::type_mismatch;
        case asset_render_resource_payload_bridge_status::cache_key_mismatch:
            return asset_render_resource_materialized_cache_status::cache_key_mismatch;
        case asset_render_resource_payload_bridge_status::duplicate_canonical_identity:
            return asset_render_resource_materialized_cache_status::duplicate_canonical_identity;
        case asset_render_resource_payload_bridge_status::duplicate_materialized_payload_id:
            return asset_render_resource_materialized_cache_status::duplicate_materialized_payload_id;
    }
    return asset_render_resource_materialized_cache_status::missing_materialized_bytes;
}

inline const asset_render_resource_address_diagnostic* find_asset_render_resource_address_diagnostic(
    const asset_render_resource_address_summary& addresses,
    std::string_view id)
{
    for (const asset_render_resource_address_diagnostic& diagnostic : addresses.diagnostics) {
        if (diagnostic.id == id) {
            return &diagnostic;
        }
    }
    return nullptr;
}

inline std::string make_asset_render_resource_materialized_cache_key(
    const asset_render_resource_address_entry& address,
    const asset_materialized_byte_payload_snapshot& payload)
{
    std::string key = "render-resource|";
    key.append(asset_type_token(address.type));
    key.push_back('|');
    key.append(address.canonical_identity);
    key.push_back('|');
    key.append(payload.cache_key);
    key.append("|hash=");
    key.append(payload.content_hash);
    return key;
}

inline void count_asset_render_resource_materialized_cache_entry(
    asset_render_resource_materialized_cache_summary& summary,
    const asset_render_resource_materialized_cache_entry& entry)
{
    switch (entry.status) {
        case asset_render_resource_materialized_cache_status::ready:
            ++summary.ready_count;
            break;
        case asset_render_resource_materialized_cache_status::address_rejected:
            ++summary.address_rejected_count;
            break;
        case asset_render_resource_materialized_cache_status::missing_render_resource_address:
            ++summary.missing_render_resource_address_count;
            break;
        case asset_render_resource_materialized_cache_status::missing_materialized_bytes:
            ++summary.missing_materialized_bytes_count;
            break;
        case asset_render_resource_materialized_cache_status::type_mismatch:
            ++summary.type_mismatch_count;
            break;
        case asset_render_resource_materialized_cache_status::cache_key_mismatch:
            ++summary.cache_key_mismatch_count;
            break;
        case asset_render_resource_materialized_cache_status::duplicate_canonical_identity:
            ++summary.duplicate_canonical_identity_count;
            break;
        case asset_render_resource_materialized_cache_status::duplicate_materialized_payload_id:
            ++summary.duplicate_materialized_payload_id_count;
            break;
    }
}

inline void add_asset_render_resource_materialized_cache_entry(
    asset_render_resource_materialized_cache_summary& summary,
    asset_render_resource_materialized_cache_entry entry)
{
    count_asset_render_resource_materialized_cache_entry(summary, entry);
    if (entry.ready()) {
        summary.ready.push_back(std::move(entry));
    } else {
        summary.blocked.push_back(std::move(entry));
    }
}

inline asset_render_resource_materialized_cache_entry make_asset_render_resource_materialized_cache_entry(
    const asset_render_resource_payload_bridge_entry& bridge_entry)
{
    asset_render_resource_materialized_cache_entry entry{
        .status = asset_render_resource_materialized_cache_status_from_bridge(bridge_entry.status),
        .id = bridge_entry.address.id,
        .type = bridge_entry.address.type,
        .canonical_identity = bridge_entry.address.canonical_identity,
        .cache_key = bridge_entry.address.cache_key,
        .source_uri = bridge_entry.address.source_uri,
        .related_id = bridge_entry.related_id,
        .diagnostic = bridge_entry.diagnostic,
    };

    if (bridge_entry.selected_snapshot.has_value()) {
        const asset_materialized_byte_payload_snapshot& payload = *bridge_entry.selected_snapshot;
        entry.cache_key = payload.cache_key;
        entry.source_uri = payload.source_uri;
        entry.materialized_path = payload.materialized_path;
        entry.byte_count = payload.byte_count;
        entry.payload_byte_count = payload.payload_byte_count;
        entry.content_hash = payload.content_hash;
        entry.runtime_cache_key = make_asset_render_resource_materialized_cache_key(
            bridge_entry.address,
            payload);
    }

    return entry;
}

inline asset_render_resource_materialized_cache_entry make_asset_render_resource_materialized_cache_entry(
    const asset_render_resource_address_diagnostic& diagnostic)
{
    return asset_render_resource_materialized_cache_entry{
        .status = asset_render_resource_materialized_cache_status::address_rejected,
        .id = diagnostic.id,
        .type = diagnostic.type,
        .cache_key = diagnostic.cache_key,
        .source_uri = diagnostic.source_uri,
        .related_id = diagnostic.related_id,
        .diagnostic = diagnostic.diagnostic,
    };
}

inline asset_render_resource_materialized_cache_entry make_asset_render_resource_materialized_cache_missing_entry(
    const asset_render_resource_materialized_cache_request& request)
{
    return asset_render_resource_materialized_cache_entry{
        .status = asset_render_resource_materialized_cache_status::missing_render_resource_address,
        .id = request.id,
        .type = request.expected_type,
        .diagnostic = "render resource address was not found",
    };
}

inline asset_render_resource_materialized_cache_entry make_asset_render_resource_materialized_cache_type_mismatch_entry(
    const asset_render_resource_materialized_cache_request& request,
    const asset_render_resource_address_entry& address)
{
    return asset_render_resource_materialized_cache_entry{
        .status = asset_render_resource_materialized_cache_status::type_mismatch,
        .id = address.id,
        .type = address.type,
        .canonical_identity = address.canonical_identity,
        .cache_key = address.cache_key,
        .source_uri = address.source_uri,
        .diagnostic = "render resource address type does not match the cache request",
    };
}

inline asset_render_resource_materialized_cache_summary make_asset_render_resource_materialized_cache_summary(
    const asset_render_resource_address_summary& addresses,
    const asset_materialized_byte_payload_bundle& bundle)
{
    const asset_render_resource_payload_bridge_summary bridge =
        bridge_asset_render_resource_addresses_to_payloads(addresses, bundle);
    asset_render_resource_materialized_cache_summary summary{
        .requested_count = addresses.entries.size() + addresses.diagnostics.size(),
    };

    for (const asset_render_resource_payload_bridge_entry& entry : bridge.accepted) {
        add_asset_render_resource_materialized_cache_entry(
            summary,
            make_asset_render_resource_materialized_cache_entry(entry));
    }
    for (const asset_render_resource_payload_bridge_entry& entry : bridge.blocked) {
        add_asset_render_resource_materialized_cache_entry(
            summary,
            make_asset_render_resource_materialized_cache_entry(entry));
    }
    for (const asset_render_resource_address_diagnostic& diagnostic : addresses.diagnostics) {
        add_asset_render_resource_materialized_cache_entry(
            summary,
            make_asset_render_resource_materialized_cache_entry(diagnostic));
    }

    return summary;
}

inline asset_render_resource_materialized_cache_summary make_asset_render_resource_materialized_cache_summary(
    const asset_render_resource_address_summary& addresses,
    const asset_materialized_byte_payload_bundle& bundle,
    const std::vector<asset_render_resource_materialized_cache_request>& requests)
{
    const asset_render_resource_payload_bridge_summary bridge =
        bridge_asset_render_resource_addresses_to_payloads(addresses, bundle);
    asset_render_resource_materialized_cache_summary summary{
        .requested_count = requests.size(),
    };

    for (const asset_render_resource_materialized_cache_request& request : requests) {
        if (const asset_render_resource_payload_bridge_entry* bridge_entry = bridge.find_entry(request.id);
            bridge_entry != nullptr) {
            if (request.expected_type != asset_type::generic
                && bridge_entry->address.type != request.expected_type) {
                add_asset_render_resource_materialized_cache_entry(
                    summary,
                    make_asset_render_resource_materialized_cache_type_mismatch_entry(
                        request,
                        bridge_entry->address));
            } else {
                add_asset_render_resource_materialized_cache_entry(
                    summary,
                    make_asset_render_resource_materialized_cache_entry(*bridge_entry));
            }
            continue;
        }

        if (const asset_render_resource_address_diagnostic* diagnostic =
                find_asset_render_resource_address_diagnostic(addresses, request.id);
            diagnostic != nullptr) {
            add_asset_render_resource_materialized_cache_entry(
                summary,
                make_asset_render_resource_materialized_cache_entry(*diagnostic));
            continue;
        }

        add_asset_render_resource_materialized_cache_entry(
            summary,
            make_asset_render_resource_materialized_cache_missing_entry(request));
    }

    return summary;
}

inline std::ptrdiff_t asset_render_resource_materialized_cache_count_delta(
    std::size_t before,
    std::size_t after)
{
    return static_cast<std::ptrdiff_t>(after) - static_cast<std::ptrdiff_t>(before);
}

template <typename Visitor>
inline void for_each_asset_render_resource_materialized_cache_entry(
    const asset_render_resource_materialized_cache_summary& summary,
    Visitor&& visitor)
{
    for (const asset_render_resource_materialized_cache_entry& entry : summary.ready) {
        visitor(entry);
    }
    for (const asset_render_resource_materialized_cache_entry& entry : summary.blocked) {
        visitor(entry);
    }
}

inline const asset_render_resource_materialized_cache_entry* find_asset_render_resource_materialized_cache_entry(
    const asset_render_resource_materialized_cache_summary& summary,
    std::string_view id)
{
    return summary.find_entry(id);
}

inline bool asset_render_resource_materialized_cache_logical_identity_changed(
    const asset_render_resource_materialized_cache_entry& before,
    const asset_render_resource_materialized_cache_entry& after)
{
    return before.id != after.id || before.type != after.type
        || before.canonical_identity != after.canonical_identity
        || before.source_uri != after.source_uri;
}

inline asset_render_resource_materialized_cache_diff_entry make_asset_render_resource_materialized_cache_diff_entry(
    asset_render_resource_materialized_cache_delta_kind kind,
    const std::optional<asset_render_resource_materialized_cache_entry>& before,
    const std::optional<asset_render_resource_materialized_cache_entry>& after)
{
    const asset_render_resource_materialized_cache_entry* before_entry =
        before.has_value() ? &*before : nullptr;
    const asset_render_resource_materialized_cache_entry* after_entry =
        after.has_value() ? &*after : nullptr;
    const asset_render_resource_materialized_cache_entry& anchor =
        after_entry != nullptr ? *after_entry : *before_entry;

    asset_render_resource_materialized_cache_diff_entry entry{
        .kind = kind,
        .id = anchor.id,
        .type = anchor.type,
        .before = before,
        .after = after,
    };

    if (before_entry != nullptr && after_entry != nullptr) {
        entry.logical_identity_changed =
            asset_render_resource_materialized_cache_logical_identity_changed(*before_entry, *after_entry);
        entry.cache_key_changed = before_entry->cache_key != after_entry->cache_key;
        entry.content_hash_changed = before_entry->content_hash != after_entry->content_hash;
        entry.runtime_cache_key_changed = before_entry->runtime_cache_key != after_entry->runtime_cache_key;
        entry.materialized_path_changed = before_entry->materialized_path != after_entry->materialized_path;
        entry.status_changed = before_entry->status != after_entry->status;
    }

    if (before_entry != nullptr) {
        entry.invalidated_runtime_cache_key = before_entry->runtime_cache_key;
    }
    if (after_entry != nullptr) {
        entry.replacement_runtime_cache_key = after_entry->runtime_cache_key;
    }

    switch (kind) {
        case asset_render_resource_materialized_cache_delta_kind::reused:
            entry.diagnostic = "render resource materialized cache entry reused";
            break;
        case asset_render_resource_materialized_cache_delta_kind::added:
            entry.diagnostic = "render resource materialized cache entry added";
            break;
        case asset_render_resource_materialized_cache_delta_kind::removed:
            entry.diagnostic = "render resource materialized cache entry removed and invalidated";
            break;
        case asset_render_resource_materialized_cache_delta_kind::replaced:
            entry.diagnostic = "render resource materialized cache entry replaced by revision or content change";
            break;
        case asset_render_resource_materialized_cache_delta_kind::invalidated:
            entry.diagnostic = "render resource materialized cache entry invalidated by blocked replacement";
            break;
    }

    return entry;
}

inline asset_render_resource_materialized_cache_delta_kind classify_asset_render_resource_materialized_cache_delta(
    const asset_render_resource_materialized_cache_entry& before,
    const asset_render_resource_materialized_cache_entry& after)
{
    if (before.ready() && after.ready() && before.runtime_cache_key == after.runtime_cache_key) {
        return asset_render_resource_materialized_cache_delta_kind::reused;
    }
    if (before.ready() && after.ready()) {
        return asset_render_resource_materialized_cache_delta_kind::replaced;
    }
    if (before.ready() && !after.ready()) {
        return asset_render_resource_materialized_cache_delta_kind::invalidated;
    }
    if (!before.ready() && after.ready()) {
        return asset_render_resource_materialized_cache_delta_kind::added;
    }
    if (before.status == after.status && before.diagnostic == after.diagnostic) {
        return asset_render_resource_materialized_cache_delta_kind::reused;
    }
    return asset_render_resource_materialized_cache_delta_kind::invalidated;
}

inline void add_asset_render_resource_materialized_cache_diff_entry(
    asset_render_resource_materialized_cache_diff_summary& diff,
    asset_render_resource_materialized_cache_diff_entry entry)
{
    switch (entry.kind) {
        case asset_render_resource_materialized_cache_delta_kind::reused:
            diff.reused.push_back(std::move(entry));
            break;
        case asset_render_resource_materialized_cache_delta_kind::added:
            diff.added.push_back(std::move(entry));
            break;
        case asset_render_resource_materialized_cache_delta_kind::removed:
            diff.removed.push_back(std::move(entry));
            break;
        case asset_render_resource_materialized_cache_delta_kind::replaced:
            diff.replaced.push_back(std::move(entry));
            break;
        case asset_render_resource_materialized_cache_delta_kind::invalidated:
            diff.invalidated.push_back(std::move(entry));
            break;
    }
}

inline asset_render_resource_materialized_cache_diff_summary diff_asset_render_resource_materialized_cache_summaries(
    const asset_render_resource_materialized_cache_summary& before,
    const asset_render_resource_materialized_cache_summary& after)
{
    asset_render_resource_materialized_cache_diff_summary diff{
        .requested_delta = asset_render_resource_materialized_cache_count_delta(
            before.requested_count,
            after.requested_count),
        .ready_delta = asset_render_resource_materialized_cache_count_delta(
            before.ready_count,
            after.ready_count),
        .blocked_delta = asset_render_resource_materialized_cache_count_delta(
            before.blocked_count(),
            after.blocked_count()),
    };

    for_each_asset_render_resource_materialized_cache_entry(
        before,
        [&](const asset_render_resource_materialized_cache_entry& before_entry) {
            const asset_render_resource_materialized_cache_entry* after_entry =
                find_asset_render_resource_materialized_cache_entry(after, before_entry.id);
            if (after_entry == nullptr) {
                add_asset_render_resource_materialized_cache_diff_entry(
                    diff,
                    make_asset_render_resource_materialized_cache_diff_entry(
                        asset_render_resource_materialized_cache_delta_kind::removed,
                        before_entry,
                        std::nullopt));
                return;
            }

            add_asset_render_resource_materialized_cache_diff_entry(
                diff,
                make_asset_render_resource_materialized_cache_diff_entry(
                    classify_asset_render_resource_materialized_cache_delta(before_entry, *after_entry),
                    before_entry,
                    *after_entry));
        });

    for_each_asset_render_resource_materialized_cache_entry(
        after,
        [&](const asset_render_resource_materialized_cache_entry& after_entry) {
            if (find_asset_render_resource_materialized_cache_entry(before, after_entry.id) != nullptr) {
                return;
            }
            add_asset_render_resource_materialized_cache_diff_entry(
                diff,
                make_asset_render_resource_materialized_cache_diff_entry(
                    asset_render_resource_materialized_cache_delta_kind::added,
                    std::nullopt,
                    after_entry));
        });

    return diff;
}

inline std::string asset_render_resource_payload_bridge_status_name(
    asset_render_resource_payload_bridge_status status)
{
    switch (status) {
        case asset_render_resource_payload_bridge_status::accepted:
            return "accepted";
        case asset_render_resource_payload_bridge_status::missing_materialized_bytes:
            return "missing_materialized_bytes";
        case asset_render_resource_payload_bridge_status::type_mismatch:
            return "type_mismatch";
        case asset_render_resource_payload_bridge_status::cache_key_mismatch:
            return "cache_key_mismatch";
        case asset_render_resource_payload_bridge_status::duplicate_canonical_identity:
            return "duplicate_canonical_identity";
        case asset_render_resource_payload_bridge_status::duplicate_materialized_payload_id:
            return "duplicate_materialized_payload_id";
    }
    return "missing_materialized_bytes";
}

inline std::string asset_render_resource_materialized_cache_status_name(
    asset_render_resource_materialized_cache_status status)
{
    switch (status) {
        case asset_render_resource_materialized_cache_status::ready:
            return "ready";
        case asset_render_resource_materialized_cache_status::address_rejected:
            return "address_rejected";
        case asset_render_resource_materialized_cache_status::missing_render_resource_address:
            return "missing_render_resource_address";
        case asset_render_resource_materialized_cache_status::missing_materialized_bytes:
            return "missing_materialized_bytes";
        case asset_render_resource_materialized_cache_status::type_mismatch:
            return "type_mismatch";
        case asset_render_resource_materialized_cache_status::cache_key_mismatch:
            return "cache_key_mismatch";
        case asset_render_resource_materialized_cache_status::duplicate_canonical_identity:
            return "duplicate_canonical_identity";
        case asset_render_resource_materialized_cache_status::duplicate_materialized_payload_id:
            return "duplicate_materialized_payload_id";
    }
    return "missing_render_resource_address";
}

inline std::string asset_render_resource_materialized_cache_delta_kind_name(
    asset_render_resource_materialized_cache_delta_kind kind)
{
    switch (kind) {
        case asset_render_resource_materialized_cache_delta_kind::reused:
            return "reused";
        case asset_render_resource_materialized_cache_delta_kind::added:
            return "added";
        case asset_render_resource_materialized_cache_delta_kind::removed:
            return "removed";
        case asset_render_resource_materialized_cache_delta_kind::replaced:
            return "replaced";
        case asset_render_resource_materialized_cache_delta_kind::invalidated:
            return "invalidated";
    }
    return "invalidated";
}

} // namespace quiz_vulkan::assets
