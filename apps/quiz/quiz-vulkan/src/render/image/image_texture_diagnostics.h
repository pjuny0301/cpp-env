#pragma once

#include "render/image/image_decoder.h"
#include "render/image/image_types.h"

#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

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

struct render_image_decoded_rgba8_sample {
    bool available = false;
    std::size_t pixel_index = 0;
    std::size_t byte_offset = 0;
    std::array<unsigned char, 4> rgba{};
};

struct render_image_decoded_payload_evidence {
    render_image_pixel_format pixel_format = render_image_pixel_format::rgba8_srgb;
    std::size_t pixel_count = 0;
    std::size_t expected_byte_count = 0;
    std::size_t decoded_byte_count = 0;
    std::uint64_t stable_byte_hash = 0;
    bool payload_valid = false;
    bool rgba8_sample_available = false;
    render_image_decoded_rgba8_sample first_pixel;
    render_image_decoded_rgba8_sample last_pixel;
    bool all_alpha_opaque = false;
    bool contains_transparent_alpha = false;
    std::string diagnostic;
};

struct render_image_texture_upload_payload_layout_evidence {
    render_image_texture_key texture_key;
    render_image_texture_key_diagnostic key_diagnostic;
    render_image_sampler_policy sampler;
    std::string stable_texture_cache_key;
    std::string sampler_summary;
    render_image_pixel_format pixel_format = render_image_pixel_format::rgba8_srgb;
    render_image_texture_color_space color_space = render_image_texture_color_space::unknown;
    std::string color_space_name;
    std::size_t extent_width = 0;
    std::size_t extent_height = 0;
    std::size_t bytes_per_pixel = 0;
    std::size_t row_stride_byte_count = 0;
    std::size_t pixel_count = 0;
    std::size_t expected_byte_count = 0;
    std::size_t decoded_byte_count = 0;
    std::size_t staging_byte_count = 0;
    render_image_decoded_payload_evidence decoded_payload;
    bool cache_key_valid = false;
    bool sampler_valid = false;
    bool sampler_matches_texture_key = false;
    bool rgba8_format = false;
    bool nonzero_extent = false;
    bool row_stride_consistent = false;
    bool byte_count_consistent = false;
    bool staging_byte_count_consistent = false;
    bool upload_layout_ready = false;
    std::string diagnostic;

    bool ok() const
    {
        return upload_layout_ready;
    }
};

struct render_image_upload_readiness_snapshot {
    render_image_texture_key key;
    render_image_texture_key_diagnostic key_diagnostic;
    render_image_sampler_policy_diagnostic sampler_policy;
    render_image_decode_metadata decode_metadata;
    render_image_texture_color_space color_space = render_image_texture_color_space::unknown;
    std::string color_space_name;
    bool placeholder_fallback = false;
    bool payload_valid = false;
    bool decode_metadata_matches_image = false;
    bool upload_ready = false;
    std::size_t pixel_count = 0;
    std::size_t pixel_byte_count = 0;
    std::size_t decoded_byte_count = 0;
    std::size_t metadata_decoded_byte_count = 0;
    std::size_t metadata_expected_decoded_byte_count = 0;
    std::size_t metadata_actual_decoded_byte_count = 0;
    std::size_t staging_byte_count = 0;
    render_image_decoded_payload_evidence decoded_payload;
    render_image_texture_upload_payload_layout_evidence upload_payload_layout;
    std::string decode_handoff_diagnostic;
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

inline std::uint64_t render_image_decoded_payload_stable_hash(const std::vector<std::byte>& pixels)
{
    std::uint64_t hash = 1469598103934665603ull;
    for (const std::byte value : pixels) {
        hash ^= static_cast<std::uint64_t>(std::to_integer<unsigned char>(value));
        hash *= 1099511628211ull;
    }
    return hash;
}

inline render_image_decoded_rgba8_sample make_render_image_decoded_rgba8_sample(
    const render_decoded_image& image,
    std::size_t pixel_index)
{
    render_image_decoded_rgba8_sample sample{
        .available = false,
        .pixel_index = pixel_index,
        .byte_offset = pixel_index * 4,
        .rgba = {},
    };
    if (render_image_pixel_format_byte_count(image.pixel_format) != 4
        || image.pixels.size() < sample.byte_offset + 4) {
        return sample;
    }

    sample.available = true;
    sample.rgba = {
        std::to_integer<unsigned char>(image.pixels[sample.byte_offset]),
        std::to_integer<unsigned char>(image.pixels[sample.byte_offset + 1]),
        std::to_integer<unsigned char>(image.pixels[sample.byte_offset + 2]),
        std::to_integer<unsigned char>(image.pixels[sample.byte_offset + 3]),
    };
    return sample;
}

inline render_image_decoded_payload_evidence make_render_image_decoded_payload_evidence(
    const render_decoded_image& image)
{
    const std::size_t expected_byte_count = expected_render_decoded_image_byte_count(image);
    const std::size_t decoded_byte_count = image.pixels.size();
    const std::size_t pixel_count = render_image_checked_pixel_count(image);
    const bool payload_valid = has_valid_render_decoded_image_payload(image);
    const bool rgba8_sample_available = payload_valid
        && pixel_count != 0
        && render_image_pixel_format_byte_count(image.pixel_format) == 4;

    bool all_alpha_opaque = false;
    bool contains_transparent_alpha = false;
    if (rgba8_sample_available) {
        all_alpha_opaque = true;
        for (std::size_t alpha_offset = 3; alpha_offset < image.pixels.size(); alpha_offset += 4) {
            const unsigned char alpha = std::to_integer<unsigned char>(image.pixels[alpha_offset]);
            if (alpha != 0xff) {
                all_alpha_opaque = false;
                contains_transparent_alpha = true;
            }
        }
    }

    std::string diagnostic;
    if (render_image_pixel_format_byte_count(image.pixel_format) == 0) {
        diagnostic = "decoded payload evidence has unsupported pixel format";
    } else if (image.empty()) {
        diagnostic = "decoded payload evidence has empty image";
    } else if (!payload_valid) {
        diagnostic = "decoded payload evidence has invalid byte count";
    } else {
        diagnostic = "decoded payload evidence recorded";
    }

    return render_image_decoded_payload_evidence{
        .pixel_format = image.pixel_format,
        .pixel_count = pixel_count,
        .expected_byte_count = expected_byte_count,
        .decoded_byte_count = decoded_byte_count,
        .stable_byte_hash = render_image_decoded_payload_stable_hash(image.pixels),
        .payload_valid = payload_valid,
        .rgba8_sample_available = rgba8_sample_available,
        .first_pixel = rgba8_sample_available
            ? make_render_image_decoded_rgba8_sample(image, 0)
            : render_image_decoded_rgba8_sample{},
        .last_pixel = rgba8_sample_available
            ? make_render_image_decoded_rgba8_sample(image, pixel_count - 1)
            : render_image_decoded_rgba8_sample{},
        .all_alpha_opaque = all_alpha_opaque,
        .contains_transparent_alpha = contains_transparent_alpha,
        .diagnostic = std::move(diagnostic),
    };
}

inline std::size_t render_image_checked_row_stride_byte_count(const render_decoded_image& image)
{
    const std::size_t bytes_per_pixel = render_image_pixel_format_byte_count(image.pixel_format);
    if (bytes_per_pixel == 0 || image.width == 0) {
        return 0;
    }

    constexpr std::size_t max_size = std::numeric_limits<std::size_t>::max();
    if (image.width > max_size / bytes_per_pixel) {
        return 0;
    }
    return image.width * bytes_per_pixel;
}

inline render_image_texture_upload_payload_layout_evidence
make_render_image_texture_upload_payload_layout_evidence(
    const render_image_texture_key& key,
    const render_image_sampler_policy& sampler,
    const render_decoded_image& image)
{
    render_image_texture_key_diagnostic key_diagnostic = make_render_image_texture_key_diagnostic(key);
    render_image_sampler_policy_diagnostic sampler_diagnostic =
        make_render_image_sampler_policy_diagnostic(sampler);
    render_image_decoded_payload_evidence decoded_payload =
        make_render_image_decoded_payload_evidence(image);
    const render_image_texture_color_space color_space =
        render_image_texture_color_space_for(image.pixel_format);
    const std::size_t bytes_per_pixel = render_image_pixel_format_byte_count(image.pixel_format);
    const std::size_t row_stride_byte_count = render_image_checked_row_stride_byte_count(image);
    const std::size_t pixel_count = render_image_checked_pixel_count(image);
    const std::size_t expected_byte_count = expected_render_decoded_image_byte_count(image);
    const std::size_t decoded_byte_count = image.pixels.size();
    const bool sampler_matches_texture_key = key.sampler == sampler;
    const bool cache_key_valid = key_diagnostic.valid;
    const std::string stable_texture_cache_key = key_diagnostic.stable_cache_key;
    const bool rgba8_format = bytes_per_pixel == 4 && color_space != render_image_texture_color_space::unknown;
    const bool nonzero_extent = image.width != 0 && image.height != 0;
    const bool row_stride_consistent = nonzero_extent && row_stride_byte_count != 0;
    const bool byte_count_consistent = expected_byte_count != 0
        && decoded_byte_count == expected_byte_count
        && decoded_payload.payload_valid;
    const std::size_t staging_byte_count = byte_count_consistent ? decoded_byte_count : 0;
    const bool staging_byte_count_consistent =
        byte_count_consistent && staging_byte_count == expected_byte_count;
    const bool upload_layout_ready = cache_key_valid
        && sampler_diagnostic.valid
        && sampler_matches_texture_key
        && rgba8_format
        && nonzero_extent
        && row_stride_consistent
        && byte_count_consistent
        && staging_byte_count_consistent;

    std::string diagnostic;
    if (!key_diagnostic.valid) {
        diagnostic = key_diagnostic.diagnostic;
    } else if (!sampler_diagnostic.valid) {
        diagnostic = sampler_diagnostic.diagnostic;
    } else if (!sampler_matches_texture_key) {
        diagnostic = "image texture upload payload layout sampler does not match texture key";
    } else if (!rgba8_format) {
        diagnostic = "image texture upload payload layout requires RGBA8 pixel format";
    } else if (!nonzero_extent) {
        diagnostic = "image texture upload payload layout requires non-zero extent";
    } else if (!row_stride_consistent) {
        diagnostic = "image texture upload payload layout row stride overflows";
    } else if (!byte_count_consistent) {
        diagnostic = "image texture upload payload layout byte count does not match extent";
    } else if (!staging_byte_count_consistent) {
        diagnostic = "image texture upload payload layout staging byte count is inconsistent";
    } else {
        diagnostic = "image texture upload payload layout is ready";
    }

    return render_image_texture_upload_payload_layout_evidence{
        .texture_key = key,
        .key_diagnostic = std::move(key_diagnostic),
        .sampler = sampler,
        .stable_texture_cache_key = stable_texture_cache_key,
        .sampler_summary = render_image_sampler_policy_stable_fragment(sampler),
        .pixel_format = image.pixel_format,
        .color_space = color_space,
        .color_space_name = render_image_texture_color_space_name(color_space),
        .extent_width = image.width,
        .extent_height = image.height,
        .bytes_per_pixel = bytes_per_pixel,
        .row_stride_byte_count = row_stride_byte_count,
        .pixel_count = pixel_count,
        .expected_byte_count = expected_byte_count,
        .decoded_byte_count = decoded_byte_count,
        .staging_byte_count = staging_byte_count,
        .decoded_payload = std::move(decoded_payload),
        .cache_key_valid = cache_key_valid,
        .sampler_valid = sampler_diagnostic.valid,
        .sampler_matches_texture_key = sampler_matches_texture_key,
        .rgba8_format = rgba8_format,
        .nonzero_extent = nonzero_extent,
        .row_stride_consistent = row_stride_consistent,
        .byte_count_consistent = byte_count_consistent,
        .staging_byte_count_consistent = staging_byte_count_consistent,
        .upload_layout_ready = upload_layout_ready,
        .diagnostic = std::move(diagnostic),
    };
}

inline bool render_image_texture_upload_payload_layout_evidence_equal(
    const render_image_texture_upload_payload_layout_evidence& before,
    const render_image_texture_upload_payload_layout_evidence& after)
{
    return before.texture_key == after.texture_key
        && before.sampler == after.sampler
        && before.stable_texture_cache_key == after.stable_texture_cache_key
        && before.sampler_summary == after.sampler_summary
        && before.pixel_format == after.pixel_format
        && before.color_space == after.color_space
        && before.extent_width == after.extent_width
        && before.extent_height == after.extent_height
        && before.bytes_per_pixel == after.bytes_per_pixel
        && before.row_stride_byte_count == after.row_stride_byte_count
        && before.pixel_count == after.pixel_count
        && before.expected_byte_count == after.expected_byte_count
        && before.decoded_byte_count == after.decoded_byte_count
        && before.staging_byte_count == after.staging_byte_count
        && before.decoded_payload.stable_byte_hash == after.decoded_payload.stable_byte_hash
        && before.decoded_payload.payload_valid == after.decoded_payload.payload_valid
        && before.cache_key_valid == after.cache_key_valid
        && before.sampler_valid == after.sampler_valid
        && before.sampler_matches_texture_key == after.sampler_matches_texture_key
        && before.rgba8_format == after.rgba8_format
        && before.nonzero_extent == after.nonzero_extent
        && before.row_stride_consistent == after.row_stride_consistent
        && before.byte_count_consistent == after.byte_count_consistent
        && before.staging_byte_count_consistent == after.staging_byte_count_consistent
        && before.upload_layout_ready == after.upload_layout_ready;
}

inline render_image_upload_readiness_snapshot make_render_image_upload_readiness_snapshot(
    const render_image_texture_key& key,
    const render_image_decode_metadata& decode_metadata,
    const render_decoded_image& image)
{
    render_image_texture_key_diagnostic key_diagnostic = make_render_image_texture_key_diagnostic(key);
    render_image_sampler_policy_diagnostic sampler_policy = key_diagnostic.sampler_policy;
    render_image_texture_upload_payload_layout_evidence upload_payload_layout =
        make_render_image_texture_upload_payload_layout_evidence(key, key.sampler, image);
    const render_image_texture_color_space color_space =
        render_image_texture_color_space_for(image.pixel_format);
    const bool payload_valid = has_valid_render_decoded_image_payload(image);
    const std::size_t decoded_byte_count = image.pixels.size();
    const std::size_t expected_byte_count = expected_render_decoded_image_byte_count(image);
    const bool decode_metadata_matches_image = decode_metadata.has_image()
        && decode_metadata.size_validation.valid
        && decode_metadata.width == image.width
        && decode_metadata.height == image.height
        && decode_metadata.pixel_format == image.pixel_format
        && decode_metadata.decoded_byte_count == decoded_byte_count
        && decode_metadata.size_validation.expected_decoded_byte_count == expected_byte_count
        && decode_metadata.size_validation.actual_decoded_byte_count == decoded_byte_count;
    std::string decode_handoff_diagnostic;
    if (!decode_metadata.has_image()) {
        decode_handoff_diagnostic = "decode handoff metadata has no image";
    } else if (!decode_metadata.size_validation.valid) {
        decode_handoff_diagnostic = "decode handoff metadata failed size validation: "
            + decode_metadata.size_validation.diagnostic;
    } else if (!decode_metadata_matches_image) {
        decode_handoff_diagnostic = "decode handoff metadata does not match decoded image";
    } else {
        decode_handoff_diagnostic = "decode handoff metadata matches decoded image";
    }

    std::string diagnostic;
    if (!key_diagnostic.valid) {
        diagnostic = key_diagnostic.diagnostic;
    } else if (color_space == render_image_texture_color_space::unknown) {
        diagnostic = "image texture upload color space is unsupported";
    } else if (image.empty()) {
        diagnostic = "decoded image is empty";
    } else if (!payload_valid) {
        diagnostic = "decoded image payload size does not match dimensions and format";
    } else if (!decode_metadata_matches_image) {
        diagnostic = decode_handoff_diagnostic;
    } else {
        diagnostic = "decoded image is upload-ready";
    }
    const bool upload_ready = key_diagnostic.valid && payload_valid
        && color_space != render_image_texture_color_space::unknown
        && decode_metadata_matches_image
        && upload_payload_layout.upload_layout_ready;

    return render_image_upload_readiness_snapshot{
        .key = key,
        .key_diagnostic = std::move(key_diagnostic),
        .sampler_policy = std::move(sampler_policy),
        .decode_metadata = decode_metadata,
        .color_space = color_space,
        .color_space_name = render_image_texture_color_space_name(color_space),
        .placeholder_fallback = decode_metadata.format_detection.placeholder_fallback,
        .payload_valid = payload_valid,
        .decode_metadata_matches_image = decode_metadata_matches_image,
        .upload_ready = upload_ready,
        .pixel_count = render_image_checked_pixel_count(image),
        .pixel_byte_count = expected_byte_count,
        .decoded_byte_count = decoded_byte_count,
        .metadata_decoded_byte_count = decode_metadata.decoded_byte_count,
        .metadata_expected_decoded_byte_count =
            decode_metadata.size_validation.expected_decoded_byte_count,
        .metadata_actual_decoded_byte_count =
            decode_metadata.size_validation.actual_decoded_byte_count,
        .staging_byte_count = payload_valid ? decoded_byte_count : 0,
        .decoded_payload = make_render_image_decoded_payload_evidence(image),
        .upload_payload_layout = std::move(upload_payload_layout),
        .decode_handoff_diagnostic = std::move(decode_handoff_diagnostic),
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
