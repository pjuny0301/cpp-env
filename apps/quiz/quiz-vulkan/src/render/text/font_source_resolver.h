#pragma once

#include "render/text/font_glyph_atlas.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace quiz_vulkan::render {

enum class render_text_font_source_kind {
    missing,
    fixture_uri,
    file_uri,
    file_path,
    unknown_uri,
};

struct font_source_resolution {
    font_face_id face_id = 0;
    std::string family;
    std::string source_uri;
    render_text_font_source_kind kind = render_text_font_source_kind::missing;
    std::string resolved_location;
    bool can_attempt_load = false;
    bool virtual_fixture = false;
};

inline bool font_source_is_ascii_alpha(const char value)
{
    return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z');
}

inline bool font_source_is_ascii_digit(const char value)
{
    return value >= '0' && value <= '9';
}

inline int font_source_hex_value(const char value)
{
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    return -1;
}

inline bool font_source_has_uri_scheme(const std::string_view source_uri)
{
    const std::size_t colon = source_uri.find(':');
    if (colon == std::string_view::npos || colon == 0U) {
        return false;
    }
    if (colon == 1U && font_source_is_ascii_alpha(source_uri.front())) {
        return false;
    }
    if (!font_source_is_ascii_alpha(source_uri.front())) {
        return false;
    }

    for (std::size_t index = 1; index < colon; ++index) {
        const char value = source_uri[index];
        const bool valid_scheme_char =
            font_source_is_ascii_alpha(value)
            || font_source_is_ascii_digit(value)
            || value == '+'
            || value == '-'
            || value == '.';
        if (!valid_scheme_char) {
            return false;
        }
    }
    return true;
}

inline std::string font_source_normalize_path_separators(const std::string_view path)
{
    std::string normalized(path);
    for (char& value : normalized) {
        if (value == '\\') {
            value = '/';
        }
    }
    return normalized;
}

inline bool font_source_is_windows_drive_path(const std::string_view path)
{
    return path.size() >= 2U
        && font_source_is_ascii_alpha(path[0])
        && path[1] == ':';
}

inline bool font_source_is_absolute_path(const std::string_view path)
{
    return path.starts_with('/') || path.starts_with('\\') || font_source_is_windows_drive_path(path);
}

inline std::string font_source_decode_uri_path(const std::string_view encoded)
{
    std::string decoded;
    decoded.reserve(encoded.size());
    for (std::size_t index = 0; index < encoded.size(); ++index) {
        if (encoded[index] == '%' && index + 2U < encoded.size()) {
            const int high = font_source_hex_value(encoded[index + 1U]);
            const int low = font_source_hex_value(encoded[index + 2U]);
            if (high >= 0 && low >= 0) {
                decoded.push_back(static_cast<char>((high << 4) | low));
                index += 2U;
                continue;
            }
        }
        decoded.push_back(encoded[index]);
    }
    return decoded;
}

inline std::string font_source_join_paths(
    const std::string_view base_path,
    const std::string_view path)
{
    const std::string normalized_path = font_source_normalize_path_separators(path);
    if (base_path.empty() || font_source_is_absolute_path(normalized_path)) {
        return normalized_path;
    }

    std::string normalized_base = font_source_normalize_path_separators(base_path);
    while (!normalized_base.empty() && normalized_base.back() == '/') {
        normalized_base.pop_back();
    }
    std::string relative_path = normalized_path;
    while (!relative_path.empty() && relative_path.front() == '/') {
        relative_path.erase(relative_path.begin());
    }
    return normalized_base + "/" + relative_path;
}

inline std::string font_source_file_uri_path(const std::string_view source_uri)
{
    constexpr std::string_view prefix = "file://";
    std::string path = font_source_decode_uri_path(source_uri.substr(prefix.size()));
    if (path.starts_with("localhost/")) {
        path.erase(0, std::string_view("localhost").size());
    }
    if (path.size() >= 3U && path[0] == '/' && font_source_is_windows_drive_path(std::string_view(path).substr(1))) {
        path.erase(path.begin());
    }
    return font_source_normalize_path_separators(path);
}

inline font_source_resolution resolve_font_source(
    const font_face_descriptor& descriptor,
    const std::string_view base_path = {})
{
    constexpr std::string_view fixture_prefix = "fixture://";
    constexpr std::string_view file_prefix = "file://";
    const std::string_view source_uri = descriptor.source_uri;

    font_source_resolution resolution{
        .face_id = descriptor.id,
        .family = descriptor.family,
        .source_uri = descriptor.source_uri,
    };

    if (source_uri.empty()) {
        return resolution;
    }

    if (source_uri.starts_with(fixture_prefix)) {
        resolution.kind = render_text_font_source_kind::fixture_uri;
        resolution.resolved_location = std::string(source_uri.substr(fixture_prefix.size()));
        resolution.virtual_fixture = true;
        return resolution;
    }

    if (source_uri.starts_with(file_prefix)) {
        resolution.kind = render_text_font_source_kind::file_uri;
        resolution.resolved_location = font_source_file_uri_path(source_uri);
        resolution.can_attempt_load = !resolution.resolved_location.empty();
        return resolution;
    }

    if (font_source_has_uri_scheme(source_uri)) {
        resolution.kind = render_text_font_source_kind::unknown_uri;
        resolution.resolved_location = descriptor.source_uri;
        return resolution;
    }

    resolution.kind = render_text_font_source_kind::file_path;
    resolution.resolved_location = font_source_join_paths(base_path, source_uri);
    resolution.can_attempt_load = !resolution.resolved_location.empty();
    return resolution;
}

} // namespace quiz_vulkan::render
