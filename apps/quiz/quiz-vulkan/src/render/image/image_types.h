#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace quiz_vulkan::render {

using render_image_cache_key = std::string;
using render_image_texture_id = std::uint64_t;
using render_image_revision = std::uint64_t;

enum class render_image_filter {
    nearest,
    linear,
};

enum class render_image_mipmap_mode {
    none,
    nearest,
    linear,
};

enum class render_image_wrap_mode {
    clamp_to_edge,
    repeat,
    mirrored_repeat,
};

struct render_image_sampler_policy {
    render_image_filter min_filter = render_image_filter::linear;
    render_image_filter mag_filter = render_image_filter::linear;
    render_image_mipmap_mode mipmap_mode = render_image_mipmap_mode::none;
    render_image_wrap_mode wrap_u = render_image_wrap_mode::clamp_to_edge;
    render_image_wrap_mode wrap_v = render_image_wrap_mode::clamp_to_edge;

    bool operator==(const render_image_sampler_policy&) const = default;
};

enum class render_image_source_kind {
    local_path,
    file_uri,
    asset_uri,
    http_uri,
    https_uri,
    data_uri,
    unsupported,
};

struct render_resolved_image_source {
    std::string original_uri;
    std::string normalized_uri;
    render_image_source_kind kind = render_image_source_kind::local_path;

    render_image_cache_key cache_key() const
    {
        return normalized_uri;
    }

    bool is_remote() const
    {
        return kind == render_image_source_kind::http_uri || kind == render_image_source_kind::https_uri;
    }
};

enum class render_image_pixel_format {
    rgba8_unorm,
    rgba8_srgb,
};

struct render_decoded_image {
    std::size_t width = 0;
    std::size_t height = 0;
    render_image_pixel_format pixel_format = render_image_pixel_format::rgba8_srgb;
    std::vector<std::byte> pixels;

    bool empty() const
    {
        return width == 0 || height == 0 || pixels.empty();
    }
};

struct render_image_texture_handle {
    render_image_texture_id id = 0;
    render_image_revision revision = 0;
    std::size_t width = 0;
    std::size_t height = 0;

    bool valid() const
    {
        return id != 0;
    }
};

struct render_image_texture_key {
    render_image_cache_key source_key;
    render_image_sampler_policy sampler;

    bool operator==(const render_image_texture_key&) const = default;
};

} // namespace quiz_vulkan::render
