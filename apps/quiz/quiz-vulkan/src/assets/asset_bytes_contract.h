#pragma once

#include "assets/asset_runtime_catalog.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace quiz_vulkan::assets {

enum class asset_bytes_load_status {
    loaded,
    missing_id,
    type_mismatch,
    unresolved_asset,
    missing_bytes,
    source_not_readable,
    invalid_rooted_path,
    source_path_mismatch,
    file_read_failed,
    cache_key_mismatch,
    noncanonical_cache_key,
};

struct asset_bytes_snapshot_request {
    runtime_asset_catalog_snapshot snapshot;
};

struct asset_bytes_catalog_request {
    std::string id;
    asset_type expected_type = asset_type::generic;
};

struct asset_materialized_bytes_request {
    runtime_materialized_asset_lookup_result materialized;
};

struct asset_bytes_load_result {
    asset_bytes_load_status status = asset_bytes_load_status::missing_bytes;
    std::vector<std::byte> bytes;
    std::size_t byte_count = 0U;
    std::string content_hash;
    asset_cache_key cache_key;
    std::string source_uri;
    std::string diagnostic;

    [[nodiscard]] bool ok() const
    {
        return status == asset_bytes_load_status::loaded;
    }
};

enum class asset_bytes_integrity_issue_kind {
    load_failed,
    cache_key_mismatch,
    source_uri_mismatch,
    byte_count_mismatch,
    content_hash_mismatch,
    missing_content,
};

struct asset_bytes_integrity_issue {
    asset_bytes_integrity_issue_kind kind = asset_bytes_integrity_issue_kind::load_failed;
    std::string id;
    asset_type type = asset_type::generic;
    asset_cache_key cache_key;
    asset_cache_key expected_cache_key;
    std::string source_uri;
    std::string expected_source_uri;
    std::size_t reported_byte_count = 0U;
    std::size_t actual_byte_count = 0U;
    std::string reported_content_hash;
    std::string actual_content_hash;
    std::string diagnostic;
};

struct asset_bytes_integrity_request {
    runtime_asset_catalog_snapshot snapshot;
    asset_bytes_load_result load;
    bool require_non_empty = true;
};

struct asset_bytes_integrity_report {
    asset_bytes_load_result load;
    std::vector<asset_bytes_integrity_issue> issues;

    [[nodiscard]] bool ok() const
    {
        return issues.empty();
    }
};

struct asset_materialized_bytes_cache_policy_entry {
    std::string id;
    asset_type expected_type = asset_type::generic;
    runtime_materialized_asset_lookup_status materialized_status =
        runtime_materialized_asset_lookup_status::missing_id;
    asset_bytes_load_status load_status = asset_bytes_load_status::missing_bytes;
    asset_cache_key cache_key;
    std::string source_uri;
    std::string materialized_source_path;
    std::filesystem::path materialized_path;
    std::size_t byte_count = 0U;
    std::string content_hash;
    std::vector<asset_bytes_integrity_issue> issues;
    std::string diagnostic;

    [[nodiscard]] bool ok() const
    {
        return materialized_status == runtime_materialized_asset_lookup_status::materialized
            && load_status == asset_bytes_load_status::loaded && issues.empty();
    }
};

struct asset_materialized_bytes_cache_policy_summary {
    std::vector<asset_materialized_bytes_cache_policy_entry> entries;
    std::size_t request_count = 0U;
    std::size_t loaded_count = 0U;
    std::size_t failed_count = 0U;
    std::size_t total_byte_count = 0U;
    std::size_t load_failed_count = 0U;
    std::size_t cache_key_mismatch_count = 0U;
    std::size_t source_uri_mismatch_count = 0U;
    std::size_t byte_count_mismatch_count = 0U;
    std::size_t content_hash_mismatch_count = 0U;
    std::size_t missing_content_count = 0U;

    [[nodiscard]] bool ok() const
    {
        return failed_count == 0U;
    }

    [[nodiscard]] const asset_materialized_bytes_cache_policy_entry* find_entry(std::string_view id) const
    {
        for (const asset_materialized_bytes_cache_policy_entry& entry : entries) {
            if (entry.id == id) {
                return &entry;
            }
        }
        return nullptr;
    }
};

} // namespace quiz_vulkan::assets
