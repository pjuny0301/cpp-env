#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace quiz_vulkan::assets {

using asset_cache_key = std::string;

enum class asset_type {
    generic,
    font,
    image,
    sound,
    shader,
    deck,
};

enum class asset_source_kind {
    local_path,
    file_uri,
    asset_uri,
    http_uri,
    https_uri,
    data_uri,
    unsupported,
};

enum class asset_resolve_status {
    resolved,
    empty_uri,
    unsupported_scheme,
    invalid_uri,
    path_traversal,
    absolute_path,
};

enum class asset_cache_key_policy_status {
    accepted,
    empty_key,
    missing_type_separator,
    unsupported_asset_type,
    missing_source_uri,
    invalid_source_uri,
    noncanonical_source_uri,
    path_traversal,
    invalid_revision,
};

struct asset_resolve_request {
    asset_type type = asset_type::generic;
    std::string uri;
};

inline constexpr std::string_view asset_type_token(asset_type type)
{
    switch (type) {
        case asset_type::font:
            return "font";
        case asset_type::image:
            return "image";
        case asset_type::sound:
            return "sound";
        case asset_type::shader:
            return "shader";
        case asset_type::deck:
            return "deck";
        case asset_type::generic:
            return "generic";
    }
    return "generic";
}

inline bool parse_asset_type_token(std::string_view token, asset_type& type)
{
    if (token == "generic") {
        type = asset_type::generic;
        return true;
    }
    if (token == "font") {
        type = asset_type::font;
        return true;
    }
    if (token == "image") {
        type = asset_type::image;
        return true;
    }
    if (token == "sound") {
        type = asset_type::sound;
        return true;
    }
    if (token == "shader") {
        type = asset_type::shader;
        return true;
    }
    if (token == "deck") {
        type = asset_type::deck;
        return true;
    }
    return false;
}

inline asset_cache_key make_asset_cache_key(asset_type type, std::string_view normalized_uri)
{
    const std::string_view type_token = asset_type_token(type);
    asset_cache_key key;
    key.reserve(type_token.size() + 1 + normalized_uri.size());
    key.append(type_token);
    key.push_back('|');
    key.append(normalized_uri);
    return key;
}

struct asset_cache_key_classification {
    asset_cache_key_policy_status status = asset_cache_key_policy_status::empty_key;
    asset_type type = asset_type::generic;
    asset_source_kind source_kind = asset_source_kind::unsupported;
    std::string normalized_uri;
    std::string source_path;
    std::string cache_revision;
    std::string diagnostic;

    bool ok() const
    {
        return status == asset_cache_key_policy_status::accepted;
    }

    bool has_cache_revision() const
    {
        return !cache_revision.empty();
    }
};

struct resolved_asset_source {
    std::string original_uri;
    std::string normalized_uri;
    asset_source_kind kind = asset_source_kind::local_path;
    asset_type type = asset_type::generic;

    asset_cache_key cache_key() const
    {
        return make_asset_cache_key(type, normalized_uri);
    }

    bool is_remote() const
    {
        return kind == asset_source_kind::http_uri || kind == asset_source_kind::https_uri;
    }

    bool is_packaged_asset() const
    {
        return kind == asset_source_kind::asset_uri;
    }
};

struct asset_resolve_result {
    asset_resolve_status status = asset_resolve_status::empty_uri;
    resolved_asset_source source;
    std::string diagnostic;

    bool ok() const
    {
        return status == asset_resolve_status::resolved;
    }
};

inline bool is_ascii_space(char value)
{
    return std::isspace(static_cast<unsigned char>(value)) != 0;
}

inline std::string trim_asset_uri(std::string_view uri)
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

inline bool is_asset_scheme_first_char(char value)
{
    return std::isalpha(static_cast<unsigned char>(value)) != 0;
}

inline bool is_asset_scheme_char(char value)
{
    return std::isalnum(static_cast<unsigned char>(value)) != 0 || value == '+' || value == '-' || value == '.';
}

inline bool is_ascii_control(char value)
{
    const auto byte = static_cast<unsigned char>(value);
    return byte < 0x20 || byte == 0x7f;
}

inline bool has_ascii_control(std::string_view value)
{
    return std::any_of(value.begin(), value.end(), is_ascii_control);
}

inline bool has_windows_drive_designator(std::string_view path)
{
    return path.size() >= 2 && std::isalpha(static_cast<unsigned char>(path[0])) != 0 && path[1] == ':';
}

inline std::string::size_type find_asset_scheme_separator(std::string_view uri)
{
    const std::string_view::size_type separator = uri.find(':');
    if (separator == std::string_view::npos || separator == 0) {
        return std::string::npos;
    }
    const std::string_view::size_type first_slash = uri.find('/');
    if (first_slash != std::string_view::npos && first_slash < separator) {
        return std::string::npos;
    }
    if (!is_asset_scheme_first_char(uri[0])) {
        return std::string::npos;
    }
    for (std::string_view::size_type index = 1; index < separator; ++index) {
        if (!is_asset_scheme_char(uri[index])) {
            return std::string::npos;
        }
    }
    return separator;
}

inline void lowercase_asset_scheme(std::string& uri, std::string::size_type separator)
{
    for (std::string::size_type index = 0; index < separator; ++index) {
        uri[index] = static_cast<char>(std::tolower(static_cast<unsigned char>(uri[index])));
    }
}

inline asset_source_kind asset_source_kind_for_scheme(std::string_view scheme)
{
    if (scheme == "file") {
        return asset_source_kind::file_uri;
    }
    if (scheme == "asset") {
        return asset_source_kind::asset_uri;
    }
    if (scheme == "http") {
        return asset_source_kind::http_uri;
    }
    if (scheme == "https") {
        return asset_source_kind::https_uri;
    }
    if (scheme == "data") {
        return asset_source_kind::data_uri;
    }
    return asset_source_kind::unsupported;
}

inline int asset_hex_digit_value(char value)
{
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    return -1;
}

inline bool percent_decode_asset_path(std::string_view path, std::string& decoded)
{
    decoded.clear();
    decoded.reserve(path.size());
    for (std::string_view::size_type index = 0; index < path.size(); ++index) {
        const char value = path[index];
        if (value != '%') {
            decoded.push_back(value == '\\' ? '/' : value);
            continue;
        }

        if (index + 2 >= path.size()) {
            return false;
        }
        const int high = asset_hex_digit_value(path[index + 1]);
        const int low = asset_hex_digit_value(path[index + 2]);
        if (high < 0 || low < 0) {
            return false;
        }
        const char decoded_value = static_cast<char>((high << 4) | low);
        decoded.push_back(decoded_value == '\\' ? '/' : decoded_value);
        index += 2;
    }
    return true;
}

struct normalized_asset_path_result {
    asset_resolve_status status = asset_resolve_status::invalid_uri;
    std::string path;
    std::string diagnostic;

    bool ok() const
    {
        return status == asset_resolve_status::resolved;
    }
};

inline std::string join_asset_path_segments(const std::vector<std::string>& segments)
{
    std::string path;
    for (const std::string& segment : segments) {
        if (!path.empty()) {
            path.push_back('/');
        }
        path.append(segment);
    }
    return path;
}

inline normalized_asset_path_result normalize_asset_path_segments(
    std::string_view path,
    bool reject_absolute_path,
    bool strip_leading_slashes,
    bool require_non_empty)
{
    normalized_asset_path_result result;
    std::string decoded;
    if (!percent_decode_asset_path(path, decoded)) {
        result.status = asset_resolve_status::invalid_uri;
        result.diagnostic = "asset path contains invalid percent encoding";
        return result;
    }
    if (has_ascii_control(decoded)) {
        result.status = asset_resolve_status::invalid_uri;
        result.diagnostic = "asset path contains control characters";
        return result;
    }

    if (strip_leading_slashes) {
        while (decoded.starts_with('/')) {
            decoded.erase(decoded.begin());
        }
    }

    if (reject_absolute_path && (decoded.starts_with('/') || has_windows_drive_designator(decoded))) {
        result.status = asset_resolve_status::absolute_path;
        result.diagnostic = "asset path must be relative";
        return result;
    }

    std::vector<std::string> segments;
    std::string segment;
    for (const char value : decoded) {
        if (value == '/') {
            if (segment == "..") {
                result.status = asset_resolve_status::path_traversal;
                result.diagnostic = "asset path traversal is not allowed";
                return result;
            }
            if (!segment.empty() && segment != ".") {
                segments.push_back(std::move(segment));
            }
            segment.clear();
            continue;
        }
        segment.push_back(value);
    }

    if (segment == "..") {
        result.status = asset_resolve_status::path_traversal;
        result.diagnostic = "asset path traversal is not allowed";
        return result;
    }
    if (!segment.empty() && segment != ".") {
        segments.push_back(std::move(segment));
    }

    result.path = join_asset_path_segments(segments);
    if (require_non_empty && result.path.empty()) {
        result.status = asset_resolve_status::invalid_uri;
        result.diagnostic = "asset path is empty";
        return result;
    }

    result.status = asset_resolve_status::resolved;
    return result;
}

class asset_resolver_interface {
public:
    virtual ~asset_resolver_interface() = default;

    virtual asset_resolve_result resolve(const asset_resolve_request& request) const = 0;
};

class normalizing_asset_resolver final : public asset_resolver_interface {
public:
    asset_resolve_result resolve(const asset_resolve_request& request) const override
    {
        asset_resolve_result result;
        result.source.original_uri = request.uri;
        result.source.type = request.type;

        std::string normalized = trim_asset_uri(request.uri);
        std::replace(normalized.begin(), normalized.end(), '\\', '/');
        if (normalized.empty()) {
            result.status = asset_resolve_status::empty_uri;
            result.diagnostic = "asset uri is empty";
            return result;
        }
        if (has_ascii_control(normalized)) {
            result.status = asset_resolve_status::invalid_uri;
            result.diagnostic = "asset uri contains control characters";
            return result;
        }

        if (has_windows_drive_designator(normalized)) {
            result.source.kind = asset_source_kind::local_path;
            result.status = asset_resolve_status::absolute_path;
            result.diagnostic = "asset path must be relative";
            return result;
        }

        const std::string::size_type scheme_separator = find_asset_scheme_separator(normalized);
        if (scheme_separator == std::string::npos) {
            result.source.kind = asset_source_kind::local_path;
            const normalized_asset_path_result path = normalize_asset_path_segments(
                normalized,
                true,
                false,
                true);
            if (!path.ok()) {
                result.status = path.status;
                result.diagnostic = path.diagnostic;
                return result;
            }

            result.source.normalized_uri = path.path;
            result.status = asset_resolve_status::resolved;
            return result;
        }

        lowercase_asset_scheme(normalized, scheme_separator);
        const std::string_view scheme(normalized.data(), scheme_separator);
        result.source.kind = asset_source_kind_for_scheme(scheme);
        if (result.source.kind == asset_source_kind::unsupported) {
            result.status = asset_resolve_status::unsupported_scheme;
            result.diagnostic = "asset uri scheme is not supported";
            return result;
        }

        if (result.source.kind == asset_source_kind::asset_uri) {
            const normalized_asset_path_result path = normalize_asset_path_segments(
                std::string_view(normalized).substr(scheme_separator + 1),
                false,
                true,
                true);
            if (!path.ok()) {
                result.status = path.status;
                result.diagnostic = path.diagnostic;
                return result;
            }
            result.source.normalized_uri = "asset://" + path.path;
            result.status = asset_resolve_status::resolved;
            return result;
        }

        if (result.source.kind == asset_source_kind::file_uri) {
            const normalized_asset_path_result path = normalize_asset_path_segments(
                std::string_view(normalized).substr(scheme_separator + 1),
                false,
                false,
                false);
            if (!path.ok()) {
                result.status = path.status;
                result.diagnostic = path.diagnostic;
                return result;
            }
        }

        result.source.normalized_uri = normalized;
        result.status = asset_resolve_status::resolved;
        return result;
    }
};

inline bool asset_cache_revision_is_valid(std::string_view revision)
{
    if (revision.empty()) {
        return false;
    }
    return std::ranges::none_of(revision, [](char character) {
        return character == '|' || character == '\n' || character == '\r' || character == '\t'
            || static_cast<unsigned char>(character) < 0x20U;
    });
}

inline asset_cache_key_policy_status cache_key_status_from_resolve_status(asset_resolve_status status)
{
    if (status == asset_resolve_status::path_traversal) {
        return asset_cache_key_policy_status::path_traversal;
    }
    return asset_cache_key_policy_status::invalid_source_uri;
}

inline std::string asset_cache_key_source_path(const resolved_asset_source& source)
{
    constexpr std::string_view asset_prefix = "asset://";
    if (source.kind == asset_source_kind::asset_uri && source.normalized_uri.starts_with(asset_prefix)) {
        return std::string(std::string_view(source.normalized_uri).substr(asset_prefix.size()));
    }
    if (source.kind == asset_source_kind::local_path) {
        return source.normalized_uri;
    }
    return {};
}

inline asset_cache_key_classification classify_asset_cache_key(std::string_view cache_key)
{
    asset_cache_key_classification classification;
    if (cache_key.empty()) {
        classification.status = asset_cache_key_policy_status::empty_key;
        classification.diagnostic = "asset cache key is empty";
        return classification;
    }

    const std::string_view::size_type type_separator = cache_key.find('|');
    if (type_separator == std::string_view::npos || type_separator == 0U) {
        classification.status = asset_cache_key_policy_status::missing_type_separator;
        classification.diagnostic = "asset cache key requires a type separator";
        return classification;
    }

    if (!parse_asset_type_token(cache_key.substr(0U, type_separator), classification.type)) {
        classification.status = asset_cache_key_policy_status::unsupported_asset_type;
        classification.diagnostic = "asset cache key type token is not recognized";
        return classification;
    }

    const std::string_view::size_type source_begin = type_separator + 1U;
    const std::string_view::size_type revision_separator = cache_key.find('|', source_begin);
    const std::string_view source_uri = cache_key.substr(
        source_begin,
        revision_separator == std::string_view::npos ? std::string_view::npos : revision_separator - source_begin);
    if (source_uri.empty()) {
        classification.status = asset_cache_key_policy_status::missing_source_uri;
        classification.diagnostic = "asset cache key requires a source uri";
        return classification;
    }

    if (revision_separator != std::string_view::npos) {
        constexpr std::string_view revision_prefix = "rev=";
        const std::string_view revision = cache_key.substr(revision_separator + 1U);
        if (!revision.starts_with(revision_prefix)
            || !asset_cache_revision_is_valid(revision.substr(revision_prefix.size()))) {
            classification.status = asset_cache_key_policy_status::invalid_revision;
            classification.normalized_uri = std::string(source_uri);
            classification.diagnostic = "asset cache key revision segment must be a non-empty rev= token";
            return classification;
        }
        classification.cache_revision = std::string(revision.substr(revision_prefix.size()));
    }

    const normalizing_asset_resolver resolver;
    const asset_resolve_result resolved = resolver.resolve(asset_resolve_request{
        .type = classification.type,
        .uri = std::string(source_uri),
    });
    classification.source_kind = resolved.source.kind;
    classification.normalized_uri = std::string(source_uri);
    if (!resolved.ok()) {
        classification.status = cache_key_status_from_resolve_status(resolved.status);
        classification.diagnostic = resolved.diagnostic;
        return classification;
    }

    if (std::string_view(resolved.source.normalized_uri) != source_uri) {
        classification.status = asset_cache_key_policy_status::noncanonical_source_uri;
        classification.diagnostic = "asset cache key source uri must already be normalized";
        return classification;
    }

    classification.status = asset_cache_key_policy_status::accepted;
    classification.source_kind = resolved.source.kind;
    classification.normalized_uri = resolved.source.normalized_uri;
    classification.source_path = asset_cache_key_source_path(resolved.source);
    return classification;
}

inline bool asset_cache_key_is_canonical(std::string_view cache_key)
{
    return classify_asset_cache_key(cache_key).ok();
}

} // namespace quiz_vulkan::assets
