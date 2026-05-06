#pragma once

#include "render/image/image_decoder.h"
#include "render/image/image_types.h"

#include <cctype>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace quiz_vulkan::render {

inline bool is_valid_render_image_cache_key(std::string_view cache_key)
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

inline bool is_valid_render_image_texture_key(const render_image_texture_key& key)
{
    return is_valid_render_image_cache_key(key.source_key);
}

inline bool is_valid_render_image_filter(render_image_filter filter)
{
    switch (filter) {
    case render_image_filter::nearest:
    case render_image_filter::linear:
        return true;
    }

    return false;
}

inline bool is_valid_render_image_mipmap_mode(render_image_mipmap_mode mode)
{
    switch (mode) {
    case render_image_mipmap_mode::none:
    case render_image_mipmap_mode::nearest:
    case render_image_mipmap_mode::linear:
        return true;
    }

    return false;
}

inline bool is_valid_render_image_wrap_mode(render_image_wrap_mode mode)
{
    switch (mode) {
    case render_image_wrap_mode::clamp_to_edge:
    case render_image_wrap_mode::repeat:
    case render_image_wrap_mode::mirrored_repeat:
        return true;
    }

    return false;
}

inline bool is_valid_render_image_sampler_policy(const render_image_sampler_policy& sampler)
{
    return is_valid_render_image_filter(sampler.min_filter)
        && is_valid_render_image_filter(sampler.mag_filter)
        && is_valid_render_image_mipmap_mode(sampler.mipmap_mode)
        && is_valid_render_image_wrap_mode(sampler.wrap_u)
        && is_valid_render_image_wrap_mode(sampler.wrap_v);
}

inline std::string render_image_filter_name(render_image_filter filter)
{
    switch (filter) {
    case render_image_filter::nearest:
        return "nearest";
    case render_image_filter::linear:
        return "linear";
    }

    return "unknown";
}

inline std::string render_image_mipmap_mode_name(render_image_mipmap_mode mode)
{
    switch (mode) {
    case render_image_mipmap_mode::none:
        return "none";
    case render_image_mipmap_mode::nearest:
        return "nearest";
    case render_image_mipmap_mode::linear:
        return "linear";
    }

    return "unknown";
}

inline std::string render_image_wrap_mode_name(render_image_wrap_mode mode)
{
    switch (mode) {
    case render_image_wrap_mode::clamp_to_edge:
        return "clamp_to_edge";
    case render_image_wrap_mode::repeat:
        return "repeat";
    case render_image_wrap_mode::mirrored_repeat:
        return "mirrored_repeat";
    }

    return "unknown";
}

enum class render_image_texture_color_space {
    unknown,
    linear,
    srgb,
};

inline render_image_texture_color_space render_image_texture_color_space_for(
    render_image_pixel_format pixel_format)
{
    switch (pixel_format) {
    case render_image_pixel_format::rgba8_unorm:
        return render_image_texture_color_space::linear;
    case render_image_pixel_format::rgba8_srgb:
        return render_image_texture_color_space::srgb;
    }

    return render_image_texture_color_space::unknown;
}

inline std::string render_image_texture_color_space_name(render_image_texture_color_space color_space)
{
    switch (color_space) {
    case render_image_texture_color_space::unknown:
        return "unknown";
    case render_image_texture_color_space::linear:
        return "linear";
    case render_image_texture_color_space::srgb:
        return "srgb";
    }

    return "unknown";
}

inline std::string render_image_sampler_policy_stable_fragment(
    const render_image_sampler_policy& sampler)
{
    return "min=" + render_image_filter_name(sampler.min_filter)
        + ";mag=" + render_image_filter_name(sampler.mag_filter)
        + ";mipmap=" + render_image_mipmap_mode_name(sampler.mipmap_mode)
        + ";wrap_u=" + render_image_wrap_mode_name(sampler.wrap_u)
        + ";wrap_v=" + render_image_wrap_mode_name(sampler.wrap_v);
}

struct render_image_sampler_policy_diagnostic {
    render_image_sampler_policy sampler;
    std::string min_filter;
    std::string mag_filter;
    std::string mipmap_mode;
    std::string wrap_u;
    std::string wrap_v;
    std::string stable_key_fragment;
    bool valid = false;
    bool uses_nearest_filtering = false;
    bool uses_linear_filtering = false;
    bool uses_mipmaps = false;
    bool repeats_u = false;
    bool repeats_v = false;
    bool clamps_u = false;
    bool clamps_v = false;
    std::string diagnostic;
};

inline render_image_sampler_policy_diagnostic make_render_image_sampler_policy_diagnostic(
    const render_image_sampler_policy& sampler)
{
    const bool valid = is_valid_render_image_sampler_policy(sampler);
    return render_image_sampler_policy_diagnostic{
        .sampler = sampler,
        .min_filter = render_image_filter_name(sampler.min_filter),
        .mag_filter = render_image_filter_name(sampler.mag_filter),
        .mipmap_mode = render_image_mipmap_mode_name(sampler.mipmap_mode),
        .wrap_u = render_image_wrap_mode_name(sampler.wrap_u),
        .wrap_v = render_image_wrap_mode_name(sampler.wrap_v),
        .stable_key_fragment = render_image_sampler_policy_stable_fragment(sampler),
        .valid = valid,
        .uses_nearest_filtering = sampler.min_filter == render_image_filter::nearest
            || sampler.mag_filter == render_image_filter::nearest
            || sampler.mipmap_mode == render_image_mipmap_mode::nearest,
        .uses_linear_filtering = sampler.min_filter == render_image_filter::linear
            || sampler.mag_filter == render_image_filter::linear
            || sampler.mipmap_mode == render_image_mipmap_mode::linear,
        .uses_mipmaps = sampler.mipmap_mode != render_image_mipmap_mode::none,
        .repeats_u = sampler.wrap_u == render_image_wrap_mode::repeat
            || sampler.wrap_u == render_image_wrap_mode::mirrored_repeat,
        .repeats_v = sampler.wrap_v == render_image_wrap_mode::repeat
            || sampler.wrap_v == render_image_wrap_mode::mirrored_repeat,
        .clamps_u = sampler.wrap_u == render_image_wrap_mode::clamp_to_edge,
        .clamps_v = sampler.wrap_v == render_image_wrap_mode::clamp_to_edge,
        .diagnostic = valid
            ? "sampler policy is valid"
            : "sampler policy contains unsupported enum value",
    };
}

struct render_image_texture_key_diagnostic {
    render_image_texture_key key;
    render_image_sampler_policy_diagnostic sampler_policy;
    std::string stable_cache_key;
    bool valid = false;
    std::string diagnostic;
};

inline render_image_texture_key_diagnostic make_render_image_texture_key_diagnostic(
    const render_image_texture_key& key)
{
    render_image_sampler_policy_diagnostic sampler_policy =
        make_render_image_sampler_policy_diagnostic(key.sampler);
    const bool valid_source_key = is_valid_render_image_texture_key(key);
    std::string diagnostic;
    if (!valid_source_key) {
        diagnostic = "image texture key source is empty or contains control characters";
    } else if (!sampler_policy.valid) {
        diagnostic = sampler_policy.diagnostic;
    } else {
        diagnostic = "image texture key is stable";
    }
    const bool valid = valid_source_key && sampler_policy.valid;

    return render_image_texture_key_diagnostic{
        .key = key,
        .sampler_policy = std::move(sampler_policy),
        .stable_cache_key = "source=" + key.source_key + "|" + render_image_sampler_policy_stable_fragment(key.sampler),
        .valid = valid,
        .diagnostic = std::move(diagnostic),
    };
}

inline std::size_t render_image_pixel_format_byte_count(render_image_pixel_format pixel_format)
{
    switch (pixel_format) {
    case render_image_pixel_format::rgba8_unorm:
    case render_image_pixel_format::rgba8_srgb:
        return 4;
    }

    return 0;
}

inline std::size_t expected_render_decoded_image_byte_count(const render_decoded_image& image)
{
    const std::size_t bytes_per_pixel = render_image_pixel_format_byte_count(image.pixel_format);
    if (bytes_per_pixel == 0 || image.width == 0 || image.height == 0) {
        return 0;
    }

    constexpr std::size_t max_size = std::numeric_limits<std::size_t>::max();
    if (image.width > max_size / image.height) {
        return 0;
    }
    const std::size_t pixel_count = image.width * image.height;
    if (pixel_count > max_size / bytes_per_pixel) {
        return 0;
    }
    return pixel_count * bytes_per_pixel;
}

inline bool has_valid_render_decoded_image_payload(const render_decoded_image& image)
{
    const std::size_t expected_byte_count = expected_render_decoded_image_byte_count(image);
    return expected_byte_count != 0 && image.pixels.size() == expected_byte_count;
}

struct render_image_upload_readiness_snapshot {
    render_image_texture_key key;
    render_image_texture_key_diagnostic key_diagnostic;
    render_image_sampler_policy_diagnostic sampler_policy;
    render_image_decode_metadata decode_metadata;
    render_image_texture_color_space color_space = render_image_texture_color_space::unknown;
    std::string color_space_name;
    bool placeholder_fallback = false;
    bool payload_valid = false;
    bool upload_ready = false;
    std::size_t pixel_count = 0;
    std::size_t pixel_byte_count = 0;
    std::size_t decoded_byte_count = 0;
    std::size_t staging_byte_count = 0;
    std::string diagnostic;
};

inline std::size_t render_image_checked_pixel_count(const render_decoded_image& image)
{
    if (image.width == 0 || image.height == 0) {
        return 0;
    }

    constexpr std::size_t max_size = std::numeric_limits<std::size_t>::max();
    if (image.width > max_size / image.height) {
        return 0;
    }
    return image.width * image.height;
}

inline render_image_upload_readiness_snapshot make_render_image_upload_readiness_snapshot(
    const render_image_texture_key& key,
    const render_image_decode_metadata& decode_metadata,
    const render_decoded_image& image)
{
    render_image_texture_key_diagnostic key_diagnostic = make_render_image_texture_key_diagnostic(key);
    render_image_sampler_policy_diagnostic sampler_policy = key_diagnostic.sampler_policy;
    const render_image_texture_color_space color_space =
        render_image_texture_color_space_for(image.pixel_format);
    const bool payload_valid = has_valid_render_decoded_image_payload(image);
    const std::size_t decoded_byte_count = image.pixels.size();
    std::string diagnostic;
    if (!key_diagnostic.valid) {
        diagnostic = key_diagnostic.diagnostic;
    } else if (color_space == render_image_texture_color_space::unknown) {
        diagnostic = "image texture upload color space is unsupported";
    } else if (image.empty()) {
        diagnostic = "decoded image is empty";
    } else if (!payload_valid) {
        diagnostic = "decoded image payload size does not match dimensions and format";
    } else {
        diagnostic = "decoded image is upload-ready";
    }
    const bool upload_ready = key_diagnostic.valid && payload_valid
        && color_space != render_image_texture_color_space::unknown;

    return render_image_upload_readiness_snapshot{
        .key = key,
        .key_diagnostic = std::move(key_diagnostic),
        .sampler_policy = std::move(sampler_policy),
        .decode_metadata = decode_metadata,
        .color_space = color_space,
        .color_space_name = render_image_texture_color_space_name(color_space),
        .placeholder_fallback = decode_metadata.format_detection.placeholder_fallback,
        .payload_valid = payload_valid,
        .upload_ready = upload_ready,
        .pixel_count = render_image_checked_pixel_count(image),
        .pixel_byte_count = expected_render_decoded_image_byte_count(image),
        .decoded_byte_count = decoded_byte_count,
        .staging_byte_count = payload_valid ? decoded_byte_count : 0,
        .diagnostic = std::move(diagnostic),
    };
}

struct render_image_texture_key_less {
    static std::tuple<
        render_image_filter,
        render_image_filter,
        render_image_mipmap_mode,
        render_image_wrap_mode,
        render_image_wrap_mode>
    sampler_tuple(const render_image_sampler_policy& sampler)
    {
        return {
            sampler.min_filter,
            sampler.mag_filter,
            sampler.mipmap_mode,
            sampler.wrap_u,
            sampler.wrap_v,
        };
    }

    bool operator()(const render_image_texture_key& left, const render_image_texture_key& right) const
    {
        if (left.source_key != right.source_key) {
            return left.source_key < right.source_key;
        }
        return sampler_tuple(left.sampler) < sampler_tuple(right.sampler);
    }
};

} // namespace quiz_vulkan::render
