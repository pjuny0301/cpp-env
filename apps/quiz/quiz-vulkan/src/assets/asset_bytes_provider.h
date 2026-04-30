#pragma once

#include "assets/asset_runtime_catalog.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
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
    file_read_failed,
};

struct asset_bytes_snapshot_request {
    runtime_asset_catalog_snapshot snapshot;
};

struct asset_bytes_catalog_request {
    std::string id;
    asset_type expected_type = asset_type::generic;
};

struct asset_bytes_load_result {
    asset_bytes_load_status status = asset_bytes_load_status::missing_bytes;
    std::vector<std::byte> bytes;
    std::size_t byte_count = 0U;
    asset_cache_key cache_key;
    std::string source_uri;
    std::string diagnostic;

    [[nodiscard]] bool ok() const
    {
        return status == asset_bytes_load_status::loaded;
    }
};

class asset_bytes_provider_interface {
public:
    virtual ~asset_bytes_provider_interface() = default;

    virtual asset_bytes_load_result load_bytes(const asset_bytes_snapshot_request& request) const = 0;
};

struct fake_asset_bytes_record {
    asset_cache_key cache_key;
    std::vector<std::byte> bytes;
};

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

} // namespace detail

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
                result.status = asset_bytes_load_status::loaded;
                result.bytes = record.bytes;
                result.byte_count = result.bytes.size();
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
        result.byte_count = result.bytes.size();
        result.status = asset_bytes_load_status::loaded;
        return result;
    }
};

inline asset_bytes_load_result load_asset_bytes(
    const asset_bytes_provider_interface& provider,
    const runtime_asset_catalog_snapshot& snapshot)
{
    return provider.load_bytes(asset_bytes_snapshot_request{.snapshot = snapshot});
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

} // namespace quiz_vulkan::assets
