#pragma once

#include "render/image/image_types.h"

#include <cctype>
#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

struct render_image_source_bytes_load_request {
    render_resolved_image_source source;
};

enum class render_image_source_bytes_load_status {
    loaded,
    missing_source,
    unsupported_source,
    missing_bytes,
    empty_bytes,
};

struct render_image_source_bytes_load_result {
    render_image_source_bytes_load_status status = render_image_source_bytes_load_status::missing_source;
    render_resolved_image_source source;
    std::vector<std::byte> encoded_bytes;
    std::string diagnostic;

    bool ok() const
    {
        return status == render_image_source_bytes_load_status::loaded;
    }
};

class image_source_bytes_loader_interface {
public:
    virtual ~image_source_bytes_loader_interface() = default;

    virtual render_image_source_bytes_load_result load(
        const render_image_source_bytes_load_request& request) const = 0;
};

inline bool is_valid_image_source_bytes_cache_key(std::string_view cache_key)
{
    if (cache_key.empty()) {
        return false;
    }

    for (const char value : cache_key) {
        if (std::iscntrl(static_cast<unsigned char>(value)) != 0) {
            return false;
        }
    }

    return true;
}

class fake_image_source_bytes_loader final : public image_source_bytes_loader_interface {
public:
    void set_source_bytes(render_image_cache_key source_key, std::vector<std::byte> encoded_bytes)
    {
        source_bytes_.insert_or_assign(std::move(source_key), std::move(encoded_bytes));
    }

    void set_source_bytes(const render_resolved_image_source& source, std::vector<std::byte> encoded_bytes)
    {
        set_source_bytes(source.cache_key(), std::move(encoded_bytes));
    }

    void clear_source_bytes(render_image_cache_key source_key)
    {
        source_bytes_.erase(source_key);
    }

    bool has_source_bytes(const render_resolved_image_source& source) const
    {
        return source_bytes_.contains(source.cache_key());
    }

    render_image_source_bytes_load_result load(
        const render_image_source_bytes_load_request& request) const override
    {
        load_requests.push_back(request);
        const render_image_cache_key source_key = request.source.cache_key();
        if (!is_valid_image_source_bytes_cache_key(source_key)) {
            return render_image_source_bytes_load_result{
                .status = render_image_source_bytes_load_status::missing_source,
                .source = request.source,
                .encoded_bytes = {},
                .diagnostic = "image source bytes cache key is empty or contains control characters",
            };
        }

        if (request.source.kind == render_image_source_kind::unsupported || request.source.is_remote()) {
            return render_image_source_bytes_load_result{
                .status = render_image_source_bytes_load_status::unsupported_source,
                .source = request.source,
                .encoded_bytes = {},
                .diagnostic = "fake image source bytes loader does not fetch unsupported or remote sources",
            };
        }

        const auto source_bytes = source_bytes_.find(source_key);
        if (source_bytes == source_bytes_.end()) {
            return render_image_source_bytes_load_result{
                .status = render_image_source_bytes_load_status::missing_bytes,
                .source = request.source,
                .encoded_bytes = {},
                .diagnostic = "fake image source bytes loader has no bytes for source",
            };
        }

        if (source_bytes->second.empty()) {
            return render_image_source_bytes_load_result{
                .status = render_image_source_bytes_load_status::empty_bytes,
                .source = request.source,
                .encoded_bytes = {},
                .diagnostic = "fake image source bytes loader found empty bytes for source",
            };
        }

        return render_image_source_bytes_load_result{
            .status = render_image_source_bytes_load_status::loaded,
            .source = request.source,
            .encoded_bytes = source_bytes->second,
            .diagnostic = {},
        };
    }

    mutable std::vector<render_image_source_bytes_load_request> load_requests;

private:
    std::map<render_image_cache_key, std::vector<std::byte>> source_bytes_;
};

} // namespace quiz_vulkan::render
