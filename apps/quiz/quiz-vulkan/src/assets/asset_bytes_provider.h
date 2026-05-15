#pragma once

#include "assets/asset_typed_materialized_bytes.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace quiz_vulkan::assets {

class asset_bytes_provider_interface {
public:
    virtual ~asset_bytes_provider_interface() = default;

    virtual asset_bytes_load_result load_bytes(const asset_bytes_snapshot_request& request) const = 0;
};

struct fake_asset_bytes_record {
    asset_cache_key cache_key;
    std::vector<std::byte> bytes;
};

inline std::string make_asset_bytes_content_hash(const std::vector<std::byte>& bytes)
{
    constexpr std::uint64_t offset_basis = 14695981039346656037ULL;
    constexpr std::uint64_t prime = 1099511628211ULL;
    constexpr char hex_digits[] = "0123456789abcdef";

    std::uint64_t hash = offset_basis;
    for (const std::byte value : bytes) {
        hash ^= static_cast<std::uint64_t>(std::to_integer<unsigned char>(value));
        hash *= prime;
    }

    std::string result = "fnv1a64:";
    for (int shift = 60; shift >= 0; shift -= 4) {
        result.push_back(hex_digits[(hash >> shift) & 0x0FU]);
    }
    return result;
}

namespace detail {

inline std::vector<std::byte> make_asset_byte_vector(std::string_view text)
{
    std::vector<std::byte> bytes;
    bytes.reserve(text.size());
    for (const char value : text) {
        bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(value)));
    }
    return bytes;
}

inline asset_bytes_load_result make_asset_bytes_metadata(const runtime_asset_catalog_snapshot& snapshot)
{
    return asset_bytes_load_result{
        .cache_key = snapshot.cache_key,
        .source_uri = snapshot.source.normalized_uri,
    };
}

inline asset_bytes_load_status asset_bytes_status_from_lookup_status(runtime_asset_catalog_lookup_status status)
{
    switch (status) {
        case runtime_asset_catalog_lookup_status::found:
            return asset_bytes_load_status::missing_bytes;
        case runtime_asset_catalog_lookup_status::missing_id:
            return asset_bytes_load_status::missing_id;
        case runtime_asset_catalog_lookup_status::type_mismatch:
            return asset_bytes_load_status::type_mismatch;
        case runtime_asset_catalog_lookup_status::unresolved_asset:
            return asset_bytes_load_status::unresolved_asset;
    }
    return asset_bytes_load_status::missing_bytes;
}

inline asset_bytes_load_status asset_bytes_status_from_materialized_status(
    runtime_materialized_asset_lookup_status status)
{
    switch (status) {
        case runtime_materialized_asset_lookup_status::materialized:
            return asset_bytes_load_status::missing_bytes;
        case runtime_materialized_asset_lookup_status::missing_id:
            return asset_bytes_load_status::missing_id;
        case runtime_materialized_asset_lookup_status::type_mismatch:
            return asset_bytes_load_status::type_mismatch;
        case runtime_materialized_asset_lookup_status::unresolved_asset:
            return asset_bytes_load_status::unresolved_asset;
        case runtime_materialized_asset_lookup_status::unsupported_source_kind:
        case runtime_materialized_asset_lookup_status::missing_rooted_path:
            return asset_bytes_load_status::source_not_readable;
        case runtime_materialized_asset_lookup_status::invalid_rooted_path:
            return asset_bytes_load_status::invalid_rooted_path;
        case runtime_materialized_asset_lookup_status::source_path_mismatch:
            return asset_bytes_load_status::source_path_mismatch;
        case runtime_materialized_asset_lookup_status::cache_key_mismatch:
            return asset_bytes_load_status::cache_key_mismatch;
        case runtime_materialized_asset_lookup_status::noncanonical_cache_key:
            return asset_bytes_load_status::noncanonical_cache_key;
    }
    return asset_bytes_load_status::missing_bytes;
}

inline asset_bytes_load_result make_asset_bytes_metadata(
    const runtime_materialized_asset_lookup_result& materialized)
{
    const runtime_asset_catalog_snapshot& snapshot = materialized.materialized.asset;
    return asset_bytes_load_result{
        .cache_key = snapshot.cache_key,
        .source_uri = snapshot.source.normalized_uri,
    };
}

inline asset_bytes_integrity_issue make_asset_bytes_integrity_issue(
    asset_bytes_integrity_issue_kind kind,
    const runtime_asset_catalog_snapshot& snapshot,
    const asset_bytes_load_result& load,
    std::string diagnostic)
{
    return asset_bytes_integrity_issue{
        .kind = kind,
        .id = snapshot.entry.id,
        .type = snapshot.entry.type,
        .cache_key = load.cache_key,
        .expected_cache_key = snapshot.cache_key,
        .source_uri = load.source_uri,
        .expected_source_uri = snapshot.source.normalized_uri,
        .reported_byte_count = load.byte_count,
        .actual_byte_count = load.bytes.size(),
        .reported_content_hash = load.content_hash,
        .actual_content_hash = make_asset_bytes_content_hash(load.bytes),
        .diagnostic = std::move(diagnostic),
    };
}

inline bool asset_bytes_path_has_parent_reference(const std::filesystem::path& path)
{
    for (const std::filesystem::path& part : path) {
        if (part == "..") {
            return true;
        }
    }
    return false;
}

inline bool asset_bytes_rooted_path_is_safe(const std::filesystem::path& path)
{
    return !path.empty() && path.is_absolute() && !asset_bytes_path_has_parent_reference(path);
}

inline void finish_loaded_asset_bytes_result(asset_bytes_load_result& result)
{
    result.byte_count = result.bytes.size();
    result.content_hash = make_asset_bytes_content_hash(result.bytes);
    result.status = asset_bytes_load_status::loaded;
}

inline void count_asset_bytes_integrity_issue(
    asset_materialized_bytes_cache_policy_summary& summary,
    asset_bytes_integrity_issue_kind kind)
{
    switch (kind) {
        case asset_bytes_integrity_issue_kind::load_failed:
            ++summary.load_failed_count;
            break;
        case asset_bytes_integrity_issue_kind::cache_key_mismatch:
            ++summary.cache_key_mismatch_count;
            break;
        case asset_bytes_integrity_issue_kind::source_uri_mismatch:
            ++summary.source_uri_mismatch_count;
            break;
        case asset_bytes_integrity_issue_kind::byte_count_mismatch:
            ++summary.byte_count_mismatch_count;
            break;
        case asset_bytes_integrity_issue_kind::content_hash_mismatch:
            ++summary.content_hash_mismatch_count;
            break;
        case asset_bytes_integrity_issue_kind::missing_content:
            ++summary.missing_content_count;
            break;
    }
}

} // namespace detail

inline asset_bytes_integrity_report validate_asset_bytes_integrity(
    const asset_bytes_integrity_request& request)
{
    asset_bytes_integrity_report report{
        .load = request.load,
    };

    if (!request.load.ok()) {
        std::string diagnostic = request.load.diagnostic;
        if (diagnostic.empty()) {
            diagnostic = "asset bytes did not load before integrity validation";
        }
        report.issues.push_back(detail::make_asset_bytes_integrity_issue(
            asset_bytes_integrity_issue_kind::load_failed,
            request.snapshot,
            request.load,
            std::move(diagnostic)));
        return report;
    }

    if (request.load.cache_key != request.snapshot.cache_key) {
        report.issues.push_back(detail::make_asset_bytes_integrity_issue(
            asset_bytes_integrity_issue_kind::cache_key_mismatch,
            request.snapshot,
            request.load,
            "loaded asset bytes cache key does not match the catalog snapshot"));
    }

    if (request.load.source_uri != request.snapshot.source.normalized_uri) {
        report.issues.push_back(detail::make_asset_bytes_integrity_issue(
            asset_bytes_integrity_issue_kind::source_uri_mismatch,
            request.snapshot,
            request.load,
            "loaded asset bytes source uri does not match the catalog snapshot"));
    }

    if (request.load.byte_count != request.load.bytes.size()) {
        report.issues.push_back(detail::make_asset_bytes_integrity_issue(
            asset_bytes_integrity_issue_kind::byte_count_mismatch,
            request.snapshot,
            request.load,
            "loaded asset byte count does not match the byte payload size"));
    }

    if (!request.load.content_hash.empty()
        && request.load.content_hash != make_asset_bytes_content_hash(request.load.bytes)) {
        report.issues.push_back(detail::make_asset_bytes_integrity_issue(
            asset_bytes_integrity_issue_kind::content_hash_mismatch,
            request.snapshot,
            request.load,
            "loaded asset content hash does not match the byte payload"));
    }

    if (request.require_non_empty && request.load.bytes.empty()) {
        report.issues.push_back(detail::make_asset_bytes_integrity_issue(
            asset_bytes_integrity_issue_kind::missing_content,
            request.snapshot,
            request.load,
            "loaded asset bytes payload is empty"));
    }

    return report;
}

class fake_asset_bytes_provider final : public asset_bytes_provider_interface {
public:
    void set_bytes(asset_cache_key cache_key, std::vector<std::byte> bytes)
    {
        for (fake_asset_bytes_record& record : records_) {
            if (record.cache_key == cache_key) {
                record.bytes = std::move(bytes);
                return;
            }
        }
        records_.push_back(fake_asset_bytes_record{
            .cache_key = std::move(cache_key),
            .bytes = std::move(bytes),
        });
    }

    void set_text(asset_cache_key cache_key, std::string_view text)
    {
        set_bytes(std::move(cache_key), detail::make_asset_byte_vector(text));
    }

    [[nodiscard]] const std::vector<fake_asset_bytes_record>& records() const
    {
        return records_;
    }

    asset_bytes_load_result load_bytes(const asset_bytes_snapshot_request& request) const override
    {
        asset_bytes_load_result result = detail::make_asset_bytes_metadata(request.snapshot);
        for (const fake_asset_bytes_record& record : records_) {
            if (record.cache_key == request.snapshot.cache_key) {
                result.bytes = record.bytes;
                detail::finish_loaded_asset_bytes_result(result);
                return result;
            }
        }

        result.status = asset_bytes_load_status::missing_bytes;
        result.diagnostic = "asset bytes are not registered for the cache key";
        return result;
    }

private:
    std::vector<fake_asset_bytes_record> records_;
};

class local_file_asset_bytes_provider final : public asset_bytes_provider_interface {
public:
    asset_bytes_load_result load_bytes(const asset_bytes_snapshot_request& request) const override
    {
        asset_bytes_load_result result = detail::make_asset_bytes_metadata(request.snapshot);
        if (!request.snapshot.rooted_path.has_value()) {
            result.status = asset_bytes_load_status::source_not_readable;
            result.diagnostic = "asset bytes require a rooted path";
            return result;
        }

        if (!detail::asset_bytes_rooted_path_is_safe(*request.snapshot.rooted_path)) {
            result.status = asset_bytes_load_status::invalid_rooted_path;
            result.diagnostic = "asset rooted path must be absolute and cannot contain parent traversal";
            return result;
        }

        const std::filesystem::path path = request.snapshot.rooted_path->lexically_normal();
        std::ifstream stream(path, std::ios::binary);
        if (!stream.is_open()) {
            result.status = asset_bytes_load_status::file_read_failed;
            result.diagnostic = "asset rooted path could not be opened";
            return result;
        }

        const std::vector<char> file_bytes{
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>(),
        };
        result.bytes.reserve(file_bytes.size());
        for (const char value : file_bytes) {
            result.bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(value)));
        }
        detail::finish_loaded_asset_bytes_result(result);
        return result;
    }
};

inline asset_bytes_load_result load_asset_bytes(
    const asset_bytes_provider_interface& provider,
    const runtime_asset_catalog_snapshot& snapshot)
{
    return provider.load_bytes(asset_bytes_snapshot_request{.snapshot = snapshot});
}

inline asset_bytes_integrity_report load_asset_bytes_with_integrity(
    const asset_bytes_provider_interface& provider,
    const runtime_asset_catalog_snapshot& snapshot)
{
    return validate_asset_bytes_integrity(asset_bytes_integrity_request{
        .snapshot = snapshot,
        .load = load_asset_bytes(provider, snapshot),
    });
}

inline asset_bytes_load_result load_asset_bytes(
    const asset_bytes_provider_interface& provider,
    const runtime_asset_catalog& catalog,
    const asset_bytes_catalog_request& request)
{
    const runtime_asset_catalog_lookup_result lookup = catalog.lookup(request.id, request.expected_type);
    if (!lookup.ok()) {
        asset_bytes_load_result result;
        result.status = detail::asset_bytes_status_from_lookup_status(lookup.status);
        result.diagnostic = lookup.diagnostic;
        if (!lookup.asset.entry.id.empty()) {
            result.cache_key = lookup.asset.cache_key;
            result.source_uri = lookup.asset.source.normalized_uri;
        }
        return result;
    }

    return load_asset_bytes(provider, lookup.asset);
}

inline asset_bytes_load_result load_materialized_asset_bytes(
    const asset_bytes_provider_interface& provider,
    const asset_materialized_bytes_request& request)
{
    if (!request.materialized.ok()) {
        asset_bytes_load_result result = detail::make_asset_bytes_metadata(request.materialized);
        result.status = detail::asset_bytes_status_from_materialized_status(request.materialized.status);
        result.diagnostic = request.materialized.diagnostic;
        return result;
    }

    runtime_asset_catalog_snapshot materialized_snapshot = request.materialized.materialized.asset;
    materialized_snapshot.rooted_path = request.materialized.materialized.local_path;
    return load_asset_bytes(provider, materialized_snapshot);
}

inline asset_bytes_load_result load_materialized_asset_bytes(
    const asset_bytes_provider_interface& provider,
    const runtime_materialized_asset_lookup_result& materialized)
{
    return load_materialized_asset_bytes(provider, asset_materialized_bytes_request{.materialized = materialized});
}

inline asset_bytes_load_result load_materialized_asset_bytes(
    const asset_bytes_provider_interface& provider,
    const runtime_asset_catalog& catalog,
    const asset_bytes_catalog_request& request)
{
    return load_materialized_asset_bytes(
        provider,
        lookup_runtime_materialized_asset(catalog, runtime_materialized_asset_lookup_request{
                                               .id = request.id,
                                               .expected_type = request.expected_type,
                                           }));
}

inline asset_bytes_integrity_report load_materialized_asset_bytes_with_integrity(
    const asset_bytes_provider_interface& provider,
    const asset_materialized_bytes_request& request)
{
    return validate_asset_bytes_integrity(asset_bytes_integrity_request{
        .snapshot = request.materialized.materialized.asset,
        .load = load_materialized_asset_bytes(provider, request),
    });
}

inline asset_bytes_integrity_report load_materialized_asset_bytes_with_integrity(
    const asset_bytes_provider_interface& provider,
    const runtime_materialized_asset_lookup_result& materialized)
{
    return load_materialized_asset_bytes_with_integrity(
        provider,
        asset_materialized_bytes_request{.materialized = materialized});
}

inline asset_bytes_integrity_report load_materialized_asset_bytes_with_integrity(
    const asset_bytes_provider_interface& provider,
    const runtime_asset_catalog& catalog,
    const asset_bytes_catalog_request& request)
{
    return load_materialized_asset_bytes_with_integrity(
        provider,
        lookup_runtime_materialized_asset(catalog, runtime_materialized_asset_lookup_request{
                                               .id = request.id,
                                               .expected_type = request.expected_type,
                                           }));
}

inline asset_materialized_bytes_cache_policy_summary summarize_materialized_asset_bytes_cache_policy(
    const asset_bytes_provider_interface& provider,
    const runtime_asset_catalog& catalog,
    const std::vector<asset_bytes_catalog_request>& requests,
    bool require_non_empty = true)
{
    asset_materialized_bytes_cache_policy_summary summary{
        .request_count = requests.size(),
    };

    for (const asset_bytes_catalog_request& request : requests) {
        const runtime_materialized_asset_lookup_result materialized = lookup_runtime_materialized_asset(
            catalog,
            runtime_materialized_asset_lookup_request{
                .id = request.id,
                .expected_type = request.expected_type,
            });
        const asset_bytes_load_result load = load_materialized_asset_bytes(provider, materialized);
        asset_bytes_integrity_report report = validate_asset_bytes_integrity(asset_bytes_integrity_request{
            .snapshot = materialized.materialized.asset,
            .load = load,
            .require_non_empty = require_non_empty,
        });

        asset_materialized_bytes_cache_policy_entry entry{
            .id = request.id,
            .expected_type = request.expected_type,
            .materialized_status = materialized.status,
            .load_status = report.load.status,
            .cache_key = report.load.cache_key,
            .source_uri = report.load.source_uri,
            .byte_count = report.load.byte_count,
            .content_hash = report.load.content_hash,
            .issues = report.issues,
            .diagnostic = report.load.diagnostic,
        };
        if (materialized.ok()) {
            entry.materialized_source_path = materialized.materialized.source_path;
            entry.materialized_path = materialized.materialized.local_path;
        }

        if (report.load.ok()) {
            ++summary.loaded_count;
            summary.total_byte_count += report.load.byte_count;
        }
        if (!entry.ok()) {
            ++summary.failed_count;
        }
        for (const asset_bytes_integrity_issue& issue : entry.issues) {
            detail::count_asset_bytes_integrity_issue(summary, issue.kind);
        }

        summary.entries.push_back(std::move(entry));
    }

    return summary;
}

inline asset_typed_materialized_bytes_summary summarize_typed_materialized_asset_bytes(
    const asset_bytes_provider_interface& provider,
    const runtime_asset_catalog& catalog,
    bool require_non_empty = true)
{
    asset_typed_materialized_bytes_summary summary;
    std::vector<asset_bytes_catalog_request> requests =
        detail::make_typed_materialized_asset_byte_requests(catalog, summary.skipped_generic_count);
    summary.cache_policy =
        summarize_materialized_asset_bytes_cache_policy(provider, catalog, requests, require_non_empty);

    for (const asset_materialized_bytes_cache_policy_entry& policy_entry : summary.cache_policy.entries) {
        detail::add_typed_materialized_bytes_entry(
            summary,
            detail::make_typed_materialized_bytes_entry(policy_entry, catalog));
    }

    return summary;
}

} // namespace quiz_vulkan::assets
