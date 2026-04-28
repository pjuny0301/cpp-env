#pragma once

#include "render/image/image_types.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace quiz_vulkan::render {

struct render_image_resolve_request {
    std::string uri;
};

enum class render_image_resolve_status {
    resolved,
    empty_uri,
    unsupported_scheme,
};

struct render_image_resolve_result {
    render_image_resolve_status status = render_image_resolve_status::empty_uri;
    render_resolved_image_source source;
    std::string diagnostic;

    bool ok() const
    {
        return status == render_image_resolve_status::resolved;
    }
};

inline bool is_ascii_space(char value)
{
    return std::isspace(static_cast<unsigned char>(value)) != 0;
}

inline std::string trim_image_uri(std::string_view uri)
{
    auto begin = uri.begin();
    auto end = uri.end();
    while (begin != end && is_ascii_space(*begin)) {
        ++begin;
    }
    while (begin != end && is_ascii_space(*(end - 1))) {
        --end;
    }
    return std::string(begin, end);
}

inline std::string normalize_image_uri(std::string_view uri)
{
    std::string normalized = trim_image_uri(uri);
    std::replace(normalized.begin(), normalized.end(), '\\', '/');

    const std::string::size_type scheme_separator = normalized.find(':');
    if (scheme_separator != std::string::npos) {
        for (std::string::size_type index = 0; index < scheme_separator; ++index) {
            normalized[index] = static_cast<char>(
                std::tolower(static_cast<unsigned char>(normalized[index])));
        }
        return normalized;
    }

    while (normalized.starts_with("./")) {
        normalized.erase(0, 2);
    }
    return normalized;
}

inline render_image_source_kind image_source_kind_for(std::string_view normalized_uri)
{
    const std::string::size_type scheme_separator = normalized_uri.find(':');
    if (scheme_separator == std::string_view::npos) {
        return render_image_source_kind::local_path;
    }

    const std::string_view scheme = normalized_uri.substr(0, scheme_separator);
    if (scheme == "file") {
        return render_image_source_kind::file_uri;
    }
    if (scheme == "asset") {
        return render_image_source_kind::asset_uri;
    }
    if (scheme == "http") {
        return render_image_source_kind::http_uri;
    }
    if (scheme == "https") {
        return render_image_source_kind::https_uri;
    }
    if (scheme == "data") {
        return render_image_source_kind::data_uri;
    }
    return render_image_source_kind::unsupported;
}

class image_resolver_interface {
public:
    virtual ~image_resolver_interface() = default;

    virtual render_image_resolve_result resolve(const render_image_resolve_request& request) const = 0;
};

class normalizing_image_resolver final : public image_resolver_interface {
public:
    render_image_resolve_result resolve(const render_image_resolve_request& request) const override
    {
        render_image_resolve_result result;
        result.source.original_uri = request.uri;
        result.source.normalized_uri = normalize_image_uri(request.uri);
        if (result.source.normalized_uri.empty()) {
            result.status = render_image_resolve_status::empty_uri;
            result.diagnostic = "image uri is empty";
            return result;
        }

        result.source.kind = image_source_kind_for(result.source.normalized_uri);
        if (result.source.kind == render_image_source_kind::unsupported) {
            result.status = render_image_resolve_status::unsupported_scheme;
            result.diagnostic = "image uri scheme is not supported by the render contract";
            return result;
        }

        result.status = render_image_resolve_status::resolved;
        return result;
    }
};

} // namespace quiz_vulkan::render
