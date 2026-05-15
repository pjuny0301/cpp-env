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

struct asset_materialized_byte_payload_snapshot {
    std::string id;
    asset_type type = asset_type::generic;
    asset_cache_key cache_key;
    std::string source_uri;
    std::filesystem::path materialized_path;
    std::size_t byte_count = 0U;
    std::size_t payload_byte_count = 0U;
    std::string content_hash;
    asset_materialized_bytes_handoff_status status =
        asset_materialized_bytes_handoff_status::materialization_blocked;
    runtime_materialized_asset_lookup_status materialized_status =
        runtime_materialized_asset_lookup_status::missing_id;
    asset_bytes_load_status load_status = asset_bytes_load_status::missing_bytes;
    bool ready = false;
};

struct asset_materialized_byte_payload_bundle_snapshot {
    std::vector<asset_materialized_byte_payload_snapshot> payloads;
    std::size_t skipped_generic_count = 0U;

    [[nodiscard]] bool ok() const
    {
        return blocked_count() == 0U;
    }

    [[nodiscard]] std::size_t ready_count() const
    {
        std::size_t count = 0U;
        for (const asset_materialized_byte_payload_snapshot& payload : payloads) {
            if (payload.ready) {
                ++count;
            }
        }
        return count;
    }

    [[nodiscard]] std::size_t blocked_count() const
    {
        return payloads.size() - ready_count();
    }

    [[nodiscard]] std::size_t payload_count() const
    {
        return payloads.size();
    }

    [[nodiscard]] const asset_materialized_byte_payload_snapshot* find_payload(std::string_view id) const
    {
        for (const asset_materialized_byte_payload_snapshot& payload : payloads) {
            if (payload.id == id) {
                return &payload;
            }
        }
        return nullptr;
    }
};

enum class asset_materialized_byte_payload_delta_kind {
    added,
    removed,
    changed,
};

struct asset_materialized_byte_payload_diff_entry {
    asset_materialized_byte_payload_delta_kind kind = asset_materialized_byte_payload_delta_kind::changed;
    std::string id;
    asset_type type = asset_type::generic;
    std::optional<asset_materialized_byte_payload_snapshot> before;
    std::optional<asset_materialized_byte_payload_snapshot> after;
    bool type_changed = false;
    bool cache_key_changed = false;
    bool source_uri_changed = false;
    bool materialized_path_changed = false;
    bool byte_count_changed = false;
    bool payload_byte_count_changed = false;
    bool content_hash_changed = false;
    bool status_changed = false;
    bool readiness_changed = false;

    [[nodiscard]] bool has_field_delta() const
    {
        return type_changed || cache_key_changed || source_uri_changed || materialized_path_changed
            || byte_count_changed || payload_byte_count_changed || content_hash_changed || status_changed
            || readiness_changed;
    }
};

struct asset_materialized_byte_payload_diff_summary {
    std::vector<asset_materialized_byte_payload_diff_entry> added;
    std::vector<asset_materialized_byte_payload_diff_entry> removed;
    std::vector<asset_materialized_byte_payload_diff_entry> changed;

    [[nodiscard]] bool empty() const
    {
        return added.empty() && removed.empty() && changed.empty();
    }

    [[nodiscard]] std::size_t change_count() const
    {
        return added.size() + removed.size() + changed.size();
    }

    [[nodiscard]] const asset_materialized_byte_payload_diff_entry* find_added(std::string_view id) const
    {
        for (const asset_materialized_byte_payload_diff_entry& entry : added) {
            if (entry.id == id) {
                return &entry;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const asset_materialized_byte_payload_diff_entry* find_removed(std::string_view id) const
    {
        for (const asset_materialized_byte_payload_diff_entry& entry : removed) {
            if (entry.id == id) {
                return &entry;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const asset_materialized_byte_payload_diff_entry* find_changed(std::string_view id) const
    {
        for (const asset_materialized_byte_payload_diff_entry& entry : changed) {
            if (entry.id == id) {
                return &entry;
            }
        }
        return nullptr;
    }
};

enum class asset_materialized_byte_payload_selection_status {
    selected,
    missing_id,
    wrong_type,
    blocked_payload,
    integrity_failure,
    duplicate_id,
    cache_key_mismatch,
};

struct asset_materialized_byte_payload_selection_request {
    std::string id;
    asset_type expected_type = asset_type::generic;
    std::optional<asset_cache_key> expected_cache_key;
    bool require_ready = true;
    bool require_integrity_ok = true;
};

struct asset_materialized_byte_payload_selection_result {
    asset_materialized_byte_payload_selection_status status =
        asset_materialized_byte_payload_selection_status::missing_id;
    const asset_materialized_byte_payload* payload = nullptr;
    std::optional<asset_materialized_byte_payload_snapshot> snapshot;
    std::string id;
    asset_type expected_type = asset_type::generic;
    std::optional<asset_cache_key> expected_cache_key;
    asset_type actual_type = asset_type::generic;
    asset_cache_key actual_cache_key;
    std::size_t match_count = 0U;
    std::string diagnostic;

    [[nodiscard]] bool selected() const
    {
        return status == asset_materialized_byte_payload_selection_status::selected && payload != nullptr;
    }
};

struct asset_materialized_byte_payload_filter {
    std::optional<asset_type> type;
    std::optional<std::string> id;
    std::optional<asset_cache_key> cache_key;
    std::optional<bool> ready;
    std::optional<bool> integrity_ok;
};

struct asset_materialized_byte_payload_filter_result {
    std::vector<const asset_materialized_byte_payload*> payloads;
    std::vector<asset_materialized_byte_payload_snapshot> snapshots;

    [[nodiscard]] bool empty() const
    {
        return payloads.empty();
    }

    [[nodiscard]] std::size_t match_count() const
    {
        return payloads.size();
    }

    [[nodiscard]] const asset_materialized_byte_payload* first_payload() const
    {
        return payloads.empty() ? nullptr : payloads.front();
    }
};

struct asset_materialized_byte_payload_request_transaction_item {
    std::size_t request_index = 0U;
    asset_materialized_byte_payload_selection_request request;
    asset_materialized_byte_payload_selection_result selection;
    std::optional<asset_materialized_byte_payload_snapshot> selected_snapshot;

    [[nodiscard]] bool selected() const
    {
        return selection.selected();
    }
};

struct asset_materialized_byte_payload_request_transaction_summary {
    std::size_t request_count = 0U;
    std::size_t selected_count = 0U;
    std::size_t ready_count = 0U;
    std::size_t blocked_count = 0U;
    std::size_t missing_count = 0U;
    std::size_t wrong_type_count = 0U;
    std::size_t cache_key_mismatch_count = 0U;
    std::size_t integrity_failure_count = 0U;
    std::size_t duplicate_count = 0U;

    [[nodiscard]] std::size_t failed_count() const
    {
        return blocked_count + missing_count + wrong_type_count + cache_key_mismatch_count + integrity_failure_count
            + duplicate_count;
    }

    [[nodiscard]] bool ok() const
    {
        return failed_count() == 0U;
    }
};

struct asset_materialized_byte_payload_request_transaction {
    std::vector<asset_materialized_byte_payload_request_transaction_item> items;
    asset_materialized_byte_payload_request_transaction_summary summary;

    [[nodiscard]] bool ok() const
    {
        return summary.ok();
    }

    [[nodiscard]] std::size_t request_count() const
    {
        return items.size();
    }

    [[nodiscard]] const asset_materialized_byte_payload_request_transaction_item* item_at(
        std::size_t index) const
    {
        return index < items.size() ? &items[index] : nullptr;
    }

    [[nodiscard]] const asset_materialized_byte_payload_request_transaction_item* find_request(
        std::string_view id) const
    {
        for (const asset_materialized_byte_payload_request_transaction_item& item : items) {
            if (item.request.id == id) {
                return &item;
            }
        }
        return nullptr;
    }
};

struct asset_materialized_byte_payload_request_transaction_count_delta {
    std::ptrdiff_t request_delta = 0;
    std::ptrdiff_t selected_delta = 0;
    std::ptrdiff_t ready_delta = 0;
    std::ptrdiff_t blocked_delta = 0;
    std::ptrdiff_t missing_delta = 0;
    std::ptrdiff_t wrong_type_delta = 0;
    std::ptrdiff_t cache_key_mismatch_delta = 0;
    std::ptrdiff_t integrity_failure_delta = 0;
    std::ptrdiff_t duplicate_delta = 0;
    std::ptrdiff_t failed_delta = 0;

    [[nodiscard]] bool empty() const
    {
        return request_delta == 0 && selected_delta == 0 && ready_delta == 0 && blocked_delta == 0
            && missing_delta == 0 && wrong_type_delta == 0 && cache_key_mismatch_delta == 0
            && integrity_failure_delta == 0 && duplicate_delta == 0 && failed_delta == 0;
    }
};

enum class asset_materialized_byte_payload_request_transaction_delta_kind {
    added_request,
    removed_request,
    changed_request,
};

struct asset_materialized_byte_payload_request_transaction_diff_entry {
    asset_materialized_byte_payload_request_transaction_delta_kind kind =
        asset_materialized_byte_payload_request_transaction_delta_kind::changed_request;
    std::string id;
    std::size_t occurrence = 0U;
    std::optional<std::size_t> before_index;
    std::optional<std::size_t> after_index;
    std::optional<asset_materialized_byte_payload_selection_status> before_status;
    std::optional<asset_materialized_byte_payload_selection_status> after_status;
    std::optional<asset_materialized_byte_payload_snapshot> before_snapshot;
    std::optional<asset_materialized_byte_payload_snapshot> after_snapshot;
    std::optional<asset_materialized_byte_payload_snapshot> before_selected_snapshot;
    std::optional<asset_materialized_byte_payload_snapshot> after_selected_snapshot;
    bool status_changed = false;
    bool selected_snapshot_changed = false;
    bool readiness_changed = false;
    bool integrity_failure_changed = false;
    bool cache_key_mismatch_changed = false;

    [[nodiscard]] bool has_field_delta() const
    {
        return status_changed || selected_snapshot_changed || readiness_changed || integrity_failure_changed
            || cache_key_mismatch_changed;
    }
};

struct asset_materialized_byte_payload_request_transaction_diff_summary {
    asset_materialized_byte_payload_request_transaction_summary before_summary;
    asset_materialized_byte_payload_request_transaction_summary after_summary;
    asset_materialized_byte_payload_request_transaction_count_delta count_delta;
    std::vector<asset_materialized_byte_payload_request_transaction_diff_entry> added;
    std::vector<asset_materialized_byte_payload_request_transaction_diff_entry> removed;
    std::vector<asset_materialized_byte_payload_request_transaction_diff_entry> changed;
    std::string diagnostic = "materialized byte payload request transaction diff computed";

    [[nodiscard]] bool empty() const
    {
        return added.empty() && removed.empty() && changed.empty() && count_delta.empty();
    }

    [[nodiscard]] std::size_t change_count() const
    {
        return added.size() + removed.size() + changed.size();
    }

    [[nodiscard]] const asset_materialized_byte_payload_request_transaction_diff_entry* find_added(
        std::string_view id) const
    {
        for (const asset_materialized_byte_payload_request_transaction_diff_entry& entry : added) {
            if (entry.id == id) {
                return &entry;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const asset_materialized_byte_payload_request_transaction_diff_entry* find_removed(
        std::string_view id) const
    {
        for (const asset_materialized_byte_payload_request_transaction_diff_entry& entry : removed) {
            if (entry.id == id) {
                return &entry;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const asset_materialized_byte_payload_request_transaction_diff_entry* find_changed(
        std::string_view id) const
    {
        for (const asset_materialized_byte_payload_request_transaction_diff_entry& entry : changed) {
            if (entry.id == id) {
                return &entry;
            }
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

template <typename Visitor>
inline void for_each_materialized_byte_payload(
    const asset_materialized_byte_payload_bundle& bundle,
    Visitor&& visitor)
{
    const auto visit_group = [&](const asset_materialized_byte_payload_group& group) {
        for (const asset_materialized_byte_payload& payload : group.ready) {
            visitor(payload);
        }
        for (const asset_materialized_byte_payload& payload : group.blocked) {
            visitor(payload);
        }
    };

    visit_group(bundle.fonts);
    visit_group(bundle.images);
    visit_group(bundle.sounds);
    visit_group(bundle.shaders);
    visit_group(bundle.decks);
}

inline bool materialized_byte_payload_integrity_ok(const asset_materialized_byte_payload& payload)
{
    return payload.issues.empty();
}

inline asset_materialized_byte_payload_snapshot make_materialized_byte_payload_snapshot(
    const asset_materialized_byte_payload& payload)
{
    return asset_materialized_byte_payload_snapshot{
        .id = payload.id,
        .type = payload.type,
        .cache_key = payload.cache_key,
        .source_uri = payload.source_uri,
        .materialized_path = payload.materialized_path,
        .byte_count = payload.byte_count,
        .payload_byte_count = payload.bytes.size(),
        .content_hash = payload.content_hash,
        .status = payload.status,
        .materialized_status = payload.materialized_status,
        .load_status = payload.load_status,
        .ready = payload.ready(),
    };
}

inline bool materialized_byte_payload_matches_filter(
    const asset_materialized_byte_payload& payload,
    const asset_materialized_byte_payload_filter& filter)
{
    if (filter.type.has_value() && payload.type != *filter.type) {
        return false;
    }
    if (filter.id.has_value() && payload.id != *filter.id) {
        return false;
    }
    if (filter.cache_key.has_value() && payload.cache_key != *filter.cache_key) {
        return false;
    }
    if (filter.ready.has_value() && payload.ready() != *filter.ready) {
        return false;
    }
    if (filter.integrity_ok.has_value() && materialized_byte_payload_integrity_ok(payload) != *filter.integrity_ok) {
        return false;
    }
    return true;
}

inline asset_materialized_byte_payload_selection_result make_materialized_byte_payload_selection_result(
    const asset_materialized_byte_payload_selection_request& request,
    asset_materialized_byte_payload_selection_status status,
    std::string diagnostic)
{
    return asset_materialized_byte_payload_selection_result{
        .status = status,
        .id = request.id,
        .expected_type = request.expected_type,
        .expected_cache_key = request.expected_cache_key,
        .diagnostic = std::move(diagnostic),
    };
}

inline void count_materialized_byte_payload_transaction_selection(
    asset_materialized_byte_payload_request_transaction_summary& summary,
    const asset_materialized_byte_payload_selection_result& result)
{
    if (result.selected()) {
        ++summary.selected_count;
    }
    if (result.selected() && result.snapshot.has_value() && result.snapshot->ready) {
        ++summary.ready_count;
    }

    switch (result.status) {
        case asset_materialized_byte_payload_selection_status::selected:
            break;
        case asset_materialized_byte_payload_selection_status::missing_id:
            ++summary.missing_count;
            break;
        case asset_materialized_byte_payload_selection_status::wrong_type:
            ++summary.wrong_type_count;
            break;
        case asset_materialized_byte_payload_selection_status::blocked_payload:
            ++summary.blocked_count;
            break;
        case asset_materialized_byte_payload_selection_status::integrity_failure:
            ++summary.integrity_failure_count;
            break;
        case asset_materialized_byte_payload_selection_status::duplicate_id:
            ++summary.duplicate_count;
            break;
        case asset_materialized_byte_payload_selection_status::cache_key_mismatch:
            ++summary.cache_key_mismatch_count;
            break;
    }
}

inline std::ptrdiff_t materialized_byte_payload_count_delta(std::size_t before, std::size_t after)
{
    return static_cast<std::ptrdiff_t>(after) - static_cast<std::ptrdiff_t>(before);
}

inline asset_materialized_byte_payload_request_transaction_count_delta make_materialized_byte_payload_transaction_count_delta(
    const asset_materialized_byte_payload_request_transaction_summary& before,
    const asset_materialized_byte_payload_request_transaction_summary& after)
{
    return asset_materialized_byte_payload_request_transaction_count_delta{
        .request_delta = materialized_byte_payload_count_delta(before.request_count, after.request_count),
        .selected_delta = materialized_byte_payload_count_delta(before.selected_count, after.selected_count),
        .ready_delta = materialized_byte_payload_count_delta(before.ready_count, after.ready_count),
        .blocked_delta = materialized_byte_payload_count_delta(before.blocked_count, after.blocked_count),
        .missing_delta = materialized_byte_payload_count_delta(before.missing_count, after.missing_count),
        .wrong_type_delta = materialized_byte_payload_count_delta(before.wrong_type_count, after.wrong_type_count),
        .cache_key_mismatch_delta =
            materialized_byte_payload_count_delta(before.cache_key_mismatch_count, after.cache_key_mismatch_count),
        .integrity_failure_delta =
            materialized_byte_payload_count_delta(before.integrity_failure_count, after.integrity_failure_count),
        .duplicate_delta = materialized_byte_payload_count_delta(before.duplicate_count, after.duplicate_count),
        .failed_delta = materialized_byte_payload_count_delta(before.failed_count(), after.failed_count()),
    };
}

inline bool materialized_byte_payload_snapshot_status_matches(
    const asset_materialized_byte_payload_snapshot& before,
    const asset_materialized_byte_payload_snapshot& after)
{
    return before.status == after.status && before.materialized_status == after.materialized_status
        && before.load_status == after.load_status;
}

inline bool materialized_byte_payload_snapshot_matches(
    const asset_materialized_byte_payload_snapshot& before,
    const asset_materialized_byte_payload_snapshot& after)
{
    return before.id == after.id && before.type == after.type && before.cache_key == after.cache_key
        && before.source_uri == after.source_uri && before.materialized_path == after.materialized_path
        && before.byte_count == after.byte_count && before.payload_byte_count == after.payload_byte_count
        && before.content_hash == after.content_hash && before.status == after.status
        && before.materialized_status == after.materialized_status && before.load_status == after.load_status
        && before.ready == after.ready;
}

inline bool materialized_byte_payload_optional_snapshot_matches(
    const std::optional<asset_materialized_byte_payload_snapshot>& before,
    const std::optional<asset_materialized_byte_payload_snapshot>& after)
{
    if (before.has_value() != after.has_value()) {
        return false;
    }
    if (!before.has_value()) {
        return true;
    }
    return materialized_byte_payload_snapshot_matches(*before, *after);
}

inline std::optional<bool> materialized_byte_payload_transaction_item_readiness(
    const asset_materialized_byte_payload_request_transaction_item& item)
{
    if (!item.selection.snapshot.has_value()) {
        return std::nullopt;
    }
    return item.selection.snapshot->ready;
}

inline bool materialized_byte_payload_transaction_item_status_is(
    const asset_materialized_byte_payload_request_transaction_item& item,
    asset_materialized_byte_payload_selection_status status)
{
    return item.selection.status == status;
}

inline std::size_t materialized_byte_payload_request_occurrence_at(
    const asset_materialized_byte_payload_request_transaction& transaction,
    std::size_t index)
{
    std::size_t occurrence = 0U;
    for (std::size_t current = 0U; current < index && current < transaction.items.size(); ++current) {
        if (transaction.items[current].request.id == transaction.items[index].request.id) {
            ++occurrence;
        }
    }
    return occurrence;
}

inline std::optional<std::size_t> find_materialized_byte_payload_request_index_by_occurrence(
    const asset_materialized_byte_payload_request_transaction& transaction,
    std::string_view id,
    std::size_t occurrence)
{
    std::size_t seen = 0U;
    for (std::size_t index = 0U; index < transaction.items.size(); ++index) {
        if (transaction.items[index].request.id != id) {
            continue;
        }
        if (seen == occurrence) {
            return index;
        }
        ++seen;
    }
    return std::nullopt;
}

inline asset_materialized_byte_payload_diff_entry make_added_materialized_byte_payload_diff_entry(
    const asset_materialized_byte_payload_snapshot& after)
{
    return asset_materialized_byte_payload_diff_entry{
        .kind = asset_materialized_byte_payload_delta_kind::added,
        .id = after.id,
        .type = after.type,
        .after = after,
    };
}

inline asset_materialized_byte_payload_diff_entry make_removed_materialized_byte_payload_diff_entry(
    const asset_materialized_byte_payload_snapshot& before)
{
    return asset_materialized_byte_payload_diff_entry{
        .kind = asset_materialized_byte_payload_delta_kind::removed,
        .id = before.id,
        .type = before.type,
        .before = before,
    };
}

inline asset_materialized_byte_payload_diff_entry make_changed_materialized_byte_payload_diff_entry(
    const asset_materialized_byte_payload_snapshot& before,
    const asset_materialized_byte_payload_snapshot& after)
{
    return asset_materialized_byte_payload_diff_entry{
        .kind = asset_materialized_byte_payload_delta_kind::changed,
        .id = after.id,
        .type = after.type,
        .before = before,
        .after = after,
        .type_changed = before.type != after.type,
        .cache_key_changed = before.cache_key != after.cache_key,
        .source_uri_changed = before.source_uri != after.source_uri,
        .materialized_path_changed = before.materialized_path != after.materialized_path,
        .byte_count_changed = before.byte_count != after.byte_count,
        .payload_byte_count_changed = before.payload_byte_count != after.payload_byte_count,
        .content_hash_changed = before.content_hash != after.content_hash,
        .status_changed = !materialized_byte_payload_snapshot_status_matches(before, after),
        .readiness_changed = before.ready != after.ready,
    };
}

inline asset_materialized_byte_payload_request_transaction_diff_entry make_added_materialized_byte_payload_transaction_diff_entry(
    const asset_materialized_byte_payload_request_transaction_item& after,
    std::size_t after_index,
    std::size_t occurrence)
{
    return asset_materialized_byte_payload_request_transaction_diff_entry{
        .kind = asset_materialized_byte_payload_request_transaction_delta_kind::added_request,
        .id = after.request.id,
        .occurrence = occurrence,
        .after_index = after_index,
        .after_status = after.selection.status,
        .after_snapshot = after.selection.snapshot,
        .after_selected_snapshot = after.selected_snapshot,
    };
}

inline asset_materialized_byte_payload_request_transaction_diff_entry make_removed_materialized_byte_payload_transaction_diff_entry(
    const asset_materialized_byte_payload_request_transaction_item& before,
    std::size_t before_index,
    std::size_t occurrence)
{
    return asset_materialized_byte_payload_request_transaction_diff_entry{
        .kind = asset_materialized_byte_payload_request_transaction_delta_kind::removed_request,
        .id = before.request.id,
        .occurrence = occurrence,
        .before_index = before_index,
        .before_status = before.selection.status,
        .before_snapshot = before.selection.snapshot,
        .before_selected_snapshot = before.selected_snapshot,
    };
}

inline asset_materialized_byte_payload_request_transaction_diff_entry make_changed_materialized_byte_payload_transaction_diff_entry(
    const asset_materialized_byte_payload_request_transaction_item& before,
    std::size_t before_index,
    const asset_materialized_byte_payload_request_transaction_item& after,
    std::size_t after_index,
    std::size_t occurrence)
{
    const std::optional<bool> before_ready = materialized_byte_payload_transaction_item_readiness(before);
    const std::optional<bool> after_ready = materialized_byte_payload_transaction_item_readiness(after);
    return asset_materialized_byte_payload_request_transaction_diff_entry{
        .kind = asset_materialized_byte_payload_request_transaction_delta_kind::changed_request,
        .id = after.request.id,
        .occurrence = occurrence,
        .before_index = before_index,
        .after_index = after_index,
        .before_status = before.selection.status,
        .after_status = after.selection.status,
        .before_snapshot = before.selection.snapshot,
        .after_snapshot = after.selection.snapshot,
        .before_selected_snapshot = before.selected_snapshot,
        .after_selected_snapshot = after.selected_snapshot,
        .status_changed = before.selection.status != after.selection.status,
        .selected_snapshot_changed =
            !materialized_byte_payload_optional_snapshot_matches(before.selected_snapshot, after.selected_snapshot),
        .readiness_changed = before_ready != after_ready,
        .integrity_failure_changed =
            materialized_byte_payload_transaction_item_status_is(
                before,
                asset_materialized_byte_payload_selection_status::integrity_failure)
            != materialized_byte_payload_transaction_item_status_is(
                after,
                asset_materialized_byte_payload_selection_status::integrity_failure),
        .cache_key_mismatch_changed =
            materialized_byte_payload_transaction_item_status_is(
                before,
                asset_materialized_byte_payload_selection_status::cache_key_mismatch)
            != materialized_byte_payload_transaction_item_status_is(
                after,
                asset_materialized_byte_payload_selection_status::cache_key_mismatch),
    };
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

inline asset_materialized_byte_payload_snapshot make_materialized_asset_byte_payload_snapshot(
    const asset_materialized_byte_payload& payload)
{
    return detail::make_materialized_byte_payload_snapshot(payload);
}

inline asset_materialized_byte_payload_bundle_snapshot snapshot_materialized_asset_byte_payload_bundle(
    const asset_materialized_byte_payload_bundle& bundle)
{
    asset_materialized_byte_payload_bundle_snapshot snapshot{
        .skipped_generic_count = bundle.skipped_generic_count,
    };
    snapshot.payloads.reserve(bundle.payload_count());
    detail::for_each_materialized_byte_payload(
        bundle,
        [&](const asset_materialized_byte_payload& payload) {
            snapshot.payloads.push_back(detail::make_materialized_byte_payload_snapshot(payload));
        });
    return snapshot;
}

inline asset_materialized_byte_payload_diff_summary diff_materialized_asset_byte_payload_snapshots(
    const asset_materialized_byte_payload_bundle_snapshot& before,
    const asset_materialized_byte_payload_bundle_snapshot& after)
{
    asset_materialized_byte_payload_diff_summary diff;

    for (const asset_materialized_byte_payload_snapshot& before_payload : before.payloads) {
        const asset_materialized_byte_payload_snapshot* after_payload = after.find_payload(before_payload.id);
        if (after_payload == nullptr) {
            diff.removed.push_back(detail::make_removed_materialized_byte_payload_diff_entry(before_payload));
            continue;
        }

        asset_materialized_byte_payload_diff_entry changed =
            detail::make_changed_materialized_byte_payload_diff_entry(before_payload, *after_payload);
        if (changed.has_field_delta()) {
            diff.changed.push_back(std::move(changed));
        }
    }

    for (const asset_materialized_byte_payload_snapshot& after_payload : after.payloads) {
        if (before.find_payload(after_payload.id) == nullptr) {
            diff.added.push_back(detail::make_added_materialized_byte_payload_diff_entry(after_payload));
        }
    }

    return diff;
}

inline asset_materialized_byte_payload_diff_summary diff_materialized_asset_byte_payload_bundles(
    const asset_materialized_byte_payload_bundle& before,
    const asset_materialized_byte_payload_bundle& after)
{
    return diff_materialized_asset_byte_payload_snapshots(
        snapshot_materialized_asset_byte_payload_bundle(before),
        snapshot_materialized_asset_byte_payload_bundle(after));
}

inline std::string asset_materialized_byte_payload_selection_status_name(
    asset_materialized_byte_payload_selection_status status)
{
    switch (status) {
        case asset_materialized_byte_payload_selection_status::selected:
            return "selected";
        case asset_materialized_byte_payload_selection_status::missing_id:
            return "missing_id";
        case asset_materialized_byte_payload_selection_status::wrong_type:
            return "wrong_type";
        case asset_materialized_byte_payload_selection_status::blocked_payload:
            return "blocked_payload";
        case asset_materialized_byte_payload_selection_status::integrity_failure:
            return "integrity_failure";
        case asset_materialized_byte_payload_selection_status::duplicate_id:
            return "duplicate_id";
        case asset_materialized_byte_payload_selection_status::cache_key_mismatch:
            return "cache_key_mismatch";
    }
    return "missing_id";
}

inline bool materialized_asset_byte_payload_integrity_ok(const asset_materialized_byte_payload& payload)
{
    return detail::materialized_byte_payload_integrity_ok(payload);
}

inline asset_materialized_byte_payload_selection_result select_materialized_asset_byte_payload(
    const asset_materialized_byte_payload_bundle& bundle,
    const asset_materialized_byte_payload_selection_request& request)
{
    std::vector<const asset_materialized_byte_payload*> matches;
    detail::for_each_materialized_byte_payload(
        bundle,
        [&](const asset_materialized_byte_payload& payload) {
            if (payload.id == request.id) {
                matches.push_back(&payload);
            }
        });

    if (matches.empty()) {
        return detail::make_materialized_byte_payload_selection_result(
            request,
            asset_materialized_byte_payload_selection_status::missing_id,
            "materialized byte payload id was not found");
    }

    if (matches.size() > 1U) {
        asset_materialized_byte_payload_selection_result result =
            detail::make_materialized_byte_payload_selection_result(
                request,
                asset_materialized_byte_payload_selection_status::duplicate_id,
                "materialized byte payload id matched multiple payloads");
        result.match_count = matches.size();
        return result;
    }

    const asset_materialized_byte_payload& payload = *matches.front();
    asset_materialized_byte_payload_selection_result result{
        .status = asset_materialized_byte_payload_selection_status::selected,
        .payload = &payload,
        .snapshot = detail::make_materialized_byte_payload_snapshot(payload),
        .id = request.id,
        .expected_type = request.expected_type,
        .expected_cache_key = request.expected_cache_key,
        .actual_type = payload.type,
        .actual_cache_key = payload.cache_key,
        .match_count = 1U,
    };

    if (request.expected_type != asset_type::generic && payload.type != request.expected_type) {
        result.status = asset_materialized_byte_payload_selection_status::wrong_type;
        result.payload = nullptr;
        result.diagnostic = "materialized byte payload type does not match the request";
        return result;
    }

    if (request.expected_cache_key.has_value() && payload.cache_key != *request.expected_cache_key) {
        result.status = asset_materialized_byte_payload_selection_status::cache_key_mismatch;
        result.payload = nullptr;
        result.diagnostic = "materialized byte payload cache key does not match the request";
        return result;
    }

    if (request.require_integrity_ok && !detail::materialized_byte_payload_integrity_ok(payload)) {
        result.status = asset_materialized_byte_payload_selection_status::integrity_failure;
        result.payload = nullptr;
        result.diagnostic = "materialized byte payload has integrity issues";
        return result;
    }

    if (request.require_ready && !payload.ready()) {
        result.status = asset_materialized_byte_payload_selection_status::blocked_payload;
        result.payload = nullptr;
        result.diagnostic = "materialized byte payload is blocked";
        return result;
    }

    result.diagnostic = "materialized byte payload selected";
    return result;
}

inline asset_materialized_byte_payload_filter_result filter_materialized_asset_byte_payloads(
    const asset_materialized_byte_payload_bundle& bundle,
    const asset_materialized_byte_payload_filter& filter)
{
    asset_materialized_byte_payload_filter_result result;
    detail::for_each_materialized_byte_payload(
        bundle,
        [&](const asset_materialized_byte_payload& payload) {
            if (!detail::materialized_byte_payload_matches_filter(payload, filter)) {
                return;
            }
            result.payloads.push_back(&payload);
            result.snapshots.push_back(detail::make_materialized_byte_payload_snapshot(payload));
        });
    return result;
}

inline asset_materialized_byte_payload_request_transaction make_materialized_asset_byte_payload_request_transaction(
    const asset_materialized_byte_payload_bundle& bundle,
    const std::vector<asset_materialized_byte_payload_selection_request>& requests)
{
    asset_materialized_byte_payload_request_transaction transaction;
    transaction.items.reserve(requests.size());
    transaction.summary.request_count = requests.size();

    for (std::size_t index = 0U; index < requests.size(); ++index) {
        asset_materialized_byte_payload_selection_result selection =
            select_materialized_asset_byte_payload(bundle, requests[index]);
        detail::count_materialized_byte_payload_transaction_selection(transaction.summary, selection);
        transaction.items.push_back(asset_materialized_byte_payload_request_transaction_item{
            .request_index = index,
            .request = requests[index],
            .selection = selection,
            .selected_snapshot = selection.selected() ? selection.snapshot : std::nullopt,
        });
    }

    return transaction;
}

inline asset_materialized_byte_payload_request_transaction_diff_summary diff_materialized_asset_byte_payload_request_transactions(
    const asset_materialized_byte_payload_request_transaction& before,
    const asset_materialized_byte_payload_request_transaction& after)
{
    asset_materialized_byte_payload_request_transaction_diff_summary diff{
        .before_summary = before.summary,
        .after_summary = after.summary,
        .count_delta =
            detail::make_materialized_byte_payload_transaction_count_delta(before.summary, after.summary),
    };

    for (std::size_t before_index = 0U; before_index < before.items.size(); ++before_index) {
        const asset_materialized_byte_payload_request_transaction_item& before_item = before.items[before_index];
        const std::size_t occurrence = detail::materialized_byte_payload_request_occurrence_at(before, before_index);
        const std::optional<std::size_t> after_index =
            detail::find_materialized_byte_payload_request_index_by_occurrence(after, before_item.request.id, occurrence);
        if (!after_index.has_value()) {
            diff.removed.push_back(detail::make_removed_materialized_byte_payload_transaction_diff_entry(
                before_item,
                before_index,
                occurrence));
            continue;
        }

        const asset_materialized_byte_payload_request_transaction_item& after_item = after.items[*after_index];
        asset_materialized_byte_payload_request_transaction_diff_entry changed =
            detail::make_changed_materialized_byte_payload_transaction_diff_entry(
                before_item,
                before_index,
                after_item,
                *after_index,
                occurrence);
        if (changed.has_field_delta()) {
            diff.changed.push_back(std::move(changed));
        }
    }

    for (std::size_t after_index = 0U; after_index < after.items.size(); ++after_index) {
        const asset_materialized_byte_payload_request_transaction_item& after_item = after.items[after_index];
        const std::size_t occurrence = detail::materialized_byte_payload_request_occurrence_at(after, after_index);
        if (detail::find_materialized_byte_payload_request_index_by_occurrence(
                before,
                after_item.request.id,
                occurrence)
                .has_value()) {
            continue;
        }
        diff.added.push_back(detail::make_added_materialized_byte_payload_transaction_diff_entry(
            after_item,
            after_index,
            occurrence));
    }

    return diff;
}

} // namespace quiz_vulkan::assets
