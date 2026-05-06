#pragma once

#include "render/text/font_source_resolver.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <limits>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

struct render_text_font_source_bytes_load_request {
    font_source_resolution source;
};

enum class render_text_font_source_bytes_load_status {
    loaded,
    available_virtual_fixture,
    missing_source,
    unsupported_source,
    invalid_cache_key,
    path_traversal_blocked,
    missing_bytes,
    empty_bytes,
    read_error,
};

inline std::string render_text_font_source_bytes_load_status_name(
    const render_text_font_source_bytes_load_status status)
{
    switch (status) {
    case render_text_font_source_bytes_load_status::loaded:
        return "loaded";
    case render_text_font_source_bytes_load_status::available_virtual_fixture:
        return "available_virtual_fixture";
    case render_text_font_source_bytes_load_status::missing_source:
        return "missing_source";
    case render_text_font_source_bytes_load_status::unsupported_source:
        return "unsupported_source";
    case render_text_font_source_bytes_load_status::invalid_cache_key:
        return "invalid_cache_key";
    case render_text_font_source_bytes_load_status::path_traversal_blocked:
        return "path_traversal_blocked";
    case render_text_font_source_bytes_load_status::missing_bytes:
        return "missing_bytes";
    case render_text_font_source_bytes_load_status::empty_bytes:
        return "empty_bytes";
    case render_text_font_source_bytes_load_status::read_error:
        return "read_error";
    }

    return "unknown";
}

struct render_text_font_source_bytes_load_result {
    render_text_font_source_bytes_load_status status =
        render_text_font_source_bytes_load_status::missing_source;
    font_source_resolution source;
    font_source_bytes_readiness readiness;
    std::string cache_key;
    std::string resolved_path;
    std::vector<std::byte> bytes;
    std::string diagnostic;

    bool ok() const
    {
        return status == render_text_font_source_bytes_load_status::loaded;
    }
};

class font_source_bytes_loader_interface {
public:
    virtual ~font_source_bytes_loader_interface() = default;

    virtual render_text_font_source_bytes_load_result load(
        const render_text_font_source_bytes_load_request& request) const = 0;
};

inline render_text_font_source_bytes_load_result make_font_source_bytes_load_result(
    const render_text_font_source_bytes_load_status status,
    font_source_resolution source,
    font_source_bytes_readiness readiness,
    std::string resolved_path,
    std::vector<std::byte> bytes,
    std::string diagnostic)
{
    std::string cache_key = readiness.cache_key;
    return render_text_font_source_bytes_load_result{
        .status = status,
        .source = std::move(source),
        .readiness = std::move(readiness),
        .cache_key = std::move(cache_key),
        .resolved_path = std::move(resolved_path),
        .bytes = std::move(bytes),
        .diagnostic = std::move(diagnostic),
    };
}

inline bool font_source_path_has_parent_reference(const std::filesystem::path& path)
{
    for (const std::filesystem::path& part : path) {
        if (part == "..") {
            return true;
        }
    }
    return false;
}

inline bool font_source_path_is_under_base(
    const std::filesystem::path& base,
    const std::filesystem::path& candidate)
{
    const std::filesystem::path normalized_base = base.lexically_normal();
    const std::filesystem::path normalized_candidate = candidate.lexically_normal();
    if (normalized_base.empty()) {
        return true;
    }
    if (normalized_base == normalized_candidate) {
        return true;
    }

    const std::filesystem::path relative = normalized_candidate.lexically_relative(normalized_base);
    if (relative.empty() || relative.is_absolute()) {
        return false;
    }
    return !font_source_path_has_parent_reference(relative);
}

inline std::filesystem::path font_source_loader_path_for(
    const font_source_resolution& source,
    const std::filesystem::path& base_directory)
{
    std::filesystem::path source_path(font_source_normalize_path_separators(source.resolved_location));
    if (base_directory.empty() || source_path.is_absolute()) {
        return source_path.lexically_normal();
    }
    return (base_directory / source_path).lexically_normal();
}

class filesystem_font_source_bytes_loader final : public font_source_bytes_loader_interface {
public:
    explicit filesystem_font_source_bytes_loader(
        std::filesystem::path base_directory = {},
        std::size_t max_byte_count = 64U * 1024U * 1024U)
        : base_directory_(std::move(base_directory))
        , max_byte_count_(max_byte_count)
    {
    }

    render_text_font_source_bytes_load_result load(
        const render_text_font_source_bytes_load_request& request) const override
    {
        load_requests.push_back(request);

        font_source_resolution source = request.source;
        font_source_bytes_readiness readiness = inspect_font_source_bytes(source);

        if (source.kind == render_text_font_source_kind::fixture_uri) {
            return make_font_source_bytes_load_result(
                render_text_font_source_bytes_load_status::available_virtual_fixture,
                std::move(source),
                std::move(readiness),
                {},
                {},
                "fixture font source is virtual and already represented by readiness metadata");
        }
        if (source.kind == render_text_font_source_kind::missing) {
            return make_font_source_bytes_load_result(
                render_text_font_source_bytes_load_status::missing_source,
                std::move(source),
                std::move(readiness),
                {},
                {},
                "font source is missing");
        }
        if (source.kind == render_text_font_source_kind::unknown_uri) {
            return make_font_source_bytes_load_result(
                render_text_font_source_bytes_load_status::unsupported_source,
                std::move(source),
                std::move(readiness),
                {},
                {},
                "font source URI scheme is not supported by filesystem loader");
        }
        if (source.kind != render_text_font_source_kind::file_path
            && source.kind != render_text_font_source_kind::file_uri) {
            return make_font_source_bytes_load_result(
                render_text_font_source_bytes_load_status::unsupported_source,
                std::move(source),
                std::move(readiness),
                {},
                {},
                "font source kind is not supported by filesystem loader");
        }
        if (!is_valid_font_source_bytes_cache_key(readiness.cache_key)) {
            return make_font_source_bytes_load_result(
                render_text_font_source_bytes_load_status::invalid_cache_key,
                std::move(source),
                std::move(readiness),
                {},
                {},
                "font source bytes cache key is empty or contains control characters");
        }

        const std::filesystem::path candidate = font_source_loader_path_for(source, base_directory_);
        if (!base_directory_.empty()
            && !font_source_path_is_under_base(base_directory_, candidate)) {
            return make_font_source_bytes_load_result(
                render_text_font_source_bytes_load_status::path_traversal_blocked,
                std::move(source),
                std::move(readiness),
                candidate.generic_string(),
                {},
                "font source path escapes the configured base directory");
        }
        if (base_directory_.empty()
            && !candidate.is_absolute()
            && font_source_path_has_parent_reference(candidate)) {
            return make_font_source_bytes_load_result(
                render_text_font_source_bytes_load_status::path_traversal_blocked,
                std::move(source),
                std::move(readiness),
                candidate.generic_string(),
                {},
                "relative font source path contains parent traversal without a base directory");
        }

        std::error_code error;
        if (!std::filesystem::exists(candidate, error) || error) {
            return make_font_source_bytes_load_result(
                render_text_font_source_bytes_load_status::missing_bytes,
                std::move(source),
                std::move(readiness),
                candidate.generic_string(),
                {},
                "font source file does not exist");
        }
        if (!std::filesystem::is_regular_file(candidate, error) || error) {
            return make_font_source_bytes_load_result(
                render_text_font_source_bytes_load_status::missing_bytes,
                std::move(source),
                std::move(readiness),
                candidate.generic_string(),
                {},
                "font source path is not a regular file");
        }

        const std::uintmax_t file_size = std::filesystem::file_size(candidate, error);
        if (error) {
            return make_font_source_bytes_load_result(
                render_text_font_source_bytes_load_status::read_error,
                std::move(source),
                std::move(readiness),
                candidate.generic_string(),
                {},
                "font source file size could not be read");
        }
        if (file_size == 0U) {
            return make_font_source_bytes_load_result(
                render_text_font_source_bytes_load_status::empty_bytes,
                std::move(source),
                std::move(readiness),
                candidate.generic_string(),
                {},
                "font source file is empty");
        }
        if (file_size > max_byte_count_
            || file_size > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max())) {
            return make_font_source_bytes_load_result(
                render_text_font_source_bytes_load_status::read_error,
                std::move(source),
                std::move(readiness),
                candidate.generic_string(),
                {},
                "font source file exceeds loader byte limit");
        }

        std::vector<std::byte> bytes(static_cast<std::size_t>(file_size));
        std::ifstream file(candidate, std::ios::binary);
        if (!file.good()) {
            return make_font_source_bytes_load_result(
                render_text_font_source_bytes_load_status::read_error,
                std::move(source),
                std::move(readiness),
                candidate.generic_string(),
                {},
                "font source file could not be opened");
        }

        file.read(
            reinterpret_cast<char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        if (!file.good()) {
            return make_font_source_bytes_load_result(
                render_text_font_source_bytes_load_status::read_error,
                std::move(source),
                std::move(readiness),
                candidate.generic_string(),
                {},
                "font source file could not be fully read");
        }

        return make_font_source_bytes_load_result(
            render_text_font_source_bytes_load_status::loaded,
            std::move(source),
            std::move(readiness),
            candidate.generic_string(),
            std::move(bytes),
            {});
    }

    mutable std::vector<render_text_font_source_bytes_load_request> load_requests;

private:
    std::filesystem::path base_directory_;
    std::size_t max_byte_count_ = 0;
};

} // namespace quiz_vulkan::render
