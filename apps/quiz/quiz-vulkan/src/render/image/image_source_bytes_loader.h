#pragma once

#include "render/image/image_types.h"

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <system_error>
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
    malformed_data_uri,
    missing_bytes,
    empty_bytes,
};

enum class render_image_data_uri_decode_status {
    decoded,
    malformed_source,
    invalid_metadata,
    invalid_percent_encoding,
    invalid_base64,
    empty_payload,
};

inline std::string render_image_data_uri_decode_status_name(render_image_data_uri_decode_status status)
{
    switch (status) {
    case render_image_data_uri_decode_status::decoded:
        return "decoded";
    case render_image_data_uri_decode_status::malformed_source:
        return "malformed_source";
    case render_image_data_uri_decode_status::invalid_metadata:
        return "invalid_metadata";
    case render_image_data_uri_decode_status::invalid_percent_encoding:
        return "invalid_percent_encoding";
    case render_image_data_uri_decode_status::invalid_base64:
        return "invalid_base64";
    case render_image_data_uri_decode_status::empty_payload:
        return "empty_payload";
    }

    return "unknown";
}

struct render_image_data_uri_decode_result {
    render_image_data_uri_decode_status status = render_image_data_uri_decode_status::malformed_source;
    std::string media_type;
    bool base64_encoded = false;
    std::vector<std::byte> encoded_bytes;
    std::string diagnostic;

    bool ok() const
    {
        return status == render_image_data_uri_decode_status::decoded;
    }
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

inline bool is_data_uri_token_char(char value)
{
    const unsigned char byte = static_cast<unsigned char>(value);
    return std::isalnum(byte) != 0 || value == '!' || value == '#' || value == '$' || value == '&'
        || value == '-' || value == '.' || value == '+' || value == '^' || value == '_' || value == '|'
        || value == '~';
}

inline bool is_valid_data_uri_media_type(std::string_view media_type)
{
    if (media_type.empty()) {
        return true;
    }

    const std::string_view::size_type slash = media_type.find('/');
    if (slash == std::string_view::npos || slash == 0 || slash + 1 >= media_type.size()) {
        return false;
    }
    if (media_type.find('/', slash + 1) != std::string_view::npos) {
        return false;
    }

    for (const char value : media_type.substr(0, slash)) {
        if (!is_data_uri_token_char(value)) {
            return false;
        }
    }
    for (const char value : media_type.substr(slash + 1)) {
        if (!is_data_uri_token_char(value)) {
            return false;
        }
    }
    return true;
}

inline int data_uri_hex_digit_value(char value)
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

inline render_image_data_uri_decode_result make_render_image_data_uri_decode_failure(
    render_image_data_uri_decode_status status,
    std::string media_type,
    bool base64_encoded,
    std::string diagnostic)
{
    return render_image_data_uri_decode_result{
        .status = status,
        .media_type = std::move(media_type),
        .base64_encoded = base64_encoded,
        .encoded_bytes = {},
        .diagnostic = std::move(diagnostic),
    };
}

inline bool is_valid_data_uri_parameter(std::string_view parameter)
{
    const std::string_view::size_type equals = parameter.find('=');
    if (equals == std::string_view::npos || equals == 0 || equals + 1 >= parameter.size()) {
        return false;
    }

    for (const char value : parameter.substr(0, equals)) {
        if (!is_data_uri_token_char(value)) {
            return false;
        }
    }
    for (const char value : parameter.substr(equals + 1)) {
        const unsigned char byte = static_cast<unsigned char>(value);
        if (std::isspace(byte) != 0 || std::iscntrl(byte) != 0 || value == ';' || value == ',') {
            return false;
        }
    }

    return true;
}

inline render_image_data_uri_decode_result decode_data_uri_percent_payload(
    std::string_view payload,
    std::string media_type,
    bool base64_encoded)
{
    std::vector<std::byte> decoded;
    decoded.reserve(payload.size());
    for (std::size_t index = 0; index < payload.size(); ++index) {
        const char value = payload[index];
        if (value != '%') {
            decoded.push_back(std::byte{static_cast<unsigned char>(value)});
            continue;
        }

        if (index + 2 >= payload.size()) {
            return make_render_image_data_uri_decode_failure(
                render_image_data_uri_decode_status::invalid_percent_encoding,
                std::move(media_type),
                base64_encoded,
                "data URI payload has incomplete percent escape");
        }

        const int high = data_uri_hex_digit_value(payload[index + 1]);
        const int low = data_uri_hex_digit_value(payload[index + 2]);
        if (high < 0 || low < 0) {
            return make_render_image_data_uri_decode_failure(
                render_image_data_uri_decode_status::invalid_percent_encoding,
                std::move(media_type),
                base64_encoded,
                "data URI payload has invalid percent escape");
        }
        decoded.push_back(std::byte{static_cast<unsigned char>((high << 4) | low)});
        index += 2;
    }

    if (decoded.empty()) {
        return make_render_image_data_uri_decode_failure(
            render_image_data_uri_decode_status::empty_payload,
            std::move(media_type),
            base64_encoded,
            "data URI payload is empty");
    }

    return render_image_data_uri_decode_result{
        .status = render_image_data_uri_decode_status::decoded,
        .media_type = std::move(media_type),
        .base64_encoded = base64_encoded,
        .encoded_bytes = std::move(decoded),
        .diagnostic = {},
    };
}

inline int data_uri_base64_value(unsigned char value)
{
    if (value >= 'A' && value <= 'Z') {
        return static_cast<int>(value - 'A');
    }
    if (value >= 'a' && value <= 'z') {
        return static_cast<int>(value - 'a' + 26);
    }
    if (value >= '0' && value <= '9') {
        return static_cast<int>(value - '0' + 52);
    }
    if (value == '+') {
        return 62;
    }
    if (value == '/') {
        return 63;
    }
    return -1;
}

inline bool append_data_uri_base64_quad(
    const int quartet[4],
    std::vector<std::byte>& decoded)
{
    if (quartet[0] < 0 || quartet[1] < 0) {
        return false;
    }
    decoded.push_back(std::byte{static_cast<unsigned char>((quartet[0] << 2) | (quartet[1] >> 4))});
    if (quartet[2] == -2) {
        return quartet[3] == -2;
    }
    if (quartet[2] < 0) {
        return false;
    }
    decoded.push_back(std::byte{static_cast<unsigned char>(((quartet[1] & 0x0f) << 4) | (quartet[2] >> 2))});
    if (quartet[3] == -2) {
        return true;
    }
    if (quartet[3] < 0) {
        return false;
    }
    decoded.push_back(std::byte{static_cast<unsigned char>(((quartet[2] & 0x03) << 6) | quartet[3])});
    return true;
}

inline render_image_data_uri_decode_result decode_data_uri_base64_payload(
    std::string_view payload,
    std::string media_type)
{
    render_image_data_uri_decode_result percent_decoded =
        decode_data_uri_percent_payload(payload, std::move(media_type), true);
    if (!percent_decoded.ok()) {
        return percent_decoded;
    }

    std::vector<std::byte> decoded;
    decoded.reserve((percent_decoded.encoded_bytes.size() / 4) * 3 + 3);
    int quartet[4] = {};
    std::size_t quartet_size = 0;
    bool saw_padding = false;

    for (const std::byte value : percent_decoded.encoded_bytes) {
        const unsigned char byte = std::to_integer<unsigned char>(value);
        if (byte == '=') {
            saw_padding = true;
            quartet[quartet_size++] = -2;
        } else {
            const int base64_value = data_uri_base64_value(byte);
            if (base64_value < 0 || saw_padding) {
                return make_render_image_data_uri_decode_failure(
                    render_image_data_uri_decode_status::invalid_base64,
                    std::move(percent_decoded.media_type),
                    true,
                    "data URI base64 payload contains invalid characters or padding");
            }
            quartet[quartet_size++] = base64_value;
        }

        if (quartet_size == 4) {
            if (!append_data_uri_base64_quad(quartet, decoded)) {
                return make_render_image_data_uri_decode_failure(
                    render_image_data_uri_decode_status::invalid_base64,
                    std::move(percent_decoded.media_type),
                    true,
                    "data URI base64 payload has invalid padding");
            }
            quartet_size = 0;
        }
    }

    if (quartet_size == 1 || (saw_padding && quartet_size != 0)) {
        return make_render_image_data_uri_decode_failure(
            render_image_data_uri_decode_status::invalid_base64,
            std::move(percent_decoded.media_type),
            true,
            "data URI base64 payload has invalid length");
    }
    if (quartet_size == 2) {
        if (quartet[0] < 0 || quartet[1] < 0) {
            return make_render_image_data_uri_decode_failure(
                render_image_data_uri_decode_status::invalid_base64,
                std::move(percent_decoded.media_type),
                true,
                "data URI base64 payload has invalid trailing values");
        }
        decoded.push_back(std::byte{static_cast<unsigned char>((quartet[0] << 2) | (quartet[1] >> 4))});
    } else if (quartet_size == 3) {
        if (quartet[0] < 0 || quartet[1] < 0 || quartet[2] < 0) {
            return make_render_image_data_uri_decode_failure(
                render_image_data_uri_decode_status::invalid_base64,
                std::move(percent_decoded.media_type),
                true,
                "data URI base64 payload has invalid trailing values");
        }
        decoded.push_back(std::byte{static_cast<unsigned char>((quartet[0] << 2) | (quartet[1] >> 4))});
        decoded.push_back(std::byte{static_cast<unsigned char>(((quartet[1] & 0x0f) << 4) | (quartet[2] >> 2))});
    }

    if (decoded.empty()) {
        return make_render_image_data_uri_decode_failure(
            render_image_data_uri_decode_status::empty_payload,
            std::move(percent_decoded.media_type),
            true,
            "data URI base64 payload is empty");
    }

    return render_image_data_uri_decode_result{
        .status = render_image_data_uri_decode_status::decoded,
        .media_type = std::move(percent_decoded.media_type),
        .base64_encoded = true,
        .encoded_bytes = std::move(decoded),
        .diagnostic = {},
    };
}

inline render_image_data_uri_decode_result decode_render_image_data_uri(std::string_view uri)
{
    if (!uri.starts_with("data:")) {
        return make_render_image_data_uri_decode_failure(
            render_image_data_uri_decode_status::malformed_source,
            {},
            false,
            "data URI must start with the data scheme");
    }

    const std::string_view::size_type comma = uri.find(',');
    if (comma == std::string_view::npos) {
        return make_render_image_data_uri_decode_failure(
            render_image_data_uri_decode_status::malformed_source,
            {},
            false,
            "data URI is missing comma separator");
    }

    const std::string_view metadata = uri.substr(5, comma - 5);
    const std::string_view payload = uri.substr(comma + 1);
    std::string media_type;
    bool base64_encoded = false;
    std::size_t token_begin = 0;
    std::size_t token_index = 0;
    while (token_begin <= metadata.size()) {
        const std::size_t token_end = metadata.find(';', token_begin);
        const std::string_view token = metadata.substr(
            token_begin,
            token_end == std::string_view::npos ? std::string_view::npos : token_end - token_begin);
        if (token_index == 0 && !token.empty() && token != "base64") {
            media_type = std::string(token);
            if (!is_valid_data_uri_media_type(media_type)) {
                return make_render_image_data_uri_decode_failure(
                    render_image_data_uri_decode_status::invalid_metadata,
                    std::move(media_type),
                    false,
                    "data URI metadata has invalid media type");
            }
        } else if (token == "base64") {
            if (base64_encoded) {
                return make_render_image_data_uri_decode_failure(
                    render_image_data_uri_decode_status::invalid_metadata,
                    std::move(media_type),
                    base64_encoded,
                    "data URI metadata repeats base64 marker");
            }
            base64_encoded = true;
        } else if (!token.empty()) {
            if (!is_valid_data_uri_parameter(token)) {
                return make_render_image_data_uri_decode_failure(
                    render_image_data_uri_decode_status::invalid_metadata,
                    std::move(media_type),
                    base64_encoded,
                    "data URI metadata parameter is malformed");
            }
        }

        if (token_end == std::string_view::npos) {
            break;
        }
        token_begin = token_end + 1;
        ++token_index;
    }

    if (base64_encoded) {
        return decode_data_uri_base64_payload(payload, std::move(media_type));
    }
    return decode_data_uri_percent_payload(payload, std::move(media_type), false);
}

inline render_image_source_bytes_load_result load_render_image_data_uri_source_bytes(
    const render_resolved_image_source& source)
{
    const render_image_data_uri_decode_result decoded = decode_render_image_data_uri(source.cache_key());
    if (!decoded.ok()) {
        return render_image_source_bytes_load_result{
            .status = decoded.status == render_image_data_uri_decode_status::empty_payload
                ? render_image_source_bytes_load_status::empty_bytes
                : render_image_source_bytes_load_status::malformed_data_uri,
            .source = source,
            .encoded_bytes = {},
            .diagnostic = decoded.diagnostic,
        };
    }

    return render_image_source_bytes_load_result{
        .status = render_image_source_bytes_load_status::loaded,
        .source = source,
        .encoded_bytes = decoded.encoded_bytes,
        .diagnostic = {},
    };
}

namespace detail {

inline bool image_source_uri_decode_path(std::string_view encoded, std::string& decoded)
{
    decoded.clear();
    decoded.reserve(encoded.size());
    for (std::size_t index = 0; index < encoded.size(); ++index) {
        const char value = encoded[index];
        if (value != '%') {
            decoded.push_back(value);
            continue;
        }

        if (index + 2 >= encoded.size()) {
            return false;
        }
        const int high = data_uri_hex_digit_value(encoded[index + 1]);
        const int low = data_uri_hex_digit_value(encoded[index + 2]);
        if (high < 0 || low < 0) {
            return false;
        }
        decoded.push_back(static_cast<char>((high << 4) | low));
        index += 2;
    }
    return true;
}

inline bool image_source_path_has_windows_drive_prefix(std::string_view path)
{
    return path.size() >= 2 && std::isalpha(static_cast<unsigned char>(path[0])) != 0 && path[1] == ':';
}

struct image_source_path_resolution {
    render_image_source_bytes_load_status status = render_image_source_bytes_load_status::missing_source;
    std::filesystem::path path;
    std::string diagnostic;

    bool ok() const
    {
        return status == render_image_source_bytes_load_status::loaded;
    }
};

inline image_source_path_resolution resolve_file_uri_image_source_path(
    const render_resolved_image_source& source)
{
    constexpr std::string_view file_scheme = "file:";
    const render_image_cache_key uri = source.cache_key();
    if (!uri.starts_with(file_scheme)) {
        return image_source_path_resolution{
            .status = render_image_source_bytes_load_status::missing_source,
            .path = {},
            .diagnostic = "file image source must start with the file scheme",
        };
    }

    std::string_view remainder(uri);
    remainder.remove_prefix(file_scheme.size());
    if (remainder.starts_with("//")) {
        remainder.remove_prefix(2);
        const std::string_view::size_type slash = remainder.find('/');
        const std::string_view authority = slash == std::string_view::npos
            ? remainder
            : remainder.substr(0, slash);
        if (!authority.empty() && authority != "localhost") {
            return image_source_path_resolution{
                .status = render_image_source_bytes_load_status::unsupported_source,
                .path = {},
                .diagnostic = "file URI authority is not supported by the local image source loader",
            };
        }
        remainder = slash == std::string_view::npos ? std::string_view{} : remainder.substr(slash);
    }

    std::string decoded;
    if (!image_source_uri_decode_path(remainder, decoded)) {
        return image_source_path_resolution{
            .status = render_image_source_bytes_load_status::missing_source,
            .path = {},
            .diagnostic = "file URI path contains invalid percent encoding",
        };
    }

    if (decoded.size() >= 3 && decoded[0] == '/' && image_source_path_has_windows_drive_prefix(
            std::string_view(decoded).substr(1))) {
        decoded.erase(0, 1);
    }
    if (decoded.empty()) {
        return image_source_path_resolution{
            .status = render_image_source_bytes_load_status::missing_source,
            .path = {},
            .diagnostic = "file URI path is empty",
        };
    }

    return image_source_path_resolution{
        .status = render_image_source_bytes_load_status::loaded,
        .path = std::filesystem::path(decoded),
        .diagnostic = {},
    };
}

inline image_source_path_resolution resolve_asset_uri_image_source_path(
    const render_resolved_image_source& source,
    const std::filesystem::path& base_directory)
{
    const render_image_cache_key source_key = source.cache_key();
    const std::string_view::size_type scheme = source_key.find(':');
    std::string asset_path = scheme == std::string_view::npos
        ? std::string(source_key)
        : std::string(source_key.substr(scheme + 1));
    while (asset_path.starts_with('/')) {
        asset_path.erase(0, 1);
    }
    if (asset_path.empty()) {
        return image_source_path_resolution{
            .status = render_image_source_bytes_load_status::missing_source,
            .path = {},
            .diagnostic = "asset image source path is empty",
        };
    }

    std::filesystem::path path(asset_path);
    if (!base_directory.empty() && !path.is_absolute()) {
        path = base_directory / path;
    }

    return image_source_path_resolution{
        .status = render_image_source_bytes_load_status::loaded,
        .path = std::move(path),
        .diagnostic = {},
    };
}

inline std::filesystem::path image_source_path_with_base_directory(
    const std::filesystem::path& base_directory,
    std::filesystem::path path)
{
    return !base_directory.empty() && !path.is_absolute()
        ? base_directory / path
        : std::move(path);
}

} // namespace detail

class filesystem_image_source_bytes_loader final : public image_source_bytes_loader_interface {
public:
    filesystem_image_source_bytes_loader() = default;

    explicit filesystem_image_source_bytes_loader(std::filesystem::path base_directory)
        : base_directory_(std::move(base_directory))
    {
    }

    const std::filesystem::path& base_directory() const
    {
        return base_directory_;
    }

    void set_base_directory(std::filesystem::path base_directory)
    {
        base_directory_ = std::move(base_directory);
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
                .diagnostic = "filesystem image source bytes loader only loads local, file, asset, or data sources",
            };
        }

        if (request.source.kind == render_image_source_kind::data_uri) {
            return load_render_image_data_uri_source_bytes(request.source);
        }

        const detail::image_source_path_resolution resolved_path = source_path_for(request.source);
        if (!resolved_path.ok()) {
            return render_image_source_bytes_load_result{
                .status = resolved_path.status,
                .source = request.source,
                .encoded_bytes = {},
                .diagnostic = resolved_path.diagnostic,
            };
        }

        return load_path_bytes(request.source, resolved_path.path);
    }

    mutable std::vector<render_image_source_bytes_load_request> load_requests;

private:
    detail::image_source_path_resolution source_path_for(
        const render_resolved_image_source& source) const
    {
        switch (source.kind) {
        case render_image_source_kind::local_path:
            if (source.normalized_uri.empty()) {
                return detail::image_source_path_resolution{
                    .status = render_image_source_bytes_load_status::missing_source,
                    .path = {},
                    .diagnostic = "local image source path is empty",
                };
            }
            return detail::image_source_path_resolution{
                .status = render_image_source_bytes_load_status::loaded,
                .path = detail::image_source_path_with_base_directory(
                    base_directory_,
                    std::filesystem::path(source.normalized_uri)),
                .diagnostic = {},
            };
        case render_image_source_kind::file_uri:
            return detail::resolve_file_uri_image_source_path(source);
        case render_image_source_kind::asset_uri:
            return detail::resolve_asset_uri_image_source_path(source, base_directory_);
        case render_image_source_kind::data_uri:
        case render_image_source_kind::http_uri:
        case render_image_source_kind::https_uri:
        case render_image_source_kind::unsupported:
            break;
        }

        return detail::image_source_path_resolution{
            .status = render_image_source_bytes_load_status::unsupported_source,
            .path = {},
            .diagnostic = "image source kind is not backed by the local filesystem",
        };
    }

    static render_image_source_bytes_load_result load_path_bytes(
        const render_resolved_image_source& source,
        const std::filesystem::path& path)
    {
        std::error_code error;
        const bool exists = std::filesystem::exists(path, error);
        if (error || !exists) {
            return render_image_source_bytes_load_result{
                .status = render_image_source_bytes_load_status::missing_bytes,
                .source = source,
                .encoded_bytes = {},
                .diagnostic = "image source file is missing or cannot be inspected: " + path.generic_string(),
            };
        }

        if (!std::filesystem::is_regular_file(path, error) || error) {
            return render_image_source_bytes_load_result{
                .status = render_image_source_bytes_load_status::missing_bytes,
                .source = source,
                .encoded_bytes = {},
                .diagnostic = "image source path is not a readable regular file: " + path.generic_string(),
            };
        }

        const std::uintmax_t file_size = std::filesystem::file_size(path, error);
        if (error) {
            return render_image_source_bytes_load_result{
                .status = render_image_source_bytes_load_status::missing_bytes,
                .source = source,
                .encoded_bytes = {},
                .diagnostic = "image source file size cannot be inspected",
            };
        }
        if (file_size == 0) {
            return render_image_source_bytes_load_result{
                .status = render_image_source_bytes_load_status::empty_bytes,
                .source = source,
                .encoded_bytes = {},
                .diagnostic = "image source file is empty",
            };
        }
        if (file_size > static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max())
            || file_size > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max())) {
            return render_image_source_bytes_load_result{
                .status = render_image_source_bytes_load_status::missing_bytes,
                .source = source,
                .encoded_bytes = {},
                .diagnostic = "image source file is too large to load into memory",
            };
        }

        std::ifstream file(path, std::ios::binary);
        if (!file) {
            return render_image_source_bytes_load_result{
                .status = render_image_source_bytes_load_status::missing_bytes,
                .source = source,
                .encoded_bytes = {},
                .diagnostic = "image source file could not be opened",
            };
        }

        std::vector<std::byte> bytes(static_cast<std::size_t>(file_size));
        file.read(
            reinterpret_cast<char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        if (!file) {
            return render_image_source_bytes_load_result{
                .status = render_image_source_bytes_load_status::missing_bytes,
                .source = source,
                .encoded_bytes = {},
                .diagnostic = "image source file could not be read completely",
            };
        }

        return render_image_source_bytes_load_result{
            .status = render_image_source_bytes_load_status::loaded,
            .source = source,
            .encoded_bytes = std::move(bytes),
            .diagnostic = {},
        };
    }

    std::filesystem::path base_directory_;
};

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

        if (request.source.kind == render_image_source_kind::data_uri) {
            return load_render_image_data_uri_source_bytes(request.source);
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
